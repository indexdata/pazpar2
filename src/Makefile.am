# $Id: Makefile.am,v 1.21 2007-05-15 08:56:03 adam Exp $

bin_PROGRAMS = pazpar2
check_PROGRAMS = test_config \
      test_icu_I18N \
      test_record \
      test_reclists \
      test_relevance \
      test_sel_thread

check_SCRIPTS = test_http.sh

EXTRA_DIST = test_http.xml test_http.cfg $(check_SCRIPTS)

TESTS = $(check_PROGRAMS) $(check_SCRIPTS)

noinst_LIBRARIES = libpazpar2.a

MAINTAINERCLEANFILES = Makefile.in cconfig.h 

CONFIG_CLEAN_FILES=*.log

AM_CFLAGS = $(YAZINC) $(ICU_CPPFLAGS)

libpazpar2_a_SOURCES = config.c config.h eventl.c eventl.h \
	http.c http_command.c http_command.h http.h \
        icu_I18N.h icu_I18N.c \
	logic.c pazpar2.h \
	record.h record.c reclists.c reclists.h \
	relevance.c relevance.h termlists.c termlists.h \
        normalize7bit.h normalize7bit.c \
	util.c util.h zeerex.c zeerex.h database.c database.h \
	settings.h settings.c sel_thread.c sel_thread.h getaddrinfo.c \
	charsets.c charsets.h \
	client.c client.h connection.c connection.h host.h parameters.h

pazpar2_SOURCES = pazpar2.c
pazpar2_LDADD = libpazpar2.a $(YAZLIB) $(ICU_LIBS)

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

test_icu_I18N_SOURCES = test_icu_I18N.c
test_icu_I18N_LDADD = libpazpar2.a $(YAZLIB) $(ICU_LIBS)

