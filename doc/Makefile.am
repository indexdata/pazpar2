# This file is part of Pazpar2.
SUBDIRS = common

SUFFIXES=.1 .5 .7 .8 .pdf .esp .xml 

XMLFILES = book.xml manref.xml gpl-2.0.xml local.ent

MAINXML = $(srcdir)/book.xml

XMLMAN = pazpar2.xml pazpar2_protocol.xml pazpar2_conf.xml

SUPPORTFILES = entities.ent

MANFILES = pazpar2.8 pazpar2_protocol.7 pazpar2_conf.5

HTMLFILES = index.html

PNGFILES =

doc_DATA = $(HTMLFILES) $(PNGFILES)
man_MANS = $(MANFILES)

EXTRA_DIST = $(XMLFILES) $(XMLMAN) $(doc_DATA) $(man_MANS) $(SUPPORTFILES)

$(HTMLFILES): $(XMLFILES) $(PNGFILES)
	rm -f *.html
	$(HTML_COMPILE) $(MAINXML)

$(MANFILES): local.ent

.xml.1:
	$(MAN_COMPILE) $<

.xml.5:
	$(MAN_COMPILE) $<

.xml.7:
	$(MAN_COMPILE) $<

.xml.8:
	$(MAN_COMPILE) $<

pazpar2.pdf: $(XMLFILES)
	rm -f pazpar2.pdf
	$(PDF_COMPILE) $(MAINXML) && mv book.pdf pazpar2.pdf

jade-check: $(XMLFILES)
	jade -E14 -D $(srcdir) -d common/print.dsl -t tex $(srcdir)/common/xml.dcl $(MAINXML)
	touch jade-check

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

doc-clean:
	rm -f manref.xml *.html *.[0-9] *.pdf

install-data-hook:
	if test -f index.html; then d=.; else d="$(srcdir)"; fi; \
	for p in $$d/*.html; do \
		$(INSTALL_DATA) $$p $(DESTDIR)$(docdir); \
	done

uninstall-hook:
	rm -r $(DESTDIR)$(docdir)

