/*
 * Copyright (C) 2005-2011 Freescale Semiconductor, Inc. All rights reserved.
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
 * Module Name:    mfw_gst_v4lsrc.c
 *
 * Description:    Implementation of V4L Source Plugin for Gstreamer
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
#include <gst/base/gstpushsrc.h>
#include <gst/interfaces/propertyprobe.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdint.h>
#include <linux/videodev2.h>
#include <sys/mman.h>
#include <string.h>
#include "mfw_gst_v4lsrc.h"
#ifdef MX27
#include "mxcfb.h"
#else
#include "linux/mxcfb.h"
#endif

#include "mfw_gst_utils.h"
#include "gstbufmeta.h"

//#define GST_DEBUG g_print

/*=============================================================================
                            LOCAL CONSTANTS
=============================================================================*/
/* None */

/*=============================================================================
                LOCAL TYPEDEFS (STRUCTURES, UNIONS, ENUMS)
=============================================================================*/

enum {
    MFW_V4L_SRC_0,
    MFW_V4L_SRC_WIDTH,
    MFW_V4L_SRC_HEIGHT,
    MFW_V4L_SRC_ROTATE,
    MFW_V4L_SRC_PREVIEW,
    MFW_V4L_SRC_PREVIEW_WIDTH,
    MFW_V4L_SRC_PREVIEW_TOP,
    MFW_V4L_SRC_PREVIEW_LEFT,
    MFW_V4L_SRC_PREVIEW_HEIGHT,
    MFW_V4L_SRC_CROP_PIXEL,
    MFW_V4L_SRC_FRAMERATE_NUM,
    MFW_V4L_SRC_FRAMERATE_DEN,
    MFW_V4L_SRC_SENSOR_WIDTH,
    MFW_V4L_SRC_SENSOR_HEIGHT,
    MFW_V4L_SRC_CAPTURE_MODE,
    MFW_V4L_SRC_BACKGROUND,
    MFW_V4L_SRC_DEVICE,
};


/*=============================================================================
                              LOCAL MACROS
=============================================================================*/

/* used for debugging */
#define GST_CAT_DEFAULT mfw_gst_v4lsrc_debug

#define ipu_fourcc(a,b,c,d)\
        (((__u32)(a)<<0)|((__u32)(b)<<8)|((__u32)(c)<<16)|((__u32)(d)<<24))

#define IPU_PIX_FMT_RGB332  ipu_fourcc('R','G','B','1') /*!<  8  RGB-3-3-2     */
#define IPU_PIX_FMT_RGB555  ipu_fourcc('R','G','B','O') /*!< 16  RGB-5-5-5     */
#define IPU_PIX_FMT_RGB565  ipu_fourcc('R','G','B','P') /*!< 16  RGB-5-6-5     */
#define IPU_PIX_FMT_RGB666  ipu_fourcc('R','G','B','6') /*!< 18  RGB-6-6-6     */
#define IPU_PIX_FMT_BGR24   ipu_fourcc('B','G','R','3') /*!< 24  BGR-8-8-8     */
#define IPU_PIX_FMT_RGB24   ipu_fourcc('R','G','B','3') /*!< 24  RGB-8-8-8     */
#define IPU_PIX_FMT_BGR32   ipu_fourcc('B','G','R','4') /*!< 32  BGR-8-8-8-8   */
#define IPU_PIX_FMT_BGRA32  ipu_fourcc('B','G','R','A') /*!< 32  BGR-8-8-8-8   */
#define IPU_PIX_FMT_RGB32   ipu_fourcc('R','G','B','4') /*!< 32  RGB-8-8-8-8   */
#define IPU_PIX_FMT_RGBA32  ipu_fourcc('R','G','B','A') /*!< 32  RGB-8-8-8-8   */
#define IPU_PIX_FMT_ABGR32  ipu_fourcc('A','B','G','R') /*!< 32  ABGR-8-8-8-8  */



/*=============================================================================
                             STATIC VARIABLES
=============================================================================*/

static GstElementDetails mfw_gst_v4lsrc_details =
GST_ELEMENT_DETAILS("Freescale Video Source plug-in",
            "Source/Video",
            "Video Source (camera) device plugin used in Capturing video "
            "YUV 4:2:0 data with the support to crop the input image",
            FSL_GST_MM_PLUGIN_AUTHOR);

/*=============================================================================
                             GLOBAL VARIABLES
=============================================================================*/
/* None */

/*=============================================================================
                        LOCAL FUNCTION PROTOTYPES
=============================================================================*/

GST_DEBUG_CATEGORY_STATIC(mfw_gst_v4lsrc_debug);
static void mfw_gst_v4lsrc_buffer_class_init (gpointer g_class,
    gpointer class_data);
static void mfw_gst_v4lsrc_buffer_init (GTypeInstance * instance, gpointer g_class);
static void mfw_gst_v4lsrc_buffer_finalize (MFWGstV4LSrcBuffer * v4lsrc_buffer);
static void mfw_gst_v4lsrc_fixate(GstPad * pad, GstCaps * caps);
static GstCaps *mfw_gst_v4lsrc_get_caps (GstBaseSrc * src);
static GstFlowReturn mfw_gst_v4lsrc_create (GstPushSrc * src, GstBuffer ** buf);
static GstBuffer * mfw_gst_v4lsrc_buffer_new(MFWGstV4LSrc *v4l_src);
static gboolean mfw_gst_v4lsrc_stop (GstBaseSrc * src);
static gboolean mfw_gst_v4lsrc_start (GstBaseSrc * src);
static gboolean mfw_gst_v4lsrc_set_caps (GstBaseSrc * src, GstCaps * caps);
static void mfw_gst_v4lsrc_get_property(GObject * object, guint prop_id,
				   GValue * value, GParamSpec * pspec);
static void mfw_gst_v4lsrc_set_property(GObject * object, guint prop_id,
				   const GValue * value, GParamSpec * pspec);
static gint mfw_gst_v4lsrc_capture_setup(MFWGstV4LSrc *v4l_src);
static gint mfw_gst_v4lsrc_stop_capturing(MFWGstV4LSrc *v4l_src);
static gint mfw_gst_v4lsrc_start_capturing(MFWGstV4LSrc *v4l_src);


GST_BOILERPLATE(MFWGstV4LSrc, mfw_gst_v4lsrc, GstPushSrc,GST_TYPE_PUSH_SRC);


