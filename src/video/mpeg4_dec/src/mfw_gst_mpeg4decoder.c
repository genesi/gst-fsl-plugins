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
 * Module Name:    mfw_gst_mpeg4decoder.c
 *
 * Description:    GStreamer Plug-in for MPEG4-Decoder
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

#include <fcntl.h>
#include <string.h>
#include <gst/gst.h>
#include <time.h>
#include <sys/time.h>
#include "mpeg4_dec_api.h"


#include "mfw_gst_utils.h"

#include "mfw_gst_mpeg4decoder.h"

/*=============================================================================
                                        LOCAL MACROS
=============================================================================*/
#define BIT_BUFFER_SIZE   1024
#define CALL_BUFF_LEN	   512
#define PROCESSOR_CLOCK    532
#define CROP_LEFT_LENGTH  16
#define CROP_TOP_LENGTH    16

#define MFW_GST_MPEG4_VIDEO_CAPS \
    "video/mpeg, " \
    "mpegversion = (int) 4, " \
    "width = (int) [0, 1280], " \
    "height = (int) [0, 720]; " \
    \
    "video/x-h263, " \
    "width = (int) [0, 1280], " \
    "height = (int)[0, 720] "



/* used	for	debugging */
#define	GST_CAT_DEFAULT    mfw_gst_mpeg4_decoder_debug
/*=============================================================================
                            LOCAL CONSTANTS
=============================================================================*/


/*=============================================================================
                LOCAL TYPEDEFS (STRUCTURES, UNIONS, ENUMS)
=============================================================================*/
enum {
    //DBL_DISABLE,
    //DBL_ENABLE,
    PROF_ENABLE = 1,
    MFW_MPEG4DEC_FRAMERATE,
	ID_BMMODE,
	ID_SFD,  /* Strategy of Frame dropping */
    
};

static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE("sink",
								   GST_PAD_SINK,
								   GST_PAD_ALWAYS,
								   GST_STATIC_CAPS
								   (MFW_GST_MPEG4_VIDEO_CAPS)
    );
/*=============================================================================
                                LOCAL MACROS
=============================================================================*/

/* None. */

/*=============================================================================
                               STATIC VARIABLES
=============================================================================*/
static GstElementClass *parent_class = NULL;

/* table with framerates expressed as fractions */
static const gint fpss[][2] = { {24000, 1001},
{24, 1}, {25, 1}, {30000, 1001},
{30, 1}, {50, 1}, {60000, 1001},
{60, 1}, {0, 1}
};

/*=============================================================================
                        STATIC FUNCTION PROTOTYPES
=============================================================================*/
GST_DEBUG_CATEGORY_STATIC(mfw_gst_mpeg4_decoder_debug);
static void mfw_gst_mpeg4_decoder_class_init(MFW_GST_MPEG4_DECODER_CLASS_T
					     * klass);
static void mfw_gst_mpeg4_decoder_base_init(MFW_GST_MPEG4_DECODER_CLASS_T *
					    klass);
static void mfw_gst_mpeg4_decoder_init(MFW_GST_MPEG4_DECODER_INFO_T *
				       filter);

static void mfw_gst_mpeg4_decoder_set_property(GObject * object,
					       guint prop_id,
					       const GValue * value,
					       GParamSpec * pspec);

static void mfw_gst_mpeg4_decoder_get_property(GObject * object,
					       guint prop_id,
					       GValue * value,
					       GParamSpec * pspec);

static gboolean mfw_gst_mpeg4_decoder_sink_event(GstPad *, GstEvent *);
static gboolean mfw_gst_mpeg4_decoder_src_event(GstPad *, GstEvent *);

static gboolean mfw_gst_mpeg4_decoder_set_caps(GstPad * pad,
					       GstCaps * caps);
static GstFlowReturn mfw_gst_mpeg4_decoder_chain(GstPad * pad,
						 GstBuffer * buf);

static GstStateChangeReturn
mfw_gst_mpeg4_decoder_change_state(GstElement *, GstStateChange);

/* Call back function used for direct render v2 */
static void* mfw_gst_mpeg4_getbuffer(void* pvAppContext);
static void mfw_gst_mpeg4_rejectbuffer(void* pbuffer, void* pvAppContext);
static void mfw_gst_mpeg4_releasebuffer(void* pbuffer, void* pvAppContext);


    
/*=============================================================================
                            LOCAL FUNCTIONS
=============================================================================*/

/*=============================================================================
FUNCTION:               mfw_gst_mpeg4_getbuffer

DESCRIPTION:            Callback function for decoder. The call is issued when 
                        decoder need a new frame buffer.

ARGUMENTS PASSED:       pvAppContext -> Pointer to the context variable.

RETURN VALUE:           Pointer to a frame buffer.  -> On success.
                        Null.                       -> On fail.

PRE-CONDITIONS:         None.

POST-CONDITIONS:  	    None.
=============================================================================*/
static void* mfw_gst_mpeg4_getbuffer(void* pvAppContext)
{

	MFW_GST_MPEG4_DECODER_INFO_T * mpeg4_dec = (MFW_GST_MPEG4_DECODER_INFO_T *)pvAppContext;
	void * pbuffer;
	GstCaps *caps = NULL;
	int output_size ;

	sMpeg4DecObject *psMpeg4DecObject = mpeg4_dec->Mpeg4DecObject;
	 
	if (mpeg4_dec->caps_set == FALSE) {
	gint64 start = 0;	/*  proper timestamp has to set here */
	GstCaps *caps;
	gint fourcc = GST_STR_FOURCC("I420");
	guint framerate_n, framerate_d;
    guint crop_right_len = 0, crop_bottom_len = 0;
    if (psMpeg4DecObject->sDecParam.sOutputBuffer.eOutputFormat = E_MPEG4D_420_YUV) {
	caps = gst_caps_new_simple("video/x-raw-yuv",
				   "format", GST_TYPE_FOURCC, fourcc,
				   "width", G_TYPE_INT,
				   psMpeg4DecObject->sDecParam.
				   u16FrameWidth, "height", G_TYPE_INT,
				   psMpeg4DecObject->sDecParam.
				   u16FrameHeight, "pixel-aspect-ratio",
				   GST_TYPE_FRACTION, 1, 1, NULL);
    	}
#ifdef PADDED_OUTPUT
     if (psMpeg4DecObject->sDecParam.sOutputBuffer.eOutputFormat = E_MPEG4D_420_YUV_PADDED) {
	 	crop_right_len =
		    mpeg4_dec->frame_width_padded - 
                    psMpeg4DecObject->sDecParam.u16FrameWidth - CROP_LEFT_LENGTH;
		     
		crop_bottom_len =
		    mpeg4_dec->frame_height_padded -
		    psMpeg4DecObject->sDecParam.u16FrameHeight - CROP_TOP_LENGTH;

		GST_DEBUG("right crop=%d,bottom crop=%d\n", crop_right_len,
			  crop_bottom_len);

		caps =
		    gst_caps_new_simple("video/x-raw-yuv", "format",
					GST_TYPE_FOURCC, fourcc, "width",
					G_TYPE_INT,
					mpeg4_dec->frame_width_padded,
					"height", G_TYPE_INT,
					mpeg4_dec->frame_height_padded,
					"pixel-aspect-ratio", GST_TYPE_FRACTION, 1, 1,
					"crop-left-by-pixel", G_TYPE_INT, CROP_LEFT_LENGTH,
					"crop-top-by-pixel", G_TYPE_INT, CROP_TOP_LENGTH,
					"crop-right-by-pixel", G_TYPE_INT,
					(crop_right_len + 7) / 8 * 8,
					"crop-bottom-by-pixel", G_TYPE_INT,
					(crop_bottom_len + 7) / 8 * 8, 
					"num-buffers-required", G_TYPE_INT,
					BM_GET_BUFFERNUM,
					   NULL); 
     	}
        if (mpeg4_dec->is_sfd) {
            GST_ADD_SFD_FIELD(caps);
        }
#endif /* PADDED_OUTPUT */
	if (mpeg4_dec->frame_rate != 0) {
	    framerate_n = (mpeg4_dec->frame_rate * 1000);
	    framerate_d = 1000;
	    gst_caps_set_simple(caps, "framerate", GST_TYPE_FRACTION,
				framerate_n, framerate_d, NULL);
	}
	if (!(gst_pad_set_caps(mpeg4_dec->srcpad, caps))) {
	    GST_ERROR
		("\nCould not set the caps for the mpeg4decoder src pad\n");
	}
	mpeg4_dec->caps_set = TRUE;
	gst_caps_unref(caps);
    }

	
	BM_GET_BUFFER(mpeg4_dec->srcpad, mpeg4_dec->outsize, pbuffer);
	return pbuffer;
}

