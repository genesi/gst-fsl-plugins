/*
 * Copyright (C) 2005-2009 Freescale Semiconductor, Inc. All rights reserved.
 *
 */
 
/*
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
 
/*
 * Module Name:    mfw_gst_mpeg4encoder.h
 *
 * Description:    declarations of gstreamer plugin for the MPEG4 hantro
 *                 Encoder. Supports upto VGA resolution input image.
 *
 * Portability:    This code is written for Linux OS and Gstreamer
 */  
 
/*
 * Changelog: 
 *
 */


/*=============================================================================
                            INCLUDE FILES
=============================================================================*/

#ifndef MFW_GST_MPEG4_ENCODER_H
#define MFW_GST_MPEG4_ENCODER_H
/*=============================================================================
                                           CONSTANTS
=============================================================================*/

/* None. */

/*=============================================================================
                                             ENUMS
=============================================================================*/

/* None. */
  
/*=============================================================================
                                            MACROS
=============================================================================*/
#ifndef MFW_MPEG4ENC_MAX_PATH
#define MFW_MPEG4ENC_MAX_PATH   256  /* Maximum lenght of the file path */
#endif

G_BEGIN_DECLS

/* #defines don't like whitespacey bits */
#define MFW_GST_TYPE_MPEG4ENCODER \
  (mfw_gst_mpeg4enc_get_type())


#define MFW_GST_MPEG4ENC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),MFW_GST_TYPE_MPEG4ENCODER,MFW_GST_MPEG4ENC_INFO_T))

#define MFW_GST_MPEG4ENC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),MFW_GST_TYPE_MPEG4ENCODER,MFW_GST_MPEG4ENC_CLASS_T))

#define MFW_GST_IS_MPEG4ENC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),MFW_GST_TYPE_MPEG4ENCODER))

#define MFW_GST_IS_MPEG4ENC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),MFW_GST_TYPE_MPEG4ENCODER))

#define MFW_GST_TYPE_MPEG4ENC_PROFILE (mfw_gst_mpeg4enc_profile_get_type())

/*=============================================================================
                                 STRUCTURES AND OTHER TYPEDEFS
=============================================================================*/
/* Structure for command line options */
typedef struct MP4EncOptions_tag
{
    gchar userDataVos[MFW_MPEG4ENC_MAX_PATH];
    gchar userDataVisObj[MFW_MPEG4ENC_MAX_PATH];
    gchar userDataVol[MFW_MPEG4ENC_MAX_PATH];
    gchar userDataGov[MFW_MPEG4ENC_MAX_PATH];
    gint width;                      /* input width */
    gint height;                     /* input height */
    gint lumWidthSrc;                /* input luma width */
    gint lumHeightSrc;               /* input luma height */
    gint horOffsetSrc;               /* horizontal offset */
    gint verOffsetSrc;               /* vertical offset */
    gint stabFrame;                  /* camera stabilisation for a frame*/
    gint preProcess;                 /* enable preprocessing of input frame*/
    gint outputRateNumer;            /* numerator value of output frame rate */ 
    gint outputRateDenom;            /* denominator value of output frame rate */
    gint inputRateNumer;             /* numerator value of input frame rate */
    gint inputRateDenom;             /* denominator value of input framrate */
    gint scheme;                     /* scheme of encoding */
    gint profile;                    /* profile at which stream is encoded */
    gint intraDcVlcThr;              /* DC threshold value for intra prediction */
    gint acPred;                     /* AC prediction enable */
    gint intraVopRate;               /* period between 2 Intra VOPS */
    gint goVopRate;                  /* period between group of VOPS */
    gint cir;                        /*  Cyclic Intra Refresh rate */
    gint inter4vPenalty;             
    gint intraPenalty;
    gint zeroMvFavor;                /* zero Motion vector favour */
    gint mvOutOfVop;                 /* motion vector out of VOP */
    gint fourMv;                     /* four motion vectors enable*/
    gint mvRange;                    /* range of motion vectors */
    gint vpSize;                     /* video packet size */
    gint dataPart;                   /* enable data partitioning */
    gint rvlc;                       /* enable RVLC */
    gint hec;                        
    gint gobPlace;
    gint bitPerSecond;               /* group of MBs*/
    gint vopRc;                      /* VOP rate control */
    gint mbRc;                       /* MB rate control */
    gint vbv;
    gint vopSkip;                    /* skip a VOP */
    gint qpHdr;                      /* QP header */
    gint qpHdrMin;                   /* QP header min */
    gint qpHdrMax;                   /* Qp header maximum */
    gint fps_num;              
    gint fps_den;
}
MP4EncOptions;

typedef struct MP4EncConfig_tag
{
    MP4EncInst encoder;             /* encoder configuration structure*/
    MP4EncIn encIn;                 /* encoder input configuration */
    MP4EncOut encOut;               /* encoder output configuration */
    MP4EncOptions encOpt;           /* encoder parameter */
    gint intraVopCnt;               /* intra VOP count*/
    gint vopCnt;                    /* number of VOPs encoded */
    gint src_img_size;              /* size of the input image */
}MP4EncConfig;

typedef struct MP4EncIORes_tag
{
    guint *outbuf;                    /* output buffer address */
    guint *picture;                   /* input buffer address */
    guint pict_size;                  /* input picture size */
    guint outbuf_size;                /* output buffer size */
    guint pict_bus_address;           /* input bus address */
    guint outbuf_bus_address;         /* output bus address */
    gint memdev_fd;                   /* memory device ID */
}MP4EncIORes;

typedef struct MFW_GST_MPEG4ENC_INFO_S
{
  GstElement    element;
  GstPad        *sinkpad;
  GstPad        *srcpad;
  gboolean      headerwrite;        /* flag to check if header write is done */
  gboolean      new_segment;        /* flag to check if new segment is sent */
  MP4EncIORes   enc_iores;          /* input output resource config structure */
  MP4EncConfig  encConfig;          /* encoder configuration */
  gboolean			direct_mem;
}MFW_GST_MPEG4ENC_INFO_T;

typedef struct MFW_GST_MPEG4ENC_CLASS_S 
{
  GstElementClass parent_class;
}MFW_GST_MPEG4ENC_CLASS_T;

/*=============================================================================
                                 GLOBAL VARIABLE DECLARATIONS
=============================================================================*/

/* None. */

/*=============================================================================
                                     FUNCTION PROTOTYPES
=============================================================================*/
GType mfw_gst_mpeg4enc_profile_get_type(void);
GType mfw_gst_mpeg4enc_get_type (void);
G_END_DECLS


/*===========================================================================*/

#endif /* MFW_GST_MPEG4_ENCODER_H */
