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
 * Module Name:    mfw_gst_mpeg4decoder.h
 *
 * Description:    Header file for MPEG4-Decode Plug-in for GStreamer.
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

#ifndef _MFW_GST_MPEG4_DECODER_H
#define _MFW_GST_MPEG4_DECODER_H

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
G_BEGIN_DECLS
#define MFW_GST_TYPE_MPEG4_DECODER \
  (mfw_gst_mpeg4_decoder_get_type())
#define MFW_GST_MPEG4_DECODER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),MFW_GST_TYPE_MPEG4_DECODER,\
   MFW_GST_MPEG4_DECODER_INFO_T))
#define MFW_GST_MPEG4_DECODER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),MFW_GST_TYPE_MPEG4_DECODER,\
   MFW_GST_DIVX_DECODER_CLASS_T))
#define MFW_GST_IS_MPEG4_DECODER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),MFW_GST_TYPE_MPEG4_DECODER))
#define MFW_GST_IS_MPEG4_DECODER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),MFW_GST_TYPE_MPEG4_DECODER))
/*=============================================================================
                                 STRUCTURES AND OTHER TYPEDEFS
=============================================================================*/
    typedef struct {
    gint fd_pf;			/*device id of the post 
				   filtering */
    guint8 *pf_inbuf[2];	/*input buffer */
    gchar *pf_qp_buf;		/*buffer to store QP values */
    guint8 *pf_outbuf;		/*output buffer */
   // pf_buf in_buf[2];		/* input buffer request 
	//			   parameters */

   // pf_buf out_buf;		/*output buffer request 
	//			   parameters */


    gboolean deblock;		/*flag to check whether de
				   blocking enabled or not */

    gboolean pf_initdone;	/* flag to update if pf 
				   initialization is done or not */
} PFHandle;

typedef struct MFW_GST_MPEG4_DECODER_INFO_S {
    GstElement element;
    GstPad *sinkpad;
    GstPad *srcpad;
    gboolean init_done;		/*flag to check the status 
				   of decoder initialisation */

    GstBuffer *input_buffer;

    guint sizebuffer;
    guint outsize;		/*size of the output buffer */
    sMpeg4DecObject *Mpeg4DecObject;	/*decoder handle */
    gint width;
    gint height;
    gint frame_width_padded;    /* padded width */
    gint frame_height_padded;
    gboolean eos;		/*flag to check the end of 
				   stream */
    gboolean caps_set;		/*flag to check whether
				   capabilities or set or not */
    //PFHandle pf_handle;
    guint64 timestamp[16];	/* array to store the timestamps */
    guint rx_timestamp;		/* timestamp recieve offset */
    guint tx_timestamp;		/* timestamp transmit offset */
    glong Time;
    glong chain_Time;
    glong no_of_frames;
    glong no_of_frames_dropped;
    gfloat avg_fps_decoding;
    gfloat frame_rate;		/* frame rate */
    gint32 decoded_frames;	/* number of frames decoded */
    gboolean profiling;
    gboolean pffirst;
    GstBuffer *outbuff;
    gint cur_buf;
    gboolean send_newseg;
    GstClockTime next_ts;    /* next output timestamp */
    /* Variable to set buffer manager working mode */
    gint bmmode;

    gboolean is_sfd;
    struct sfd_frames_info sfd_info;    
    gint demo_mode; /* 0: Normal mode, 1: Demo mode 2: Demo ending */

    
} MFW_GST_MPEG4_DECODER_INFO_T;

typedef struct MFW_GST_MPEG4_DECODER_CLASS_S {
    GstElementClass parent_class;

} MFW_GST_MPEG4_DECODER_CLASS_T;

/*=============================================================================
                                 GLOBAL VARIABLE DECLARATIONS
=============================================================================*/

/* None. */

/*=============================================================================
                                     FUNCTION PROTOTYPES
=============================================================================*/

GType mfw_gst_mpeg4_decoder_get_type(void);

G_END_DECLS
/*===========================================================================*/
#endif				/* __MFW_GST_MPEG4DECODER_H__ */