/*=============================================================================
FUNCTION:               mfw_gst_mpeg4_rejectbuffer

DESCRIPTION:            Callback function for decoder. The call is issued when 
                        decoder want to indicate a frame buffer would not be 
                        used as a output.

ARGUMENTS PASSED:       pbuffer      -> Pointer to the frame buffer for reject
                        pvAppContext -> Pointer to the context variable.

RETURN VALUE:           None

PRE-CONDITIONS:         None.

POST-CONDITIONS:  	    None.
=============================================================================*/
static void mfw_gst_mpeg4_rejectbuffer(void* pbuffer, void* pvAppContext)
{
    BM_REJECT_BUFFER(pbuffer);
}


/*=============================================================================
FUNCTION:               mfw_gst_mpeg4_releasebuffer

DESCRIPTION:            Callback function for decoder. The call is issued when 
                        decoder want to indicate a frame buffer would never used
                        as a reference.

ARGUMENTS PASSED:       pbuffer      -> Pointer to the frame buffer for release
                        pvAppContext -> Pointer to the context variable.

RETURN VALUE:           None

PRE-CONDITIONS:         None.

POST-CONDITIONS:  	    None.
=============================================================================*/
static void mfw_gst_mpeg4_releasebuffer(void* pbuffer, void* pvAppContext)
{
    BM_RELEASE_BUFFER(pbuffer);
}
/*=============================================================================
FUNCTION: mfw_gst_mpeg4_decoder_set_property

DESCRIPTION: sets the property of the element

ARGUMENTS PASSED:
        object     - pointer to the elements object
        prop_id    - ID of the property;
        value      - value of the property set by the application
        pspec      - pointer to the attributes of the property

RETURN VALUE:
        None

PRE-CONDITIONS:
        None

POST-CONDITIONS:


IMPORTANT NOTES:
        None
=============================================================================*/

static void
mfw_gst_mpeg4_decoder_set_property(GObject * object, guint prop_id,
				   const GValue * value,
				   GParamSpec * pspec)
{

    MFW_GST_MPEG4_DECODER_INFO_T *mpeg4_dec =
	MFW_GST_MPEG4_DECODER(object);
    switch (prop_id) {
#if 0        
    case DBL_ENABLE:
	mpeg4_dec->pf_handle.deblock = g_value_get_boolean(value);
	GST_DEBUG("deblock=%d\n", mpeg4_dec->pf_handle.deblock);
	break;
#endif
    case PROF_ENABLE:
	mpeg4_dec->profiling = g_value_get_boolean(value);
	GST_DEBUG("profiling=%d\n", mpeg4_dec->profiling);
	break;
    case MFW_MPEG4DEC_FRAMERATE:
	mpeg4_dec->frame_rate = g_value_get_float(value);
	GST_DEBUG("framerate=%f\n", mpeg4_dec->frame_rate);
	break;
	case ID_BMMODE:
	mpeg4_dec->bmmode= g_value_get_int(value);
	GST_DEBUG("buffermanager mode=%d\n", mpeg4_dec->bmmode);
	break;

	case ID_SFD:
	mpeg4_dec->is_sfd = g_value_get_boolean(value);

    default:
	G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
	break;
    }

}

/*=============================================================================
FUNCTION: mfw_gst_mpeg4_decoder_get_property

DESCRIPTION: gets the property of the element

ARGUMENTS PASSED:
        object     - pointer to the elements object
        prop_id    - ID of the property;
        value      - value of the property to be set for the next element
        pspec      - pointer to the attributes of the property

RETURN VALUE:
        None

PRE-CONDITIONS:
        None

POST-CONDITIONS:


IMPORTANT NOTES:
        None
=============================================================================*/
static void
mfw_gst_mpeg4_decoder_get_property(GObject * object, guint prop_id,
				   GValue * value, GParamSpec * pspec)
{

    MFW_GST_MPEG4_DECODER_INFO_T *mpeg4_dec =
	MFW_GST_MPEG4_DECODER(object);
    switch (prop_id) {
#if 0        
    case DBL_ENABLE:
	g_value_set_boolean(value, mpeg4_dec->pf_handle.deblock);
	break;
#endif
    case PROF_ENABLE:
	g_value_set_boolean(value, mpeg4_dec->profiling);
	break;
    case MFW_MPEG4DEC_FRAMERATE:
	g_value_set_float(value, mpeg4_dec->frame_rate);
	break;
	case ID_BMMODE:
	g_value_set_int(value, BM_GET_MODE);
	break;

	case ID_SFD:
	g_value_set_boolean(value, mpeg4_dec->is_sfd);

    default:
	G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
	break;
    }

}

/*=============================================================================
FUNCTION: pvAllocateFastMem

DESCRIPTION: allocates memory of required size from a fast memory

ARGUMENTS PASSED:
        size       - size of memory required to allocate
        align      - alignment required

RETURN VALUE:
        void *      - base address of the memory allocated

PRE-CONDITIONS:
        None

POST-CONDITIONS:


IMPORTANT NOTES:
        We have not taken any extra step to align the memory to 
        the required type. Assumption is that the allocated memory 
        is always word aligned. 
=============================================================================*/

void *pvAllocateFastMem(gint size, gint align)
{
    return g_malloc(size);

}

/*=============================================================================
FUNCTION: pvAllocateSlowMem

DESCRIPTION: allocates memory of required size from a slow memory

ARGUMENTS PASSED:
        size       - size of memory required to allocate
        align      - alignment required

RETURN VALUE:
        void *      - base address of the memory allocated

PRE-CONDITIONS:
        None

POST-CONDITIONS:


IMPORTANT NOTES:
        We have not taken any extra step to align the memory to 
        the required type. Assumption is that the allocated memory 
        is always word aligned. 
=============================================================================*/

void *pvAllocateSlowMem(gint size, gint align)
{
    return g_malloc(size);

}


/*=============================================================================
FUNCTION:		mfw_gst_mpeg4_decoder_allocatememory

DESCRIPTION:	This function allocates memory required by the decoder

ARGUMENTS PASSED:
        psMemAllocInfo     -  is a pointer to a structure which holds allocated
							  memory. This allocated memory is required by the
							  decoder.
RETURN VALUE:
				returns the status of Memory Allocation, -1/ 0

PRE-CONDITIONS:
        None

POST-CONDITIONS:
        None

IMPORTANT NOTES:
        None
=============================================================================*/

gint mfw_gst_mpeg4_decoder_allocatememory(sMpeg4DecMemAllocInfo *
					  psMemAllocInfo)
{

    gint s32MemBlkCnt = 0;
    sMpeg4DecMemBlock *psMemInfo;

    for (s32MemBlkCnt = 0; s32MemBlkCnt < psMemAllocInfo->s32NumReqs;
	 s32MemBlkCnt++) {
	psMemInfo = &psMemAllocInfo->asMemBlks[s32MemBlkCnt];
	if (MPEG4D_IS_FAST_MEMORY(psMemInfo->s32Type))
	    psMemInfo->pvBuffer = pvAllocateFastMem(psMemInfo->s32Size,
						    psMemInfo->s32Align);
	else
	    psMemInfo->pvBuffer = pvAllocateSlowMem(psMemInfo->s32Size,
						    psMemInfo->s32Align);
	GST_DEBUG("allocate mem=0x%x, size=%d \n", psMemInfo->pvBuffer,
		  psMemInfo->s32Size);
	if (psMemInfo->pvBuffer == NULL) {
	    return -1;
	}
    }

    return 0;

}

/*=============================================================================
FUNCTION:		mfw_gst_mpeg4_decoder_freememory

DESCRIPTION:	It deallocates all the memory which was allocated for the decoder

ARGUMENTS PASSED:
		psMemAllocInfo	-  is a pointer to a structure which holds allocated
							  memory. This allocated memory is required by the
							  decoder.
RETURN VALUE:
		None

PRE-CONDITIONS:
        None

POST-CONDITIONS:
        None

IMPORTANT NOTES:
        None
=============================================================================*/
void mfw_gst_mpeg4_decoder_freememory(sMpeg4DecMemAllocInfo *
				      psMemAllocInfo)
{

    gint s32MemBlkCnt = 0;


    for (s32MemBlkCnt = 0; s32MemBlkCnt < psMemAllocInfo->s32NumReqs;
	 s32MemBlkCnt++) {
	if (psMemAllocInfo->asMemBlks[s32MemBlkCnt].pvBuffer != NULL) {
	    g_free(psMemAllocInfo->asMemBlks[s32MemBlkCnt].pvBuffer);
	    psMemAllocInfo->asMemBlks[s32MemBlkCnt].pvBuffer = NULL;
	}
    }



}

