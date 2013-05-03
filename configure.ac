AC_INIT

AC_DEFUN([AG_GST_CHECK_FEATURE],[
echo
AC_MSG_NOTICE([*** checking feature: [$2] ***])
if test "x[$3]" != "x"
then
  AC_MSG_NOTICE([*** for plug-ins: [$3] ***])
fi
dnl
builtin(define, [gst_endisable], ifelse($5, [disabled], [enable], [disable]))dnl
dnl if it is set to NO, then don't even consider it for building
NOUSE=
if test "x$USE_[$1]" = "xno"; then
  NOUSE="yes"
fi
AC_ARG_ENABLE(translit([$1], A-Z, a-z),
  builtin(format, --%-26s gst_endisable %s, gst_endisable-translit([$1], A-Z, a-z), [$2]ifelse([$3],,,: [$3])),
  [ case "${enableval}" in
      yes) USE_[$1]=yes;;
      no) USE_[$1]=no;;
      *) AC_MSG_ERROR(bad value ${enableval} for --enable-translit([$1], A-Z, a-z)) ;;
    esac],
  [ [USE_$1=]ifelse($5, [disabled], [no], [yes])] )        dnl DEFAULT

dnl *** set it back to no if it was preset to no
if test "x$NOUSE" = "xyes"; then
  USE_[$1]="no"
  AC_MSG_WARN(*** $3 pre-configured not to be built)
fi
NOUSE=

dnl *** If it's enabled

if test x$USE_[$1] = xyes; then
  dnl save compile variables before the test

  gst_check_save_LIBS=$LIBS
  gst_check_save_LDFLAGS=$LDFLAGS
  gst_check_save_CFLAGS=$CFLAGS
  gst_check_save_CPPFLAGS=$CPPFLAGS
  gst_check_save_CXXFLAGS=$CXXFLAGS

  HAVE_[$1]=no
  dnl TEST_FOR_FEATURE
  $4

  LIBS=$gst_check_save_LIBS
  LDFLAGS=$gst_check_save_LDFLAGS
  CFLAGS=$gst_check_save_CFLAGS
  CPPFLAGS=$gst_check_save_CPPFLAGS
  CXXFLAGS=$gst_check_save_CXXFLAGS

  dnl If it isn't found, unset USE_[$1]
  if test x$HAVE_[$1] = xno; then
    USE_[$1]=no
  else
    ifelse([$3], , :, [AC_MSG_NOTICE(*** These plugins will be built: [$3])])
  fi
fi
dnl *** Warn if it's disabled or not found
if test x$USE_[$1] = xyes; then
  ifelse([$6], , :, [$6])
  if test "x$3" != "x"; then
    GST_PLUGINS_YES="\t[$3]\n$GST_PLUGINS_YES"
  fi
  AC_DEFINE(HAVE_[$1], , [support for features: $3])
else
  ifelse([$3], , :, [AC_MSG_NOTICE(*** These plugins will not be built: [$3])])
  if test "x$3" != "x"; then
    GST_PLUGINS_NO="\t[$3]\n$GST_PLUGINS_NO"
  fi
  ifelse([$7], , :, [$7])
fi
dnl *** Define the conditional as appropriate
AM_CONDITIONAL(USE_[$1], test x$USE_[$1] = xyes)
])


AC_DEFUN([AG_GST_CHECK_CONFIGPROG],
[
  AC_PATH_PROG([$1]_CONFIG, [$2], no)
  if test x$[$1]_CONFIG = xno; then
    [$1]_LIBS=
    [$1]_CFLAGS=
    HAVE_[$1]=no
  else
    if [$2] --plugin-libs [$3] &> /dev/null; then
      [$1]_LIBS=`[$2] --plugin-libs [$3]`
    else
      [$1]_LIBS=`[$2] --libs [$3]`
    fi
    [$1]_CFLAGS=`[$2] --cflags [$3]`
    HAVE_[$1]=yes
  fi
  AC_SUBST([$1]_LIBS)
  AC_SUBST([$1]_CFLAGS)
])


AC_DEFUN([AG_GST_CHECK_LIBHEADER],
[
  AC_CHECK_LIB([$2], [$3], HAVE_[$1]=yes, HAVE_[$1]=no,[$4])
  if test "x$HAVE_[$1]" = "xyes"; then
    AC_CHECK_HEADER([$5], :, HAVE_[$1]=no)
    if test "x$HAVE_[$1]" = "xyes"; then
      dnl execute what needs to be
      ifelse([$6], , :, [$6])
    else
      ifelse([$7], , :, [$7])
    fi
  else
    ifelse([$7], , :, [$7])
  fi
  AC_SUBST(HAVE_[$1])
]
)


