CPP_USER=g++
CFLAGS_USER=-O3 -MMD
EXTRA_CFLAGS=-mhard-float
KDIR := /lib/modules/$(shell uname -r)/build

obj-m := nf-hishape.o

HISHAPE_OBJS = nf-hishape-user.o nf-hishape_util.o
DEPS = nf-hishape-user.d nf-hishape_util.d
PACKAGE = nf-HiShape
VERSION = 0.1
distdir = $(PACKAGE)-$(VERSION)

all: modules nf-hishape

-include ${DEPS}

modules:
	$(MAKE) -C $(KDIR) SUBDIRS=$(shell pwd) modules

nf-hishape: ${HISHAPE_OBJS}
	${CPP_USER} ${CFLAGS_USER} ${HISHAPE_OBJS} -o $@

%.o: %.cc
	${CPP_USER} ${CFLAGS_USER} ${INCLUDES} -c $< -o $@
	
install:
	install -m 644 nf-hishape.ko /lib/modules/`uname -r`/kernel/net/ipv4/netfilter/nf-hishape.ko
	/sbin/depmod -a
	install -m 755 nf-hishape /sbin/nf-hishape
	install -m 555 nf-hishape.man `manpath | cut -d\: -f1`/man8/nf-hishape.8

uninstall:
	rm -f /lib/modules/`uname -r`/kernel/net/ipv4/netfilter/nf-hishape.ko
	/sbin/depmod -a
	rm -f /sbin/nf-hishape
	rm -f `manpath | cut -d\: -f1`/man8/nf-hishape.8

clean:
	rm -f .nf-hishape*
	rm -f nf-hishape.o nf-hishape.ko nf-hishape.mod.o nf-hishape.mod.c nf-hishape
	rm -f ${HISHAPE_OBJS}
	rm -f ${DEPS}

dist:
	@rm -rf $(distdir)
	@mkdir $(distdir)
	@ln -s ../Makefile $(distdir)/Makefile
	@ln -s ../README $(distdir)/README
	@ln -s ../LICENSE $(distdir)/LICENSE
	@ln -s ../nf-hishape_util.cc $(distdir)/nf-hishape_util.cc
	@ln -s ../nf-hishape_util.h $(distdir)/nf-hishape_util.h
	@ln -s ../nf-hishape.c $(distdir)/nf-hishape.c
	@ln -s ../nf-hishape.h $(distdir)/nf-hishape.h
	@ln -s ../nf-hishape.man $(distdir)/nf-hishape.man
	@ln -s ../nf-hishape-user.cc $(distdir)/nf-hishape-user.cc
	@tar chozf $(distdir).tar.gz $(distdir)
	@rm -rf $(distdir)
