lib_LTLIBRARIES = libgplaycore.la

libgplaycore_la_SOURCES = mfw_gplay_core.c fsl_player_debug.c fsl_player_osal.c fsl_player_queue.c fsl_player_ui_message.c gst_snapshot.c
libgplaycore_la_CFLAGS  = $(GST_CFLAGS) 
libgplaycore_la_LDFLAGS = -avoid-version
libgplaycore_la_LIBADD = $(GST_LIBS) -lz

bin_PROGRAMS = gplay
gplay_SOURCES = mfw_gplay.c playlist.c
gplay_CFLAGS  = $(GST_CFLAGS) 
gplay_LDADD   = libgplaycore.la