/*=============================================================================
FUNCTION:		mfw_gst_mpeg4_decoder_cleanup

DESCRIPTION:	It deallocates all the memory which was allocated by Application

ARGUMENTS PASSED:
        psMpeg4DecObject    -   is a pointer to Mpeg4 Decoder handle

RETURN VALUE:
		None

PRE-CONDITIONS:
        None

POST-CONDITIONS:
        None

IMPORTANT NOTES:
        None
=============================================================================*/

void mfw_gst_mpeg4_decoder_cleanup(sMpeg4DecObject * psMpeg4DecObject)
{

    sMpeg4DecMemAllocInfo *psMemAllocInfo;
    psMemAllocInfo = &(psMpeg4DecObject->sMemInfo);
    MFW_GST_MPEG4_DECODER_INFO_T *mpeg4_dec;

    GST_DEBUG("In function mfw_gst_mpeg4_decoder_cleanup.\n");
    mpeg4_dec =
	(MFW_GST_MPEG4_DECODER_INFO_T *) psMpeg4DecObject->pvAppContext;
    /*!
     *   Freeing Memory Allocated by the Application for Decoder
     */
    mfw_gst_mpeg4_decoder_freememory(psMemAllocInfo);
    psMemAllocInfo = NULL;

    if (psMpeg4DecObject != NULL) {

	psMpeg4DecObject->pvMpeg4Obj = NULL;
	psMpeg4DecObject->pvAppContext = NULL;

	g_free(psMpeg4DecObject);
	psMpeg4DecObject = NULL;
    }
#if 0
    if (mpeg4_dec->pf_handle.deblock == TRUE) {
	if (mpeg4_dec->pf_handle.pf_initdone == TRUE)
	    pf_uninit(mpeg4_dec);
    }
#endif

    GST_DEBUG("out of function mfw_gst_mpeg4_decoder_cleanup.\n");

}

/*=============================================================================
FUNCTION:		cbkMPEG4DBufRead

DESCRIPTION:	cbkMPEG4DBufRead() is used by the decoder to get a new input
				buffer for decoding. It copies the "s32EncStrBufLen" of encoded
				data into "pu8EncStrBuf". This function is called by the decoder
				in eMPEG4DInit() and eMPEG4DDecode() functions, when it runs out
				of current bit stream input buffer.

ARGUMENTS PASSED:
        s32EncStrBufLen     -   is the Length of the Buffer used to hold
								the encoded bitstream
		pu8EncStrBuf		-	Buffer used to hold the encoded bitstream
		s32Offset			-	offset of the first byte starting from start
                                of bitstream
		pvAppContext		-	is a pointer which points to application
								handle of a specific instance of the decoder


RETURN VALUE:
				returns number of bytes copied in to the Encoded
			    Stream Buffer.

PRE-CONDITIONS:
        None

POST-CONDITIONS:
        None

IMPORTANT NOTES:
        None
=============================================================================*/


gint cbkMPEG4DBufRead(gint s32EncStrBufLen, guint8 * pu8EncStrBuf,
		      gint s32Offset, void *pvAppContext)
{
    /* this function is no need for SHA optimized MPEG4 dec */

    return 0;

}

/*=============================================================================
FUNCTION:   InitailizeMpeg4DecObject

DESCRIPTION: This function initialzes all the members of the decoder handle.

ARGUMENTS PASSED:
        psMpeg4DecObject     -   pointer to decoder handle


RETURN VALUE:
		None

PRE-CONDITIONS:
        None

POST-CONDITIONS:
        None

IMPORTANT NOTES:
        None
=============================================================================*/
static
void InitailizeMpeg4DecObject(sMpeg4DecObject * psMpeg4DecObject)
{

    gint S32Count;
    /* Memory info initialization */
    psMpeg4DecObject->sMemInfo.s32NumReqs = 0;
    for (S32Count = 0; S32Count < MAX_NUM_MEM_REQS; S32Count++) {
	psMpeg4DecObject->sMemInfo.asMemBlks[S32Count].s32Size = 0;
	psMpeg4DecObject->sMemInfo.asMemBlks[S32Count].s32Type = 0;
	psMpeg4DecObject->sMemInfo.asMemBlks[S32Count].s32Priority = 0;
	psMpeg4DecObject->sMemInfo.asMemBlks[S32Count].s32Align =
	    E_MPEG4D_ALIGN_NONE;
	psMpeg4DecObject->sMemInfo.asMemBlks[S32Count].pvBuffer = NULL;
    }

    psMpeg4DecObject->sDecParam.sOutputBuffer.pu8YBuf = NULL;
    psMpeg4DecObject->sDecParam.sOutputBuffer.pu8CbBuf = NULL;
    psMpeg4DecObject->sDecParam.sOutputBuffer.pu8CrBuf = NULL;
    psMpeg4DecObject->sDecParam.sOutputBuffer.s32YBufLen = 0;
    psMpeg4DecObject->sDecParam.sOutputBuffer.s32CbBufLen = 0;
    psMpeg4DecObject->sDecParam.sOutputBuffer.s32CrBufLen = 0;

    psMpeg4DecObject->sDecParam.u16FrameWidth = 0;
    psMpeg4DecObject->sDecParam.u16FrameHeight = 0;
    psMpeg4DecObject->sDecParam.u16DecodingScheme =
	MPEG4D_START_DECODE_AT_IFRAME;
    psMpeg4DecObject->sDecParam.u16TicksPerSec = 0;

    psMpeg4DecObject->sDecParam.sTime.s32Seconds = 0;
    psMpeg4DecObject->sDecParam.sTime.s32MilliSeconds = 0;
    psMpeg4DecObject->sDecParam.s32TimeIncrementInTicks = 1;
    psMpeg4DecObject->sDecParam.u8VopType = 'I';


    psMpeg4DecObject->sDecParam.p8MbQuants = NULL;
    psMpeg4DecObject->sVisParam.s32NumberOfVos = 1;
    for (S32Count = 0; S32Count < MAX_VIDEO_OBJECTS; S32Count++) {
	psMpeg4DecObject->sVisParam.as32NumberOfVols[S32Count] = 1;
    }

    psMpeg4DecObject->pvMpeg4Obj = NULL;
    psMpeg4DecObject->pvAppContext = NULL;
    psMpeg4DecObject->eState = E_MPEG4D_INVALID;
    psMpeg4DecObject->ptr_cbkMPEG4DBufRead = NULL;



}

/*=============================================================================
FUNCTION:   mfw_gst_mpeg4_decframe

DESCRIPTION: This function decodes and outputs one frame per call to the next 
             element

ARGUMENTS PASSED:
        mpeg4_dec            -   mpeg4 decoder plugin handle
        psMpeg4DecObject     -   pointer to decoder handle


RETURN VALUE:
            GST_FLOW_OK if decode is successfull
            GST_FLOW_ERROR if error in decoding
    

PRE-CONDITIONS:
        None

POST-CONDITIONS:
        None

IMPORTANT NOTES:
        None
=============================================================================*/

