## Makefile.am

MAINTAINERCLEANFILES = Makefile.in aclocal.m4 configure ac-tools/config.guess\
                       ac-tools/config.h.in ac-tools/config.h.in~\
                       ac-tools/config.sub ac-tools/depcomp\
                       ac-tools/install-sh ac-tools/missing\
                       ac-tools/mkinstalldirs ac-tools/compile\
                       $(PACKAGE)-$(VERSION).tar.gz\
                       $(PACKAGE)-$(VERSION).tar.bz2\
                       afd.spec\
                       errors
CLEANFILES           = core core.* typescript configmake.h configmake.h-t\
                       po/$(PACKAGE).pot afd.spec\
                       $(PACKAGE)-$(VERSION).tar.gz\
                       $(PACKAGE)-$(VERSION).tar.bz2
BUILT_SOURCES        = configmake.h
SUBDIRS              = po src scripts doc
DISTDOCS             = COPYING CREDITS INSTALL KNOWN-BUGS README.configure\
                       THANKS TODO changes-1.2.x-1.3.x
EXTRA_DIST           = ac-tools scripts afd.spec.in $(DISTDOCS)
doc_DATA             = $(DISTDOCS)

configmake.h:	Makefile
	rm -f $@-t $@
	{ echo '/* DO NOT EDIT! GENERATED AUTOMATICALLY! */'; \
	  echo '#define PREFIX "$(prefix)"'; \
	  echo '#define EXEC_PREFIX "$(exec_prefix)"'; \
	  echo '#define BINDIR "$(bindir)"'; \
	  echo '#define SBINDIR "$(sbindir)"'; \
	  echo '#define LIBEXECDIR "$(libexecdir)"'; \
	  echo '#define DATAROOTDIR "$(datarootdir)"'; \
	  echo '#define DATADIR "$(datadir)"'; \
	  echo '#define SYSCONFDIR "$(sysconfdir)"'; \
	  echo '#define SHAREDSTATEDIR "$(sharedstatedir)"'; \
	  echo '#define LOCALSTATEDIR "$(localstatedir)"'; \
	  echo '#define INCLUDEDIR "$(includedir)"'; \
	  echo '#define OLDINCLUDEDIR "$(oldincludedir)"'; \
	  echo '#define DOCDIR "$(docdir)"'; \
	  echo '#define INFODIR "$(infodir)"'; \
	  echo '#define HTMLDIR "$(htmldir)"'; \
	  echo '#define DVIDIR "$(dvidir)"'; \
	  echo '#define PDFDIR "$(pdfdir)"'; \
	  echo '#define PSDIR "$(psdir)"'; \
	  echo '#define LIBDIR "$(libdir)"'; \
	  echo '#define LISPDIR "$(lispdir)"'; \
	  echo '#define LOCALEDIR "$(localedir)"'; \
	  echo '#define MANDIR "$(mandir)"'; \
	  echo '#define MANEXT "$(manext)"'; \
	  echo '#define PKGDATADIR "$(pkgdatadir)"'; \
	  echo '#define PKGINCLUDEDIR "$(pkgincludedir)"'; \
	  echo '#define PKGLIBDIR "$(pkglibdir)"'; \
	  echo '#define PKGLIBEXECDIR "$(pkglibexecdir)"'; \
	} | sed '/""/d' > $@-t
	mv $@-t $@

rpm:	dist
	$(RPM) -ta $(PACKAGE)-$(VERSION).tar.bz2 </dev/null;

dist-hook:
	cp afd.spec $(distdir)

.PHONY: rpm
