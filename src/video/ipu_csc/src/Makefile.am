if LTIBBUILD

else
FBHEADER_PATH=-I../../../../inc/misc
endif
plugin_LTLIBRARIES = libmfw_gst_ipu_csc.la 

libmfw_gst_ipu_csc_la_SOURCES =  mfw_gst_ipu_csc.c 
libmfw_gst_ipu_csc_la_CFLAGS = -O2 $(GST_BASE_CFLAGS) -fPIC -fno-omit-frame-pointer  -I../../../../inc/plugin $(FBHEADER_PATH) $(IPU_CFLAGS) -I../../../../libs/gstbufmeta
libmfw_gst_ipu_csc_la_LIBADD = $(GST_BASE_LIBS) $(GST_PLUGINS_BASE_LIBS) $(GST_LIBS) $(IPU_LIBS) -lipu ../../../../libs/gstbufmeta/libgstbufmeta.la
libmfw_gst_ipu_csc_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)

noinst_HEADERS = 
