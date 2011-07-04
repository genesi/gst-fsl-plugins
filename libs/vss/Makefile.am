if LTIBBUILD

else
FBHEADER_PATH=../../../../inc/misc
endif


lib_LTLIBRARIES = libmfwvss.la 

libmfwvss_la_SOURCES = vss_common.c video_surface.c
libmfwvss_la_CFLAGS  = -O2 -I$(FBHEADER_PATH) $(IPU_CFLAGS) -I../../inc/common -I../sconf
libmfwvss_la_LDFLAGS =  -version-info @GSTMXLIBS_VERSION@ 
libmfwvss_la_LIBADD = $(GST_BASE_LIBS) -lgstvideo-$(GST_MAJORMINOR) -lgstinterfaces-$(GST_MAJORMINOR) $(IPU_LIBS) -lipu -ldl
libmfwvss_la_LIBADD += ../sconf/libmfwsconf.la


data_DATA = vssconfig vssconfig.dvi_tv vssconfig.dvi_wvga
