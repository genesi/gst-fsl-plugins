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
 * Module Name:    mfw_gst_mpeg4encoder.c
 *
 * Description:    Implementation of gstreamer plugin for the MPEG4 hantro
 *                 Encoder. Supports upto VGA resolution input image.
 *                 The Defult parameters of encoding are 
 *                 1) Profile and level: MPEG4_ADV_SIMPLE_PROFILE_LEVEL_5
 *                 2) scheme: MPEG4_PLAIN_STRM
 *                 3) Width  176
 *                 4) Height 144
 *                 5) Bitrate 384kbps
 *                 6) Frame rate 30fps
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
#include <gst/gst.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>

/* For printing and file IO */
#include <stdio.h>

/* For dynamic memory allocation */
#include <stdlib.h>

/* For command line parsing */
#include <getopt.h>
#include <string.h> 

/* For Hantro MPEG4 encoder */
#include "mp4encapi.h"

/* For linear memory allocation */
#include "memalloc.h"   
#include "mfw_gst_utils.h"
#include "mfw_gst_mpeg4encoder.h"

/*=============================================================================
                            LOCAL CONSTANTS
=============================================================================*/

/*=============================================================================
                LOCAL TYPEDEFS (STRUCTURES, UNIONS, ENUMS)
=============================================================================*/

enum
{
    MP4ENC_0=0,
    MP4ENC_BITRATE,         /* Selection of Bitrate of encoding  */     
    MP4ENC_WIDTH,           /* to set input image width  */    
    MP4ENC_HEIGHT,          /* to set input image height */    
    MP4ENC_FRAMERATE_NUM,       
    MP4ENC_FRAMERATE_DEN,   /* set the frame rate fraction */    
    MP4ENC_SCHEME,          /* set the scheme of encoding MPEG4 or H263 or SVH */    
    MP4ENC_MB_RC,           /* set the MB rate control value */
    MP4ENC_VOP_RC,          /* set the VOP rate control value */
    MP4ENC_VOPSKIP_RC,      /* enable skipping of VOPs for RC */
    MP4ENC_PROFILE_LEVEL,   /* set the profile and level for encoding */
    MP4ENC_GOP_SIZE,        /* set the Group of Picture size for encoding */
    MP4ENC_CROP_HOR,        /* set the cropping horizontal offset for preprocessing */
    MP4ENC_CROP_VER,        /* set the cropping vertical offset for preprocessing */
    MP4ENC_STAB_AREA        /* set the Camera stabilisation */
};
static GstStaticPadTemplate sink_factory =
GST_STATIC_PAD_TEMPLATE (
  "sink",
  GST_PAD_SINK,
  GST_PAD_ALWAYS,
  GST_STATIC_CAPS ("video/x-raw-yuv")
);

static GstStaticPadTemplate src_factory =
GST_STATIC_PAD_TEMPLATE (
  "src",
  GST_PAD_SRC,
  GST_PAD_ALWAYS,
  GST_STATIC_CAPS ("video/mpeg")
);

/*=============================================================================
                                        LOCAL MACROS
=============================================================================*/

#define	GST_CAT_DEFAULT    mfw_gst_mpeg4enc_debug
#define DEFAULT -1

/*=============================================================================
                                STAITIC LOCAL VARIABLES
=============================================================================*/
static GstElementClass *parent_class_mpeg4enc = NULL;

/*=============================================================================
                               STATIC FUNCTION PROTOTYPES
=============================================================================*/
GST_DEBUG_CATEGORY_STATIC(mfw_gst_mpeg4enc_debug);

static void	mfw_gst_mpeg4enc_class_init	 (MFW_GST_MPEG4ENC_CLASS_T *klass);
static void	mfw_gst_mpeg4enc_base_init	 (MFW_GST_MPEG4ENC_CLASS_T *klass);
static void	mfw_gst_mpeg4enc_init	     (MFW_GST_MPEG4ENC_INFO_T *mpeg4enc_info,
                                          MFW_GST_MPEG4ENC_CLASS_T *gclass);
static void	mfw_gst_mpeg4enc_set_property (GObject *object, guint prop_id,
                                           const GValue *value,
					                       GParamSpec *pspec);
static void	mfw_gst_mpeg4enc_get_property (GObject *object, guint prop_id,
                                           GValue *value,
						                   GParamSpec *pspec);
static gboolean mfw_gst_mpeg4enc_set_caps (GstPad *pad, GstCaps *caps);
static GstFlowReturn mfw_gst_mpeg4enc_chain (GstPad *pad, GstBuffer *buf);
static gint mfw_gst_mpeg4enc_encode_data(MFW_GST_MPEG4ENC_INFO_T *,GstBuffer *);
static void mfw_mpeg4enc_FreeRes(MFW_GST_MPEG4ENC_INFO_T *mpeg4enc_info);
static gint mfw_mpeg4enc_AllocRes(MFW_GST_MPEG4ENC_INFO_T *mpeg4enc_info);
static gint mfw_mpeg4enc_OpenEncoder(MP4EncOptions * encOpt, MP4EncInst * encoder);
static void mfw_mpeg4enc_CloseEncoder(MP4EncInst encoder);
static MP4EncStrmType mfw_mpeg4enc_SelectStreamType(MP4EncOptions * encOpt);
static gint mfw_mpeg4enc_UserData(MP4EncInst inst, gchar *name, MP4EncUsrDataType);
static void mfw_gst_mpeg4enc_initialise_params(MFW_GST_MPEG4ENC_INFO_T *mpeg4enc_info);

/*=============================================================================
                            GLOBAL VARIABLES
=============================================================================*/
/* None */

/*=============================================================================
                            LOCAL FUNCTIONS
=============================================================================*/

/*=============================================================================
FUNCTION:   mfw_mpeg4enc_FreeRes

DESCRIPTION: 
             frees all the resources allocated for the encoder

ARGUMENTS PASSED:
        mpeg4enc_info     -   pointer to plug-in's context
      

RETURN VALUE:
        None

PRE-CONDITIONS:
        None

POST-CONDITIONS:
        None

IMPORTANT NOTES:
        None
=============================================================================*/

static void mfw_mpeg4enc_FreeRes(MFW_GST_MPEG4ENC_INFO_T *mpeg4enc_info)
{
		if(mpeg4enc_info->direct_mem == FALSE)
		{	
	    if(mpeg4enc_info->enc_iores.picture != MAP_FAILED)
	        munmap(mpeg4enc_info->enc_iores.picture, mpeg4enc_info->enc_iores.pict_size);
	    if(mpeg4enc_info->enc_iores.pict_bus_address != 0)
	       ioctl(mpeg4enc_info->enc_iores.memdev_fd, MEMALLOC_IOCSFREEBUFFER, &mpeg4enc_info->enc_iores.pict_bus_address);   
	  }
	  
    if(mpeg4enc_info->enc_iores.outbuf != MAP_FAILED)
        munmap(mpeg4enc_info->enc_iores.outbuf, mpeg4enc_info->enc_iores.outbuf_size);

    
    if(mpeg4enc_info->enc_iores.outbuf_bus_address != 0)
        ioctl(mpeg4enc_info->enc_iores.memdev_fd, MEMALLOC_IOCSFREEBUFFER, &mpeg4enc_info->enc_iores.outbuf_bus_address);

    if(mpeg4enc_info->enc_iores.memdev_fd != -1)
        close(mpeg4enc_info->enc_iores.memdev_fd);
}

/*=============================================================================
FUNCTION:   mfw_mpeg4enc_AllocRes

DESCRIPTION: 
            OS dependent implementation for allocating the physical memories 
            used by both SW and HW: input picture and output buffer.

            To access the memory HW uses the physical linear address (bus address) 
            and SW uses virtual address (user address).

            In Linux the physical memories can only be allocated with sizes
            of power of two times the page size.


ARGUMENTS PASSED:
        mpeg4enc_info     -   pointer to plug-in's context
      

RETURN VALUE:
        0  - Allocation is successfull
        1  - Error while allocating memory

PRE-CONDITIONS:
        None

POST-CONDITIONS:
        None

IMPORTANT NOTES:
        None
=============================================================================*/

