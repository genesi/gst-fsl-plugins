plugin_LTLIBRARIES = libmfw_gst_audio_src.la 

libmfw_gst_audio_src_la_SOURCES =  mfw_gst_audio_src.c
libmfw_gst_audio_src_la_CFLAGS = -O2 $(GST_BASE_CFLAGS) -fPIC -fno-omit-frame-pointer $(FSL_MM_CORE_CFLAGS) -I../../../../inc/plugin 
libmfw_gst_audio_src_la_LIBADD = $(GST_BASE_LIBS) $(GST_PLUGINS_BASE_LIBS) $(GST_LIBS) -l_src_ppp_arm11_elinux
libmfw_gst_audio_src_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS) $(FSL_MM_CORE_LIBS)

noinst_HEADERS = 
