if LTIBBUILD
else
FBHEADER_PATH=-I../../../../inc/misc
endif
lib_LTLIBRARIES = libmfwba.la 

libmfwba_la_SOURCES = hwbuffer_allocator.c
libmfwba_la_CFLAGS  = -O2 $(FBHEADER_PATH) $(IPU_CFLAGS) -fPIC --shared
libmfwba_la_LDFLAGS = -version-info @GSTMXLIBS_VERSION@
libmfwba_la_LIBADD  = -lpthread 
