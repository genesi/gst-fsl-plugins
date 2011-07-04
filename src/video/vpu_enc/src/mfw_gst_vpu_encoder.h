/*
 * Copyright (C) 2005-2010 Freescale Semiconductor, Inc. All rights reserved.
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
 * Module Name:    mfw_gst_vpu_encoder.h  
 *
 * Description:    Include File for Hardware (VPU) Encoder Plugin 
 *                 for Gstreamer
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
#ifndef __MFW_GST_VPU_ENCODER_H__
#define __MFW_GST_VPU_ENCODER_H__


/*======================================================================================
                                     LOCAL CONSTANTS
=======================================================================================*/

/*maximum limit of the output buffer */
#define BUFF_FILL_SIZE (200 * 1024)

/* Maximum width and height */
#define MAX_WIDTH		4096 
#define MAX_HEIGHT		4096

/* Default height, width - Set to QCIF */
#define DEFAULT_HEIGHT	176
#define DEFAULT_WIDTH	144

#define DEFAULT_FRAME_RATE	30
#define DEFAULT_I_FRAME_INTERVAL 0
#define DEFAULT_GOP_SIZE    30

#define VPU_HUFTABLE_SIZE 432
#define VPU_QMATTABLE_SIZE 192

#define VPU_DEFAULT_QP 10
#define VPU_DEFAULT_H264_QP 35
#define VPU_DEFAULT_MPEG4_QP 15
#define VPU_MAX_H264_QP 51
#define VPU_MAX_MPEG4_QP 31

#define MAX_TIMESTAMP         32
#define TIMESTAMP_INDEX_MASK (MAX_TIMESTAMP-1)


#define TIMESTAMP_IN(context, timestamp)\
    do{\
        (context)->timestamp_buffer[(context)->ts_rx] = (timestamp);\
        (context)->ts_rx = (((context)->ts_rx+1) & TIMESTAMP_INDEX_MASK);\
    }while(0)

#define TIMESTAMP_OUT(context, timestamp)\
    do{\
        (timestamp) = (context)->timestamp_buffer[(context)->ts_tx];\
        (context)->ts_tx = (((context)->ts_tx+1) & TIMESTAMP_INDEX_MASK);\
    }while(0)

#define DESIRED_FRAME_TIMESTAMP(context, noffset)\
    ((context)->segment_starttime\
                +gst_util_uint64_scale_int(GST_SECOND, ((context)->segment_encoded_frame+(noffset))*(context)->framerate_d,\
                (context)->framerate_n))

#if (defined(VPU_MX51) || defined(VPU_MX53))
#define MFW_GST_VPUENC_VIDEO_CAPS \
    "video/mpeg, " \
    "width = (int) [16,  " STR(MAX_WIDTH) "], " \
    "height = (int) [16, " STR(MAX_HEIGHT) "]; " \
    \
    "video/x-h263, " \
    "width = (int) [16, " STR(MAX_WIDTH) "], " \
    "height = (int)[16, " STR(MAX_HEIGHT) "]; " \
    \
    "video/x-h264, " \
    "width = (int) [16, " STR(MAX_WIDTH) "], " \
    "height = (int)[16, " STR(MAX_HEIGHT) "]; "\
    \
    "image/jpeg, " \
    "width = (int) [16, " STR(MAX_WIDTH) "], " \
    "height = (int)[16, " STR(MAX_HEIGHT) "] "
#else
#define MFW_GST_VPUENC_VIDEO_CAPS \
    "video/mpeg, " \
    "width = (int) [16,  " STR(MAX_WIDTH) "], " \
    "height = (int) [16, " STR(MAX_HEIGHT) "]; " \
    \
    "video/x-h263, " \
    "width = (int) [16, " STR(MAX_WIDTH) "], " \
    "height = (int)[16, " STR(MAX_HEIGHT) "]; " \
    \
    "video/x-h264, " \
    "width = (int) [16, " STR(MAX_WIDTH) "], " \
    "height = (int)[16, " STR(MAX_HEIGHT) "] "
#endif

/*======================================================================================
                          STATIC TYPEDEFS (STRUCTURES, UNIONS, ENUMS)
=======================================================================================*/

