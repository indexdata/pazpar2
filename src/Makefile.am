# $Id: Makefile.am,v 1.17 2007-04-26 21:33:32 marc Exp $

bin_PROGRAMS = pazpar2
check_PROGRAMS = test_config \
      test_record \
      test_reclists \
      test_relevance \
      test_sel_thread


TESTS = $(check_PROGRAMS)

noinst_LIBRARIES = libpazpar2.a

MAINTAINERCLEANFILES = Makefile.in cconfig.h 

CONFIG_CLEAN_FILES=*.log

AM_CFLAGS = $(YAZINC)

libpazpar2_a_SOURCES = config.c config.h eventl.c eventl.h \
	http.c http_command.c http_command.h http.h \
	logic.c pazpar2.h \
	record.h record.c reclists.c reclists.h \
	relevance.c relevance.h termlists.c termlists.h \
        normalize7bit.h normalize7bit.c \
	util.c util.h zeerex.c zeerex.h database.c database.h \
	settings.h settings.c sel_thread.c sel_thread.h getaddrinfo.c \
	client.c client.h connection.c connection.h host.h parameters.h

pazpar2_SOURCES = pazpar2.c
pazpar2_LDADD = libpazpar2.a $(YAZLIB)

test_config_SOURCES = test_config.c
test_config_LDADD = libpazpar2.a $(YAZLIB)

test_record_SOURCES = test_record.c
test_record_LDADD = libpazpar2.a $(YAZLIB)

test_reclists_SOURCES = test_reclists.c
test_reclists_LDADD = libpazpar2.a $(YAZLIB)

test_relevance_SOURCES = test_relevance.c
test_relevance_LDADD = libpazpar2.a $(YAZLIB)

test_sel_thread_SOURCES = test_sel_thread.c
test_sel_thread_LDADD = libpazpar2.a $(YAZLIB)