/*=============================================================================
FUNCTION:           mfw_gst_v4lsrc_buffer_get_type    

DESCRIPTION:        This funtion registers the  buffer type on to the V4L Source plugin
             
ARGUMENTS PASSED:   void   

RETURN VALUE:       Return the registered buffer type
PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
GType mfw_gst_v4lsrc_buffer_get_type (void)
{
  static GType v4lsrc_buffer_type;

  if (G_UNLIKELY (v4lsrc_buffer_type == 0)) {
    static const GTypeInfo v4lsrc_buffer_info = {
      sizeof (GstBufferClass),
      NULL,
      NULL,
      mfw_gst_v4lsrc_buffer_class_init,
      NULL,
      NULL,
      sizeof (MFWGstV4LSrcBuffer),
      0,
      mfw_gst_v4lsrc_buffer_init,
      NULL
    };
    v4lsrc_buffer_type = g_type_register_static (GST_TYPE_BUFFER,
        "MFWGstV4LSrcBuffer", &v4lsrc_buffer_info, 0);
  }
  return v4lsrc_buffer_type;
}


/*=============================================================================
FUNCTION:           mfw_gst_v4lsrc_buffer_class_init    

DESCRIPTION:   This funtion registers the  funtions used by the 
                buffer class of the V4l source plug-in
             
ARGUMENTS PASSED:
        g_class        -   class from which the mini objext is derived
        class_data     -   global class data

RETURN VALUE:       None
PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
static void mfw_gst_v4lsrc_buffer_class_init (gpointer g_class, gpointer class_data)
{
  GstMiniObjectClass *mini_object_class = GST_MINI_OBJECT_CLASS (g_class);

  mini_object_class->finalize = (GstMiniObjectFinalizeFunction)
      mfw_gst_v4lsrc_buffer_finalize;
}


/*=============================================================================
FUNCTION:      mfw_gst_v4lsrc_buffer_init    

DESCRIPTION:   This funtion initialises the buffer class of the V4l source plug-in
             
ARGUMENTS PASSED:
        instance       -   pointer to buffer instance
        g_class        -   global pointer

RETURN VALUE:       None
PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
static void mfw_gst_v4lsrc_buffer_init(GTypeInstance * instance, gpointer g_class)
{

}


/*=============================================================================
FUNCTION:      mfw_gst_v4lsrc_buffer_finalize    

DESCRIPTION:   This function is invoked whenever the buffer object belonging 
               to the V4L Source buffer glass is tried to un-refrenced. Here 
               only the refernce count of the buffer object is increased without 
               freeing the memory allocated.

ARGUMENTS PASSED:
        v4lsrc_buffer -   pointer to V4L sou4rce buffer class

RETURN VALUE:       None
PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
static void mfw_gst_v4lsrc_buffer_finalize (MFWGstV4LSrcBuffer * v4lsrc_buffer)
{
  MFWGstV4LSrc *v4l_src;
  gint num;
  v4l_src = v4lsrc_buffer->v4lsrccontext;
  num = v4lsrc_buffer->num;
  GST_LOG_OBJECT (v4l_src, "freeing buffer %p for frame %d", v4lsrc_buffer, num);
  gst_buffer_ref(GST_BUFFER_CAST(v4lsrc_buffer));
}

 
/*=============================================================================
FUNCTION:      mfw_gst_v4lsrc_start_capturing    
        
DESCRIPTION:   This function triggers the V4L Driver to start Capturing

ARGUMENTS PASSED:
        v4l_src -   The V4L Souce plug-in context.

RETURN VALUE:       None
PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
static gint mfw_gst_v4lsrc_start_capturing(MFWGstV4LSrc *v4l_src)
{
    guint i;
    struct v4l2_buffer buf;
    enum v4l2_buf_type type;

    // query for NUM_BUFFERS number of buffers to store the captured data 
    for (i = 0; i < NUM_BUFFERS; i++)
    {
        memset(&buf, 0, sizeof (buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.index = i;
        if (ioctl(v4l_src->fd_v4l, VIDIOC_QUERYBUF, &buf) < 0)
        {
            GST_ERROR(">>V4L_SRC: VIDIOC_QUERYBUF error\n");
            return -1;
        }
        v4l_src->buffers[i] = gst_buffer_new();
        GST_BUFFER_SIZE(v4l_src->buffers[i]) = buf.length;
        GST_BUFFER_OFFSET(v4l_src->buffers[i]) = (size_t) buf.m.offset;
        GST_BUFFER_DATA(v4l_src->buffers[i]) = mmap (NULL, 
        GST_BUFFER_SIZE(v4l_src->buffers[i]), 
                PROT_READ | PROT_WRITE, MAP_SHARED, 
                v4l_src->fd_v4l, 
                GST_BUFFER_OFFSET(v4l_src->buffers[i]));
        memset(GST_BUFFER_DATA(v4l_src->buffers[i]), 0xFF, GST_BUFFER_SIZE(v4l_src->buffers[i]));
        {
            gint index;
            GstBufferMeta *meta;
            index = G_N_ELEMENTS(v4l_src->buffers[i]->_gst_reserved)-1;
            meta = gst_buffer_meta_new();
            meta->physical_data = (gpointer) (buf.m.offset);
            v4l_src->buffers[i]->_gst_reserved[index] = meta;
        }
         
    }

    /* queue the buffers allocated to store the captured data */
    for (i = 0; i < NUM_BUFFERS; i++)
    {
        memset(&buf, 0, sizeof (buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        buf.m.offset = GST_BUFFER_OFFSET(v4l_src->buffers[i]); 
        if (v4l_src->crop_pixel){
            buf.m.offset += v4l_src->crop_pixel * 
                    (v4l_src->capture_width ) + v4l_src->crop_pixel;
        }
            
        if (ioctl (v4l_src->fd_v4l, VIDIOC_QBUF, &buf) < 0) {
            GST_ERROR(">>V4L_SRC: VIDIOC_QBUF error\n");
            return -1;
        }
    }

    /* Switch ON the capture device */
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl (v4l_src->fd_v4l, VIDIOC_STREAMON, &type) < 0) {
        GST_ERROR(">>V4L_SRC: VIDIOC_STREAMON error\n");
        return -1;   
    }
    v4l_src->time_per_frame = gst_util_uint64_scale_int (GST_SECOND, v4l_src->fps_d, v4l_src->fps_n);
    GST_DEBUG(">>V4L_SRC: time per frame %d\n", (guint32) v4l_src->time_per_frame);
    v4l_src->last_ts = 0;
    return 0;
}