static GstFlowReturn mfw_gst_mpeg4_decframe(MFW_GST_MPEG4_DECODER_INFO_T *
					    mpeg4_dec,
					    sMpeg4DecObject *
					    psMpeg4DecObject)
{
    eMpeg4DecRetType eDecRetVal = E_MPEG4D_FAILURE;
    GstBuffer *outbuffer = NULL;
    guint8 *outdata = NULL;
    GstCaps *src_caps = NULL;
    GstFlowReturn result = GST_FLOW_OK;
    gchar *hw_qp;
    gint8 *quant;
    sMpeg4DecoderParams *pDecPar;
    unsigned short row, col;
    /*gint cur_buf = 0; */
    PFHandle *pfhandle;
    struct timeval tv_prof, tv_prof1;
    long time_before = 0, time_after = 0;
    unsigned int addr = 0;
   // pfhandle = &mpeg4_dec->pf_handle;
    src_caps = GST_PAD_CAPS(mpeg4_dec->srcpad);
    guint pLength = mpeg4_dec->sizebuffer;
    guint8 *pvBuf = NULL;
    guint32 copy_size=0;
	GstClockTime ts;

    if (mpeg4_dec->demo_mode == 2)
        return GST_FLOW_ERROR;
   

    /*The main decoder function is eMPEG4DDecode. This function decodes 
       the MPEG4 bit stream in the input buffers to generate one frame of decoder 
       output in every call. */
    if (mpeg4_dec->profiling) {
	    gettimeofday(&tv_prof, 0);
    }

	pvBuf = (guint8 *) GST_BUFFER_DATA(mpeg4_dec->input_buffer);
	eDecRetVal = eMPEG4DDecode(psMpeg4DecObject, pvBuf, &pLength);

    GST_BUFFER_OFFSET(mpeg4_dec->input_buffer) += pLength;
	
    if (mpeg4_dec->profiling) {
	gettimeofday(&tv_prof1, 0);
	time_before = (tv_prof.tv_sec * 1000000) + tv_prof.tv_usec;
	time_after = (tv_prof1.tv_sec * 1000000) + tv_prof1.tv_usec;
	mpeg4_dec->Time += time_after - time_before;
	if (eDecRetVal == E_MPEG4D_SUCCESS) {
	    mpeg4_dec->no_of_frames++;
	} else {
	    mpeg4_dec->no_of_frames_dropped++;
	}
    }

	if (eDecRetVal == E_MPEG4D_NOT_ENOUGH_BITS) {
	GST_WARNING("ret E_MPEG4D_NOT_ENOUGH_BITS.\n");
	return GST_FLOW_OK;
    } else if (eDecRetVal == E_MPEG4D_ENDOF_BITSTREAM) {
        
	GST_WARNING("ret E_MPEG4D_ENDOF_BITSTREAM.\n");
	return GST_FLOW_OK;
    }
#ifdef OUTPUT_BUFFER_CHANGES       
    else 
    if ( (eDecRetVal == E_MPEG4D_SUCCESS) 
        || ( E_MPEG4D_DEMO_PROTECT == eDecRetVal)
       )
	eDecRetVal = eMPEG4DGetOutputFrame(psMpeg4DecObject);
#endif


    if ( (eDecRetVal == E_MPEG4D_SUCCESS) 
        || ( E_MPEG4D_DEMO_PROTECT == eDecRetVal)
       )
    {

		if (mpeg4_dec->frame_rate == 0) {
	    	ts =
				mpeg4_dec->timestamp[(mpeg4_dec->tx_timestamp++) % 16];
		} else {
	    	if (GST_CLOCK_TIME_IS_VALID
				(GST_BUFFER_TIMESTAMP(mpeg4_dec->input_buffer))) {
		    	mpeg4_dec->next_ts =
				GST_BUFFER_TIMESTAMP(mpeg4_dec->input_buffer);
	    	} else {
				mpeg4_dec->next_ts =
		    	gst_util_uint64_scale(mpeg4_dec->decoded_frames,
					  GST_SECOND,
					  mpeg4_dec->frame_rate);
	    	}

	    	ts = mpeg4_dec->next_ts;

		}
        
        DEMO_LIVE_CHECK(mpeg4_dec->demo_mode, 
            ts, 
            mpeg4_dec->srcpad);
    
	    /* the data is pushed onto the next element */

	    if (mpeg4_dec->send_newseg) {
	    gboolean ret = FALSE;
	    ret =
		gst_pad_push_event(mpeg4_dec->srcpad,
				   gst_event_new_new_segment(FALSE, 1.0,
							     GST_FORMAT_TIME,
							     ts,
							     GST_CLOCK_TIME_NONE,
							     ts));
	    mpeg4_dec->send_newseg = FALSE;
	    }
	    BM_RENDER_BUFFER(psMpeg4DecObject->sDecParam.sOutputBuffer.pu8YBuf, mpeg4_dec->srcpad, result, ts, 0);
    }



#if 0//disabe pp deblock for direct render v1
	if (pfhandle->deblock == TRUE) {
	    guint offset;
	    if (!mpeg4_dec->pffirst) {
		if (pf_wait(mpeg4_dec) < 0) {
		    GST_ERROR
			("\nError waiting for post-filter to complete.\n");
		    result = GST_FLOW_ERROR;
                    gst_buffer_unref(outbuffer);
		    return result;
		}
	    }


	    /* The parallelism is achived here by calling the pfstart for 
	       current frame  after returning  out of pf_wait of the previous frame 
	       Also the previous frame is pushed on to the sink element */

	    /* Note the pf_wait and the gst_pad_push is not called for 
	       the first frame */

	    /* deblocking for a frame happens here */
	    offset = GST_BUFFER_OFFSET(outbuffer);
	    if (pf_start(mpeg4_dec->cur_buf, offset, mpeg4_dec) < 0) {
		GST_ERROR("\nError in pf_start\n");
		result = GST_FLOW_ERROR;
                gst_buffer_unref(outbuffer);
		return result;
	    }
	    mpeg4_dec->cur_buf = !mpeg4_dec->cur_buf;

	    if (mpeg4_dec->pffirst) {

		mpeg4_dec->outbuff = outbuffer;
		mpeg4_dec->pffirst = FALSE;
		return GST_FLOW_OK;

	    }
	}
#endif
	
    mpeg4_dec->decoded_frames++;
    mpeg4_dec->outbuff = outbuffer;
    return result;
}


/*=============================================================================
FUNCTION: mfw_gst_mpeg4_decoder_chain

DESCRIPTION: Initializing the decoder and calling the actual decoding function

ARGUMENTS PASSED:
        pad     - pointer to pad
        buffer  - pointer to received buffer

RETURN VALUE:
        GST_FLOW_OK		- Frame decoded successfully
		GST_FLOW_ERROR	- Failure

PRE-CONDITIONS:
        None

POST-CONDITIONS:


IMPORTANT NOTES:
        None
=============================================================================*/

