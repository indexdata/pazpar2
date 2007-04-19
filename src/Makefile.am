# $Id: Makefile.am,v 1.9 2007-04-19 19:42:30 marc Exp $

bin_PROGRAMS = pazpar2
check_PROGRAMS = test_config test_relevance
TESTS = $(check_PROGRAMS)

noinst_LIBRARIES = libpazpar2.a

MAINTAINERCLEANFILES = Makefile.in cconfig.h

AM_CFLAGS = $(YAZINC)

libpazpar2_a_SOURCES = config.c config.h eventl.c eventl.h \
	http.c http_command.c http_command.h http.h \
	logic.c pazpar2.h reclists.c reclists.h \
	relevance.c relevance.h termlists.c termlists.h \
	util.c util.h zeerex.c zeerex.h database.c database.h \
	settings.h settings.c 


pazpar2_SOURCES = pazpar2.c
pazpar2_LDADD = libpazpar2.a $(YAZLIB)

test_config_SOURCES = test_config.c
test_config_LDADD = libpazpar2.a $(YAZLIB)

test_relevance_SOURCES = test_relevance.c
test_relevance_LDADD = libpazpar2.a $(YAZLIB)