static gint mfw_mpeg4enc_AllocRes(MFW_GST_MPEG4ENC_INFO_T *mpeg4enc_info)
{
    /* Kernel driver for linear memory allocation of SW/HW shared memories */
    const gchar *memdev = "/dev/memalloc";
    mpeg4enc_info->enc_iores.memdev_fd = open(memdev, O_RDWR);
    if(mpeg4enc_info->enc_iores.memdev_fd == -1)
    {
        GST_ERROR("Failed to open dev: %s\n", memdev);
        mfw_mpeg4enc_FreeRes(mpeg4enc_info);
        return (-1);
    }
   
    mpeg4enc_info->enc_iores.pict_size = mpeg4enc_info->enc_iores.pict_bus_address = 
	     256 * sysconf(_SC_PAGESIZE);
	  GST_DEBUG("Input picture buffer size:\t\t%d\n", mpeg4enc_info->enc_iores.pict_size);
		 
    if(mpeg4enc_info->direct_mem == FALSE)
    {	
	 		ioctl(mpeg4enc_info->enc_iores.memdev_fd, MEMALLOC_IOCXGETBUFFER, 
	        &mpeg4enc_info->enc_iores.pict_bus_address);
	    GST_DEBUG("Input picture bus address:\t\t0x%08x\n",mpeg4enc_info->enc_iores.pict_bus_address);
	    mpeg4enc_info->enc_iores.picture =
	        (guint *) mmap(0, mpeg4enc_info->enc_iores.pict_size, PROT_READ | PROT_WRITE, MAP_SHARED,
	                     mpeg4enc_info->enc_iores.memdev_fd,mpeg4enc_info->enc_iores.pict_bus_address);
	    GST_DEBUG("Input picture user address:\t\t0x%08x\n",(guint) mpeg4enc_info->enc_iores.picture);
	
	
	    if(mpeg4enc_info->enc_iores.picture == MAP_FAILED)
	    {
	        GST_ERROR("Failed to alloc input image\n");
	        mfw_mpeg4enc_FreeRes(mpeg4enc_info);
	        return (-1);
	    }
  	}
	
    mpeg4enc_info->enc_iores.outbuf_size = mpeg4enc_info->enc_iores.outbuf_bus_address = 
        64 * sysconf(_SC_PAGESIZE);
    GST_DEBUG("Output buffer size:\t\t\t%d\n", mpeg4enc_info->enc_iores.outbuf_size);
    ioctl(mpeg4enc_info->enc_iores.memdev_fd, MEMALLOC_IOCXGETBUFFER, 
        &mpeg4enc_info->enc_iores.outbuf_bus_address);
    GST_DEBUG("Output buffer bus address:\t\t0x%08x\n", mpeg4enc_info->enc_iores.outbuf_bus_address);
    mpeg4enc_info->enc_iores.outbuf =
        (guint *) mmap(0, mpeg4enc_info->enc_iores.outbuf_size, PROT_READ | PROT_WRITE, MAP_SHARED,
                     mpeg4enc_info->enc_iores.memdev_fd, 
                     mpeg4enc_info->enc_iores.outbuf_bus_address);
    GST_DEBUG("Output buffer user address:\t\t0x%08x\n", (guint)mpeg4enc_info->enc_iores.outbuf);
    if(mpeg4enc_info->enc_iores.outbuf == MAP_FAILED)
    {
        GST_ERROR("Failed to alloc input image\n");
        mfw_mpeg4enc_FreeRes(mpeg4enc_info);
        return (-1);
    }

    return 0;
}

/*=============================================================================
FUNCTION:   mfw_mpeg4enc_CloseEncoder

DESCRIPTION: 
            releases the encoder hardware

ARGUMENTS PASSED:
        encoder       -   encoder instance
      

RETURN VALUE:
        None

PRE-CONDITIONS:
        None

POST-CONDITIONS:
        None

IMPORTANT NOTES:
        None
=============================================================================*/

static void mfw_mpeg4enc_CloseEncoder(MP4EncInst encoder)
{
    MP4EncRet ret;

    if((ret = MP4EncRelease(encoder)) != ENC_OK)
    {
        GST_ERROR("Failed to release the encoder. Error code: %8i\n", ret);
    }
}

/*=============================================================================
FUNCTION:   mfw_mpeg4enc_UserData

DESCRIPTION: 
            write the user data into the stream

ARGUMENTS PASSED:
        inst       -   encoder instance
        name       -   file name where the user data is present
        type       -   type of user data

RETURN VALUE:
        None

PRE-CONDITIONS:
        None

POST-CONDITIONS:
        None

IMPORTANT NOTES:
        None
=============================================================================*/

static gint mfw_mpeg4enc_UserData(MP4EncInst inst, gchar *name, MP4EncUsrDataType type)
{
    FILE *file = NULL;
    gint byteCnt;
    guint8 *data;

    if(name[0] == '\n')
    {
        return 0;
    }

    /* Get user data length from file */
    file = fopen(name, "rb");
    if(file == NULL)
    {
        GST_DEBUG("Unable to open User Data file: %s: Continuing encoding without the user data\n",
            name);
        return 0;
    }
    fseek(file, 0L, SEEK_END);
    byteCnt = ftell(file);
    rewind(file);

    /* Allocate memory for user data */
    if((data = (guint8 *) g_malloc(sizeof(guint8) * byteCnt)) == NULL)
    {
        GST_ERROR("Unable to alloc User Data memory\n");
        return -1;
    }

    /* Read user data from file */
    fread(data, sizeof(guint8), byteCnt, file);
    fclose(file);
    file = NULL;

    /* Set stream user data */
    MP4EncSetUsrData(inst, data, byteCnt, type);

    g_free(data);
    data = NULL;

    return 0;
}

/*=============================================================================
FUNCTION:   mfw_mpeg4enc_SelectStreamType

DESCRIPTION: 
            selects the stream type of encoding

ARGUMENTS PASSED:
        encOpt       -  structure to hold encoder parameters

RETURN VALUE:
        None

PRE-CONDITIONS:
        None

POST-CONDITIONS:
        None

IMPORTANT NOTES:
        None
=============================================================================*/
static MP4EncStrmType mfw_mpeg4enc_SelectStreamType(MP4EncOptions * encOpt)
{
    /* Short Video Header stream */
    if(encOpt->scheme == 1)
        return MPEG4_SVH_STRM;

    /* H.263 stream */
    if(encOpt->scheme == 3)
    {
        encOpt->profile = H263_PROFILE_0_LEVEL_70;
        return H263_STRM;
    }

    if(encOpt->rvlc != DEFAULT && encOpt->rvlc != 0)
        return MPEG4_VP_DP_RVLC_STRM;

    if(encOpt->dataPart != DEFAULT && encOpt->dataPart != 0)
        return MPEG4_VP_DP_STRM;

    if(encOpt->vpSize != DEFAULT && encOpt->vpSize != 0)
        return MPEG4_VP_STRM;

    return MPEG4_PLAIN_STRM;
}

