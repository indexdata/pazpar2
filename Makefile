# ParaZ. Copyright (C) 2000-2004, Index Data ApS
# All rights reserved.
# $Id: Makefile,v 1.5 2006-12-08 22:21:36 quinn Exp $

SHELL=/bin/sh

CC=gcc

YAZCONF=yaz-config
YAZLIBS=`$(YAZCONF) --libs`
YAZCFLAGS=`$(YAZCONF) --cflags`

PROG=pazpar2
PROGO=pazpar2.o eventl.o util.o command.o http.o http_command.o termlists.o \
		reclists.o relevance.o

all: $(PROG)

$(PROG): $(PROGO)
	$(CC) $(CFLAGS) $(YAZCFLAGS) -o $(PROG) $(PROGO) $(YAZLIBS)

.c.o:
	$(CC) -c $(CFLAGS) -I. $(YAZCFLAGS) $<

clean:
	rm -f *.[oa] test core mon.out gmon.out errlist $(PROG)

