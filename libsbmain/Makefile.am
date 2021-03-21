AUTOMAKE_OPTIONS = foreign

instdir = $(moduledir)
noinst_LTLIBRARIES = libsbmain.la

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libsbutil \
	-I$(top_srcdir)/libsbutil/include \
	$(SANDBOX_DEFINES)

libsbmain_la_LDFLAGS = -no-undefined
libsbmain_la_LIBADD = \
	$(top_builddir)/libsbutil/libsbutil.la \
	$(top_builddir)/libsandbox/libsandbox.la \
	$(LIBDL)

libsbmain_la_SOURCES = \
	environ.c \
	namespaces.c \
	options.c \
	sandbox.h \
	sandbox.c