/*=============================================================================
FUNCTION:      mfw_gst_v4lsrc_stop_capturing    
        
DESCRIPTION:   This function triggers the V4L Driver to stop Capturing

ARGUMENTS PASSED:
        v4l_src -   The V4L Souce plug-in context.

RETURN VALUE:       None
PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
static gint mfw_gst_v4lsrc_stop_capturing(MFWGstV4LSrc *v4l_src)
{
    enum v4l2_buf_type type;
    guint i;
    gint index; 

    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if( ioctl (v4l_src->fd_v4l, VIDIOC_STREAMOFF, &type) < 0 )
    {
        GST_ERROR(">>V4L_SRC: error in VIDIOC_STREAMOFF\n");
        return - 1;
    }
    for (i = 0; i < NUM_BUFFERS; i++)
    {
        munmap(GST_BUFFER_DATA(v4l_src->buffers[i]),
               GST_BUFFER_SIZE(v4l_src->buffers[i]));

        index = G_N_ELEMENTS(v4l_src->buffers[i]->_gst_reserved)-1;
        gst_buffer_meta_free(v4l_src->buffers[i]->_gst_reserved[index]);
        gst_buffer_unref(v4l_src->buffers[i]);
    }
    return 0;
}



/*=============================================================================
FUNCTION:      mfw_gst_v4lsrc_capture_setup    
        
DESCRIPTION:   This function does the necessay initialistions for the V4L capture
               device driver.

ARGUMENTS PASSED:
        v4l_src -   The V4L Souce plug-in context.

RETURN VALUE:       None
PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
static gint mfw_gst_v4lsrc_capture_setup(MFWGstV4LSrc *v4l_src)
{
    struct v4l2_format fmt;
    struct v4l2_control ctrl;
    struct v4l2_streamparm parm;
    gint fd_v4l = 0;
    struct v4l2_mxc_offset off;
    gint in_width=0,in_height=0;

    if ((fd_v4l = open( v4l_src->devicename, O_RDWR, 0)) < 0)
    {
        GST_ERROR(">>V4L_SRC: Unable to open %s\n", v4l_src->devicename);
        return 0;
    }

    if(v4l_src->crop_pixel)
    {
        in_width = v4l_src->capture_width - (2*v4l_src->crop_pixel);
        in_height = v4l_src->capture_height - (2*v4l_src->crop_pixel);
    }
    else
    {
        in_width = v4l_src->capture_width;
        in_height = v4l_src->capture_height;
    }
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
#ifdef MX51
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_NV12;
#else
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUV420;
#endif
    fmt.fmt.pix.width = in_width;
    fmt.fmt.pix.height = in_height;

    if (v4l_src->crop_pixel){
	    off.u_offset = (2 * v4l_src->crop_pixel + in_width) 
                * (in_height + v4l_src->crop_pixel)
				 - v4l_src->crop_pixel + (v4l_src->crop_pixel / 2) * ((in_width / 2) 
				 + v4l_src->crop_pixel) + v4l_src->crop_pixel / 2;  
		off.v_offset = off.u_offset + (v4l_src->crop_pixel + in_width / 2) * 
				((in_height / 2) + v4l_src->crop_pixel);  
        	fmt.fmt.pix.bytesperline = in_width + v4l_src->crop_pixel * 2;
			fmt.fmt.pix.priv = (uint32_t) &off;
        	fmt.fmt.pix.sizeimage = (in_width + v4l_src->crop_pixel * 2 ) 
        		* (in_height + v4l_src->crop_pixel * 2) * 3 / 2;
    } else {
	    fmt.fmt.pix.bytesperline = in_width;
		fmt.fmt.pix.priv = 0;
        fmt.fmt.pix.sizeimage = 0;
	}


    parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    parm.parm.capture.timeperframe.numerator = v4l_src->fps_d;
    parm.parm.capture.timeperframe.denominator = v4l_src->fps_n;

    parm.parm.capture.capturemode = v4l_src->capture_mode;
    if (parm.parm.capture.capturemode >=4) {
        gint input = 1;
        if (ioctl(fd_v4l, VIDIOC_S_INPUT, &input) < 0)
        {
            GST_ERROR(">>V4L_SRC: VIDIOC_S_INPUT failed\n");
            return -1;
        } 
    }
    if (ioctl(fd_v4l, VIDIOC_S_PARM, &parm) < 0)
    {
        GST_ERROR(">>V4L_SRC: VIDIOC_S_PARM failed\n");
        return -1;
    } 
    if (ioctl(fd_v4l, VIDIOC_S_FMT, &fmt) < 0)
    {
        GST_ERROR(">>V4L_SRC: set format failed\n");
        return 0;
    } 

    // Set rotation
    ctrl.id = V4L2_CID_PRIVATE_BASE + 0;
    ctrl.value = v4l_src->rotate;
    if (ioctl(fd_v4l, VIDIOC_S_CTRL, &ctrl) < 0)
    {
        GST_ERROR(">>V4L_SRC: rotation set ctrl failed\n");
        return 0;
    }

    struct v4l2_requestbuffers req;
    memset(&req, 0, sizeof (req));
    req.count = NUM_BUFFERS;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (ioctl(fd_v4l, VIDIOC_REQBUFS, &req) < 0)
    {
        GST_ERROR(">>V4L_SRC: v4l_mfw_gst_v4lsrc_capture_setup: VIDIOC_REQBUFS failed\n");
        return 0;
    }

    return fd_v4l;
}


/*=============================================================================
FUNCTION:           mfw_gst_v4lsrc_set_property   
        
DESCRIPTION:        This function is notified if application changes the values of 
                    a property.            

ARGUMENTS PASSED:
        object  -   pointer to GObject   
        prop_id -   id of element
        value   -   pointer to Gvalue
        pspec   -   pointer to GParamSpec

RETURN VALUE:       None
PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
static void mfw_gst_v4lsrc_set_property(GObject * object, guint prop_id,
				   const GValue * value,
				   GParamSpec * pspec)
{
    MFWGstV4LSrc *v4l_src =
	MFW_GST_V4LSRC(object);
    switch (prop_id) {
    case MFW_V4L_SRC_WIDTH:
	v4l_src->capture_width = g_value_get_int(value);
	GST_DEBUG("width=%d\n", v4l_src->capture_width);
	break;
    case MFW_V4L_SRC_HEIGHT:
	v4l_src->capture_height = g_value_get_int(value);
	GST_DEBUG("height=%d\n", v4l_src->capture_height);
	break;
    case MFW_V4L_SRC_ROTATE:
	v4l_src->rotate = g_value_get_int(value);
	GST_DEBUG("rotate=%d\n", v4l_src->rotate);
	break;

    case MFW_V4L_SRC_PREVIEW:
	v4l_src->preview = g_value_get_boolean(value);
	GST_DEBUG("preview=%d\n", v4l_src->preview);
	break;


    case MFW_V4L_SRC_PREVIEW_WIDTH:
	v4l_src->preview_width = g_value_get_int(value);
	GST_DEBUG("preview_width=%d\n", v4l_src->preview_width);
	break;

    case MFW_V4L_SRC_PREVIEW_HEIGHT:
	v4l_src->preview_height = g_value_get_int(value);
	GST_DEBUG("preview_height=%d\n", v4l_src->preview_height);
	break;

    case MFW_V4L_SRC_PREVIEW_TOP:
	v4l_src->preview_top = g_value_get_int(value);
	GST_DEBUG("preview_top=%d\n", v4l_src->preview_top);
	break;

    case MFW_V4L_SRC_PREVIEW_LEFT:
	v4l_src->preview_left = g_value_get_int(value);
	GST_DEBUG("preview_left=%d\n", v4l_src->preview_left);
	break;



    case MFW_V4L_SRC_CROP_PIXEL:
	v4l_src->crop_pixel = g_value_get_int(value);
	GST_DEBUG("crop_pixel=%d\n", v4l_src->crop_pixel);
	break;
    case MFW_V4L_SRC_FRAMERATE_NUM:
	v4l_src->fps_n = g_value_get_int(value);
	GST_DEBUG("framerate numerator =%d\n", v4l_src->fps_n);
    break;
    case MFW_V4L_SRC_FRAMERATE_DEN:
	v4l_src->fps_d = g_value_get_int(value);
	GST_DEBUG("framerate denominator=%d\n", v4l_src->fps_d);
	break;
    case MFW_V4L_SRC_SENSOR_WIDTH:
	v4l_src->sensor_width = g_value_get_int(value);
	GST_DEBUG("sensor width=%d\n", v4l_src->sensor_width);
	break;
    case MFW_V4L_SRC_SENSOR_HEIGHT:
	v4l_src->sensor_height = g_value_get_int(value);
	GST_DEBUG("sensor height=%d\n", v4l_src->sensor_height);
	break;
    case MFW_V4L_SRC_CAPTURE_MODE:
    v4l_src->capture_mode = g_value_get_int(value);
	GST_DEBUG("capture mode=%d\n", v4l_src->capture_mode);
    break;

    case MFW_V4L_SRC_BACKGROUND:
	v4l_src->bg = g_value_get_boolean(value);
	GST_DEBUG("bg value=%d\n", v4l_src->bg);
	break;
    case MFW_V4L_SRC_DEVICE:
      if (v4l_src->devicename)
        g_free (v4l_src->devicename);
      v4l_src->devicename = g_strdup (g_value_get_string (value));
    break;
    default:
	G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
	break;
    }
    
}


/*=============================================================================
FUNCTION:   mfw_gst_v4lsrc_get_property    
        
DESCRIPTION:    This function is notified if application requests the values of 
                a property.                  

ARGUMENTS PASSED:
        object  -   pointer to GObject   
        prop_id -   id of element
        value   -   pointer to Gvalue
        pspec   -   pointer to GParamSpec

RETURN VALUE:       None
PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
static void mfw_gst_v4lsrc_get_property(GObject * object, guint prop_id,
				   GValue * value,
				   GParamSpec * pspec)
{
    
    MFWGstV4LSrc *v4l_src =
	MFW_GST_V4LSRC(object);
    switch (prop_id) {
    case MFW_V4L_SRC_WIDTH:
	g_value_set_int(value,v4l_src->capture_width);
	break;
    case MFW_V4L_SRC_HEIGHT:
	g_value_set_int(value,v4l_src->capture_height);
	break;
    case MFW_V4L_SRC_ROTATE:
	g_value_set_int(value,v4l_src->rotate);
	break;

    case MFW_V4L_SRC_PREVIEW:
	g_value_set_boolean(value, v4l_src->preview);
	break;
    case MFW_V4L_SRC_PREVIEW_WIDTH:
    g_value_set_int(value, v4l_src->preview_width);
    break;
    case MFW_V4L_SRC_PREVIEW_HEIGHT:
    g_value_set_int(value, v4l_src->preview_height);
    break;

    case MFW_V4L_SRC_PREVIEW_TOP:
    g_value_set_int(value, v4l_src->preview_top);
    break;

    case MFW_V4L_SRC_PREVIEW_LEFT:
    g_value_set_int(value, v4l_src->preview_left);
    break;


    case MFW_V4L_SRC_CROP_PIXEL:
	g_value_set_int(value,v4l_src->crop_pixel);
	break;
    case MFW_V4L_SRC_FRAMERATE_NUM:
	g_value_set_int(value,v4l_src->fps_n);
	break;
    case MFW_V4L_SRC_FRAMERATE_DEN:
	g_value_set_int(value,v4l_src->fps_d);
	break;
    
    case MFW_V4L_SRC_SENSOR_WIDTH:
	g_value_set_int(value, v4l_src->sensor_width);
	break;
    case MFW_V4L_SRC_SENSOR_HEIGHT:
	g_value_set_int(value, v4l_src->sensor_height);
	break;

    case MFW_V4L_SRC_CAPTURE_MODE:
    g_value_set_int(value, v4l_src->capture_mode);
    break;

    case MFW_V4L_SRC_BACKGROUND:
	g_value_set_boolean(value, v4l_src->bg);
	break;
    case MFW_V4L_SRC_DEVICE:
    g_value_set_string (value, v4l_src->devicename);
    break;

    default:
	G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
	break;
    }
    
}


/*=============================================================================
FUNCTION:            mfw_gst_v4lsrc_set_caps
         
DESCRIPTION:         this function does the capability negotiation between adjacent pad  

ARGUMENTS PASSED:    
        src       -   pointer to base source 
        caps      -   pointer to GstCaps
        
  
RETURN VALUE:       TRUE or FALSE depending on capability is negotiated or not.
PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
static gboolean mfw_gst_v4lsrc_set_caps (GstBaseSrc * src, GstCaps * caps)
{
    MFWGstV4LSrc *v4l_src = MFW_GST_V4LSRC(src);    
    return TRUE;
}


/*=============================================================================
FUNCTION:            mfw_gst_v4lsrc_overlay_setup
         
DESCRIPTION:         This function performs the initialisations required for preview

ARGUMENTS PASSED:    
        fd_v4l    -   capture device ID
        fmt       -   pointer to the V4L format structure.
        
  
RETURN VALUE:       TRUE - preview setup initialised successfully
                    FALSE - Error in initialising the preview set up.
PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
gboolean mfw_gst_v4lsrc_overlay_setup(MFWGstV4LSrc *v4l_src, struct v4l2_format *fmt)
{
    struct v4l2_streamparm parm;
    v4l2_std_id id;
    struct v4l2_control ctl;
    struct v4l2_crop crop;
    int g_sensor_width = v4l_src->sensor_width;
    int g_sensor_height = v4l_src->sensor_height;
    int g_sensor_top = 0;
    int g_sensor_left = 0;
    int g_display_lcd = 0;
    int g_camera_color = 0;
    int fd_v4l = v4l_src->fd_v4l;

    /* this ioctl sets up the LCD display for preview */
    if (ioctl(fd_v4l, VIDIOC_S_OUTPUT, &g_display_lcd) < 0)
    {
        GST_ERROR(">>V4L_SRC: VIDIOC_S_OUTPUT failed\n");
        return FALSE;
    } 

	ctl.id = V4L2_CID_PRIVATE_BASE+2;
	ctl.value = v4l_src->rotate;

    /* this ioctl sets rotation value on the display */
    if (ioctl(fd_v4l, VIDIOC_S_CTRL, &ctl) < 0)
    {
        GST_ERROR(">>V4L_SRC: rotation set control failed\n");
        return FALSE;
    } 

    crop.type = V4L2_BUF_TYPE_VIDEO_OVERLAY;
    crop.c.left = g_sensor_left;
    crop.c.top = g_sensor_top;
    crop.c.width = g_sensor_width;
    crop.c.height = g_sensor_height;

    /* this ioctl sets capture rectangle */
    if (ioctl(fd_v4l, VIDIOC_S_CROP, &crop) < 0)
    {
        GST_ERROR(">>V4L_SRC: set capture rectangle for cropping failed\n");
        return FALSE;
    } 

    if (ioctl(fd_v4l, VIDIOC_S_FMT, fmt) < 0)
    {
        GST_ERROR(">>V4L_SRC: set format failed\n");
        return FALSE;
    } 

    if (ioctl(fd_v4l, VIDIOC_G_FMT, fmt) < 0)
    {
        GST_ERROR(">>V4L_SRC: get format failed\n");
        return FALSE;
    } 

    if (ioctl(fd_v4l, VIDIOC_G_STD, &id) < 0)
    {
        GST_ERROR(">>V4L_SRC: VIDIOC_G_STD failed\n");
        return FALSE;
    } 

    return TRUE;
}



