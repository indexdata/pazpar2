# ParaZ. Copyright (C) 2000-2004, Index Data ApS
# All rights reserved.
# $Id: Makefile,v 1.3 2006-12-27 21:11:10 quinn Exp $

SHELL=/bin/sh

CC=gcc

YAZCONF=yaz/yaz-config
YAZLIBS=`$(YAZCONF) --libs`
YAZCFLAGS=`$(YAZCONF) --cflags`

PROG=pazpar2
PROGO=pazpar2.o eventl.o util.o command.o http.o http_command.o termlists.o \
		reclists.o relevance.o config.o

all: $(PROG)

$(PROG): $(PROGO)
	$(CC) $(CFLAGS) $(YAZCFLAGS) -o $(PROG) $(PROGO) $(YAZLIBS)

.c.o:
	$(CC) -c $(CFLAGS) -I. $(YAZCFLAGS) $<

clean:
	rm -f *.[oa] test core mon.out gmon.out errlist $(PROG)


## Dependencies go below

command.o: command.c command.h util.h eventl.h pazpar2.h termlists.h \
  relevance.h reclists.h
config.o: config.c config.h
eventl.o: eventl.c eventl.h
http.o: http.c command.h util.h eventl.h pazpar2.h termlists.h \
  relevance.h reclists.h http.h http_command.h
http_command.o: http_command.c command.h util.h eventl.h pazpar2.h \
  termlists.h relevance.h reclists.h http.h http_command.h
pazpar2.o: pazpar2.c pazpar2.h termlists.h relevance.h reclists.h \
  eventl.h command.h http.h config.h
reclists.o: reclists.c pazpar2.h termlists.h relevance.h reclists.h \
  eventl.h
relevance.o: relevance.c relevance.h pazpar2.h termlists.h eventl.h \
  reclists.h
termlists.o: termlists.c termlists.h
util.o: util.c