/* properties set on the encoder */
enum {
    MFW_GST_VPU_PROP_0,
    MFW_GST_VPU_CODEC_TYPE,
    MFW_GST_VPU_PROF_ENABLE,
    MFW_GST_VPUENC_WIDTH,
    MFW_GST_VPUENC_HEIGHT,
    MFW_GST_VPUENC_FRAME_RATE,
    MFW_GST_VPUENC_BITRATE,
    MFW_GST_VPUENC_FORCEIINTERVAL,
    MFW_GST_VPUENC_GOP,
    MFW_GST_VPUENC_QP,
    MFW_GST_VPUENC_MAX_QP,
    MFW_GST_VPUENC_GAMMA,
    MFW_GST_VPUENC_INTRAREFRESH,
    MFW_GST_VPUENC_H263PROFILE0,
    MFW_GST_VPUENC_LOOPBACK,
    MFW_GST_VPUENC_INTRA_QP,
    MFW_GST_VPUENC_CROP_LEFT,
    MFW_GST_VPUENC_CROP_TOP,
    MFW_GST_VPUENC_CROP_RIGHT,
    MFW_GST_VPUENC_CROP_BOTTOM,
    
};

/* get the element details */
static GstElementDetails mfw_gst_vpuenc_details =
    GST_ELEMENT_DETAILS("Freescale: Hardware (VPU) Encoder",
		    "Codec/Encoder/Video",
#if (defined(VPU_MX51) || defined(VPU_MX53))
		    "Encodes Raw YUV Data to MPEG4 SP,or H.264 BP, or H.263 Format, or MJPG Format",
#else
		    "Encodes Raw YUV Data to MPEG4 SP,or H.264 BP, or H.263 Format",
#endif
		    FSL_GST_MM_PLUGIN_AUTHOR);

static GstStaticPadTemplate mfw_gst_vpuenc_src_factory =
    GST_STATIC_PAD_TEMPLATE("src",
			GST_PAD_SRC,
			GST_PAD_ALWAYS,
			GST_STATIC_CAPS(MFW_GST_VPUENC_VIDEO_CAPS));

/* defines the source pad  properties of VPU Encoder element */
static GstStaticPadTemplate mfw_gst_vpuenc_sink_factory =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw-yuv, "
#if (defined(VPU_MX51) || defined(VPU_MX53))
        "format = (fourcc) {I420, NV12}, "
#else
        "format = (fourcc) {I420}, "
#endif
        "width = (int) [ 16, " STR(MAX_WIDTH) " ], "
        "height = (int) [ 16, " STR(MAX_HEIGHT) " ], "
        "framerate = (fraction) [ 0/1, 60/1 ]")
    );

/*======================================================================================
                                        LOCAL MACROS
=======================================================================================*/
#define	GST_CAT_DEFAULT	mfw_gst_vpuenc_debug

/*======================================================================================
                                      STATIC VARIABLES
=======================================================================================*/

/*======================================================================================
                                     GLOBAL VARIABLES
=======================================================================================*/

/*=============================================================================
                                           CONSTANTS
=============================================================================*/
#define NUM_INPUT_BUF   3

/*=============================================================================
                                             ENUMS
=============================================================================*/

/* None. */

/*=============================================================================
                                            MACROS
=============================================================================*/

G_BEGIN_DECLS

#define MFW_GST_TYPE_VPU_ENC (mfw_gst_type_vpu_enc_get_type())

#define MFW_GST_VPU_ENC(obj)  \
    (G_TYPE_CHECK_INSTANCE_CAST((obj),MFW_GST_TYPE_VPU_ENC,MfwGstVPU_Enc))

#define MFW_GST_VPU_ENC_CLASS(klass) \
    G_TYPE_CHECK_CLASS_CAST((klass),MFW_GST_TYPE_VPU_ENC,MfwGstVPU_EncClass))

#define MFW_GST_IS_VPU_ENC(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj),MFW_GST_TYPE_VPU_ENC))

#define MFW_GST_IS_VPU_ENC_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass),MFW_GST_TYPE_VPU_ENC))

// headers for H264
#define SPS_HDR 0
#define PPS_HDR 1

// headers for MPEG4
#define VOS_HDR 0
#define VIS_HDR 1
#define VOL_HDR 2

/*=============================================================================
                                 STRUCTURES AND OTHER TYPEDEFS
=============================================================================*/

