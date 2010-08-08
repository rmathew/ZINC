#
# Makefile for creating the documentation for ZINC.
#

DEL=rm -f

MAIN_DOC_SRC=zinc.texi

DOC_SRCS=\
  $(MAIN_DOC_SRC) \
  intro.texi \
  guide.texi \
  ref.texi \
  design.texi \

.PHONY: all clean

all: zinc.html

zinc.html: $(DOC_SRCS)
	makeinfo --html --no-split --no-headers --output=$@ $(MAIN_DOC_SRC)

zinc.pdf: $(DOC_SRCS)
	texi2pdf $(MAIN_DOC_SRC)

zinc.info: $(DOC_SRCS)
	makeinfo $(MAIN_DOC_SRC)

clean:
	$(DEL) zinc.html
	$(DEL) zinc.pdf zinc.aux zinc.cp zinc.cps zinc.fn zinc.ky zinc.log \
	  zinc.pg zinc.toc zinc.tp zinc.vr
	$(DEL) zinc.info
