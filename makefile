#! /bin/make

rel=`date +%Y%m%d`

notice:
	echo rel=$(rel)
	@echo "make release    - create a new tarball for distribution"
	@echo "make all        - build everything (which builds!)"

release:
	cd .. ; mv dtrace dtrace-$(rel) ; \
	tar cvf - --exclude=*.o --exclude=*.ko --exclude=*.a --exclude=tags \
		dtrace-$(rel) | bzip2 >/tmp/dtrace-$(rel).tar.bz2 ; \
	mv dtrace-$(rel) dtrace
	rcp /tmp/dtrace-$(rel).tar.bz2 minny:release/website/dtrace
	ls -l /tmp/dtrace-$(rel).tar.bz2

all:
	cd libctf ; $(MAKE)
	cd libdtrace/common ; $(MAKE)
	cd liblinux ; $(MAKE)
	cd cmd/dtrace ; $(MAKE)
	cd drivers/dtrace ; ./make-me
