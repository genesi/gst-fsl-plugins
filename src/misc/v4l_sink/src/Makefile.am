plugin_LTLIBRARIES = libmfw_gst_v4lsink.la

libmfw_gst_v4lsink_la_SOURCES =  mfw_gst_fb.c mfw_gst_v4l.c mfw_gst_v4l_buffer.c mfw_gst_v4lsink.c
libmfw_gst_v4lsink_la_CFLAGS =

if PLATFORM_IS_MX233
libmfw_gst_v4lsink_la_CFLAGS += -O2 $(GST_BASE_CFLAGS) -fPIC -fno-omit-frame-pointer -D_$(PLATFORM) -I../../../../inc/plugin -I../../../../inc/misc -march=armv5te
endif
if PLATFORM_IS_MX25
libmfw_gst_v4lsink_la_CFLAGS += -O2 $(GST_BASE_CFLAGS) -fPIC -fno-omit-frame-pointer -D_$(PLATFORM) -I../../../../inc/plugin -I../../../../inc/misc -march=armv5te
endif
if PLATFORM_IS_MX27
libmfw_gst_v4lsink_la_CFLAGS += -O2 $(GST_BASE_CFLAGS) -fPIC -fno-omit-frame-pointer -DENABLE_TVOUT -D_$(PLATFORM) -I../../../../inc/plugin -I../../../../inc/misc -march=armv5te
endif
if PLATFORM_IS_MX28
libmfw_gst_v4lsink_la_CFLAGS += -O2 $(GST_BASE_CFLAGS) -fPIC -fno-omit-frame-pointer -D_$(PLATFORM) -I../../../../inc/plugin -I../../../../inc/misc -march=armv5te
endif

if PLATFORM_IS_MX3X
libmfw_gst_v4lsink_la_CFLAGS += -O2 $(GST_BASE_CFLAGS) -fPIC -fno-omit-frame-pointer -DENABLE_TVOUT -D_$(PLATFORM) -I../../../../inc/plugin -I../../../../inc/misc
endif
if PLATFORM_IS_MX5X
libmfw_gst_v4lsink_la_CFLAGS += -O2 $(GST_BASE_CFLAGS) -fPIC -fno-omit-frame-pointer -DENABLE_TVOUT -D_$(PLATFORM) -I../../../../inc/plugin -I../../../../inc/misc
endif

libmfw_gst_v4lsink_la_LIBADD = $(GST_BASE_LIBS) -lgstvideo-$(GST_MAJORMINOR) -lgstinterfaces-$(GST_MAJORMINOR) ../../../../libs/gstbufmeta/libgstbufmeta.la


libmfw_gst_v4lsink_la_CFLAGS += $(FBHEADER_PATH) $(IPU_CFLAGS) -I. -I../../../../libs/gstbufmeta

if USE_X11
libmfw_gst_v4lsink_la_SOURCES +=  mfw_gst_v4l_xlib.c mfw_gst_xlib.c mfw_gst_v4l_suspend.c
libmfw_gst_v4lsink_la_LIBADD += -lX11 -lXext 
libmfw_gst_v4lsink_la_CFLAGS += -DUSE_X11 -DSUSPEND_SUPPORT 
endif
libmfw_gst_v4lsink_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)

noinst_HEADERS =
