bin_PROGRAMS = %D%/hubble
%C%_hubble_SOURCES = %D%/hubble.c
%C%_hubble_CPPFLAGS = -DEXECDIR='"$(pkglibexecdir)"'

pkglibexec_PROGRAMS = %D%/responder
%C%_responder_SOURCES = %D%/responder.c