AC_DEFUN([AG_GST_CHECK_SUBSYSTEM_DISABLE],
[
  dnl this define will replace each literal subsys_def occurrence with
  dnl the lowercase hyphen-separated subsystem
  dnl e.g. if $1 is GST_DEBUG then subsys_def will be a macro with gst-debug
  define([subsys_def],translit([$1], _A-Z, -a-z))

  AC_ARG_ENABLE(subsys_def,
    AC_HELP_STRING(--disable-subsys_def, [disable $2]),
    [
      case "${enableval}" in
        yes) GST_DISABLE_[$1]=no ;;
        no) GST_DISABLE_[$1]=yes ;;
        *) AC_MSG_ERROR([bad value ${enableval} for --enable-subsys_def]) ;;
       esac
    ],
    [GST_DISABLE_[$1]=no]) dnl Default value

  if test x$GST_DISABLE_[$1] = xyes; then
    AC_MSG_NOTICE([disabled subsystem [$2]])
    GST_DISABLE_[$1]_DEFINE="#define GST_DISABLE_$1 1"
  else
    GST_DISABLE_[$1]_DEFINE="/* #undef GST_DISABLE_$1 */"
  fi
  AC_SUBST(GST_DISABLE_[$1]_DEFINE)
  undefine([subsys_def])
])


AC_DEFUN([AG_GST_OUTPUT_PLUGINS], [

echo "configure: *** Plug-ins without external dependencies that will be built:"
( for i in $GST_PLUGINS_SELECTED; do echo -e '\t'$i; done ) | sort
echo

echo "configure: *** Plug-ins without external dependencies that will NOT be built:"
( for i in $GST_PLUGINS_ALL; do
    case $GST_PLUGINS_SELECTED in
      *$i*)
	;;
      *)
	echo -e '\t'$i
	;;
    esac
  done ) | sort
echo

if test "x$BUILD_EXTERNAL" = "xno"; then
  echo "configure: *** No plug-ins with external dependencies will be built"
else
  echo -n "configure: *** Plug-ins with dependencies that will be built:"
  echo -e "$GST_PLUGINS_YES" | sort
  echo
  echo -n "configure: *** Plug-ins with dependencies that will NOT be built:"
  echo -e "$GST_PLUGINS_NO" | sort
  echo
fi
])



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

dnl check target type
if test "$PLATFORM" = "MX28" || test "$PLATFORM" = "MX233" || test "$PLATFORM" = "MX25" || test "$PLATFORM" = "MX27" || \
   test "$PLATFORM" = "MX31" || test "$PLATFORM" = "MX35" || test "$PLATFORM" = "MX37" \
   || test "$PLATFORM" = "MX51" || test "$PLATFORM" = "MX53" || test "$PLATFORM" = "MX50" || test "$PLATFORM" = "MX5X";
then
  AC_MSG_NOTICE(build for $PLATFORM)
else
  AC_MSG_ERROR(No target platform specified! Use ./configure PLATFORM=(MX28/MX233/MX25/MX27/MX31/MX35/MX37/MX51/MX53/MX50/MX5X) to retry)
fi

AC_SUBST(PLATFORM)

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
PKG_CHECK_MODULES(GST_BASE, gstreamer-base-$GST_MAJORMINOR >= $GST_REQUIRED,
                  HAVE_GST_BASE=yes, HAVE_GST_BASE=no)

dnl Give a warning if we don't have gstreamer libs
if test "x$HAVE_GST_BASE" = "xno"; then
  AC_MSG_ERROR(no GStreamer base found)
fi

dnl make GST_BASE_CFLAGS and GST_BASE_LIBS available
AC_SUBST(GST_BASE_CFLAGS)
AC_SUBST(GST_BASE_LIBS)

dnl If we need them, we can also use the plugin libraries
PKG_CHECK_MODULES(GST_PLUGINS_BASE, gstreamer-plugins-base-$GST_MAJORMINOR >= $GST_REQUIRED,
                  HAVE_GST_PLUGINS_BASE=yes, HAVE_GST_PLUGINS_BASE=no)

dnl Give a warning if we don't have gstreamer libs
if test "x$HAVE_GST_PLUGINS_BASE" = "xno"; then
  AC_MSG_ERROR(no GStreamer plugin base libs found)