/*=============================================================================
FUNCTION:   mfw_gst_mpeg4enc_encode_data

DESCRIPTION: this function allocates resources for the encoder, initialises the encoder 
             and encodes the given frame of data

ARGUMENTS PASSED:
        mpeg4enc_info     -   pointer to plug-in's context
        inbuffer          -   pointer to the input gst buffer        

RETURN VALUE:
            0             -   encoding is successfull
            -1            -   error in encoding

PRE-CONDITIONS:
        None

POST-CONDITIONS:
        None

IMPORTANT NOTES:
        None
=============================================================================*/
static gint mfw_gst_mpeg4enc_encode_data(MFW_GST_MPEG4ENC_INFO_T *mpeg4enc_info,
                                        GstBuffer *inbuffer)
{


    MP4EncApiVersion        ver;
    GstBuffer               *outbuffer;
    guint8                  *outdata;
    GstCaps                 *src_caps;
    GstFlowReturn           result;
    MP4EncRet               ret;
    MP4EncFrmPos            frmPos;
    gint                    headerSize = 0;
    
    mpeg4enc_info->encConfig.encIn.pVpSizes = NULL;


     
    if(mpeg4enc_info->encConfig.vopCnt != 0)
        mpeg4enc_info->encConfig.encIn.timeIncr = 
        mpeg4enc_info->encConfig.encOpt.outputRateDenom;
    
    /* Select VOP type */
    if(mpeg4enc_info->encConfig.intraVopCnt == mpeg4enc_info->encConfig.encOpt.intraVopRate)
    {
        mpeg4enc_info->encConfig.encIn.vopType = INTRA_VOP;
        mpeg4enc_info->encConfig.intraVopCnt = 0;
    }
    else
    {
        mpeg4enc_info->encConfig.encIn.vopType = PREDICTED_VOP;
    }
    
    
    if(mpeg4enc_info->direct_mem == FALSE)
    	memcpy((guint8 *) mpeg4enc_info->enc_iores.picture,GST_BUFFER_DATA(inbuffer),
        mpeg4enc_info->encConfig.src_img_size);
		else
		{
	    mpeg4enc_info->enc_iores.pict_bus_address = GST_BUFFER_OFFSET(inbuffer);
	    mpeg4enc_info->enc_iores.picture = GST_BUFFER_DATA(inbuffer);
	
	
	    mpeg4enc_info->encConfig.encIn.busLuma = mpeg4enc_info->enc_iores.pict_bus_address;
	    
	    mpeg4enc_info->encConfig.encIn.busChromaU = mpeg4enc_info->encConfig.encIn.busLuma 
	        + (mpeg4enc_info->encConfig.encOpt.lumWidthSrc * 
	        mpeg4enc_info->encConfig.encOpt.lumHeightSrc);
	    
	    mpeg4enc_info->encConfig.encIn.busChromaV = 
	        mpeg4enc_info->encConfig.encIn.busChromaU +
	        (((mpeg4enc_info->encConfig.encOpt.lumWidthSrc + 1) >> 1) 
	        * ((mpeg4enc_info->encConfig.encOpt.lumHeightSrc + 1) >> 1));
		}


    /* Set Group of Vop header */
    if (mpeg4enc_info->encConfig.vopCnt != 0 && mpeg4enc_info->encConfig.encOpt.goVopRate != 0 
        && (mpeg4enc_info->encConfig.vopCnt % mpeg4enc_info->encConfig.encOpt.goVopRate) == 0)
    {
        MP4EncCodingCtrl codingCfg;
        
        if(MP4EncGetCodingCtrl(mpeg4enc_info->encConfig.encoder, &codingCfg) != ENC_OK)
            GST_ERROR("Could not get coding control.\n");
        
        codingCfg.insGOV = 1;
        
        if(MP4EncSetCodingCtrl(mpeg4enc_info->encConfig.encoder, &codingCfg) != ENC_OK)
            GST_ERROR("Could not set GOV header.\n");
        
    }
    GST_DEBUG("Vop %3d started...\n", mpeg4enc_info->encConfig.vopCnt);
    ret = MP4EncStrmEncode(mpeg4enc_info->encConfig.encoder, &mpeg4enc_info->encConfig.encIn, 
          &mpeg4enc_info->encConfig.encOut);
   
    switch (ret)
    {
        case ENC_VOP_READY_VBV_FAIL:
            GST_ERROR("  VBV overflow!");
            return -1;
            break;


        case ENC_VOP_READY:
            GST_DEBUG("  Ready! %6u bytes, %s (%u.%u.%02u:%03u)\n", 
                mpeg4enc_info->encConfig.encOut.strmSize, 
                mpeg4enc_info->encConfig.encOut.vopType == INTRA_VOP ? "intra" : 
            mpeg4enc_info->encConfig.encOut.vopType == PREDICTED_VOP ? "inter" : "not coded",
                mpeg4enc_info->encConfig.encOut.timeCode.hours,
                mpeg4enc_info->encConfig.encOut.timeCode.minutes,
                mpeg4enc_info->encConfig.encOut.timeCode.seconds,
                mpeg4enc_info->encConfig.encOut.timeCode.timeRes == 0 ? 0 : 
            (mpeg4enc_info->encConfig.encOut.timeCode.timeIncr * 1000) /
                mpeg4enc_info->encConfig.encOut.timeCode.timeRes);
        
            if (mpeg4enc_info->encConfig.encOpt.stabFrame > 0)
            {
                MP4EncGetLastFrmPos(mpeg4enc_info->encConfig.encoder, &frmPos);
                GST_DEBUG(" Frame pos (%d, %d)\n", frmPos.xOffset, frmPos.yOffset);
            }
            break;
   
        case ENC_GOV_READY:
            GST_DEBUG("  GOV %6u bytes", mpeg4enc_info->encConfig.encOut.strmSize);
            break;
        
        case ENC_OUTPUT_BUFFER_OVERFLOW:
            GST_ERROR("  Output buffer overflow, VOP lost!\n");
            return -1;
            break;
        
        default:
            GST_ERROR("  FAILED. Error code: %i\n", ret);
            return -1;
            break;
    }
    
    mpeg4enc_info->encConfig.vopCnt++;
    mpeg4enc_info->encConfig.intraVopCnt++;

    return 0;
}

/*=============================================================================
FUNCTION:   mfw_mpeg4enc_OpenEncoder

DESCRIPTION: 
            Initialise the encoder based on the options set

ARGUMENTS PASSED:
        encOpt     -   parameters to be set for the encoder
        pEnc       -   encoder instance
      

RETURN VALUE:
        0  - initialisation is successfull
        1  - Error while initialising the encoder

PRE-CONDITIONS:
        None

POST-CONDITIONS:
        None

IMPORTANT NOTES:
        None
=============================================================================*/

static gint mfw_mpeg4enc_OpenEncoder(MP4EncOptions * encOpt, MP4EncInst * pEnc)
{
    MP4EncRet ret;
    MP4EncCfg cfg;
    MP4EncCodingCtrl codingCfg;
    MP4EncRateCtrl rcCfg;
    MP4EncCropCfg ppCfg;
    MP4EncInst encoder;
    encOpt->width = encOpt->lumWidthSrc - encOpt->horOffsetSrc 
        - (2 * encOpt->stabFrame);
    encOpt->height = encOpt->lumHeightSrc - encOpt->verOffsetSrc
        - (2 * encOpt->stabFrame);
    cfg.frmRateDenom = encOpt->outputRateDenom;
    cfg.frmRateNum = encOpt->outputRateNumer;
    cfg.width = encOpt->width;
    cfg.height = encOpt->height;
    cfg.strmType = mfw_mpeg4enc_SelectStreamType(encOpt);
    cfg.profileAndLevel = encOpt->profile;

		if((ret = MP4EncInit(&cfg, pEnc)) != ENC_OK)
    {
        GST_ERROR("Failed to initialize the encoder. Error code: %8i\n", ret);
        return -1;
    }

    encoder = *pEnc;

    /* Encoder setup: rate control */
    if((ret = MP4EncGetRateCtrl(encoder, &rcCfg)) != ENC_OK)
    {
        GST_ERROR("Failed to get RC info. Error code: %8i\n", ret);
        mfw_mpeg4enc_CloseEncoder(encoder);
        return -1;
    }
    else
    {
        if (encOpt->qpHdr != DEFAULT)
            rcCfg.qpHdr = encOpt->qpHdr;
        if (encOpt->qpHdrMin != DEFAULT)
            rcCfg.qpHdrMin = encOpt->qpHdrMin;
        if (encOpt->qpHdrMax != DEFAULT)
            rcCfg.qpHdrMax = encOpt->qpHdrMax;
        if (encOpt->vopSkip != DEFAULT)
            rcCfg.vopSkip = encOpt->vopSkip;
        if (encOpt->vopRc != DEFAULT)
            rcCfg.vopRc = encOpt->vopRc;
        if (encOpt->mbRc != DEFAULT)
            rcCfg.mbRc = encOpt->mbRc;
        if (encOpt->bitPerSecond != DEFAULT)
            rcCfg.bitPerSecond = encOpt->bitPerSecond;
        if (encOpt->cir != DEFAULT)
            rcCfg.cir = encOpt->cir;
        if (encOpt->vbv != DEFAULT)
            rcCfg.vbv = encOpt->vbv;

        if((ret = MP4EncSetRateCtrl(encoder, &rcCfg)) != ENC_OK)
        {
            GST_ERROR("Failed to set RC info. Error code: %8i\n",
                    ret);
            mfw_mpeg4enc_CloseEncoder(encoder);
            return -1;
        }
    }

    /* Encoder setup: coding control */
    if((ret = MP4EncGetCodingCtrl(encoder, &codingCfg)) != ENC_OK)
    {
        GST_ERROR("Failed to get CODING info. Error code: %8i\n",
                ret);
        mfw_mpeg4enc_CloseEncoder(encoder);
        return -1;
    }
    else
    {
        if(encOpt->vpSize != DEFAULT && encOpt->vpSize != 0)
            codingCfg.vpSize = encOpt->vpSize;

        if(encOpt->hec != DEFAULT)
        {
            if(encOpt->hec != 0)
                codingCfg.insHEC = 1;
            else
                codingCfg.insHEC = 0;
        }

        if(encOpt->gobPlace != DEFAULT)
            codingCfg.insGOB = encOpt->gobPlace;

        if((ret = MP4EncSetCodingCtrl(encoder, &codingCfg)) != ENC_OK)
        {
            GST_ERROR("Failed to set CODING info. Error code: %8i\n", ret);
            mfw_mpeg4enc_CloseEncoder(encoder);
            return -1;
        }
    }

    /* Encoder setup: pre processing */
    if(encOpt->preProcess != DEFAULT && encOpt->preProcess != 0)
    {
        if((ret = MP4EncSetSmooth(encoder, 1)) != ENC_OK)
        {
            GST_ERROR("Failed to set SMOOTHING. Error code: %8i\n", ret);
            mfw_mpeg4enc_CloseEncoder(encoder);
            return -1;
        }
        else
            GST_DEBUG("\nsmoothing enabled!\n");
    }
    else
        GST_DEBUG("\nSmoothing disabled!\n");

    /* User data */
    if (mfw_mpeg4enc_UserData(encoder, encOpt->userDataVos, MPEG4_VOS_USER_DATA) != 0)
    {
        mfw_mpeg4enc_CloseEncoder(encoder);
        return -1;
    }
    if (mfw_mpeg4enc_UserData(encoder, encOpt->userDataVisObj, MPEG4_VO_USER_DATA) != 0)
    {
        mfw_mpeg4enc_CloseEncoder(encoder);
        return -1;
    }
    if (mfw_mpeg4enc_UserData(encoder, encOpt->userDataVol, MPEG4_VOL_USER_DATA) != 0)
    {
        mfw_mpeg4enc_CloseEncoder(encoder);
        return -1;
    }
    if (mfw_mpeg4enc_UserData(encoder, encOpt->userDataGov, MPEG4_GOV_USER_DATA) != 0)
    {
        mfw_mpeg4enc_CloseEncoder(encoder);
        return -1;
    }

   

     /* Encoder setup: camera stabilization */
    if((ret = MP4EncGetCrop(encoder, &ppCfg)) != ENC_OK)
    {
        GST_ERROR("Failed to get CROP info. Error code: %8i\n", ret);
        mfw_mpeg4enc_CloseEncoder(encoder);
        return -1;
    }
    
    ppCfg.origWidth = encOpt->lumWidthSrc;
    ppCfg.origHeight = encOpt->lumHeightSrc;

    if(encOpt->horOffsetSrc != DEFAULT)
        ppCfg.xOffset = encOpt->horOffsetSrc;
    if(encOpt->verOffsetSrc != DEFAULT)
        ppCfg.yOffset = encOpt->verOffsetSrc;
    if(encOpt->stabFrame != DEFAULT)
    {
        ppCfg.xOffset += encOpt->stabFrame;
        ppCfg.yOffset += encOpt->stabFrame;
        ppCfg.stabArea = encOpt->stabFrame;
    }


    if((ret = MP4EncSetCrop(encoder, &ppCfg)) != ENC_OK)
    {
        GST_ERROR("Failed to set CROP info. Error code: %8i\n", ret);
        mfw_mpeg4enc_CloseEncoder(encoder);
        return -1;
    }

    return 0;
}

