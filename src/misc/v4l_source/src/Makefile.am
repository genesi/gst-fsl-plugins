if LTIBBUILD

else
FBHEADER_PATH=../../../../inc/misc
endif

plugin_LTLIBRARIES = libmfw_gst_v4lsrc.la 

libmfw_gst_v4lsrc_la_SOURCES =  mfw_gst_v4lsrc.c 
libmfw_gst_v4lsrc_la_CFLAGS = $(GST_BASE_CFLAGS) -fno-omit-frame-pointer $(IPU_CFLAGS) -D$(PLATFORM) -I$(FBHEADER_PATH) -I. -I../../../../inc/plugin -I../../../../libs/gstbufmeta 
libmfw_gst_v4lsrc_la_LIBADD = $(GST_BASE_LIBS) -lgstvideo-$(GST_MAJORMINOR) -lgstinterfaces-$(GST_MAJORMINOR)
libmfw_gst_v4lsrc_la_LIBADD += ../../../../libs/gstbufmeta/libgstbufmeta.la
libmfw_gst_v4lsrc_la_LIBADD += ../../../../libs/hbuf_alloc/libmfwba.la
libmfw_gst_v4lsrc_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)

noinst_HEADERS = mfw_gst_v4lsrc.h
