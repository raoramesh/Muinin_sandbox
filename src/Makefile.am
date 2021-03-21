AUTOMAKE_OPTIONS = foreign

bin_PROGRAMS = sandbox

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libsbutil \
	-I$(top_srcdir)/libsbutil/include \
	-I$(top_srcdir)/libsbmain \
	$(SANDBOX_DEFINES)

sandbox_LDADD = \
	$(top_builddir)/libsbutil/libsbutil.la \
	$(top_builddir)/libsbmain/libsbmain.la \
	$(LIBDL)
sandbox_SOURCES =	sbmain.c