static GstFlowReturn
mfw_gst_mpeg4_decoder_chain(GstPad * pad, GstBuffer * buffer)
{

    MFW_GST_MPEG4_DECODER_INFO_T *mpeg4_dec;
    sMpeg4DecObject *psMpeg4DecObject = NULL;
    eMpeg4DecRetType eDecRetVal = E_MPEG4D_FAILURE;
    gint ret = GST_FLOW_ERROR;
    guint64 outsize = 0;
    GstBuffer *outbuffer = NULL;
    guint8 *outdata = NULL;
    GstCaps *src_caps = NULL;
    GstFlowReturn result = GST_FLOW_OK;
    sMpeg4DecMemAllocInfo *psMemAllocInfo = NULL;
    guint8 *frame_buffer = NULL;
    guint temp_length = 0;
    gchar *hw_qp;
    gint8 *quant;
    sMpeg4DecoderParams *pDecPar;
    unsigned short row, col;
    gint cur_buf = 0;
    //PFHandle *pfhandle;
    struct timeval tv_prof2, tv_prof3;
    long time_before = 0, time_after = 0;
    mpeg4_dec = MFW_GST_MPEG4_DECODER(GST_PAD_PARENT(pad));
    if (mpeg4_dec->profiling) {
	gettimeofday(&tv_prof2, 0);
    }

    //pfhandle = &mpeg4_dec->pf_handle;
    mpeg4_dec->timestamp[(mpeg4_dec->rx_timestamp++) % 16] =
	GST_BUFFER_TIMESTAMP(buffer);

    mpeg4_dec->input_buffer = buffer;
    mpeg4_dec->sizebuffer = GST_BUFFER_SIZE(buffer);

    if (!mpeg4_dec->init_done) {
	if (GST_CLOCK_TIME_IS_VALID(GST_BUFFER_TIMESTAMP(buffer))) {
	    mpeg4_dec->next_ts = GST_BUFFER_TIMESTAMP(buffer);
	}

	/* allocate memory for the decoder */
	psMpeg4DecObject =
	    (sMpeg4DecObject *) g_malloc(sizeof(sMpeg4DecObject));

	if (psMpeg4DecObject == NULL) {
	    GST_ERROR
		("\nUnable to allocate memory for Mpeg4 Decoder structure\n");
	    //mfw_gst_mpeg4_decoder_cleanup(psMpeg4DecObject);
	    return GST_FLOW_ERROR;
	} else {
	    InitailizeMpeg4DecObject(psMpeg4DecObject);
	}

	psMpeg4DecObject->pvAppContext = (void *) (mpeg4_dec);
	frame_buffer = (guint8 *) GST_BUFFER_DATA(mpeg4_dec->input_buffer);

#ifndef OUTPUT_BUFFER_CHANGES
	    psMpeg4DecObject->sDecParam.sOutputBuffer.eOutputFormat = E_MPEG4D_420_YUV;
#else
#ifdef PADDED_OUTPUT
	    psMpeg4DecObject->sDecParam.sOutputBuffer.eOutputFormat = E_MPEG4D_420_YUV_PADDED;
#else
	    psMpeg4DecObject->sDecParam.sOutputBuffer.eOutputFormat = E_MPEG4D_420_YUV;
#endif
#endif

	/*This function returns the memory requirement for the decoder. 
	   The decoder will parse the sent bit stream to determine the type of 
	   video content, based on which sMpeg4DecObject.sMemInfo structure 
	   will be populated. The plugin will use this structure to pre-allocate 
	   the requested memory block (chunks) by setting the pointers of asMemBlks 
	   in sMpeg4DecObject.sMemInfo structure to the required size, type & 
	   aligned memory. */
	eDecRetVal = eMPEG4DQuerymem(psMpeg4DecObject,
				     frame_buffer, mpeg4_dec->sizebuffer);

	if (eDecRetVal != E_MPEG4D_SUCCESS) {
	    GST_ERROR("Function eMPEG4DQuerymem() resulted in failure\n");
	    GST_ERROR("MPEG4D Error Type : %d\n", eDecRetVal);

	    /*! Freeing Memory allocated by the Application */
	    mfw_gst_mpeg4_decoder_cleanup(psMpeg4DecObject);
	    return GST_FLOW_ERROR;
	}

	/*!
	 *   Allocating Memory for MPEG4 Decoder
	 */
	psMemAllocInfo = &(psMpeg4DecObject->sMemInfo);
	if (mfw_gst_mpeg4_decoder_allocatememory(psMemAllocInfo) == -1) {
	    GST_ERROR("\nUnable to allocate memory for Mpeg4 Decoder\n");

	    /*! Freeing Memory allocated by the Application */
	    mfw_gst_mpeg4_decoder_cleanup(psMpeg4DecObject);
	    return GST_FLOW_ERROR;
	}

	/* In current version, it should return "E_MPEG4D_SUCCESS", 
	   and the application don't need to check any other */
	eDecRetVal = eMPEG4DInit(psMpeg4DecObject);

	if (eDecRetVal != E_MPEG4D_SUCCESS) {
	    /*!  Freeing Memory allocated by the Application */
	    mfw_gst_mpeg4_decoder_cleanup(psMpeg4DecObject);
	    return GST_FLOW_ERROR;
	}

	/* Allocate memory to hold the quant values, make sure that we round it
	 * up in the higher side, as non-multiple of 16 will be extended to
	 * next 16 bits value
	 */

    /* Init buffer manager for correct working mode.*/
	BM_INIT((mpeg4_dec->bmmode)? BMINDIRECT : BMDIRECT, psMpeg4DecObject->sMemInfo.s32MinFrameBufferNum, RENDER_BUFFER_MAX_NUM);
	{
		eMPEG4DSetAdditionalCallbackFunction (psMpeg4DecObject, E_GET_FRAME, (void*)mfw_gst_mpeg4_getbuffer);        
        eMPEG4DSetAdditionalCallbackFunction (psMpeg4DecObject, E_REJECT_FRAME, (void*)mfw_gst_mpeg4_rejectbuffer);        
        eMPEG4DSetAdditionalCallbackFunction (psMpeg4DecObject, E_RELEASE_FRAME, (void*)mfw_gst_mpeg4_releasebuffer);
        
	}
    
	if (1){//(!pfhandle->deblock) {
	    psMpeg4DecObject->sDecParam.p8MbQuants = (gint8 *)
		g_malloc(((psMpeg4DecObject->sDecParam.u16FrameWidth +
			   15) >> 4) *
			 ((psMpeg4DecObject->sDecParam.u16FrameHeight +
			   15) >> 4) * sizeof(gint8));
	}
    

	psMpeg4DecObject->s32EnableErrorConceal = 0;
        mpeg4_dec->frame_width_padded = (psMpeg4DecObject->sDecParam.u16FrameWidth
	                                 +15)/16*16+32; 
        mpeg4_dec->frame_height_padded = (psMpeg4DecObject->sDecParam.u16FrameHeight
	                                  +15)/16*16+32; 
	/* output buffer size */
	mpeg4_dec->outsize = (mpeg4_dec->frame_width_padded * mpeg4_dec->frame_height_padded * 3) / 2;

 #if 0//disable pp deblock for direct render v1       
	if (pfhandle->deblock == TRUE) {
	    pfhandle->out_buf.size = mpeg4_dec->outsize;

	    /* Initialization of deblocking driver */
	    pf_init(psMpeg4DecObject->sDecParam.u16FrameWidth,
		    psMpeg4DecObject->sDecParam.u16FrameHeight,
		    psMpeg4DecObject->sDecParam.u16FrameWidth, mpeg4_dec);

	    pfhandle->pf_initdone = TRUE;


	    /* Input to the deblocking driver is set */
	    pfhandle->in_buf[0].y_offset = pfhandle->in_buf[1].y_offset =
		0;
	    pfhandle->in_buf[0].u_offset = pfhandle->in_buf[1].u_offset =
		pfhandle->in_buf[0].y_offset +
		psMpeg4DecObject->sDecParam.sOutputBuffer.s32YBufLen + 0;
	    pfhandle->in_buf[0].v_offset = pfhandle->in_buf[1].v_offset =
		pfhandle->in_buf[0].u_offset +
		psMpeg4DecObject->sDecParam.sOutputBuffer.s32CbBufLen + 0;
	}
#endif
	ret = GST_FLOW_OK;
	mpeg4_dec->Mpeg4DecObject = psMpeg4DecObject;
	mpeg4_dec->init_done = 1;
	//    return ret;

    }

    psMpeg4DecObject = mpeg4_dec->Mpeg4DecObject;

    if (mpeg4_dec->is_sfd) { //
        static int is_key_frame = 0;
        GstFlowReturn ret;
        struct sfd_frames_info *pSfd_info = &mpeg4_dec->sfd_info;
        is_key_frame = GST_BUFFER_FLAG_IS_SET(buffer,GST_BUFFER_FLAG_IS_SYNC);
        if (is_key_frame) {
         GST_DEBUG("Sync count = %d.\n",pSfd_info->total_key_frames);
        }
        ret = Strategy_FD(is_key_frame,pSfd_info);

        if (ret == GST_FLOW_ERROR)
        {
  	        mpeg4_dec->tx_timestamp =
    		    (mpeg4_dec->tx_timestamp + 1) % 16;

            gst_buffer_unref(buffer);
            return GST_FLOW_OK;
        }
    }

    /* no need while to decode since demuxer need to make sure only one frame data pushed to here */
    result = mfw_gst_mpeg4_decframe(mpeg4_dec, psMpeg4DecObject);
    if (mpeg4_dec->profiling) {
	gettimeofday(&tv_prof3, 0);
	time_before = (tv_prof2.tv_sec * 1000000) + tv_prof2.tv_usec;
	time_after = (tv_prof3.tv_sec * 1000000) + tv_prof3.tv_usec;
	mpeg4_dec->chain_Time += time_after - time_before;
    }
    gst_buffer_unref(buffer);
    GST_DEBUG("Out of function mfw_gst_mpeg4_decoder_chain.\n");
    return result;
}

