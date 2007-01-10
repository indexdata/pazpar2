## $Id: Makefile.am,v 1.1 2007-01-10 09:44:20 adam Exp $
docdir=$(datadir)/doc/@PACKAGE@

SUBDIRS = common

SUFFIXES=.3 .1 .pdf .tkl .esp .xml 

XMLFILES = book.xml manref.xml

MAINXML = $(srcdir)/book.xml

XMLMAN = pazpar2.xml

SUPPORTFILES = entities.ent local.ent.in

MANFILES = pazpar2.1

HTMLFILES = index.html

PNGFILES =

doc_DATA = $(HTMLFILES) $(PNGFILES)
man_MANS = $(MANFILES)

EXTRA_DIST = $(XMLFILES) $(XMLMAN) $(doc_DATA) $(man_MANS)

$(HTMLFILES): $(XMLFILES) $(PNGFILES)
	rm -f *.html
	$(HTML_COMPILE) $(MAINXML)

.xml.1:
	$(MAN_COMPILE) $<

index.tkl: $(XMLFILES)
	$(TKL_COMPILE) $(MAINXML)

pazpar2.pdf: $(XMLFILES)
	jade -E14 -D $(srcdir) -d common/print.dsl -t tex $(srcdir)/common/xml.dcl $(MAINXML)
	rm -f pazpar2.tex
	mv book.tex pazpar2.tex
	pdfjadetex pazpar2.tex >/dev/null
	pdfjadetex pazpar2.tex >/dev/null
	pdfjadetex pazpar2.tex >/dev/null

manref.xml: $(XMLMAN) $(srcdir)/common/stripref.xsl
	rm -f manref.xml
	for i in $(XMLMAN); do \
		xsltproc $(srcdir)/common/stripref.xsl $(srcdir)/$$i | sed 1d >> manref.xml; \
	done

clean-data-hook:
	rm -f [0-9]* *.bak

dist-hook:
	if test -f index.html; then d=.; else d="$(srcdir)"; fi; \
	for p in $$d/*.html; do \
		cp $$p $(distdir); \
	done

install-data-hook:
	if test -f index.html; then d=.; else d="$(srcdir)"; fi; \
	for p in $$d/*.html; do \
		$(docDATA_INSTALL) $$p $(DESTDIR)/$(docdir); \
	done

uninstall-hook:
	rm -r $(DESTDIR)/$(docdir)

