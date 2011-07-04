# plugindir is set in configure

# change libgstplugin.la to something more suitable
plugin_LTLIBRARIES = libmfw_gst_mpeg4dec.la

# for the next set of variables, rename the prefix if you renamed the .la

# sources used to compile this plug-in
libmfw_gst_mpeg4dec_la_SOURCES = mfw_gst_mpeg4decoder.c

# flags used to compile this plugin
# we use the GST_LIBS flags because we might be using plug-in libs
libmfw_gst_mpeg4dec_la_CFLAGS = $(GST_BASE_CFLAGS) -O2 -fno-omit-frame-pointer -fPIC -DPADDED_OUTPUT -DOUTPUT_BUFFER_CHANGES $(FSL_MM_CORE_CFLAGS) -I../../../../inc/plugin -DDIRECT_RENDER_VERSION=2 -DFRAMEDROPING_ENALBED
libmfw_gst_mpeg4dec_la_LIBADD = $(GST_BASE_LIBS) -l_MPEG4_dec_arm11_elinux
libmfw_gst_mpeg4dec_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS) $(FSL_MM_CORE_LIBS)

# headers we need but don't want installed
noinst_HEADERS = mfw_gst_mpeg4decoder.h