/*=============================================================================
FUNCTION: mfw_gst_mpeg4enc_set_property

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
mfw_gst_mpeg4enc_set_property (GObject *object, guint prop_id,
                                  const GValue *value, GParamSpec *pspec)
{
    MFW_GST_MPEG4ENC_INFO_T *mpeg4enc_info = MFW_GST_MPEG4ENC (object);
    switch (prop_id)
    {
        
        case MP4ENC_0:
            break;
        case MP4ENC_BITRATE:
            mpeg4enc_info->encConfig.encOpt.bitPerSecond = 
                g_value_get_int(value);
            GST_DEBUG("bitrate = %d\n",
                mpeg4enc_info->encConfig.encOpt.bitPerSecond);
            break;
        case MP4ENC_WIDTH:
            mpeg4enc_info->encConfig.encOpt.width = g_value_get_int(value);
            GST_DEBUG("width = %d\n",mpeg4enc_info->encConfig.encOpt.width);
            break;
        case MP4ENC_HEIGHT:
            mpeg4enc_info->encConfig.encOpt.height = g_value_get_int(value);
            GST_DEBUG("height = %d\n",mpeg4enc_info->encConfig.encOpt.height);
            break;
        case MP4ENC_FRAMERATE_NUM:
            mpeg4enc_info->encConfig.encOpt.fps_num = g_value_get_int (value);
            GST_DEBUG("framerate numerator = %d\n",
                mpeg4enc_info->encConfig.encOpt.fps_num);
            break;

        case MP4ENC_FRAMERATE_DEN:
            mpeg4enc_info->encConfig.encOpt.fps_den = g_value_get_int (value);
            GST_DEBUG("framerate denominator = %d\n",
                mpeg4enc_info->encConfig.encOpt.fps_den);
            break;

        case MP4ENC_SCHEME:
            mpeg4enc_info->encConfig.encOpt.scheme = g_value_get_int(value);
            GST_DEBUG("scheme = %d\n",mpeg4enc_info->encConfig.encOpt.scheme);
            break;
        case MP4ENC_MB_RC:
            mpeg4enc_info->encConfig.encOpt.mbRc = g_value_get_int(value);
            GST_DEBUG("mbRc = %d\n",mpeg4enc_info->encConfig.encOpt.mbRc);
            break;
        case MP4ENC_VOP_RC:
            mpeg4enc_info->encConfig.encOpt.vopRc = g_value_get_int(value);
            GST_DEBUG("vopRc = %d\n",mpeg4enc_info->encConfig.encOpt.vopRc);
            break;
        case MP4ENC_VOPSKIP_RC:
            mpeg4enc_info->encConfig.encOpt.vopSkip = g_value_get_int(value);
            GST_DEBUG("vopSkip = %d\n",mpeg4enc_info->encConfig.encOpt.vopSkip);
            break;
        case MP4ENC_PROFILE_LEVEL:
            mpeg4enc_info->encConfig.encOpt.profile = g_value_get_enum(value);
            GST_DEBUG("profile = %d\n",mpeg4enc_info->encConfig.encOpt.profile);
            break;

        case MP4ENC_GOP_SIZE:
            mpeg4enc_info->encConfig.encOpt.goVopRate = g_value_get_int(value);
            GST_DEBUG("gopsize = %d\n",mpeg4enc_info->encConfig.encOpt.goVopRate);
            break;

        case MP4ENC_CROP_HOR:
            mpeg4enc_info->encConfig.encOpt.horOffsetSrc = g_value_get_int(value);
            GST_DEBUG("horOffsetSrc = %d\n",mpeg4enc_info->encConfig.encOpt.horOffsetSrc);
            break;

        case MP4ENC_CROP_VER:
            mpeg4enc_info->encConfig.encOpt.verOffsetSrc = g_value_get_int(value);
            GST_DEBUG("verOffsetSrc = %d\n",mpeg4enc_info->encConfig.encOpt.verOffsetSrc);
            break;
        
        case MP4ENC_STAB_AREA:
            mpeg4enc_info->encConfig.encOpt.stabFrame = g_value_get_int(value);
            GST_DEBUG("stabFrame = %d\n",mpeg4enc_info->encConfig.encOpt.stabFrame);
            break;


        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
     }

                
}

/*=============================================================================
FUNCTION: mfw_gst_mpeg4enc_get_property

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
mfw_gst_mpeg4enc_get_property (GObject *object, guint prop_id,
                                  GValue *value, GParamSpec *pspec)
{
  MFW_GST_MPEG4ENC_INFO_T *mpeg4enc_info = MFW_GST_MPEG4ENC(object);
  switch (prop_id)
  {
      case MP4ENC_0:
          break;
      case MP4ENC_BITRATE:
          g_value_set_int(value,mpeg4enc_info->encConfig.encOpt.bitPerSecond);
          break;
      case MP4ENC_WIDTH:
          g_value_set_int(value,mpeg4enc_info->encConfig.encOpt.width);
          break;
      case MP4ENC_HEIGHT:
          g_value_set_int(value,mpeg4enc_info->encConfig.encOpt.height);
          break;
      case MP4ENC_FRAMERATE_NUM:
          g_value_set_int(value,mpeg4enc_info->encConfig.encOpt.fps_num);
          break;
      case MP4ENC_FRAMERATE_DEN:
          g_value_set_int(value,mpeg4enc_info->encConfig.encOpt.fps_den);
          break;

      case MP4ENC_SCHEME:
          g_value_set_int(value,mpeg4enc_info->encConfig.encOpt.scheme);
          break;
      case MP4ENC_MB_RC:
          g_value_set_int(value,mpeg4enc_info->encConfig.encOpt.mbRc);
          break;
      case MP4ENC_VOP_RC:
          g_value_set_int(value,mpeg4enc_info->encConfig.encOpt.vopRc);
          break;
      case MP4ENC_VOPSKIP_RC:
          g_value_set_int(value,mpeg4enc_info->encConfig.encOpt.vopSkip);
          break;
          
      case MP4ENC_PROFILE_LEVEL:
          g_value_set_enum(value,mpeg4enc_info->encConfig.encOpt.profile);
          break;

      case MP4ENC_GOP_SIZE:
          g_value_set_int(value,mpeg4enc_info->encConfig.encOpt.goVopRate);
          break;

      case MP4ENC_CROP_HOR:
          g_value_set_int(value,mpeg4enc_info->encConfig.encOpt.horOffsetSrc);
          break;
          
      case MP4ENC_CROP_VER:
          g_value_set_int(value,mpeg4enc_info->encConfig.encOpt.verOffsetSrc);
          break;
          
      case MP4ENC_STAB_AREA:
          g_value_set_int(value,mpeg4enc_info->encConfig.encOpt.stabFrame);
          break;

              
      default:
          G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
          break;
  }
 
}



/*=============================================================================
FUNCTION:               mfw_gst_mpeg4enc_post_fatal_error_msg

DESCRIPTION:            This function is used to post a fatal error message and 
                         terminate the pipeline during an unrecoverable error.
                        

ARGUMENTS PASSED:
                        mpeg4enc_info  - MPEG4 encoder plugins context
                        error_msg message -  to be posted 
        

RETURN VALUE:
        None

PRE-CONDITIONS:
        None

POST-CONDITIONS:


IMPORTANT NOTES:
        None
=============================================================================*/

static void  mfw_gst_mpeg4enc_post_fatal_error_msg
(MFW_GST_MPEG4ENC_INFO_T *mpeg4enc_info,gchar *error_msg)
{
    GError *error = NULL;
    GQuark domain;
    domain = g_quark_from_string("mfw_mpeg4encoder");
    error = g_error_new(domain, 10, "fatal error");
    gst_element_post_message(GST_ELEMENT(mpeg4enc_info),
        gst_message_new_error(GST_OBJECT
        (mpeg4enc_info),error,error_msg));
    g_error_free(error);
}