/*=============================================================================
FUNCTION:            mfw_gst_v4lsrc_start_preview
         
DESCRIPTION:         This function starts the preview of capture

ARGUMENTS PASSED:    
        fd_v4l    -   capture device ID
        
  
RETURN VALUE:        TRUE - preview start initialised successfully
                     FALSE - Error in starting the preview
        
PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
gboolean mfw_gst_v4lsrc_start_preview(int fd_v4l)
{
    int i;
    int overlay = 1;
    struct v4l2_control ctl;
    int g_camera_color = 0;

    if (ioctl(fd_v4l, VIDIOC_OVERLAY, &overlay) < 0)
    {
        GST_ERROR(">>V4L_SRC: VIDIOC_OVERLAY start failed\n");
		return FALSE;
    } 

    for (i = 0; i < 3 ; i++) { 
        // flash a frame
        ctl.id = V4L2_CID_PRIVATE_BASE + 1;
        if (ioctl(fd_v4l, VIDIOC_S_CTRL, &ctl) < 0)
        {
            GST_ERROR(">>V4L_SRC: set ctl failed\n");
			return FALSE;
        }
	    sleep(1);
    }
    if (g_camera_color == 1) {
        ctl.id = V4L2_CID_BRIGHTNESS;
        for (i = 0; i < 0xff; i+=0x20) {
		    ctl.value = i;
            GST_DEBUG(">>V4L_SRC: change the brightness %d\n", i);
            ioctl(fd_v4l, VIDIOC_S_CTRL, &ctl);
		    sleep(1);
        } 
    } else if (g_camera_color == 2) {
        ctl.id = V4L2_CID_SATURATION;
        for (i = 25; i < 150; i+= 25) {
		    ctl.value = i;
	        GST_DEBUG(">>V4L_SRC: change the color saturation %d\n", i);
            ioctl(fd_v4l, VIDIOC_S_CTRL, &ctl);
		    sleep(5);
        } 
    } else if (g_camera_color == 3) {
        ctl.id = V4L2_CID_RED_BALANCE;
        for (i = 0; i < 0xff; i+=0x20) {
		    ctl.value = i;
	        GST_DEBUG(">>V4L_SRC: change the red balance %d\n", i);
            ioctl(fd_v4l, VIDIOC_S_CTRL, &ctl);
		    sleep(1);
        } 
    } else if (g_camera_color == 4) {
        ctl.id = V4L2_CID_BLUE_BALANCE;
        for (i = 0; i < 0xff; i+=0x20) {
		    ctl.value = i;
            GST_DEBUG(">>V4L_SRC: change the blue balance %d\n", i);
            ioctl(fd_v4l, VIDIOC_S_CTRL, &ctl);
		    sleep(1);
        } 
    } else if (g_camera_color == 5) {
        ctl.id = V4L2_CID_BLACK_LEVEL;
        for (i = 0; i < 4; i++) {
		    ctl.value = i;
            GST_DEBUG(">>V4L_SRC: change the black balance %d\n", i);
            ioctl(fd_v4l, VIDIOC_S_CTRL, &ctl);
		    sleep(5);
        } 
    } else {
	    sleep(2);
    }
	return TRUE;
}

/*=============================================================================
FUNCTION:            mfw_gst_v4lsrc_start
         
DESCRIPTION:         this function is registered  with the Base Source Class of
                     the gstreamer to start the video capturing process 
                     from this function

ARGUMENTS PASSED:    
        src       -   pointer to base source 
        
RETURN VALUE:        TRUE or FALSE depending on the sate of capture initiation
        
PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
static gboolean mfw_gst_v4lsrc_start (GstBaseSrc * src)
{
    MFWGstV4LSrc *v4l_src = MFW_GST_V4LSRC(src);
    struct v4l2_format fmt;
    struct v4l2_framebuffer fb_v4l2;
    char fb_device[100] = "/dev/fb0";
    int fd_fb = 0;
    struct fb_fix_screeninfo fix;
    struct fb_var_screeninfo var;
    struct mxcfb_color_key color_key;
    struct mxcfb_gbl_alpha alpha;
    unsigned short * fb0;
    unsigned char * cur_fb8;
    unsigned short * cur_fb16;
    unsigned int * cur_fb32;
    __u32 screen_size;
    int h, w;
    int ret = 0;
    int g_display_width = 0;
    int g_display_height = 0;
    int g_display_top = 0;
    int g_display_left = 0;
    
    v4l_src->fd_v4l = mfw_gst_v4lsrc_capture_setup(v4l_src);
    if(v4l_src->fd_v4l <=0)
    {
        GST_ERROR("v4lsrc:error in opening the device\n");
        return FALSE;
    }

    if(TRUE==v4l_src->preview)
    {
        g_display_width=v4l_src->preview_width;
        g_display_height=v4l_src->preview_height;
        g_display_top=v4l_src->preview_top;
        g_display_left=v4l_src->preview_left;
        fmt.type = V4L2_BUF_TYPE_VIDEO_OVERLAY;
        fmt.fmt.win.w.top=  g_display_top ;
        fmt.fmt.win.w.left= g_display_left;
        fmt.fmt.win.w.width=g_display_width;
        fmt.fmt.win.w.height=g_display_height;
        
        /* this function sets up the V4L for preview */
        if (mfw_gst_v4lsrc_overlay_setup(v4l_src,&fmt) == FALSE) {
            GST_ERROR(">>V4L_SRC: Setup overlay failed.\n");
            return FALSE;
        }
        
        /* open the frame buffer to display the preview  */
        if ((fd_fb = open(fb_device, O_RDWR )) < 0)	{
            GST_ERROR(">>V4L_SRC: Unable to open frame buffer\n");
            return FALSE;
        }
        
        if (ioctl(fd_fb, FBIOGET_VSCREENINFO, &var) < 0) {
            close(fd_fb);
            return FALSE;
        }
        if (ioctl(fd_fb, FBIOGET_FSCREENINFO, &fix) < 0) {
            close(fd_fb);
            return FALSE;
        }
        
        if (!v4l_src->bg) {
            fb_v4l2.fmt.width = var.xres;
            fb_v4l2.fmt.height = var.yres;
            if (var.bits_per_pixel == 32) {
                fb_v4l2.fmt.pixelformat = IPU_PIX_FMT_BGR32;
                fb_v4l2.fmt.bytesperline = 4 * fb_v4l2.fmt.width; 
            }
            else if (var.bits_per_pixel == 24) {
                fb_v4l2.fmt.pixelformat = IPU_PIX_FMT_BGR24;
                fb_v4l2.fmt.bytesperline = 3 * fb_v4l2.fmt.width; 
            }
            else if (var.bits_per_pixel == 16) {
                fb_v4l2.fmt.pixelformat = IPU_PIX_FMT_RGB565;
                fb_v4l2.fmt.bytesperline = 2 * fb_v4l2.fmt.width; 
            }
            
            fb_v4l2.flags = V4L2_FBUF_FLAG_PRIMARY;
            fb_v4l2.base = (void *) fix.smem_start;
        } else {
            /* alpha blending in done in case of display happeing both in the 
            back ground and foreground simultaneously */
            alpha.alpha = 0;
            alpha.enable = 1;
            if ( ioctl(fd_fb, MXCFB_SET_GBL_ALPHA, &alpha) < 0) {
                
                GST_ERROR("MXCFB_SET_GBL_ALPHA ioctl failed\n");
                close(fd_fb);
                return FALSE;
            }
            
            color_key.color_key = 0x00080808;
            color_key.enable = 1;
            if ( ioctl(fd_fb, MXCFB_SET_CLR_KEY, &color_key) < 0) {
                GST_ERROR("MXCFB_SET_CLR_KEY ioctl failed\n");
                close(fd_fb);
                return FALSE;
            }
            
            if (ioctl(v4l_src->fd_v4l, VIDIOC_G_FBUF, &fb_v4l2) < 0) {
                GST_ERROR(">>V4L_SRC: Get framebuffer failed\n");
                return FALSE;
            }
            fb_v4l2.flags = V4L2_FBUF_FLAG_OVERLAY;
        }
        
        close(fd_fb);
        
        if (ioctl(v4l_src->fd_v4l, VIDIOC_S_FBUF, &fb_v4l2) < 0)
        {
            GST_ERROR(">>V4L_SRC: set framebuffer failed\n");
            return FALSE;
        } 
        
        if (ioctl(v4l_src->fd_v4l, VIDIOC_G_FBUF, &fb_v4l2) < 0) {
            GST_ERROR(">>V4L_SRC: set framebuffer failed\n");
            return FALSE;
        } 
        
        GST_DEBUG("\n frame buffer width %d, height %d, bytesperline %d\n", 
            fb_v4l2.fmt.width, fb_v4l2.fmt.height, fb_v4l2.fmt.bytesperline);
        
        
    }
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(v4l_src->fd_v4l, VIDIOC_G_FMT, &fmt) < 0)
    {
        GST_ERROR(">>V4L_SRC: get format failed\n");
        return FALSE;
    } 
    else
    {
        v4l_src->buffer_size=fmt.fmt.pix.sizeimage;
        GST_DEBUG("\t Width = %d", fmt.fmt.pix.width);
        GST_DEBUG("\t Height = %d", fmt.fmt.pix.height);
        GST_DEBUG("\t Image size = %d\n", fmt.fmt.pix.sizeimage);
        GST_DEBUG("\t pixelformat = %d\n", fmt.fmt.pix.pixelformat);
    }

  
    if (mfw_gst_v4lsrc_start_capturing(v4l_src) < 0)
    {
        GST_ERROR("start_capturing failed\n");
        return FALSE;
    } 

    if(TRUE==v4l_src->preview)
    {
        mfw_gst_v4lsrc_start_preview(v4l_src->fd_v4l);
    }
	
    v4l_src->offset = 0;
    return TRUE;
}


