#include <stdio.h>
#include <sys/select.h>

int main(int argc, char **argv) {
  struct timeval tv = {5, 0};
  printf("responding...\n");
  if (select(0, NULL, NULL, NULL, &tv) == -1)
    perror("select");
  return 0;
}