/*=============================================================================
FUNCTION: mfw_gst_mpeg4_decoder_chain

DESCRIPTION: this function recieves the data from the previuos element and 
             calls the encoder routine

ARGUMENTS PASSED:
        pad     - pointer to pad
        buffer  - pointer to received buffer

RETURN VALUE:
        GST_FLOW_OK		- Frame encoded successfully
		GST_FLOW_ERROR	- Failure

PRE-CONDITIONS:
        None

POST-CONDITIONS:


IMPORTANT NOTES:
        None
=============================================================================*/


static GstFlowReturn
mfw_gst_mpeg4enc_chain (GstPad *pad, GstBuffer *buf)
{
  MFW_GST_MPEG4ENC_INFO_T *mpeg4enc_info;
  GstBuffer               *outbuffer;
  guint8                  *outdata;
  GstCaps                 *src_caps;
  GstFlowReturn            result;
  MP4EncRet                ret;
  MP4EncApiVersion        ver;
  gint                    headerSize = 0;
  GstCaps                 *caps=NULL;  

  mpeg4enc_info = MFW_GST_MPEG4ENC(GST_OBJECT_PARENT (pad));

  if(mpeg4enc_info->headerwrite==FALSE)
  {
      /* initialise encoder parameters */
      mfw_gst_mpeg4enc_initialise_params(mpeg4enc_info);
      ver = MP4EncGetVersion();
      GST_DEBUG("API version %d.%d\n",  ver.major, ver.minor);
 			
 			if(GST_BUFFER_FLAG_IS_SET(buf,GST_BUFFER_FLAG_LAST))
 				mpeg4enc_info->direct_mem = TRUE;
 			
      /* Allocate input and output buffers */
      if(mfw_mpeg4enc_AllocRes(mpeg4enc_info) != 0)
      {
          GST_ERROR("Failed to allocate the external resources!");
          mfw_gst_mpeg4enc_post_fatal_error_msg(
              mpeg4enc_info, "Failed to allocate the external resources");
          return GST_FLOW_ERROR;
      }
      /* Encoder initialization */
      if(mfw_mpeg4enc_OpenEncoder(&mpeg4enc_info->encConfig.encOpt, 
          &mpeg4enc_info->encConfig.encoder) != 0)
      {
          GST_ERROR("Failed to initialise the encoder ");
          mfw_mpeg4enc_FreeRes(mpeg4enc_info);
          mfw_gst_mpeg4enc_post_fatal_error_msg(
              mpeg4enc_info, "Failed to initialise the encoder");
          return GST_FLOW_ERROR;
      }
      
      mpeg4enc_info->encConfig.encIn.pOutBuf = mpeg4enc_info->enc_iores.outbuf;
      mpeg4enc_info->encConfig.encIn.outBufSize = mpeg4enc_info->enc_iores.outbuf_size;
      
      
      /* Start stream */
      ret = MP4EncStrmStart(mpeg4enc_info->encConfig.encoder, 
          &mpeg4enc_info->encConfig.encIn,
          &mpeg4enc_info->encConfig.encOut);
      
      
      if(ret != ENC_OK)
      {
          GST_ERROR("Failed to start the stream. Error code: %8i\n", ret);
          mfw_gst_mpeg4enc_post_fatal_error_msg(
              mpeg4enc_info, "Failed to start the stream");
          return GST_FLOW_ERROR;
      }
      
      
      /* Setup encoder input */
      mpeg4enc_info->encConfig.encIn.outBufBusAddress = 
          mpeg4enc_info->enc_iores.outbuf_bus_address;
      
      if(mpeg4enc_info->direct_mem == FALSE)
      {    
	      mpeg4enc_info->encConfig.encIn.busLuma = mpeg4enc_info->enc_iores.pict_bus_address;
	        
	      mpeg4enc_info->encConfig.encIn.busChromaU = mpeg4enc_info->encConfig.encIn.busLuma 
	          + (mpeg4enc_info->encConfig.encOpt.lumWidthSrc * 
	          mpeg4enc_info->encConfig.encOpt.lumHeightSrc);
	        
	      mpeg4enc_info->encConfig.encIn.busChromaV = 
	          mpeg4enc_info->encConfig.encIn.busChromaU +
	          (((mpeg4enc_info->encConfig.encOpt.lumWidthSrc + 1) >> 1) 
	          * ((mpeg4enc_info->encConfig.encOpt.lumHeightSrc + 1) >> 1));     
      }
      
      mpeg4enc_info->encConfig.encIn.vopType = INTRA_VOP;
      mpeg4enc_info->encConfig.encIn.timeIncr = 0;
      
      /* Allocate memory for video packet size buffer, optional */
      mpeg4enc_info->encConfig.encIn.pVpSizes = (gint *)g_malloc(1024*sizeof(gint));
      if (!mpeg4enc_info->encConfig.encIn.pVpSizes)
      {
          GST_ERROR("Failed to allocate VP buffer.\n");
          mfw_mpeg4enc_CloseEncoder(mpeg4enc_info->encConfig.encoder);
          mfw_mpeg4enc_FreeRes(mpeg4enc_info);
          mfw_gst_mpeg4enc_post_fatal_error_msg(
              mpeg4enc_info, "Failed to allocate VP buffer");
          return GST_FLOW_ERROR;
      }
      
      /* Source Image Size */
      mpeg4enc_info->encConfig.src_img_size = 
          mpeg4enc_info->encConfig.encOpt.lumWidthSrc * 
          mpeg4enc_info->encConfig.encOpt.lumHeightSrc +
          2 * (((mpeg4enc_info->encConfig.encOpt.lumWidthSrc + 1) >> 1) 
          * ((mpeg4enc_info->encConfig.encOpt.lumHeightSrc + 1) >> 1));
      
      if (mpeg4enc_info->encConfig.src_img_size > mpeg4enc_info->enc_iores.pict_size)
      {
          GST_ERROR("Input picture doesn't fit into input buffe, src_img_size %d pict size %d.",mpeg4enc_info->encConfig.src_img_size,mpeg4enc_info->enc_iores.pict_size);
          mfw_mpeg4enc_CloseEncoder(mpeg4enc_info->encConfig.encoder);
          mfw_mpeg4enc_FreeRes(mpeg4enc_info);
          g_free(mpeg4enc_info->encConfig.encIn.pVpSizes);
          mpeg4enc_info->encConfig.encIn.pVpSizes=NULL;
           mfw_gst_mpeg4enc_post_fatal_error_msg(
              mpeg4enc_info, "nput picture doesn't fit into input buffer");
          return GST_FLOW_ERROR;
      }


       caps = gst_caps_new_simple("video/mpeg",
            "width", G_TYPE_INT,mpeg4enc_info->encConfig.encOpt.width, 
            "height", G_TYPE_INT,mpeg4enc_info->encConfig.encOpt.height, 
            "mpegversion", G_TYPE_INT, 4,
            "systemstream", G_TYPE_BOOLEAN, FALSE,
            "framerate", GST_TYPE_FRACTION,mpeg4enc_info->encConfig.encOpt.fps_num,
            mpeg4enc_info->encConfig.encOpt.fps_den,NULL);
        
        if (!(gst_pad_set_caps(mpeg4enc_info->srcpad, caps))) {
            GST_ERROR ("\nCould not set the caps" 
                "for the MPEG4 Hantro encoder's src pad\n");
        }
        if(caps != NULL){
            gst_caps_unref(caps);
            caps=NULL;
        }


      /* send the new segment event to the next element in the pipeline */
      if(!mpeg4enc_info->new_segment)
      {
          gint64 start = 0;
          gst_pad_push_event(mpeg4enc_info->srcpad,
              gst_event_new_new_segment(FALSE, 1.0,   GST_FORMAT_TIME, start,
              GST_CLOCK_TIME_NONE, start));
          mpeg4enc_info->new_segment = TRUE;
      }
      
      /* set the sorce pad capabilities */
      src_caps    = GST_PAD_CAPS(mpeg4enc_info->srcpad);
      
      /* allocate the output buffer */
      result = gst_pad_alloc_buffer_and_set_caps(mpeg4enc_info->srcpad,0,
          mpeg4enc_info->encConfig.encOut.strmSize,src_caps,&outbuffer);
      outdata = (guint8 *)GST_BUFFER_DATA(outbuffer);
      
      if(result != GST_FLOW_OK)
      {
          GST_ERROR("Can not create output buffer \n");
          return -1;
      }
      
      
      memcpy(outdata,mpeg4enc_info->enc_iores.outbuf,mpeg4enc_info->encConfig.
          encOut.strmSize);
      headerSize = mpeg4enc_info->encConfig.encOut.strmSize;
      GST_BUFFER_SIZE(outbuffer) = mpeg4enc_info->encConfig.encOut.strmSize;
      GST_DEBUG("Stream start header %u bytes\n", GST_BUFFER_SIZE(outbuffer));
      
      /* push the data to the next element */
      result=gst_pad_push(mpeg4enc_info->srcpad,outbuffer);
      if(result == GST_FLOW_OK)
      {
          GST_DEBUG("Encoder: Pushed the frame to the output element\n");
      }
      else
      {
          GST_ERROR("Can not push the output data to sink element\n");
          
      }
      /* initialise encoder parameters */  
      mpeg4enc_info->headerwrite = TRUE;
      
  } /* mpeg4enc_info->headerwrite */

  if(mfw_gst_mpeg4enc_encode_data(mpeg4enc_info,buf)!=0)
  {
     GST_ERROR("Error in encoding" );
     gst_buffer_unref(buf);
     buf = NULL;
     return GST_FLOW_ERROR;
  }
  else
  {
      src_caps    = GST_PAD_CAPS(mpeg4enc_info->srcpad);
      result = gst_pad_alloc_buffer_and_set_caps(mpeg4enc_info->srcpad,0,
          mpeg4enc_info->encConfig.encOut.strmSize,src_caps,&outbuffer);
      outdata = (guint8 *)GST_BUFFER_DATA(outbuffer);
      if(result != GST_FLOW_OK)
      {
          GST_ERROR("Can not create output buffer \n");
          return -1;
      }
      memcpy(outdata,mpeg4enc_info->enc_iores.outbuf,mpeg4enc_info->encConfig.encOut.strmSize);
      headerSize = mpeg4enc_info->encConfig.encOut.strmSize;
      GST_BUFFER_SIZE(outbuffer) = mpeg4enc_info->encConfig.encOut.strmSize;
      GST_BUFFER_TIMESTAMP(outbuffer) = GST_BUFFER_TIMESTAMP(buf);
      result=gst_pad_push(mpeg4enc_info->srcpad,outbuffer);
      if(result == GST_FLOW_OK)
      {
          GST_DEBUG ("Encoder: Pushed the frame to the output element\n");
      }
      else
      {
          GST_ERROR("Can not push the output data to sink element\n");
          
      }

    gst_buffer_unref(buf);
    buf = NULL;
    return GST_FLOW_OK;
  }
}

