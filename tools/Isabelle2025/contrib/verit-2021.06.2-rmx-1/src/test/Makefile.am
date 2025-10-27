LOG_DRIVER = env AM_TAP_AWK='$(AWK)' $(SHELL) \
             $(top_srcdir)/scripts/tap-driver.sh

TESTS = veriT-test

check_PROGRAMS = veriT-test

veriT_test_SOURCES = \
	test/main.c \
	test/picotest.h \
  test/suits/discrimination-tree.h \
  test/suits/syntactic-unify.h \
	$(BUILT_SOURCES) \
	$(REGULAR_SOURCES) \
	$(NULL)

veriT_test_CFLAGS = \
	$(AM_CFLAGS) \
	-Wno-unused-variable \
	-Wno-switch \
	$(NULL)

veriT_test_LDADD = \
	$(AM_LDADD) \
	$(NULL)

veriT_test_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	-I$(top_builddir)/src \
	-I$(top_srcdir)/src \
	-I$(top_builddir)/test \
	-I$(top_srcdir)/test \
	$(NULL)

