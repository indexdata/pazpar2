# This file is part of Pazpar2.

sbin_PROGRAMS = pazpar2
noinst_PROGRAMS = pazpar2_play

check_PROGRAMS = \
      test_sel_thread \
      test_normalize

TESTS = $(check_PROGRAMS)

noinst_LIBRARIES = libpazpar2.a

CONFIG_CLEAN_FILES=*.log

AM_CFLAGS = $(YAZINC)
        
libpazpar2_a_SOURCES = \
	charsets.c charsets.h \
	client.c client.h \
	connection.c connection.h \
	database.c database.h \
	eventl.c eventl.h \
	getaddrinfo.c \
	host.h \
	http.c http.h http_command.c \
	incref.c incref.h \
	jenkins_hash.c jenkins_hash.h \
	marchash.c marchash.h \
	marcmap.c marcmap.h \
        normalize7bit.c normalize7bit.h \
	normalize_cache.c normalize_cache.h \
	normalize_record.c normalize_record.h \
	parameters.h \
	pazpar2_config.c pazpar2_config.h \
	ppmutex.c ppmutex.h \
	reclists.c reclists.h \
	record.c record.h \
	relevance.c relevance.h \
	sel_thread.c sel_thread.h \
	session.c session.h \
	settings.c settings.h \
	termlists.c termlists.h

pazpar2_SOURCES = pazpar2.c
pazpar2_LDADD = libpazpar2.a $(YAZLIB)

pazpar2_play_SOURCES = pazpar2_play.c
pazpar2_play_LDADD = $(YAZLIB)

test_sel_thread_SOURCES = test_sel_thread.c
test_sel_thread_LDADD = libpazpar2.a $(YAZLIB)

test_normalize_SOURCES = test_normalize.c
test_normalize_LDADD = libpazpar2.a $(YAZLIB)