typedef struct _MfwGstVPU_Enc 
{
    // Gstreamer plugin members
    GstElement          element;         // instance of base class
    GstPad              *sinkpad;        // sink pad of element
    GstPad              *srcpad;         // source pad of element
    GstElementClass     *parent_class;   // parent class

	// VPU specific members defined in vpu_lib.h
    EncHandle           handle;
    EncOpenParam        *encOP;
    EncInitialInfo      *initialInfo;
    EncOutputInfo       *outputInfo;
    EncParam            *encParam;


    // Input buffer members
    vpu_mem_desc        bit_stream_buf;  // input bit stream allocated memory
    guint8              *start_addr;     // start addres of the Hardware output buffer
    guint8              *curr_addr;      // current addres of the Hardware input buffer
    guint8              *end_addr;       // end address of hardware input buffer
    guint               gst_copied;      // amt copied before previous encode
    GstBuffer           *gst_buffer;     // buffer for wrap around to copy remainder after encode frame complete
 

	// State members
    gboolean            vpu_init;        // Signifies if VPU is initialized yet
    gboolean            is_frame_started;// Is VPU in encoding state

    // Header members
    gint                hdrsize;                       // total size of headers
    gint                num_total_frames;              // frame counter of all frames including skipped
    gint                num_encoded_frames;            // num_encoded_frames
    gint                frames_since_hdr_sent;         // num frames between headers sent
    GstBuffer           *hdr_data;
    GstBuffer           *codec_data;

    // Frame buffer members
    vpu_mem_desc        *vpuRegisteredFramesDesc;
    FrameBuffer         *vpuRegisteredFrames;
    FrameBuffer         vpuInFrame;
    gint                numframebufs;
    guint               yuv_frame_size;  // size of input
    guint               bytes_consumed;  // consumed from buffer
    
    // Properties set by input
    guint           	width;           // width of output
    guint           	height;          // height of output
    CodStd              codec;           // codec standard to be selected
    gboolean 	        heightProvided;  // Set when the user provides the height on the command line 
    gboolean 	        widthProvided; 	 // Set when the user provides the width on the command line 
    gboolean 	        codecTypeProvided; 	// Set when the user provides the compression format on the command line     
    guint               intraRefresh;    // least N MB's in every P-frame will be encoded as intra MB's.
    gboolean            h263profile0;    // to encode H.263 Profile 0 instead of default H.263 Profile 3
    guint               intra_qp;        // qp to set for all I frames

    // members for bitrate
    guint            	bitrate;         // bitrate
    guint               max_qp;          // maximum qp used for CBR only                
    guint               gamma;           // gamma used for CBR only
    guint               bits_per_second; // calculate using number of frames per second
    guint               bits_per_stream; // keep total per stream to use avg
    guint               frames_per_sec;  // counter to use for calculating avg bitrate per sec
    guint               fs_read;         // idx into array below for reading
    guint               fs_write;        // idx into array below for writing
    guint               frame_size[MAX_TIMESTAMP];  //subtract oldest frame size to keep running bit

    gboolean            profile;         // profiling info 
    guint               qp;              // quantization parameter if bitrate not set
    guint            	gopsize;	     // gop size - interval between I frames
    guint               forceIFrameInterval;  // Force i frame interval

    // Frame rate members
    gfloat          	src_framerate;  // source frame rate set on EncOpen
    gfloat          	tgt_framerate;  // target frame rate set on first frame with Enc_set
    gint                framerate_n;    // frame rate numerator passed from caps
    gint                framerate_d;    // frame rate denominator passed from caps
    guint               num_in_interval;   //number of frames in interval
    guint               num_enc_in_interval; // number of frames to encode before skipping
    guint               idx_interval;  // counter of frames in skipping interval
    gboolean            fDropping_till_IFrame;  // flag to drop until I frame    

    // timestamps
    GstClockTime        timestamp_buffer[MAX_TIMESTAMP];
    guint               ts_rx;           // received timestamps
    guint               ts_tx;           // output timestamps

	// members for fixed bit rate
    guint64             segment_encoded_frame;  // Used for generating fixed bit rate
    GstClockTime        segment_starttime;      // fixed bit rate timestamp generation
    gboolean            forcefixrate;       
    GstClockTime        time_per_frame;   // save the time for one frame to use later
    GstClockTime        total_time;       // time based on number of frames
    GstClockTime        time_before_enc;  // time saved before encode started

    guint32             format;          // fourcc format of input
    GstClockTime        enc_start;
    GstClockTime        enc_stop;
    gboolean            loopback;

    guint               crop_left;
    guint               crop_top;
    guint               crop_right;
    guint               crop_bottom;
    
}MfwGstVPU_Enc;

typedef struct _MfwGstVPU_EncClass 
{
    GstElementClass parent_class;

}MfwGstVPU_EncClass;

G_END_DECLS
#endif				/* __MFW_GST_VPU_ENCODER_H__ */