/*=============================================================================
FUNCTION:            mfw_gst_v4lsrc_stop
         
DESCRIPTION:         this function is registered  with the Base Source Class of
                     the gstreamer to stop the video capturing process 
                     by this function

ARGUMENTS PASSED:    
        src       -   pointer to base source 
        
  
RETURN VALUE:        TRUE or FALSE depending on the sate of capture initiation
        
PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
static gboolean mfw_gst_v4lsrc_stop (GstBaseSrc * src)
{
    MFWGstV4LSrc *v4l_src = MFW_GST_V4LSRC(src);
    gint overlay=0;

	
    if (mfw_gst_v4lsrc_stop_capturing(v4l_src) < 0)
    {
        GST_ERROR(">>V4L_SRC: stop_capturing failed\n");
        return FALSE;
    } 

    if(TRUE==v4l_src->preview)
    {
        
        if (ioctl(v4l_src->fd_v4l, VIDIOC_OVERLAY, &overlay) < 0)
        {
            printf("VIDIOC_OVERLAY stop failed\n");
            return FALSE;
        } 
    }
    close(v4l_src->fd_v4l);
    v4l_src->fd_v4l=-1;
    return TRUE;
}


/*=============================================================================
FUNCTION:           mfw_gst_v4lsrc_buffer_new
         
DESCRIPTION:        This function is used to store the frames captured by the
                    V4L capture driver

ARGUMENTS PASSED:   v4l_src     - 
        
RETURN VALUE:       TRUE or FALSE depending on the sate of capture initiation
        
PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
static GstBuffer * mfw_gst_v4lsrc_buffer_new(MFWGstV4LSrc *v4l_src)
{
    GstBuffer *buf;
    gint fps_n, fps_d;
    struct v4l2_buffer v4lbuf;
    GstClockTime ts, res;

    v4l_src->count++;
    memset(&v4lbuf, 0, sizeof (v4lbuf));
    v4lbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    v4lbuf.memory = V4L2_MEMORY_MMAP;
    
    if (ioctl (v4l_src->fd_v4l, VIDIOC_DQBUF, &v4lbuf) < 0)	{
        GST_ERROR(">>V4L_SRC: VIDIOC_DQBUF failed.\n");
        return NULL;
    }
	
    buf = (GstBuffer *)(v4l_src->buffers[v4lbuf.index]);
    GST_BUFFER_FLAG_SET (buf, GST_BUFFER_FLAG_LAST);
    gst_buffer_ref(buf);

    if (ioctl (v4l_src->fd_v4l, VIDIOC_QBUF, &v4lbuf) < 0) {
        GST_ERROR(">>V4L_SRC: VIDIOC_QBUF failed\n");
        return NULL;
    }  

    GST_BUFFER_SIZE (buf) = v4l_src->buffer_size;

    ts = gst_clock_get_time (GST_ELEMENT (v4l_src)->clock);
    if (ts != GST_CLOCK_TIME_NONE)
       ts -= gst_element_get_base_time(GST_ELEMENT (v4l_src));
    else 
       ts = v4l_src->count * v4l_src->time_per_frame;
    GST_BUFFER_TIMESTAMP(buf) = ts;
    GST_BUFFER_DURATION (buf) = v4l_src->time_per_frame;

    if (v4l_src->last_ts)
    {
        guint num_frame_delay = 0;
        GstClockTimeDiff diff= ts - v4l_src->last_ts;
        if (ts < v4l_src->last_ts)
            diff = v4l_src->last_ts + ts;
        while (diff > v4l_src->time_per_frame)
        {
            diff -= v4l_src->time_per_frame;
            num_frame_delay++;
        }
        if (num_frame_delay > 1)
            GST_DEBUG(">>V4L_SRC: Camera ts late by %d frames\n", num_frame_delay);
    }
    v4l_src->last_ts = ts;

    gst_buffer_set_caps (buf, GST_PAD_CAPS (GST_BASE_SRC_PAD (v4l_src)));
    return buf;
}


/*=============================================================================
FUNCTION:            mfw_gst_v4lsrc_create
         
DESCRIPTION:         This function is registered with the Base Source Class 
                     This function updates the the buffer to be pushed to the
                     next element with the frame captured.
                     
ARGUMENTS PASSED:    v4l_src     - 
        
  
RETURN VALUE:        
              GST_FLOW_OK       -    buffer create successfull.
              GST_FLOW_ERROR    -    Error in buffer creation.
        
PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
static GstFlowReturn mfw_gst_v4lsrc_create (GstPushSrc * src, GstBuffer ** buf)
{
  MFWGstV4LSrc *v4l_src = MFW_GST_V4LSRC(src);
  *buf = mfw_gst_v4lsrc_buffer_new (v4l_src);
  return GST_FLOW_OK;

}


/*=============================================================================
FUNCTION:            mfw_gst_v4lsrc_get_caps
         
DESCRIPTION:         This function gets the caps to be set on the source pad.
                     

ARGUMENTS PASSED:    
        v4l_src     - 
         
RETURN VALUE:       Returns the caps to be set.
        
PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
static GstCaps *mfw_gst_v4lsrc_get_caps (GstBaseSrc * src)
{ 
    GstCaps *list;
    MFWGstV4LSrc *v4l_src =	MFW_GST_V4LSRC(src);
    GstCaps *capslist;
    GstPadTemplate *src_template = NULL;
    gint i;
#ifndef MX51    
    guint32 format = GST_MAKE_FOURCC('I', '4', '2', '0');
#else
    guint32 format = GST_MAKE_FOURCC('N', 'V', '1', '2');
#endif

    capslist = gst_caps_new_empty();

    gst_caps_append_structure(capslist,
				  gst_structure_new("video/x-raw-yuv",
						    "format",
						    GST_TYPE_FOURCC,
						    format, "width",
						    GST_TYPE_INT_RANGE, 16,
						    1768, "height",
						    GST_TYPE_INT_RANGE, 16,
						    1168, "framerate",
						    GST_TYPE_FRACTION_RANGE,
						    0, 1, 100, 1,"pixel-aspect-ratio",
						    GST_TYPE_FRACTION_RANGE,
						    0, 1, 100, 1, NULL));

    

    return capslist;
}


/*=============================================================================
FUNCTION:            mfw_gst_v4lsrc_fixate
         
DESCRIPTION:         Fixes the Caps on the source pad
                     

ARGUMENTS PASSED:    v4l_src     - 
        
RETURN VALUE:        None
PRE-CONDITIONS:      None
POST-CONDITIONS:     None
IMPORTANT NOTES:     None
=============================================================================*/
static void mfw_gst_v4lsrc_fixate(GstPad * pad, GstCaps * caps)
{
  
    gint i=0;
    GstStructure *structure=NULL;
    MFWGstV4LSrc *v4l_src =	
        MFW_GST_V4LSRC(gst_pad_get_parent (pad));
#ifndef MX51    
    guint32 fourcc = GST_MAKE_FOURCC('I', '4', '2', '0');
#else
    guint32 fourcc = GST_MAKE_FOURCC('N', 'V', '1', '2');
#endif

    const GValue *v=NULL;
    for (i = 0; i < gst_caps_get_size (caps); ++i) {
        structure = gst_caps_get_structure (caps, i);
        gst_structure_fixate_field_nearest_int (structure, "width", 
            v4l_src->capture_width);
        gst_structure_fixate_field_nearest_int (structure, "height",
            v4l_src->capture_height);
        gst_structure_fixate_field_nearest_fraction (structure, "framerate", 
            v4l_src->fps_n,v4l_src->fps_d);
        gst_structure_fixate_field_nearest_fraction (structure, 
            "pixel-aspect-ratio", 1,1);

        gst_structure_set (structure, "format", GST_TYPE_FOURCC, fourcc, NULL);
    }
    gst_object_unref(v4l_src);

}
/*=============================================================================
FUNCTION:   mfw_gst_v4lsrc_init   
        
DESCRIPTION:     create the pad template that has been registered with the 
                element class in the _base_init and do library table 
                initialization      

ARGUMENTS PASSED:
        context  -    pointer to v4lsrc element structure      
  
RETURN VALUE:       None
      
PRE-CONDITIONS:     _base_init and _class_init are called 
 
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=============================================================================*/
static void mfw_gst_v4lsrc_init(MFWGstV4LSrc * v4l_src, MFWGstV4LSrcClass * klass)
{
    v4l_src->capture_width=176;
    v4l_src->capture_height=144;
    v4l_src->fps_n=30;
    v4l_src->fps_d=1;
    v4l_src->fd_v4l=-1;
    v4l_src->count=0;
    v4l_src->buffer_size=0;
    v4l_src->offset=0;
    v4l_src->crop_pixel=0;
    v4l_src->rotate=0;
    v4l_src->preview=FALSE;
    v4l_src->preview_width=160;
    v4l_src->preview_height=128;
    v4l_src->preview_top=0;
    v4l_src->preview_left=0;
    v4l_src->sensor_width=1280;
    v4l_src->sensor_height=1024;
    v4l_src->capture_mode = 0;
    v4l_src->bg = FALSE;
#ifdef MX27
    v4l_src->devicename = g_strdup ("/dev/v4l/video0");
#else
    v4l_src->devicename = g_strdup ("/dev/video0");
#endif 

    gst_pad_set_fixatecaps_function (GST_BASE_SRC_PAD (v4l_src),
                                  mfw_gst_v4lsrc_fixate);
    gst_base_src_set_live (GST_BASE_SRC (v4l_src), TRUE);
#define MFW_GST_V4LSRC_PLUGIN VERSION
    PRINT_PLUGIN_VERSION(MFW_GST_V4LSRC_PLUGIN);
    return;
}