/*=============================================================================
FUNCTION:   mfw_gst_mpeg4_decoder_change_state

DESCRIPTION: this function keeps track of different states of pipeline.

ARGUMENTS PASSED:
        element     -   pointer to element
        transition  -   state of the pipeline

RETURN VALUE:
        GST_STATE_CHANGE_FAILURE    - the state change failed
        GST_STATE_CHANGE_SUCCESS    - the state change succeeded
        GST_STATE_CHANGE_ASYNC      - the state change will happen
                                        asynchronously
        GST_STATE_CHANGE_NO_PREROLL - the state change cannot be prerolled

PRE-CONDITIONS:
        None

POST-CONDITIONS:
        None

IMPORTANT NOTES:
        None
=============================================================================*/
static GstStateChangeReturn
mfw_gst_mpeg4_decoder_change_state(GstElement * element,
				   GstStateChange transition)
{

    GstStateChangeReturn ret = 0;
    MFW_GST_MPEG4_DECODER_INFO_T *mpeg4_dec;
    mpeg4_dec = MFW_GST_MPEG4_DECODER(element);
    sMpeg4DecObject *psMpeg4DecObject = NULL;
    switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
	{

	    mpeg4_dec->input_buffer = NULL;
	    mpeg4_dec->sizebuffer = 0;
	    mpeg4_dec->eos = 0;
	    mpeg4_dec->caps_set = FALSE;
	    //mpeg4_dec->pf_handle.pf_initdone = FALSE;
	    mpeg4_dec->Time = 0;
	    mpeg4_dec->chain_Time = 0;
	    mpeg4_dec->no_of_frames = 0;
	    mpeg4_dec->avg_fps_decoding = 0.0;
	    mpeg4_dec->no_of_frames_dropped = 0;

	}
	break;

    case GST_STATE_CHANGE_READY_TO_PAUSED:
	{
	    mpeg4_dec->decoded_frames = 0;
	    mpeg4_dec->pffirst = 1;
	    mpeg4_dec->cur_buf = 0;
	    mpeg4_dec->send_newseg = FALSE;
	    mpeg4_dec->next_ts = 0;
	}
	break;

    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
	break;
    default:
	break;
    }

    ret = parent_class->change_state(element, transition);

    switch (transition) {
	float avg_mcps = 0, avg_plugin_time = 0, avg_dec_time = 0;
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
	break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:

	mpeg4_dec->send_newseg = FALSE;
	mpeg4_dec->cur_buf = 0;
	mpeg4_dec->pffirst = 1;
	psMpeg4DecObject = mpeg4_dec->Mpeg4DecObject;

	if (psMpeg4DecObject) {
            if ((eMPEG4DFree(psMpeg4DecObject)) != E_MPEG4D_SUCCESS) {
                GST_ERROR("Function eMPEG4DFree() failed\n");
            }

	    /*! Freeing Memory allocated by the Application */
	    mfw_gst_mpeg4_decoder_cleanup(psMpeg4DecObject);
	    if (1){//(!mpeg4_dec->pf_handle.deblock) {
		if (psMpeg4DecObject->sDecParam.p8MbQuants) {
		    g_free(psMpeg4DecObject->sDecParam.p8MbQuants);
		    psMpeg4DecObject->sDecParam.p8MbQuants = NULL;
		}
	    }
	}
	if (mpeg4_dec->profiling) {

	    g_print("PROFILE FIGURES OF MPEG4 DECODER PLUGIN");
	    g_print("\nTotal decode time is                   %ldus",
		    mpeg4_dec->Time);
	    g_print("\nTotal plugin time is                   %ldus",
		    mpeg4_dec->chain_Time);
	    g_print("\nTotal number of frames decoded is      %d",
		    mpeg4_dec->no_of_frames);
	    g_print("\nTotal number of frames dropped is      %d\n",
		    mpeg4_dec->no_of_frames_dropped);

	    if (mpeg4_dec->frame_rate != 0) {
		avg_mcps = ((float) mpeg4_dec->Time * PROCESSOR_CLOCK /
			    (1000000 *
			     (mpeg4_dec->no_of_frames -
			      mpeg4_dec->no_of_frames_dropped)))
		    * mpeg4_dec->frame_rate;
		g_print("\nAverage decode MCPS is               %f",
			avg_mcps);

		avg_mcps =
		    ((float) mpeg4_dec->chain_Time * PROCESSOR_CLOCK /
		     (1000000 *
		      (mpeg4_dec->no_of_frames -
		       mpeg4_dec->no_of_frames_dropped)))
		    * mpeg4_dec->frame_rate;
		g_print("\nAverage plug-in MCPS is               %f",
			avg_mcps);
	    } else {
		g_print
		    ("enable the Frame Rate property of the decoder to get the MCPS \
               ..... \n ! mfw_mpeg4decoder framerate=value ! .... \
               \n Note: value denotes the framerate to be set");
	    }



	    avg_dec_time =
		((float) mpeg4_dec->Time) / mpeg4_dec->no_of_frames;
	    g_print("\nAverage decoding time is               %fus",
		    avg_dec_time);
	    avg_plugin_time =
		((float) mpeg4_dec->chain_Time) / mpeg4_dec->no_of_frames;
	    g_print("\nAverage plugin time is                 %fus\n",
		    avg_plugin_time);

	    mpeg4_dec->Time = 0;
	    mpeg4_dec->chain_Time = 0;
	    mpeg4_dec->no_of_frames = 0;
	    mpeg4_dec->avg_fps_decoding = 0.0;
	    mpeg4_dec->no_of_frames_dropped = 0;
	}


	mpeg4_dec->input_buffer = NULL;
	mpeg4_dec->sizebuffer = 0;
	mpeg4_dec->eos = 0;
	mpeg4_dec->caps_set = FALSE;
	//mpeg4_dec->pf_handle.pf_initdone = FALSE;
	mpeg4_dec->caps_set = FALSE;
	mpeg4_dec->init_done = 0;
	mpeg4_dec->rx_timestamp = 0;
	mpeg4_dec->tx_timestamp = 0;
	break;
    case GST_STATE_CHANGE_READY_TO_NULL:
        BM_CLEAN_LIST;
	break;
    default:
	break;
    }

    return ret;

}

/*=============================================================================
FUNCTION:		mfw_gst_mpeg4_decoder_sink_event

DESCRIPTION:	This functions handles the events that triggers the
				sink pad of the mpeg4 decoder element.

ARGUMENTS PASSED:
        pad        -    pointer to pad
        event      -    pointer to event
RETURN VALUE:
        TRUE       -	if event is sent to sink properly
	    FALSE	   -	if event is not sent to sink properly

PRE-CONDITIONS:
        None

POST-CONDITIONS:
        None

IMPORTANT NOTES:
        None
=============================================================================*/
static gboolean
mfw_gst_mpeg4_decoder_sink_event(GstPad * pad, GstEvent * event)
{


    GstFlowReturn result = GST_FLOW_OK;
    gboolean ret = TRUE;
    MFW_GST_MPEG4_DECODER_INFO_T *mpeg4_dec;
    GstBuffer *outbuffer;
    guint8 *outdata;
    eMpeg4DecRetType eDecRetVal = E_MPEG4D_FAILURE;
    guint size;
    GstCaps *src_caps = NULL;
    sMpeg4DecObject *psMpeg4DecObject = NULL;
    gchar *hw_qp;
    gint8 *quant;
    sMpeg4DecoderParams *pDecPar;
    unsigned short row, col;
    gint cur_buf = 0;
    GstFormat format;

    mpeg4_dec = MFW_GST_MPEG4_DECODER(GST_PAD_PARENT(pad));
    psMpeg4DecObject = mpeg4_dec->Mpeg4DecObject;

    switch (GST_EVENT_TYPE(event)) {
    case GST_EVENT_NEWSEGMENT:
	{


	    GstFormat format;
	    gint64 start, stop, position;
	    gdouble rate;

	    gst_event_parse_new_segment(event, NULL, &rate, &format,
					&start, &stop, &position);

	    GST_DEBUG(" start = %" GST_TIME_FORMAT, GST_TIME_ARGS(start));
	    GST_DEBUG(" stop = %" GST_TIME_FORMAT, GST_TIME_ARGS(stop));


	    GST_DEBUG(" position in mpeg4  =%" GST_TIME_FORMAT,
		      GST_TIME_ARGS(position));

	    if (GST_FORMAT_TIME == format) {
		mpeg4_dec->decoded_frames = (gint32) (start * (gfloat) (mpeg4_dec->frame_rate) / GST_SECOND);	

		result = gst_pad_push_event(mpeg4_dec->srcpad, event);
	    } else {
		GST_DEBUG("dropping newsegment	event in format	%s",
			  gst_format_get_name(format));
		gst_event_unref(event);
		mpeg4_dec->send_newseg = TRUE;
	    }
	    break;
	}

    case GST_EVENT_EOS:
	{
	    GST_DEBUG("\nDecoder: Got an EOS from Demuxer\n");
	    mpeg4_dec->eos = 1;
        GST_WARNING("total frames :%d, dropped frames: %d.\n",mpeg4_dec->sfd_info.total_frames,
            mpeg4_dec->sfd_info.dropped_frames);

	    result = gst_pad_push_event(mpeg4_dec->srcpad, event);
	    if (!result) {
		GST_ERROR("\n Error in pushing the event,result is %d\n",
			  result);
	    } else {
		GST_DEBUG("\n EOS event sent to the peer element\n");
	    }
	    break;
	}
    case GST_EVENT_FLUSH_STOP:
	{

	    result = gst_pad_push_event(mpeg4_dec->srcpad, event);

	    if (TRUE != result) {
		GST_ERROR("\n Error in pushing the event,result	is %d\n",
			  result);
	    }
	    break;
	}
	/* not handling flush start */
    case GST_EVENT_FLUSH_START:
    default:
	{
	    result = gst_pad_event_default(pad, event);
	    break;
	}

    }

    if (result == GST_FLOW_OK)
	ret = TRUE;
    else
	ret = FALSE;
    return ret;
}


