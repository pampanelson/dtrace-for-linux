#
# Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
#
# The contents of this file are subject to the terms of the
# Common Development and Distribution License, Version 1.0 only.
# See the file usr/src/LICENSING.NOTICE in this distribution or
# http://www.opensolaris.org/license/ for details.
#
#ident	"@(#)Makefile	1.3	04/06/11 SMI"

ASFLAGS += -D_ASM -K PIC -P

MAPDIR = ../spec/sparcv9
include ../Makefile.com
include ../../Makefile.lib.64

CPPFLAGS += -D_ELF64

SRCS += ../sparc/dt_asmsubr.s
OBJECTS += dt_asmsubr.o

install yydebug: all $(ROOTLIBS64) $(ROOTLINKS64) $(ROOTLINT64) \
	$(ROOTDLIBS) $(ROOTDOBJS64)
