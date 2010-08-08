#
# Makefile for ZINC.
#

DEL=rm -f

SDL_INC=$(shell sdl-config --cflags)
SDL_LIB=$(shell sdl-config --libs)

CC=gcc
CFLAGS=-std=c99 -pedantic -Wall -g -O2 -fomit-frame-pointer -pipe $(SDL_INC)

LFLAGS=$(SDL_LIB)

OBJECTS=\
  zinc.o \
  zasm.o \
  exec.o \
  sym.o \
  expr.o \
  dump.o \
  sdlui.o \
  sdltxt.o \

PROG=zinc

.PHONY: clean

$(PROG): $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) -o $(PROG) $(LFLAGS)

clean:
	$(DEL) $(OBJECTS)
	$(DEL) $(PROG)

keyword.h: keyword.gperf
	gperf -L ANSI-C -t -C -F ', TK_INVALID' -N find_keyword -m 10 \
	keyword.gperf >keyword.h

# Manual enumeration of dependencies. FIXME.

zinc.o:  zinc.h  zasm.h  exec.h  sdlui.h  dump.h

zasm.o:  zinc.h  zasm.h  expr.h  sym.h  keyword.h

exec.o:  zinc.h  exec.h  sdlui.h

expr.o:  zinc.h  zasm.h  expr.h  sym.h

sym.o:  zinc.h  zasm.h  expr.h  sym.h

dump.o:  zinc.h  dump.h

sdlui.o:  zinc.h  dump.h  sdlui.h  sdltxt.h

sdltxt.o:  sdltxt.h
