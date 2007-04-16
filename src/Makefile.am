# $Id: Makefile.am,v 1.7 2007-04-16 09:07:57 adam Exp $

bin_PROGRAMS = pazpar2

noinst_LIBRARIES = libpazpar2.a

MAINTAINERCLEANFILES = Makefile.in cconfig.h

AM_CFLAGS = $(YAZINC)

pazpar2_SOURCES = pazpar2.c
pazpar2_LDADD = libpazpar2.a $(YAZLIB)

libpazpar2_a_SOURCES = config.c config.h eventl.c eventl.h \
	http.c http_command.c http_command.h http.h \
	logic.c pazpar2.h reclists.c reclists.h \
	relevance.c relevance.h termlists.c termlists.h \
	util.c util.h zeerex.c zeerex.h database.c database.h \
	settings.h settings.c 
