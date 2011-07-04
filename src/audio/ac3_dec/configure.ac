
AC_INIT

dnl AM_MAINTAINER_MODE provides the option to enable maintainer mode
AM_MAINTAINER_MODE

AM_INIT_AUTOMAKE($PACKAGE, $VERSION)

dnl Added for BUILD options.
my_add_modules=
AC_MSG_CHECKING([for generating Plugins for gcc])
my_build_gcc=no
AC_ARG_WITH(unix,
    AC_HELP_STRING([--with-unix], [create gcc supported module]),
    [
        if test "$withval" = yes; then
            my_build_gcc=yes
            AC_MSG_RESULT(yes)
            my_add_modules="$my_add_modules src/"
        else
            AC_MSG_RESULT(no)
        fi
    ],
    [AC_MSG_RESULT(no)]
)
 
AM_CONDITIONAL(BUILD_GCC, test $my_build_gcc = yes)
dnl make aclocal work in maintainer mode
AC_SUBST(ACLOCAL_AMFLAGS, "-I m4")

dnl check for tools
AC_PROG_CC
AC_PROG_LIBTOOL

dnl Check for pkgconfig first
AC_CHECK_PROG(HAVE_PKGCONFIG, pkg-config, yes, no)

dnl Give error and exit if we don't have pkgconfig
if test "x$HAVE_PKGCONFIG" = "xno"; then
  AC_MSG_ERROR(you need to have pkgconfig installed !)
fi

dnl Now we're ready to ask for gstreamer libs and cflags
dnl And we can also ask for the right version of gstreamer

GST_REQUIRED=0.10

GST_MAJORMINOR=0.10
PKG_CHECK_MODULES(GST, \
  gstreamer-$GST_MAJORMINOR >= $GST_REQUIRED,
  HAVE_GST=yes,HAVE_GST=no)

dnl Give error and exit if we don't have gstreamer
if test "x$HAVE_GST" = "xno"; then
  AC_MSG_ERROR(you need gstreamer development packages installed !)
fi

dnl make GST_CFLAGS and GST_LIBS available
AC_SUBST(GST_CFLAGS)
AC_SUBST(GST_LIBS)

dnl make GST_MAJORMINOR available in Makefile.am
AC_SUBST(GST_MAJORMINOR)

dnl If we need them, we can also use the plugin libraries
PKG_CHECK_MODULES(GST_LIBS, gstreamer-plugins-base-$GST_MAJORMINOR >= $GST_REQUIRED,
                  HAVE_GST_LIBS=yes, HAVE_GST_LIBS=no)

dnl Give a warning if we don't have gstreamer libs
if test "x$HAVE_GST_LIBS" = "xno"; then
  AC_MSG_NOTICE(no GStreamer plugin libs found)
fi

dnl make GST_LIBS_CFLAGS and GST_LIBS_LIBS available
AC_SUBST(GST_LIBS_CFLAGS)
AC_SUBST(GST_LIBS_LIBS)

dnl set the plugindir where plugins should be installed
plugindir="\$(libdir)/gstreamer-$GST_MAJORMINOR"
AC_SUBST(plugindir)

dnl set proper LDFLAGS for plugins
GST_PLUGIN_LDFLAGS='-module -avoid-version -export-symbols-regex [_]*\(gst_\|Gst\|GST_\).*'
AC_SUBST(GST_PLUGIN_LDFLAGS)

AC_OUTPUT(Makefile src/Makefile)

