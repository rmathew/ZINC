#
# Makefile for ZINC version 0.1.
# Copyright (C) 2006 Ranjit Mathew <rmathew@gmail.com>.
#
# NOTE: Assumes GNU Make and a UNIX-like build environment.
#

SUBDIRS=src doc examples

.PHONY: all clean

all:
	for dir in $(SUBDIRS); do \
	  if [ -f $$dir/Makefile ]; then \
	    $(MAKE) -C $$dir; \
	  fi; \
	done

clean:
	for dir in $(SUBDIRS); do \
	  if [ -f $$dir/Makefile ]; then \
	    $(MAKE) -C $$dir clean; \
	  fi; \
	done
