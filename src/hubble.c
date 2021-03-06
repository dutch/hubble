#define _GNU_SOURCE
#define MAXERR 256
#define MAXBUF 64
#define TYPE_EOF 0
#define TYPE_HEADER_KEY 1
#define TYPE_HEADER_VALUE 2

#include "picohttpparser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/signalfd.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

int bind_to_port(const char *port, char *err, size_t errlen) {
  int ret, res, sockfd, yes;
  struct addrinfo hints, *info, *p;

  ret = -1;
  yes = 1;

  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  if ((res = getaddrinfo(NULL, "5000", &hints, &info)) != 0) {
    snprintf(err, errlen, "getaddrinfo: %s", gai_strerror(res));
    goto done;
  }

  for (p = info; p != NULL; p = p->ai_next) {
    if ((sockfd = socket(p->ai_family, p->ai_socktype | SOCK_NONBLOCK | SOCK_CLOEXEC, p->ai_protocol)) == -1) {
      snprintf(err, errlen, "socket: %s", strerror(errno));
      continue;
    }

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
      snprintf(err, errlen, "sockopt: %s", strerror(errno));
      goto clean1;
    }

    if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
      snprintf(err, errlen, "bind: %s", strerror(errno));
      close(sockfd);
      goto clean1;
    }

    break;
  }

  ret = sockfd;

clean1:
  freeaddrinfo(info);

done:
  return ret;
}

void *get_in_addr(struct sockaddr *sa) {
  if (sa->sa_family == AF_INET)
    return &((struct sockaddr_in *)sa)->sin_addr;
  return &((struct sockaddr_in6 *)sa)->sin6_addr;
}

int main(int argc, char **argv) {
  char err[MAXERR], buf[MAXBUF];
  int ret, epfd, sigfd, nfds, sockfd, connfd;
  ssize_t res;
  sigset_t mask;
  pid_t pid;
  struct epoll_event ev, evs[10];
  socklen_t socklen;
  struct sockaddr_storage client;
  struct signalfd_siginfo fdsi;

  ret = EXIT_FAILURE;

  sigemptyset(&mask);
  sigaddset(&mask, SIGINT);
  sigaddset(&mask, SIGCHLD);

  if ((epfd = epoll_create1(0)) == -1) {
    perror("epoll_create1");
    goto done;
  }

  if ((sigfd = signalfd(-1, &mask, SFD_NONBLOCK)) == -1) {
    perror("signalfd");
    goto done;
  }

  ev.events = EPOLLIN | EPOLLET;
  ev.data.fd = sigfd;

  if (epoll_ctl(epfd, EPOLL_CTL_ADD, sigfd, &ev) == -1) {
    perror("epoll_ctl");
    goto done;
  }

  if ((sockfd = bind_to_port("5000", err, MAXERR)) == -1) {
    fprintf(stderr, "bind_to_port: %s\n", err);
    goto done;
  }

  if (listen(sockfd, 10) == -1) {
    perror("listen");
    goto done;
  }

  ev.events = EPOLLIN | EPOLLET;
  ev.data.fd = sockfd;

  if (epoll_ctl(epfd, EPOLL_CTL_ADD, sockfd, &ev) == -1) {
    perror("epoll_ctl");
    goto done;
  }

  if (sigprocmask(SIG_BLOCK, &mask, NULL) == -1) {
    perror("sigprocmask");
    goto done;
  }

  for (;;) {
    if ((nfds = epoll_wait(epfd, evs, 10, -1)) == -1 && errno != EINTR) {
      perror("epoll_wait");
      break;
    }

    while (nfds --> 0) {
      if (evs[nfds].data.fd == sigfd) {
        for (;;) {
          if ((res = read(sigfd, &fdsi, sizeof(struct signalfd_siginfo))) == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
              break;
            perror("read");
            goto done;
          }

          switch (fdsi.ssi_signo) {
          case SIGCHLD:
            while (waitpid(-1, 0, WNOHANG) > 0);
            break;

          case SIGINT:
            printf("Caught SIGINT\n");
            goto done;
          }
        }
      }

      else if (evs[nfds].data.fd == sockfd) {
        for (;;) {
          if ((connfd = accept4(sockfd, (struct sockaddr *)&client, &socklen, SOCK_NONBLOCK | SOCK_CLOEXEC)) == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
              break;
            perror("accept4");
            goto done;
          }

          ev.events = EPOLLIN | EPOLLRDHUP | EPOLLET;
          ev.data.fd = connfd;

          if (epoll_ctl(epfd, EPOLL_CTL_ADD, connfd, &ev) == -1) {
            perror("epoll_ctl");
            goto done;
          }
        }
      }

      else if (evs[nfds].events & EPOLLIN) {
        while ((res = recv(evs[nfds].data.fd, buf, MAXBUF, 0)) > 0) {
          if (res == -1 && errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("recv");
            goto done;
          }
	}

	evs[nfds].events = EPOLLOUT | EPOLLET;

        if (epoll_ctl(epfd, EPOLL_CTL_MOD, evs[nfds].data.fd, &evs[nfds]) == -1) {
          perror("epoll_ctl");
          goto done;
        }
      }

      else if (evs[nfds].events & EPOLLOUT) {
        send(evs[nfds].data.fd, "HTTP/1.1 200 OK\r\n\r\n", 19, 0);
        close(evs[nfds].data.fd);

        pid = fork();

        switch (pid) {
        case -1:
          perror("fork");
          goto done;

        case 0:
          sigprocmask(SIG_UNBLOCK, &mask, NULL);
          execl(EXECDIR "/responder", "responder", NULL);

        default:
          break;
        }
      }
    }
  }

  ret = EXIT_SUCCESS;

done:
  return ret;
}