/*=============================================================================
FUNCTION:   mfw_gst_mpeg4_decoder_src_event

DESCRIPTION: This functions handles the events that triggers the
			 source pad of the mpeg4 decoder element.

ARGUMENTS PASSED:
        pad        -    pointer to pad
        event      -    pointer to event
RETURN VALUE:
	    FALSE	   -	if event is not sent to src properly
        TRUE       -	if event is sent to src properly
PRE-CONDITIONS:
        None

POST-CONDITIONS:
        None

IMPORTANT NOTES:
        None
=============================================================================*/

static gboolean
mfw_gst_mpeg4_decoder_src_event(GstPad * pad, GstEvent * event)
{
    gboolean res;

    MFW_GST_MPEG4_DECODER_INFO_T *mpeg4_dec =
	MFW_GST_MPEG4_DECODER(gst_pad_get_parent(pad));

    if (mpeg4_dec == NULL) {
	GST_DEBUG_OBJECT(mpeg4_dec, "no decoder, cannot handle event");
	gst_event_unref(event);
	return FALSE;
    }

    switch (GST_EVENT_TYPE(event)) {
    case GST_EVENT_SEEK:
	res = gst_pad_push_event(mpeg4_dec->sinkpad, event);
	break;
	/* judge the timestamp from system time */
    case GST_EVENT_QOS:
    {
    if (mpeg4_dec->is_sfd) { //
        struct sfd_frames_info *pSfd_info = &mpeg4_dec->sfd_info;
		gdouble proportion;
		GstClockTimeDiff diff;
		GstClockTime timestamp;

		gst_event_parse_qos(event, &proportion, &diff, &timestamp);

		if (diff >= 0) {
	        GST_QOS_EVENT_HANDLE(pSfd_info,diff,mpeg4_dec->frame_rate);
		} else {
		    GST_DEBUG
			("the time of decoding is before the system, it is OK\n");
		}
		res = gst_pad_push_event(mpeg4_dec->sinkpad, event);
    }
	break;
    }
    case GST_EVENT_NAVIGATION:
	/* Forward a navigation event unchanged */
    default:
	res = gst_pad_push_event(mpeg4_dec->sinkpad, event);
	break;
    }

    gst_object_unref(mpeg4_dec);
    return res;

}

/*=============================================================================
FUNCTION:               src_templ

DESCRIPTION:            Template to create a srcpad for the decoder.

ARGUMENTS PASSED:       None.


RETURN VALUE:           a GstPadTemplate


PRE-CONDITIONS:  	    None

POST-CONDITIONS:   	    None

IMPORTANT NOTES:   	    None
=============================================================================*/
static GstPadTemplate *src_templ(void)
{
    static GstPadTemplate *templ = NULL;

    if (!templ) {
	GstCaps *caps;
	GstStructure *structure;
	GValue list = { 0 }
	, fps = {
	0}
	, fmt = {
	0};
	gchar *fmts[] = { "YV12", "I420", "Y42B", NULL };
	guint n;

	caps = gst_caps_new_simple("video/x-raw-yuv",
				   "format", GST_TYPE_FOURCC,
				   GST_MAKE_FOURCC('I', '4', '2', '0'),
				   "width", GST_TYPE_INT_RANGE, 16, 4096,
				   "height", GST_TYPE_INT_RANGE, 16, 4096,
				   NULL);

	structure = gst_caps_get_structure(caps, 0);

	g_value_init(&list, GST_TYPE_LIST);
	g_value_init(&fps, GST_TYPE_FRACTION);
	for (n = 0; fpss[n][0] != 0; n++) {
	    gst_value_set_fraction(&fps, fpss[n][0], fpss[n][1]);
	    gst_value_list_append_value(&list, &fps);
	}
	gst_structure_set_value(structure, "framerate", &list);
	g_value_unset(&list);
	g_value_unset(&fps);

	g_value_init(&list, GST_TYPE_LIST);
	g_value_init(&fmt, GST_TYPE_FOURCC);
	for (n = 0; fmts[n] != NULL; n++) {
	    gst_value_set_fourcc(&fmt, GST_STR_FOURCC(fmts[n]));
	    gst_value_list_append_value(&list, &fmt);
	}
	gst_structure_set_value(structure, "format", &list);
	g_value_unset(&list);
	g_value_unset(&fmt);

	templ =
	    gst_pad_template_new("src", GST_PAD_SRC, GST_PAD_ALWAYS, caps);
    }

    return templ;
}

/*=============================================================================
FUNCTION:   mfw_gst_mpeg4_decoder_set_caps

DESCRIPTION:    this function handles the link with other plug-ins and used for
                capability negotiation  between pads

ARGUMENTS PASSED:
        pad        -    pointer to GstPad
        caps       -    pointer to GstCaps

RETURN VALUE:
        TRUE       -    if capabilities are set properly
        FALSE      -    if capabilities are not set properly
PRE-CONDITIONS:
        None

POST-CONDITIONS:
        None

IMPORTANT NOTES:
        None
=============================================================================*/
static gboolean
mfw_gst_mpeg4_decoder_set_caps(GstPad * pad, GstCaps * caps)
{
    MFW_GST_MPEG4_DECODER_INFO_T *mpeg4_dec;
    GstStructure *structure = gst_caps_get_structure(caps, 0);
    mpeg4_dec = MFW_GST_MPEG4_DECODER(GST_OBJECT_PARENT(pad));

    const gchar *mime;
    gint32 frame_rate_de = 0;
    gint32 frame_rate_nu = 0;
    mime = gst_structure_get_name(structure);

    gst_structure_get_fraction(structure, "framerate", &frame_rate_nu,
			       &frame_rate_de);

    if (frame_rate_de != 0) {
	mpeg4_dec->frame_rate = (gfloat) (frame_rate_nu) / frame_rate_de;
    }
    GST_DEBUG(" Frame Rate = %f \n", mpeg4_dec->frame_rate);
#ifdef PADDED_OUTPUT
    gst_structure_get_int(structure, "width",
			  &mpeg4_dec->width);
    gst_structure_get_int(structure, "height",
			  &mpeg4_dec->height);

    GST_DEBUG(" Frame Width  = %d\n", mpeg4_dec->width);

    GST_DEBUG(" Frame Height = %d\n", mpeg4_dec->height);

    mpeg4_dec->frame_width_padded = (mpeg4_dec->width+15)/16*16+32; 
    mpeg4_dec->frame_height_padded = (mpeg4_dec->height+15)/16*16+32;
#endif /* OUTPUT_BUFFER_CHANGES */
    if (!gst_pad_set_caps(mpeg4_dec->srcpad, caps)) {
	return FALSE;
    }
    return TRUE;
}

