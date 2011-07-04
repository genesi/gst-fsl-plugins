
# plugindir is set in configure
plugin_LTLIBRARIES = libmfw_gst_mpeg4enc.la

# for the next set of variables, rename the prefix if you renamed the .la
# sources used to compile this plug-in
libmfw_gst_mpeg4enc_la_SOURCES =  mfw_gst_mpeg4encoder.c 

# flags used to compile this plugin
# we use the GST_LIBS flags because we might be using plug-in libs
libmfw_gst_mpeg4enc_la_CFLAGS = -O0 $(GST_BASE_CFLAGS) $(FSL_MM_CORE_CFLAGS) -I../../../../inc/plugin
libmfw_gst_mpeg4enc_la_CPPFLAGS = $(GST_LIBS_CPPFLAGS) 

libmfw_gst_mpeg4enc_la_LIBADD = $(GST_BASE_LIBS) $(GST_PLUGINS_BASE_LIBS) $(GST_LIBS) -lmpeg4_encoder_arm11_ELINUX

libmfw_gst_mpeg4enc_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS) $(FSL_MM_CORE_LIBS)

# headers we need but don't want installed
noinst_HEADERS = mfw_gst_mpeg4encoder.h 
