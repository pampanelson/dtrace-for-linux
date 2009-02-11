#
# Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
#
# The contents of this file are subject to the terms of the
# Common Development and Distribution License, Version 1.0 only.
# See the file usr/src/LICENSING.NOTICE in this distribution or
# http://www.opensolaris.org/license/ for details.
#
#ident	"@(#)Makefile.com	1.16	04/11/13 SMI"
#

LIBRARY = libdtrace.a
VERS = .1

LIBSRCS = \
	dt_aggregate.c \
	dt_as.c \
	dt_cc.c \
	dt_cg.c \
	dt_consume.c \
	dt_decl.c \
	dt_dis.c \
	dt_dof.c \
	dt_error.c \
	dt_errtags.c \
	dt_handle.c \
	dt_ident.c \
	dt_inttab.c \
	dt_link.c \
	dt_list.c \
	dt_open.c \
	dt_options.c \
	dt_program.c \
	dt_map.c \
	dt_module.c \
	dt_parser.c \
	dt_pid.c \
	dt_pragma.c \
	dt_printf.c \
	dt_proc.c \
	dt_provider.c \
	dt_regset.c \
        dt_string.c \
	dt_strtab.c \
	dt_subr.c \
	dt_work.c \
	dt_xlator.c

LIBISASRCS = \
	dt_isadep.c

OBJECTS = dt_lex.o dt_grammar.o $(MACHOBJS) $(LIBSRCS:%.c=%.o) $(LIBISASRCS:%.c=%.o)

DRTISRC = drti.c
DRTIOBJ = $(DRTISRC:%.c=%.o)

DLIBSRCS += \
	errno.d \
	io.d \
	procfs.d \
	regs.d \
	sched.d \
	signal.d \
	unistd.d

include ../../Makefile.lib

SRCS = $(LIBSRCS:%.c=../common/%.c) $(LIBISASRCS:%.c=../$(MACH)/%.c) 
LIBS = $(DYNLIB) $(LINTLIB)

SRCDIR = ../common
SPECMAPFILE = $(MAPDIR)/mapfile

CLEANFILES += dt_lex.c dt_grammar.c dt_grammar.h y.output
CLEANFILES += ../common/procfs.sed ../common/procfs.d
CLEANFILES += ../common/io.sed ../common/io.d
CLEANFILES += ../common/dt_errtags.c ../common/errno.d ../common/signal.d

CLOBBERFILES += drti.o

CPPFLAGS += -I../common -I.
CFLAGS += $(CCVERBOSE) $(C_BIGPICFLAGS)
CFLAGS64 += $(CCVERBOSE) $(C_BIGPICFLAGS)
YYCFLAGS =
LDLIBS += -lgen -lproc -lrtld_db -lctf -lelf -lc
DRTILDLIBS = $(LDLIBS.lib) -lc

yydebug := YYCFLAGS += -DYYDEBUG

$(LINTLIB) := SRCS = $(SRCDIR)/$(LINTSRC)

LFLAGS = -t -v
YFLAGS = -d -v

ROOTDLIBDIR = $(ROOT)/usr/lib/dtrace
ROOTDLIBDIR64 = $(ROOT)/usr/lib/dtrace/64

ROOTDLIBS = $(DLIBSRCS:%=$(ROOTDLIBDIR)/%)
ROOTDOBJS = $(ROOTDLIBDIR)/$(DRTIOBJ)
ROOTDOBJS64 = $(ROOTDLIBDIR64)/$(DRTIOBJ)

.KEEP_STATE:

all: $(LIBS) $(DRTIOBJ)

lint: lintdrti lintcheck

lintdrti: ../common/$(DRTISRC)
	$(LINT.c) ../common/$(DRTISRC) $(DRTILDLIBS)

dt_lex.c: $(SRCDIR)/dt_lex.l dt_grammar.h
	$(LEX) $(LFLAGS) $(SRCDIR)/dt_lex.l > $@

dt_grammar.c dt_grammar.h: $(SRCDIR)/dt_grammar.y
	$(YACC) $(YFLAGS) $(SRCDIR)/dt_grammar.y
	@mv y.tab.h dt_grammar.h
	@mv y.tab.c dt_grammar.c

pics/dt_lex.o pics/dt_grammar.o := CFLAGS += $(YYCFLAGS)
pics/dt_lex.o pics/dt_grammar.o := CFLAGS64 += $(YYCFLAGS)

pics/dt_lex.o pics/dt_grammar.o := CERRWARN += -erroff=E_STATEMENT_NOT_REACHED
pics/dt_lex.o pics/dt_grammar.o := CCVERBOSE =

../common/dt_errtags.c: ../common/mkerrtags.sh ../common/dt_errtags.h
	sh ../common/mkerrtags.sh < ../common/dt_errtags.h > $@

../common/errno.d: ../common/mkerrno.sh $(SRC)/uts/common/sys/errno.h
	sh ../common/mkerrno.sh < $(SRC)/uts/common/sys/errno.h > $@

../common/signal.d: ../common/mksignal.sh $(SRC)/uts/common/sys/iso/signal_iso.h
	sh ../common/mksignal.sh < $(SRC)/uts/common/sys/iso/signal_iso.h > $@

../common/%.sed: ../common/%.sed.in
	$(COMPILE.cpp) -D_KERNEL $< | tr -d ' ' | tr '"' '@' | grep '^s/' > $@

../common/procfs.d: ../common/procfs.sed ../common/procfs.d.in
	sed -f ../common/procfs.sed < ../common/procfs.d.in > $@

../common/io.d: ../common/io.sed ../common/io.d.in
	sed -f ../common/io.sed < ../common/io.d.in > $@

pics/%.o: ../$(MACH)/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

pics/%.o: ../$(MACH)/%.s
	$(COMPILE.s) -o $@ $<
	$(POST_PROCESS_O)

%.o: ../common/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

$(ROOTDLIBDIR):
	$(INS.dir)

$(ROOTDLIBDIR64): $(ROOTDLIBDIR)
	$(INS.dir)

$(ROOTDLIBDIR)/%.d: ../common/%.d
	$(INS.file)

$(ROOTDLIBDIR)/%.d: ../$(MACH)/%.d
	$(INS.file)

$(ROOTDLIBDIR)/%.d: %.d
	$(INS.file)

$(ROOTDLIBDIR)/%.o: %.o
	$(INS.file)

$(ROOTDLIBDIR64)/%.o: %.o
	$(INS.file)

$(ROOTDLIBS): $(ROOTDLIBDIR)

$(ROOTDOBJS): $(ROOTDLIBDIR)

$(ROOTDOBJS64): $(ROOTDLIBDIR64)

include ../../Makefile.targ