/*=============================================================================
FUNCTION:   mfw_gst_mpeg4_decoder_init

DESCRIPTION:This function creates the pads on the elements and register the
			function pointers which operate on these pads.

ARGUMENTS PASSED:
        pointer the mpeg4_decoder element handle.

RETURN VALUE:
        None

PRE-CONDITIONS:
        _base_init and _class_init are called

POST-CONDITIONS:
        None

IMPORTANT NOTES:
        None
=============================================================================*/
static void
mfw_gst_mpeg4_decoder_init(MFW_GST_MPEG4_DECODER_INFO_T * mpeg4_dec)
{
    GstElementClass *klass = GST_ELEMENT_GET_CLASS(mpeg4_dec);
    mpeg4_dec->init_done = 0;

    mpeg4_dec->sinkpad =
	gst_pad_new_from_template(gst_element_class_get_pad_template
				  (klass, "sink"), "sink");

    gst_pad_set_setcaps_function(mpeg4_dec->sinkpad,
				 mfw_gst_mpeg4_decoder_set_caps);
    gst_pad_set_chain_function(mpeg4_dec->sinkpad,
			       mfw_gst_mpeg4_decoder_chain);
    gst_pad_set_event_function(mpeg4_dec->sinkpad,
			       GST_DEBUG_FUNCPTR
			       (mfw_gst_mpeg4_decoder_sink_event));

    gst_element_add_pad(GST_ELEMENT(mpeg4_dec), mpeg4_dec->sinkpad);

    mpeg4_dec->srcpad = gst_pad_new_from_template(src_templ(), "src");
    gst_pad_set_event_function(mpeg4_dec->srcpad,
			       GST_DEBUG_FUNCPTR
			       (mfw_gst_mpeg4_decoder_src_event));

    gst_element_add_pad(GST_ELEMENT(mpeg4_dec), mpeg4_dec->srcpad);
    //mpeg4_dec->pf_handle.deblock = DBL_DISABLE;
    mpeg4_dec->profiling = FALSE;
    mpeg4_dec->frame_rate = 0.0;
    INIT_SFD_INFO(&mpeg4_dec->sfd_info);
    mpeg4_dec->is_sfd = TRUE;

#define MFW_GST_MPEG4_DECODER_PLUGIN VERSION
    PRINT_CORE_VERSION(MPEG4DCodecVersionInfo());
    PRINT_PLUGIN_VERSION(MFW_GST_MPEG4_DECODER_PLUGIN);

    INIT_DEMO_MODE(MPEG4DCodecVersionInfo(), mpeg4_dec->demo_mode);
}

/*=============================================================================
FUNCTION:   mfw_gst_mpeg4_decoder_class_init

DESCRIPTION:Initialise the class only once (specifying what signals,
            arguments and virtual functions the class has and setting up
            global state)
ARGUMENTS PASSED:
       	klass   - pointer to mpeg4 element class

RETURN VALUE:
        None

PRE-CONDITIONS:
        None

POST-CONDITIONS:
        None

IMPORTANT NOTES:
        None
=============================================================================*/

static void
mfw_gst_mpeg4_decoder_class_init(MFW_GST_MPEG4_DECODER_CLASS_T * klass)
{
    GObjectClass *gobject_class = NULL;
    GstElementClass *gstelement_class = NULL;
    gobject_class = (GObjectClass *) klass;
    gstelement_class = (GstElementClass *) klass;
    parent_class = g_type_class_ref(GST_TYPE_ELEMENT);
    gobject_class->set_property = mfw_gst_mpeg4_decoder_set_property;
    gobject_class->get_property = mfw_gst_mpeg4_decoder_get_property;
    gstelement_class->change_state = mfw_gst_mpeg4_decoder_change_state;

/* disable pp deblock since direct render v1 cannot make sure the output buffer is writable or not*/
#if 0
    g_object_class_install_property(gobject_class, DBL_ENABLE,
				    g_param_spec_boolean("deblock",
							 "Deblock",
							 "apply post filtering to the image ",
							 FALSE,
							 G_PARAM_READWRITE));
#endif
    g_object_class_install_property(gobject_class, PROF_ENABLE,
				    g_param_spec_boolean("profiling", "Profiling", "enable time profiling of the plug-in \
        and the decoder", FALSE, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, MFW_MPEG4DEC_FRAMERATE,
				    g_param_spec_float("framerate",
						       "FrameRate",
						       "gets the framerate at which the input stream is to be displayed",
						       -G_MAXFLOAT,
						       G_MAXFLOAT, 0.0,
						       G_PARAM_READWRITE));

    /* install property for buffer manager mode control. */
	g_object_class_install_property(gobject_class, ID_BMMODE,
				    g_param_spec_int("bmmode",
						       "BMMode",
						       "set the buffer manager mode direct/indirect",
						       0,
						       1, 0,
						       G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, ID_SFD,
				    g_param_spec_boolean("sfd",
						       "Strategy of Frame Dropping",
						       "Strategy of Frame Dropping, 0: Disable, 1: Enable",
						       TRUE,
						       G_PARAM_READWRITE));
	
}

/*=============================================================================
FUNCTION:  mfw_gst_mpeg4_decoder_base_init

DESCRIPTION:
            mpeg4decoder element details are registered with the plugin during
            _base_init ,This function will initialise the class and child
            class properties during each new child class creation


ARGUMENTS PASSED:
        Klass   -   pointer to mpeg4 decoder plug-in class
        g_param_spec_float("framerate", "FrameRate", 
        "gets the framerate at which the input stream is to be displayed",
        None

PRE-CONDITIONS:
        None

POST-CONDITIONS:
        None

IMPORTANT NOTES:
        None
=============================================================================*/
static void
mfw_gst_mpeg4_decoder_base_init(MFW_GST_MPEG4_DECODER_CLASS_T * klass)
{
    static GstElementDetails element_details = {
	"Freescale MPEG4 Decoder",
	"Codec/Decoder/Video",
	"Decodes Simple Profile MPEG4 Bitstreams",
	"Multimedia Team <teammmsw@freescale.com>"
    };
    GstElementClass *element_class = GST_ELEMENT_CLASS(klass);
    gst_element_class_add_pad_template(element_class, src_templ());
    gst_element_class_add_pad_template(element_class,
				       gst_static_pad_template_get
				       (&sink_factory));
    gst_element_class_set_details(element_class, &element_details);
}

/*=============================================================================
FUNCTION: mfw_gst_mpeg4_decoder_get_type

DESCRIPTION:    intefaces are initiated in this function.you can register one
                or more interfaces  after having registered the type itself.

ARGUMENTS PASSED:
            None

RETURN VALUE:
                 A numerical value ,which represents the unique identifier of this
            element(mpeg4decoder)

PRE-CONDITIONS:
            None

POST-CONDITIONS:
            None

IMPORTANT NOTES:
            None
=============================================================================*/

GType mfw_gst_mpeg4_decoder_get_type(void)
{
    static GType mpeg4_decoder_type = 0;

    if (!mpeg4_decoder_type) {
	static const GTypeInfo mpeg4_decoder_info = {
	    sizeof(MFW_GST_MPEG4_DECODER_CLASS_T),
	    (GBaseInitFunc) mfw_gst_mpeg4_decoder_base_init,
	    NULL,
	    (GClassInitFunc) mfw_gst_mpeg4_decoder_class_init,
	    NULL,
	    NULL,
	    sizeof(MFW_GST_MPEG4_DECODER_INFO_T),
	    0,
	    (GInstanceInitFunc) mfw_gst_mpeg4_decoder_init,
	};
	mpeg4_decoder_type = g_type_register_static(GST_TYPE_ELEMENT,
						    "MFW_GST_MPEG4_DECODER_INFO_T",
						    &mpeg4_decoder_info,
						    0);
    }
    GST_DEBUG_CATEGORY_INIT(mfw_gst_mpeg4_decoder_debug,
			    "mfw_mpeg4decoder", 0,
			    "FreeScale's MPEG4 Decoder's Log");
    return mpeg4_decoder_type;
}

/*=============================================================================
FUNCTION:   plugin_init

DESCRIPTION:    special function , which is called as soon as the plugin or
                element is loaded and information returned by this function
                will be cached in central registry

ARGUMENTS PASSED:
        plugin     -    pointer to container that contains features loaded
                        from shared object module

RETURN VALUE:
        return TRUE or FALSE depending on whether it loaded initialized any
        dependency correctly

PRE-CONDITIONS:
        None

POST-CONDITIONS:
        None

IMPORTANT NOTES:
        None
=============================================================================*/
static gboolean plugin_init(GstPlugin * plugin)
{
    return gst_element_register(plugin, "mfw_mpeg4decoder",
				GST_RANK_PRIMARY,
				MFW_GST_TYPE_MPEG4_DECODER);
}

/*****************************************************************************/
/*    This is used to define the entry point and meta data of plugin         */
/*****************************************************************************/
GST_PLUGIN_DEFINE(GST_VERSION_MAJOR,	/* major version of Gstreamer    */
		  GST_VERSION_MINOR,	/* minor version of Gstreamer    */
		  "mfw_mpeg4decoder",	/* name of the plugin            */
		  "decodes the mpeg4 simple profile video bitstreams",	/* what plugin actually does     */
		  plugin_init,	/* first function to be called   */
		  VERSION,
		  GST_LICENSE_UNKNOWN,
		  "Freescale Semiconductor", "www.freescale.com ")
