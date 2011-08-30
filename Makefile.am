LIBSDIRS =  \
           libs/sconf \
           libs/me   \
           libs/gstbufmeta
		   
IPUDIRS = \
          libs/hbuf_alloc \
          libs/vss
#          src/video/vpu_enc/src

if PLATFORM_IS_MX5X
BASEDIRS =	src/video/vpu_dec.full/src       \
		src/misc/v4l_sink/src      \
		src/misc/xvimage

SUBDIRS = $(LIBSDIRS) $(IPUDIRS) $(BASEDIRS)
endif
