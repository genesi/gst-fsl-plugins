plugin_LTLIBRARIES = libmfw_gst_xvimagesink.la

libmfw_gst_xvimagesink_la_SOURCES =  xvimagesink.c 
libmfw_gst_xvimagesink_la_CFLAGS = $(GST_PLUGINS_BASE_CFLAGS) $(GST_BASE_CFLAGS) $(GST_CFLAGS) $(X_CFLAGS) -I../../../libs/gstbufmeta -I../../../inc/plugin/
libmfw_gst_xvimagesink_la_LIBADD = \
	$(GST_BASE_LIBS) \
	$(GST_LIBS) \
	$(X_LIBS) $(XVIDEO_LIBS) $(XSHM_LIBS) $(LIBM) \
	../../../libs/gstbufmeta/libgstbufmeta.la \
	../../../libs/hbuf_alloc/libmfwba.la \
	-lX11 -lXv -lgstinterfaces-0.10 -lgstvideo-0.10 -lm
	
 
libmfw_gst_xvimagesink_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)

noinst_HEADERS = xvimagesink.h 
