CCOPTS=-O
NROFFOPTS=-man

default: all

all: squeeze unsqueeze squeeze.1 unsqueeze.1 sqz.1 sqzdir.1 mwcat.1

squeeze: squeeze.c Makefile
	cc $(CCOPTS) -o squeeze squeeze.c

unsqueeze: unsqueeze.c Makefile
	cc $(CCOPTS) -o unsqueeze unsqueeze.c

squeeze.1: squeeze.man Makefile
	nroff $(NROFFOPTS) < squeeze.man > squeeze.1

unsqueeze.1: unsqueeze.man Makefile
	nroff $(NROFFOPTS) < unsqueeze.man > unsqueeze.1

sqz.1: sqz.man Makefile
	nroff $(NROFFOPTS) < sqz.man > sqz.1

sqzdir.1: sqzdir.man Makefile
	nroff $(NROFFOPTS) < sqzdir.man > sqzdir.1

mwcat.1: mwcat.man Makefile
	nroff $(NROFFOPTS) < mwcat.man > mwcat.1
