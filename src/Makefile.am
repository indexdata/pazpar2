# This file is part of Pazpar2.

sbin_PROGRAMS = pazpar2

check_PROGRAMS = \
      test_sel_thread \
      test_normalize

TESTS = $(check_PROGRAMS)

noinst_LIBRARIES = libpazpar2.a

MAINTAINERCLEANFILES = Makefile.in cconfig.h 

CONFIG_CLEAN_FILES=*.log

AM_CFLAGS = $(YAZINC)
        
libpazpar2_a_SOURCES = pazpar2_config.c pazpar2_config.h eventl.c eventl.h \
	http.c http_command.c http.h \
	logic.c pazpar2.h \
	record.h record.c reclists.c reclists.h \
	relevance.c relevance.h termlists.c termlists.h \
        normalize7bit.h normalize7bit.c \
	zeerex.c zeerex.h database.c database.h \
	settings.h settings.c sel_thread.c sel_thread.h getaddrinfo.c \
	charsets.c charsets.h \
	client.c client.h connection.c connection.h host.h parameters.h \
	marcmap.c marcmap.h marchash.c marchash.h \
	jenkins_hash.c jenkins_hash.h normalize_record.c normalize_record.h \
	normalize_cache.c normalize_cache.h

pazpar2_SOURCES = pazpar2.c
pazpar2_LDADD = libpazpar2.a $(YAZLIB)

test_sel_thread_SOURCES = test_sel_thread.c
test_sel_thread_LDADD = libpazpar2.a $(YAZLIB)

test_normalize_SOURCES = test_normalize.c
test_normalize_LDADD = libpazpar2.a $(YAZLIB)

