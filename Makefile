ifndef NAVISERVER
    NAVISERVER  = /usr/local/ns
endif

#
# Module name
#
MOD      =  libtrequests.so

#
# Objects to build.
#
MODOBJS     = src/library.o

#MODLIBS  +=

CFLAGS += -DUSE_NAVISERVER

include  $(NAVISERVER)/include/Makefile.module