/*=============================================================================
FUNCTION:   mfw_gst_mpeg4enc_initialise_params

DESCRIPTION: 
             Initialises all the required encoder configuration parameters

ARGUMENTS PASSED:
        mpeg4enc_info     -   pointer to plug-in's context
      
RETURN VALUE:
        None

PRE-CONDITIONS:
        None

POST-CONDITIONS:
        None

IMPORTANT NOTES:
        None
=============================================================================*/

static void mfw_gst_mpeg4enc_initialise_params(MFW_GST_MPEG4ENC_INFO_T	*mpeg4enc_info)
{
    
    mpeg4enc_info->encConfig.encOpt.lumWidthSrc=
        mpeg4enc_info->encConfig.encOpt.width;
    mpeg4enc_info->encConfig.encOpt.lumHeightSrc=
        mpeg4enc_info->encConfig.encOpt.height;

    mpeg4enc_info->encConfig.encOpt.preProcess=0;
    mpeg4enc_info->encConfig.encOpt.inputRateNumer = 
        mpeg4enc_info->encConfig.encOpt.fps_num;
    mpeg4enc_info->encConfig.encOpt.inputRateDenom=
        mpeg4enc_info->encConfig.encOpt.fps_den;;
    mpeg4enc_info->encConfig.encOpt.outputRateNumer=
        mpeg4enc_info->encConfig.encOpt.inputRateNumer;
    mpeg4enc_info->encConfig.encOpt.outputRateDenom=
        mpeg4enc_info->encConfig.encOpt.inputRateDenom;


    mpeg4enc_info->encConfig.encOpt.intraDcVlcThr=0;
    mpeg4enc_info->encConfig.encOpt.acPred=1;
    mpeg4enc_info->encConfig.encOpt.intraVopRate=0;
    mpeg4enc_info->encConfig.encOpt.cir=2400;
    mpeg4enc_info->encConfig.encOpt.inter4vPenalty=5;
    mpeg4enc_info->encConfig.encOpt.intraPenalty=11;
    mpeg4enc_info->encConfig.encOpt.zeroMvFavor=2;
    mpeg4enc_info->encConfig.encOpt.mvOutOfVop=1;
    mpeg4enc_info->encConfig.encOpt.fourMv=1;
    mpeg4enc_info->encConfig.encOpt.mvRange=DEFAULT;
    mpeg4enc_info->encConfig.encOpt.vpSize=0;
    mpeg4enc_info->encConfig.encOpt.dataPart=0;
    mpeg4enc_info->encConfig.encOpt.rvlc=0;
    mpeg4enc_info->encConfig.encOpt.hec=0;
    mpeg4enc_info->encConfig.encOpt.gobPlace=0;
    mpeg4enc_info->encConfig.encOpt.vbv=0;
    mpeg4enc_info->encConfig.encOpt.qpHdr=10;
    mpeg4enc_info->encConfig.encOpt.qpHdrMin=1;
    mpeg4enc_info->encConfig.encOpt.qpHdrMax=31;
    
    strcpy(mpeg4enc_info->encConfig.encOpt.userDataVos,
        "userDataVos.txt");
    strcpy(mpeg4enc_info->encConfig.encOpt.userDataVisObj,
        "userDataVisObj.txt");
    strcpy(mpeg4enc_info->encConfig.encOpt.userDataVol,
        "userDataVol.txt");
    strcpy(mpeg4enc_info->encConfig.encOpt.userDataGov,
        "userDataGov.txt");

    mpeg4enc_info->enc_iores.outbuf = MAP_FAILED;
    mpeg4enc_info->enc_iores.picture = MAP_FAILED;
    mpeg4enc_info->enc_iores.pict_size=0;
    mpeg4enc_info->enc_iores.outbuf_size=0;
    mpeg4enc_info->enc_iores.pict_bus_address = 0;
    mpeg4enc_info->enc_iores.outbuf_bus_address = 0;
    mpeg4enc_info->enc_iores.memdev_fd = -1;

}
/*=============================================================================
FUNCTION:   mfw_gst_mpeg4enc_change_state

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
mfw_gst_mpeg4enc_change_state(GstElement* element, GstStateChange transition)
{
    
    GstStateChangeReturn    retstate = 0;
    GstCaps                 *src_caps;
    GstFlowReturn           result;
    gint                     headerSize = 0;
    GstBuffer               *outbuffer;
    guint8                  *outdata;

	MFW_GST_MPEG4ENC_INFO_T	*mpeg4enc_info;
    mpeg4enc_info = MFW_GST_MPEG4ENC(element);
	
    switch (transition)
    {
        case GST_STATE_CHANGE_NULL_TO_READY:
            GST_DEBUG(" in NULL to READY state \n");
            mpeg4enc_info->headerwrite = FALSE;
            mpeg4enc_info->new_segment = FALSE;
            mpeg4enc_info->direct_mem = FALSE;
    		break;

        case GST_STATE_CHANGE_READY_TO_PAUSED:
            GST_DEBUG(" in READY_TO_PAUSED state \n");
            break;

        case  GST_STATE_CHANGE_PAUSED_TO_PLAYING:
            GST_DEBUG(" in  to  PAUSED_TO_PLAYING state \n");
            break;
        default:
            break;
    }
    retstate = parent_class_mpeg4enc->change_state (element, transition);
    switch (transition)
    {
        case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
            GST_DEBUG(" in  to PLAYING_TO_PAUSED state \n");
            break;
        case GST_STATE_CHANGE_PAUSED_TO_READY:
            GST_DEBUG(" in  to PAUSED_TO_READY state \n");
            break;
        case GST_STATE_CHANGE_READY_TO_NULL:
            GST_DEBUG(" in  to READY_TO_NULL state \n");

            /* Free all resources */
            mfw_mpeg4enc_CloseEncoder(mpeg4enc_info->encConfig.encoder);
            g_free(mpeg4enc_info->encConfig.encIn.pVpSizes);
            mpeg4enc_info->encConfig.encIn.pVpSizes = NULL;
            mfw_mpeg4enc_FreeRes(mpeg4enc_info);
            break;
        default:
            break;
    }
    return retstate;
   
}

