# ParaZ. Copyright (C) 2006-2007, Index Data.
# $Id: Makefile.am,v 1.2 2007-01-29 03:19:25 quinn Exp $

bin_PROGRAMS = pazpar2

MAINTAINERCLEANFILES = Makefile.in cconfig.h

AM_CFLAGS = $(YAZINC)
LDADD = $(YAZLIB)

pazpar2_SOURCES = config.c config.h eventl.c eventl.h \
	http.c http_command.c http_command.h http.h \
	pazpar2.c pazpar2.h reclists.c reclists.h \
	relevance.c relevance.h termlists.c termlists.h \
	util.c util.h zeerex.c zeerex.h