/*=============================================================================
FUNCTION:   mfw_gst_v4lsrc_class_init    
        
DESCRIPTION:     Initialise the class only once (specifying what signals,
                arguments and virtual functions the class has and setting up 
                global state)    
     

ARGUMENTS PASSED:
       klass   -   pointer to mp3decoder element class
        
RETURN VALUE:        None
PRE-CONDITIONS:      None
POST-CONDITIONS:     None
IMPORTANT NOTES:     None
=============================================================================*/
static void mfw_gst_v4lsrc_class_init(MFWGstV4LSrcClass * klass)
{

    GObjectClass *gobject_class;
    GstBaseSrcClass *basesrc_class;
    GstPushSrcClass *pushsrc_class;
    
    gobject_class = (GObjectClass *) klass;
    basesrc_class = (GstBaseSrcClass *) klass;
    pushsrc_class = (GstPushSrcClass *) klass;


    gobject_class->set_property =   mfw_gst_v4lsrc_set_property;
    gobject_class->get_property =   mfw_gst_v4lsrc_get_property;

    g_object_class_install_property(gobject_class, MFW_V4L_SRC_WIDTH,
				    g_param_spec_int("capture-width",
						     "capture_width",
						     "gets the width of the image to be captured",
						     16, 1768, 176,
						     G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, MFW_V4L_SRC_HEIGHT,
				    g_param_spec_int("capture-height",
						     "capture_height",
						     "gets the height of the image to be captured",
						     16, 1168, 144,
						     G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, MFW_V4L_SRC_PREVIEW_WIDTH,
				    g_param_spec_int("preview-width",
						     "preview_width",
						     "gets the width of the image to be displayed for preview. \n"
                             "\t\t\tNote:property is valid only when preview property is enabled",
						     16, 1768, 176,
						     G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, MFW_V4L_SRC_PREVIEW_HEIGHT,
				    g_param_spec_int("preview-height",
						     "preview_height",
						     "gets the height of the image to be displayed for preview. \n"
                             "\t\t\tNote:property is valid only when preview property is enabled",
						     16, 1168, 144,
						     G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, MFW_V4L_SRC_PREVIEW_TOP,
				    g_param_spec_int("preview-top",
						     "preview_top",
						     "gets the top pixel offset at which the preview should start. \n"
                             "\t\t\tNote:property is valid only when preview property is enabled",
						     0, 320, 0,
						     G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, MFW_V4L_SRC_PREVIEW_LEFT,
				    g_param_spec_int("preview-left",
						     "preview_left",
						     "gets the left pixel offset at which the preview should start. \n"
                             "\t\t\tNote:property is valid only when preview property is enabled",
						     0, 240, 0,
						     G_PARAM_READWRITE));



     g_object_class_install_property (gobject_class, MFW_V4L_SRC_PREVIEW,
				   g_param_spec_boolean ("preview", "Preview",
							 "enable the preview of capture",
							 FALSE,
							 G_PARAM_READWRITE));



    g_object_class_install_property(gobject_class, MFW_V4L_SRC_ROTATE,
				    g_param_spec_int("rotate",
						     "Rotate",
						     "gets the values by which the camera rotation angle can "   
                             "be specified. \n\t\t\tRotation angles "                       
                             "for different values are as follows: \n"                   
                             "\t\t\t\trotate=1:Vertical flip \n"                                
                             "\t\t\t\trotate=2:Horizontal flip \n"                              
                             "\t\t\t\trotate=3:180 degree rotation \n"                          
                             "\t\t\t\trotate=4:90 degree rotation clockwise \n"
                             "\t\t\t\trotate=5:90 degree rotation clockwise and vertical flip \n"
                             "\t\t\t\trotate=6:90 degree rotation clockwise and horizontal flip \n"
                             "\t\t\t\trotate=7:90 degree rotation counter-clockwise\n",
						     0, 7, 0,
						     G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, MFW_V4L_SRC_CROP_PIXEL,
				    g_param_spec_int("crop-by-pixel",
						     "crop_by_pixel",
						     "gets the number of pixels by which the image " 
                             "is to be cropped on either sides for capture ",
						     0, 320, 0,
						     G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, MFW_V4L_SRC_FRAMERATE_NUM,
				    g_param_spec_int("fps-n",
						       "fps_n",
						       "gets the numerator of the framerate at which" 
                               "the input stream is to be captured",
						       0,
						       G_MAXINT, 0,
						       G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, MFW_V4L_SRC_FRAMERATE_DEN,
				    g_param_spec_int("fps-d",
						       "fps_d",
						       "gets the denominator of the framerate at which" 
                               "the input stream is to be captured",
						       1,
						       G_MAXINT, 1,
						       G_PARAM_READWRITE));

     g_object_class_install_property(gobject_class, MFW_V4L_SRC_SENSOR_WIDTH,
				    g_param_spec_int("sensor-width",
						       "sensor_width",
						       "gets the width of the sensor", 
						       16,
						       G_MAXINT, 1280,
						       G_PARAM_READWRITE));
     g_object_class_install_property(gobject_class, MFW_V4L_SRC_SENSOR_HEIGHT,
				    g_param_spec_int("sensor-height",
						       "sensor_height",
						       "gets the height of the sensor", 
						       16,
						       G_MAXINT, 1024,
						       G_PARAM_READWRITE));

     g_object_class_install_property(gobject_class, MFW_V4L_SRC_CAPTURE_MODE,
				    g_param_spec_int("capture-mode",
						       "capture mode",
						       "set the capture mode of camera, please check the bsp release "
						       "notes to decide which value can be applied. ", 
						       0,
						       5/*for D1 support, the value should 5 */, 0,
						       G_PARAM_READWRITE));

     g_object_class_install_property (gobject_class, MFW_V4L_SRC_BACKGROUND,
				   g_param_spec_boolean ("bg", "BG display",
							 "Set BG display or FG display",
							 FALSE,
							 G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class, MFW_V4L_SRC_DEVICE,
                  g_param_spec_string ("device", "Device", "Device location",
                            NULL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
     
    basesrc_class->get_caps = mfw_gst_v4lsrc_get_caps;
    basesrc_class->set_caps = mfw_gst_v4lsrc_set_caps;
    basesrc_class->start =    mfw_gst_v4lsrc_start;
    basesrc_class->stop =     mfw_gst_v4lsrc_stop;
    pushsrc_class->create =   mfw_gst_v4lsrc_create;
    return;
}


/*=============================================================================
FUNCTION:   mfw_gst_v4lsrc_base_init   
        
DESCRIPTION:     v4l source element details are registered with the plugin during
                _base_init ,This function will initialise the class and child 
                class properties during each new child class creation       

ARGUMENTS PASSED:
        Klass   -   void pointer
  
RETURN VALUE:        None
PRE-CONDITIONS:      None
POST-CONDITIONS:     None
IMPORTANT NOTES:     None
=============================================================================*/
static void mfw_gst_v4lsrc_base_init(gpointer g_class)
{

    GstElementClass *element_class = GST_ELEMENT_CLASS(g_class);

    gst_element_class_set_details (element_class, &mfw_gst_v4lsrc_details);

    gst_element_class_add_pad_template (element_class,
      gst_pad_template_new ("src", GST_PAD_SRC, GST_PAD_ALWAYS,
          gst_caps_new_any ()));

    GST_DEBUG_CATEGORY_INIT(mfw_gst_v4lsrc_debug, "mfw_v4lsrc", 0,
			    "V4L video src element");

return;
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

PRE-CONDITIONS:      None
POST-CONDITIONS:     None
IMPORTANT NOTES:     None
=============================================================================*/
static gboolean plugin_init(GstPlugin * plugin)
{
    if (!gst_element_register(plugin, "mfw_v4lsrc", GST_RANK_PRIMARY,
			      MFW_GST_TYPE_V4LSRC))
	return FALSE;

    return TRUE;
}

/*****************************************************************************/
/*    This is used to define the entry point and meta data of plugin         */
/*****************************************************************************/

GST_PLUGIN_DEFINE(GST_VERSION_MAJOR,	/* major version of Gstreamer */
		  GST_VERSION_MINOR,	/* minor version of Gstreamer    */
		  "mfw_v4lsrc",	/* name of the plugin            */
		  "Video Source plug in based on V4L2",	/* what plugin actually does     */
		  plugin_init,	/* first function to be called   */
		  VERSION,
		  GST_LICENSE_UNKNOWN,
		  FSL_GST_MM_PLUGIN_PACKAGE_NAME, FSL_GST_MM_PLUGIN_PACKAGE_ORIG)