/*=============================================================================
FUNCTION:   mfw_gst_mpeg4enc_sink_event

DESCRIPTION:

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
mfw_gst_mpeg4enc_sink_event (GstPad* pad, GstEvent* event)
{
   gboolean                  result = TRUE;
   MFW_GST_MPEG4ENC_INFO_T   *mpeg4enc_info;
   GstBuffer               *outbuffer;
   guint8                  *outdata;
   GstCaps                 *src_caps;
   GstFlowReturn           flow_result;
   gint                     headerSize = 0;
   MP4EncRet               ret;

   mpeg4enc_info = MFW_GST_MPEG4ENC(GST_OBJECT_PARENT (pad));
   g_print("MPEG4 enc sink_event:%d.\n",GST_EVENT_TYPE (event));
	switch (GST_EVENT_TYPE (event))
	{
		case GST_EVENT_NEWSEGMENT:
		{
            gst_event_unref (event);
            event = NULL;
			break;
		}

		case GST_EVENT_EOS:
        {
            g_print("\nMPEG4 encoder: Got an EOS from filesrc\n");
            
            GST_DEBUG("total frames = %d\n",mpeg4enc_info->encConfig.vopCnt);
            ret = MP4EncStrmEnd(mpeg4enc_info->encConfig.encoder, 
                &mpeg4enc_info->encConfig.encIn, 
                &mpeg4enc_info->encConfig.encOut);
            
            if(ret != ENC_OK)
            {
                GST_ERROR("Failed to end the stream. Error code: %8i\n", ret);
                gst_pad_push_event(mpeg4enc_info->srcpad,event);
                return FALSE;
            }
            else
            {
                GST_DEBUG("Stream end header %u bytes\n", 
                    mpeg4enc_info->encConfig.encOut.strmSize);
                src_caps    = GST_PAD_CAPS(mpeg4enc_info->srcpad);
                flow_result = gst_pad_alloc_buffer_and_set_caps(mpeg4enc_info->srcpad,0,
                    mpeg4enc_info->encConfig.encOut.strmSize,src_caps,&outbuffer);
                outdata = (guint8 *)GST_BUFFER_DATA(outbuffer);
                
                if(flow_result != GST_FLOW_OK)
                {
                    GST_ERROR("Can not create output buffer \n");
                    gst_pad_push_event(mpeg4enc_info->srcpad,event);
                    return FALSE;
                }
                memcpy(outdata,mpeg4enc_info->enc_iores.outbuf,
                    mpeg4enc_info->encConfig.encOut.strmSize);
                headerSize = mpeg4enc_info->encConfig.encOut.strmSize;
                GST_BUFFER_SIZE(outbuffer) = mpeg4enc_info->encConfig.encOut.strmSize;
                flow_result=gst_pad_push(mpeg4enc_info->srcpad,outbuffer);
                if(flow_result == GST_FLOW_OK)
                {
                    GST_DEBUG("Decoder: Pushed the frame to the output element\n");
                }
                else
                {
                    GST_ERROR("Can not push the output data to sink element\n");
                    
                }
            }
            gst_pad_push_event(mpeg4enc_info->srcpad,event);
			break;
        }
        default:
        {
            result = gst_pad_event_default (pad, event);
            gst_event_unref (event);
            event = NULL;
			break;
        }

     }
    return result;
}
/*=============================================================================
FUNCTION:   mfw_gst_mpeg4enc_set_caps

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
mfw_gst_mpeg4enc_set_caps (GstPad *pad, GstCaps *caps)
{
  MFW_GST_MPEG4ENC_INFO_T *mpeg4enc_info;
  mpeg4enc_info = MFW_GST_MPEG4ENC(gst_pad_get_parent (pad));
  MP4EncApiVersion        ver;
  gint width=0;
  gint height=0;
  GstStructure *structure     = gst_caps_get_structure(caps, 0);

  gst_structure_get_int(structure, "width", &width);
  if(width != 0)
  {
      
       mpeg4enc_info->encConfig.encOpt.width=width;
  }
  gst_structure_get_int(structure, "height", &height);
  if(height!=0)
  {
      mpeg4enc_info->encConfig.encOpt.height =height;
  }
  
  gst_structure_get_fraction(structure, "framerate",
      &mpeg4enc_info->encConfig.encOpt.fps_num,
      &mpeg4enc_info->encConfig.encOpt.fps_den);
  GST_DEBUG("frameratenu=%d\n",mpeg4enc_info->encConfig.encOpt.fps_num);
  GST_DEBUG("frameratedu=%d\n",mpeg4enc_info->encConfig.encOpt.fps_den);
  GST_DEBUG("\nInput Height is %d\n", mpeg4enc_info->encConfig.encOpt.height);
  GST_DEBUG("\nInput width is %d\n", mpeg4enc_info->encConfig.encOpt.width);


  if(!gst_pad_set_caps(mpeg4enc_info->srcpad, caps))
  {
      gst_object_unref(mpeg4enc_info);   
      return FALSE;
  }
  
  gst_object_unref(mpeg4enc_info);   
  return TRUE;
}
/*=============================================================================
FUNCTION:   mfw_gst_mpeg4enc_init

DESCRIPTION:This function creates the pads on the elements and register the
			function pointers which operate on these pads.

ARGUMENTS PASSED:
        pointer the mpeg4 encoder's element handle.

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
mfw_gst_mpeg4enc_init (MFW_GST_MPEG4ENC_INFO_T *mpeg4enc_info,
    MFW_GST_MPEG4ENC_CLASS_T * gclass)
{
  GstElementClass *klass = GST_ELEMENT_GET_CLASS (mpeg4enc_info);

  mpeg4enc_info->sinkpad = gst_pad_new_from_template (
	gst_element_class_get_pad_template (klass, "sink"), "sink");

  gst_pad_set_setcaps_function (mpeg4enc_info->sinkpad, mfw_gst_mpeg4enc_set_caps);
  gst_pad_set_chain_function (mpeg4enc_info->sinkpad, mfw_gst_mpeg4enc_chain);

  gst_pad_set_event_function (mpeg4enc_info->sinkpad,GST_DEBUG_FUNCPTR (
        mfw_gst_mpeg4enc_sink_event));

  mpeg4enc_info->srcpad = gst_pad_new_from_template (
	gst_element_class_get_pad_template (klass, "src"), "src");
  
  gst_element_add_pad (GST_ELEMENT (mpeg4enc_info), mpeg4enc_info->sinkpad);
  gst_element_add_pad (GST_ELEMENT (mpeg4enc_info), mpeg4enc_info->srcpad);

  mpeg4enc_info->encConfig.encOpt.bitPerSecond = 384000;
  mpeg4enc_info->encConfig.encOpt.width = 176;
  mpeg4enc_info->encConfig.encOpt.height = 144;
  mpeg4enc_info->encConfig.encOpt.fps_num = 30;
  mpeg4enc_info->encConfig.encOpt.fps_den = 1;
  mpeg4enc_info->encConfig.encOpt.scheme = 0;
  mpeg4enc_info->encConfig.encOpt.vopRc = 0;
  mpeg4enc_info->encConfig.encOpt.mbRc = 0;
  mpeg4enc_info->encConfig.encOpt.vopSkip = 0;
  mpeg4enc_info->encConfig.encOpt.profile = MPEG4_ADV_SIMPLE_PROFILE_LEVEL_5;
  mpeg4enc_info->encConfig.encOpt.goVopRate=15;
  mpeg4enc_info->encConfig.encOpt.horOffsetSrc=0;
  mpeg4enc_info->encConfig.encOpt.verOffsetSrc=0;
  mpeg4enc_info->encConfig.encOpt.stabFrame=0;
}
/*=============================================================================
FUNCTION:   mfw_gst_mpeg4enc_class_init

DESCRIPTION:Initialise the class only once (specifying what signals,
            arguments and virtual functions the class has and setting up
            global state)
ARGUMENTS PASSED:
       	klass   - pointer to mpeg4 encoder's element class

RETURN VALUE:
        None

PRE-CONDITIONS:
        None

POST-CONDITIONS:
        None

IMPORTANT NOTES:
        None
=============================================================================*/
/* initialize the plugin's class */
static void
mfw_gst_mpeg4enc_class_init (MFW_GST_MPEG4ENC_CLASS_T *klass)
{
  GObjectClass *gobject_class        = NULL;
  GstElementClass *gstelement_class  = NULL;
  gobject_class                      = (GObjectClass*) klass;
  gstelement_class                   = (GstElementClass*) klass;
  parent_class_mpeg4enc              = (GstElementClass*)g_type_class_ref(GST_TYPE_ELEMENT);
  gobject_class->set_property        = mfw_gst_mpeg4enc_set_property;
  gobject_class->get_property        = mfw_gst_mpeg4enc_get_property;
  gstelement_class->change_state     = mfw_gst_mpeg4enc_change_state;

  g_object_class_install_property (gobject_class,MP4ENC_BITRATE,
        g_param_spec_int("bitrate", "BitRate","Gets the bitrate at which the input stream \
        is to be encoded. The bitrate control will be considered enabled when any of \
        the VOP or MB based rate control is enabled. ",0,G_MAXINT, 0, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, MP4ENC_WIDTH,
        g_param_spec_int("width", "Width", "gets the width of the input frame to be encoded",
        0,G_MAXINT, 0, G_PARAM_READABLE));

  g_object_class_install_property (gobject_class, MP4ENC_HEIGHT,
        g_param_spec_int("height", "Height", "gets the height of the input frame to be encoded",
        0,G_MAXINT, 0, G_PARAM_READABLE));

  g_object_class_install_property (gobject_class, MP4ENC_FRAMERATE_NUM,
        g_param_spec_int("fps-n", "fps_n", "gets the numerator fraction of framerate at which \
        the input stream is to be encoded",1,G_MAXINT, 30, G_PARAM_READABLE));

  g_object_class_install_property (gobject_class, MP4ENC_FRAMERATE_DEN,
        g_param_spec_int("fps-d", "fps_d", "gets the denominator fraction of framerate at which \
        the input stream is to be encoded",1,G_MAXINT, 1, G_PARAM_READABLE));


  g_object_class_install_property (gobject_class, MP4ENC_SCHEME,
        g_param_spec_int("scheme", "Scheme", "gets the scheme of encoding",
        0,G_MAXINT, 0, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, MP4ENC_MB_RC,
        g_param_spec_int("mbrc", "mbRc", "enable the macroblock based rate control",
        0,G_MAXINT, 0, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, MP4ENC_VOP_RC,
        g_param_spec_int("voprc", "vopRc", "enable the VOP based rate control",
        0,G_MAXINT, 0, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, MP4ENC_VOPSKIP_RC,
        g_param_spec_int("vopskip", "vopSkip", "frame skipping by RC to maintain bitrate is enabled,",
        0,G_MAXINT, 0, G_PARAM_READWRITE));

  g_object_class_install_property(gobject_class, MP4ENC_PROFILE_LEVEL,
				    g_param_spec_enum("profile", "Profile", 
                    " select MPEG4 and H263 profile and level supported by encoder",
                    MFW_GST_TYPE_MPEG4ENC_PROFILE, MPEG4_SIMPLE_PROFILE_LEVEL_3, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, MP4ENC_GOP_SIZE,
      g_param_spec_int("gop-size", "gop_size", "Number of frames within one GOP",
      0,G_MAXINT, 15, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, MP4ENC_CROP_HOR,
      g_param_spec_int("crop-hor-src", "crop_hor_src", 
      "horizontal offset at which the input source frame has to be cropped ",
      0,G_MAXINT, 0, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, MP4ENC_CROP_VER,
      g_param_spec_int("crop-ver-src", "crop_ver_src", 
      "vertical offset at which the input source frame has to be cropped ",
      0,G_MAXINT, 0, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, MP4ENC_STAB_AREA,
      g_param_spec_int("stab-area", "stab_area", 
      "camera stabilization area to be selected ",
      0,G_MAXINT, 0, G_PARAM_READWRITE));



    



}


/*=============================================================================
FUNCTION:  mfw_gst_mpeg4enc_base_init 

DESCRIPTION:
            mpeg4 encoder element details are registered with the plugin during
            _base_init ,This function will initialise the class and child
            class properties during each new child class creation


ARGUMENTS PASSED:
        Klass   -   pointer to mpeg4 encoder plug-in class

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
mfw_gst_mpeg4enc_base_init (MFW_GST_MPEG4ENC_CLASS_T *klass)
{
  static GstElementDetails element_details = {
    "Freescale MPEG4 Encoder Plugin",
    "Codec/Encoder/Video",
    "Encodes the raw input YUV data into a compressed MPEG4 data stream",
    FSL_GST_MM_PLUGIN_AUTHOR
  };
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  gst_element_class_add_pad_template (element_class,
	gst_static_pad_template_get (&src_factory));
  gst_element_class_add_pad_template (element_class,
	gst_static_pad_template_get (&sink_factory));
  gst_element_class_set_details (element_class, &element_details);
}




/*======================================================================================

FUNCTION:       mfw_gst_mpeg4enc_profile_get_type

DESCRIPTION:    Gets an enumeration for the different 
                profile and levels supported by the MPEG4 
                Hantro Encoder
ARGUMENTS PASSED:
                None

RETURN VALUE:
                enumerated type of the profile and levels
                supported by the encoder

PRE-CONDITIONS:
                None

POST-CONDITIONS:
                None

IMPORTANT NOTES:
                None

========================================================================================*/

GType
mfw_gst_mpeg4enc_profile_get_type(void)
{
  static GType mpeg4enc_profile_type = 0;
  static GEnumValue mpeg4enc_profiles[] = {
    {MPEG4_SIMPLE_PROFILE_LEVEL_0,  "MPEG4_SIMPLE_PROFILE_LEVEL_0", "mpeg4-sp-level0"},
    {MPEG4_SIMPLE_PROFILE_LEVEL_0B, "MPEG4_SIMPLE_PROFILE_LEVEL_0B", "mpeg4-sp-level0b"},
    {MPEG4_SIMPLE_PROFILE_LEVEL_1,  "MPEG4_SIMPLE_PROFILE_LEVEL_1", "mpeg4-sp-level1"},
    {MPEG4_SIMPLE_PROFILE_LEVEL_2,  "MPEG4_SIMPLE_PROFILE_LEVEL_2", "mpeg4-sp-level2"},
    {MPEG4_SIMPLE_PROFILE_LEVEL_3,  "MPEG4_SIMPLE_PROFILE_LEVEL_3", "mpeg4-sp-level3"},
    {MPEG4_ADV_SIMPLE_PROFILE_LEVEL_3,  "MPEG4_ADV_SIMPLE_PROFILE_LEVEL_3", "mpeg4-asp-level3"},
    {MPEG4_ADV_SIMPLE_PROFILE_LEVEL_4,  "MPEG4_ADV_SIMPLE_PROFILE_LEVEL_4", "mpeg4-asp-level4"},
    {MPEG4_ADV_SIMPLE_PROFILE_LEVEL_5,  "MPEG4_ADV_SIMPLE_PROFILE_LEVEL_5", "mpeg4-asp-level5"},
    {H263_PROFILE_0_LEVEL_10,  "H263_PROFILE_0_LEVEL_10", "h264-prof0-level10"},
    {H263_PROFILE_0_LEVEL_20,  "H263_PROFILE_0_LEVEL_20", "h264-prof0-level20"},
    {H263_PROFILE_0_LEVEL_30,  "H263_PROFILE_0_LEVEL_30", "h264-prof0-level30"},
    {H263_PROFILE_0_LEVEL_40,  "H263_PROFILE_0_LEVEL_40", "h264-prof0-level40"},
    {H263_PROFILE_0_LEVEL_50,  "H263_PROFILE_0_LEVEL_50", "h264-prof0-level50"},
    {H263_PROFILE_0_LEVEL_60,  "H263_PROFILE_0_LEVEL_60", "h264-prof0-level60"},
    {H263_PROFILE_0_LEVEL_70,  "H263_PROFILE_0_LEVEL_70", "h264-prof0-level70"},
    {0, NULL, NULL},
  };
  if (!mpeg4enc_profile_type) {
    mpeg4enc_profile_type =
        g_enum_register_static ("MfwGstMPEG4EncProfs", mpeg4enc_profiles);
    
  }
  return mpeg4enc_profile_type;
}


/*=============================================================================
FUNCTION: mfw_gst_mpeg4enc_get_type

DESCRIPTION:    intefaces are initiated in this function.you can register one
                or more interfaces  after having registered the type itself.

ARGUMENTS PASSED:
            None

RETURN VALUE:
                 A numerical value ,which represents the unique identifier of this
            element(mpeg4encoder)

PRE-CONDITIONS:
            None

POST-CONDITIONS:
            None

IMPORTANT NOTES:
            None
=============================================================================*/

GType
mfw_gst_mpeg4enc_get_type (void)
{
  static GType mpeg4enc_type = 0;

  if (!mpeg4enc_type)
  {
    static const GTypeInfo mpeg4enc_info =
    {
      sizeof (MFW_GST_MPEG4ENC_CLASS_T),
      (GBaseInitFunc) mfw_gst_mpeg4enc_base_init,
      NULL,
      (GClassInitFunc) mfw_gst_mpeg4enc_class_init,
      NULL,
      NULL,
      sizeof (MFW_GST_MPEG4ENC_INFO_T),
      0,
      (GInstanceInitFunc) mfw_gst_mpeg4enc_init,
    };
    mpeg4enc_type = g_type_register_static (GST_TYPE_ELEMENT,
	                                  "MFW_GST_MPEG4ENC_INFO_T",
	                                  &mpeg4enc_info, 0);
    
    GST_DEBUG_CATEGORY_INIT(mfw_gst_mpeg4enc_debug, "mfw_mpeg4encoder", 0,
								"FreeScale's MPEG4 Encoder's Log");

  }
  return mpeg4enc_type;
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

static gboolean
plugin_init (GstPlugin *plugin)
{
  return gst_element_register (plugin, "mfw_mpeg4encoder",
			       GST_RANK_NONE,
			       MFW_GST_TYPE_MPEG4ENCODER);
}


/*****************************************************************************/
/*    This is used to define the entry point and meta data of plugin         */
/*****************************************************************************/
GST_PLUGIN_DEFINE (
  GST_VERSION_MAJOR,
  GST_VERSION_MINOR,
  "mfw_mpeg4encoder",
  "Encodes the raw input YUV data to a compressed MPEG4 data stream",
  plugin_init,
  VERSION,
  GST_LICENSE_UNKNOWN,
  FSL_GST_MM_PLUGIN_PACKAGE_NAME, FSL_GST_MM_PLUGIN_PACKAGE_ORIG)
  