fi

dnl make GST_PLUGINS_BASE_CFLAGS and GST_PLUGINS_BASE_LIBS available
AC_SUBST(GST_PLUGINS_BASE_CFLAGS)
AC_SUBST(GST_PLUGINS_BASE_LIBS)

FSL_VPU_CORE_REQUIRED=4.4.0
PKG_CHECK_MODULES(VPU, \
  vpu-core-imx >= $FSL_VPU_CORE_REQUIRED,
  HAVE_VPU_CORE=yes,HAVE_VPU_CORE=no)

AC_SUBST(VPU_CFLAGS)
AC_SUBST(VPU_LIBS)

FSL_IPU_CORE_REQUIRED=9.12.0
PKG_CHECK_MODULES(IPU, \
  ipu-core-imx >= $FSL_IPU_CORE_REQUIRED,
  HAVE_IPU_CORE=yes,HAVE_IPU_CORE=no)

AC_SUBST(IPU_CFLAGS)
AC_SUBST(IPU_LIBS)

EXTRAPLATFORMCFLAGS="-DARM -D_GNUC_ -I/usr/src/linux/include"
XTRAINCPATHS="$FSL_MM_CORE_CFLAGS"
CFLAGS="$CFLAGS $XTRAINCPATHS $EXTRAPLATFORMCFLAGS"

dnl set the plugindir where plugins should be installed
plugindir="\$(libdir)/gstreamer-$GST_MAJORMINOR"
AC_SUBST(plugindir)

dnl set proper LDFLAGS for plugins
GST_PLUGIN_LDFLAGS='-module -avoid-version -export-symbols-regex [_]*\(gst_\|Gst\|GST_\).*'
AC_SUBST(GST_PLUGIN_LDFLAGS)

AM_CONDITIONAL(PLATFORM_IS_MX2X, test "x$PLATFORM" = "xMX28" || test "x$PLATFORM" = "xMX233" || test "x$PLATFORM" = "xMX25" || test "x$PLATFORM" = "xMX27")
AM_CONDITIONAL(PLATFORM_IS_MX233, test "x$PLATFORM" = "xMX233")
AM_CONDITIONAL(PLATFORM_IS_MX25, test "x$PLATFORM" = "xMX25")
AM_CONDITIONAL(PLATFORM_IS_MX27, test "x$PLATFORM" = "xMX27")
AM_CONDITIONAL(PLATFORM_IS_MX28, test "x$PLATFORM" = "xMX28")

AM_CONDITIONAL(PLATFORM_IS_MX3X, test "x$PLATFORM" = "xMX31" || test "x$PLATFORM" = "xMX35" || test "x$PLATFORM" = "xMX37")
AM_CONDITIONAL(PLATFORM_IS_MX31, test "x$PLATFORM" = "xMX31")
AM_CONDITIONAL(PLATFORM_IS_MX35, test "x$PLATFORM" = "xMX35")
AM_CONDITIONAL(PLATFORM_IS_MX37, test "x$PLATFORM" = "xMX37")


AM_CONDITIONAL(PLATFORM_IS_MX5X, test "x$PLATFORM" = "xMX51" || test "x$PLATFORM" = "xMX53" || test "x$PLATFORM" = "xMX50" || test "x$PLATFORM" = "xMX5X")
AM_CONDITIONAL(PLATFORM_IS_MX51, test "x$PLATFORM" = "xMX51")
AM_CONDITIONAL(PLATFORM_IS_MX53, test "x$PLATFORM" = "xMX53")
AM_CONDITIONAL(PLATFORM_IS_MX50, test "x$PLATFORM" = "xMX50")






AM_CONDITIONAL(LTIBBUILD, test "x$BUILDER" = "xLTIB")

dnl *** X11 check ***
AC_CHECK_HEADER(X11/Xlib.h, HAVE_X="yes", HAVE_X="no")
AM_CONDITIONAL(USE_X11, test "x$HAVE_X" = "xyes")

AC_SUBST(GSTMXLIBS_VERSION) 
GSTMXLIBS_VERSION=0:1:0

AC_OUTPUT(
Makefile
libs/hbuf_alloc/Makefile
libs/vss/Makefile
libs/me/Makefile
libs/sconf/Makefile
libs/gstbufmeta/Makefile
src/video/vpu_dec.full/src/Makefile
src/video/vpu_enc/src/Makefile
src/misc/v4l_source/src/Makefile
src/misc/xvimage/Makefile
)



