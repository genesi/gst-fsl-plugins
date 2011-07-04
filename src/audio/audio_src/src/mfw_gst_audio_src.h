/*
 * Copyright (C) 2009 Freescale Semiconductor, Inc. All rights reserved.
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
 * Module Name:    mfw_gst_audio_src.h
 *
 * Description:    
 *
 * Portability:    This code is written for Linux OS and Gstreamer
 */  
 
/*
 * Changelog: 
 * Apr 20 2009 Haiting Yin <B00625@freescale.com>
 * - Initial version
 */


/*=============================================================================
                            INCLUDE FILES
=============================================================================*/
#ifndef __MFW_GST_AUDIO_SRC_H__
#define __MFW_GST_AUDIO_SRC_H__

#include <gst/gst.h>
#include <gst/base/gstadapter.h>

/*=============================================================================
                                           CONSTANTS
=============================================================================*/
/* None. */
#define BUF_NUM 4
/*=============================================================================
                                             ENUMS
=============================================================================*/
/* plugin property ID */
enum{
    PROPER_ID_OUTPUT_SAMPLE_RATE = 1,
    PROPER_ID_OUTPUT_BITS_PER_SAMPLE = 2,
    PROPER_ID_OUTPUT_PACKED_24BITS = 3,
    PROPER_ID_FAST_SRC_MODE = 4,
    PROPER_ID_USE_ASRC = 5,
    PROPER_ID_DMA_BUFFER_SIZE_ASRC = 6,
    PROPER_ID_ASRC_IN_CLK = 7,
    PROPER_ID_ASRC_OUT_CLK = 8,

};

/*=============================================================================
                                            MACROS
=============================================================================*/
G_BEGIN_DECLS

/* #defines don't like whitespacey bits */
#define MFW_GST_TYPE_AUDIO_SRC \
    (mfw_gst_audio_src_get_type())
#define MFW_GST_AUDIO_SRC(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj),MFW_GST_TYPE_AUDIO_SRC, MfwGstAudioSrc))
#define MFW_GST_AUDIO_SRC_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass),MFW_GST_TYPE_AUDIO_SRC, MfwGstAudioSrcClass))
#define MFW_GST_IS_AUDIO_SRC(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj),MFW_GST_TYPE_AUDIO_SRC))
#define MFW_GST_IS_AUDIO_SRC_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass),MFW_GST_TYPE_AUDIO_SRC))

/*=============================================================================
                                 STRUCTURES AND OTHER TYPEDEFS
=============================================================================*/
typedef struct _MfwGstAudioASrcBuf {
	gint8 *start;
	gint index;
	gint length;
	gint max_len;
}MfwGstAudioASrcBuf;

typedef struct _MfwGstAudioASrcInfo {
	gint input_sample_rate;
	gint channel;
	gint  data_len;
	gint  output_data_len;
	gint output_sample_rate;
	gint bitwidth;
	gint output_bitwidth;
}MfwGstAudioASrcInfo;

typedef struct _MfwGstAudioSrc
{
    GstElement element;
    GstPad *sinkpad, *srcpad;
    gboolean capsSet;
    gboolean init;
    GstAdapter *pAdapter_in;
   //Used for SRC
    Src_Config *pAudioSrcConfig;
    Src_Params *pAudioSrcParams;
    gint8 *inptr;
    gint8 *outptr;
    gint OutSampleRate;
    gint OutBitsPerSample;
    gboolean Packed_24Bit_out;
    gboolean FastSrcMode;
    //Used for ASRC
    gboolean UseASRC;           // 1: use ASRC, 0 : use FSL SRC
    gboolean ASRC_START;
    gboolean paused;
    gint     fd_asrc;           // device handle of ASRC
    gint    dma_buffer_size;
    gint    dma_buffer_num;
    MfwGstAudioASrcInfo *pAudioASrcInfo;
    MfwGstAudioASrcBuf sAduioASrcBufIn[BUF_NUM];
    MfwGstAudioASrcBuf sAduioASrcBufOut[BUF_NUM];
    gint    nInBufQueued;
    gint    nOutBufDequeued;
    enum asrc_pair_index pair_index;
    enum asrc_inclk inclk;
    enum asrc_outclk outclk;	

}MfwGstAudioSrc;

typedef struct _MfwGstAudioSrcClass 
{
    GstElementClass parent_class;
}MfwGstAudioSrcClass;


/*=============================================================================
                                 GLOBAL VARIABLE DECLARATIONS
=============================================================================*/
/* None. */

/*=============================================================================
                                     FUNCTION PROTOTYPES
=============================================================================*/
GType mfw_gst_audio_src_get_type (void);

G_END_DECLS

/*===========================================================================*/

#endif /* __MFW_GST_AUDIO_SRC_H__ */
