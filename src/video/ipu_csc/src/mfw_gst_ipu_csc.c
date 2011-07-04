/*
 * Copyright (C) 2010,2011 Freescale Semiconductor, Inc. All rights reserved.
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
 * Module Name:    mfw_gst_ipu_csc.c
 *
 * Description:
 *
 * Portability:    This code is written for Linux OS and Gstreamer
 */

/*
 * Changelog:
 * Jan 07 2010 Guo Yue <B13906@freescale.com>
 * - Initial version
 */

/*=============================================================================
                            INCLUDE FILES
=============================================================================*/
#include <gst/gst.h>
#include <string.h>
#include "mfw_gst_utils.h"
#ifdef MEMORY_DEBUG
#include "mfw_gst_debug.h"
#endif
//#include "src_ppp_interface.h"  /*fsl src ppp*/
#include <fcntl.h>              /* fcntl */
#include <sys/mman.h>           /* mmap  */
#include <sys/ioctl.h>          /* ioctl */
#include "gstbufmeta.h"

// core ipu library

typedef unsigned char       uint8_t;
typedef unsigned short      uint16_t;
typedef unsigned int        uint32_t;

#include <linux/mxcfb.h>
#include "mxc_ipu_hl_lib.h"

#include "mfw_gst_ipu_csc.h"

#define IPU_PHYSICAL_OUTPUT_BUFFER
#define WALKAROUND_YUY2_YUYV
#define TIME_PROFILE


#define MEMORY_DEVICE_NAME "/dev/mxc_ipu"

#define DEFAULT_OUTPUT_WIDTH 0//640
#define DEFAULT_OUTPUT_HEIGHT 0//480
//#define DEFAULT_OUTPUT_FORMAT 0x30323449 //"I420"
//#define DEFAULT_OUTPUT_CSTYPE CSTYPE_YUV //YUV
#define DEFAULT_OUTPUT_FORMAT 0x50424752 //"RGBP"
#define DEFAULT_OUTPUT_CSTYPE CSTYPE_RGB //RGB

#define CAPS_CSTYPE_YUV "video/x-raw-yuv"
#define CAPS_CSTYPE_RGB "video/x-raw-rgb"
#define CAPS_CSTYPE_GRAY "video/x-raw-gray"

#define IS_DMABLE_BUFFER(buffer) ( (GST_IS_BUFFER_META(buffer->_gst_reserved[G_N_ELEMENTS(buffer->_gst_reserved)-1])) \
                                 || ( GST_IS_BUFFER(buffer) \
                                 &&  GST_BUFFER_FLAG_IS_SET((buffer),GST_BUFFER_FLAG_LAST)))
#define DMABLE_BUFFER_PHY_ADDR(buffer) (GST_IS_BUFFER_META(buffer->_gst_reserved[G_N_ELEMENTS(buffer->_gst_reserved)-1]) ? \
                                        ((GstBufferMeta *)(buffer->_gst_reserved[G_N_ELEMENTS(buffer->_gst_reserved)-1]))->physical_data : \
                                        GST_BUFFER_OFFSET(buffer))

/*=============================================================================
                            LOCAL CONSTANTS
=============================================================================*/
/* None. */

/*=============================================================================
                LOCAL TYPEDEFS (STRUCTURES, UNIONS, ENUMS)
=============================================================================*/
/* None. */

/*=============================================================================
                                        LOCAL MACROS
=============================================================================*/
#define MFW_GST_IPU_CSC_CAPS    \
    "video/x-raw-yuv"

#ifdef MEMORY_DEBUG
    static Mem_Mgr mem_mgr = {0};

#define IPU_CSC_MALLOC( size)\
        dbg_malloc((&mem_mgr),(size), "line" STR(__LINE__) "of" STR(__FUNCTION__) )
#define IPU_CSC_FREE( ptr)\
        dbg_free(&mem_mgr, (ptr), "line" STR(__LINE__) "of" STR(__FUNCTION__) )

#else
#define IPU_CSC_MALLOC(size)\
        g_malloc((size))
#define IPU_CSC_FREE( ptr)\
        g_free((ptr))

#endif

#define IPU_CSC_FATAL_ERROR(...) g_print(RED_STR(__VA_ARGS__))
#define IPU_CSC_FLOW(...) g_print(BLUE_STR(__VA_ARGS__))
#define IPU_CSC_FLOW_DEFAULT IPU_CSC_FLOW("%s:%d\n", __FUNCTION__, __LINE__)

#define	GST_CAT_DEFAULT	mfw_gst_ipucsc_debug

/*=============================================================================
                                      LOCAL VARIABLES
=============================================================================*/

/*=============================================================================
                        LOCAL FUNCTION PROTOTYPES
=============================================================================*/
GST_DEBUG_CATEGORY_STATIC(mfw_gst_ipucsc_debug);

static void	mfw_gst_ipu_csc_class_init(MfwGstIPUCSCClass * klass);
static void	mfw_gst_ipu_csc_base_init(MfwGstIPUCSCClass * klass);
static void	mfw_gst_ipu_csc_init(MfwGstIPUCSC *filter);

//static void	mfw_gst_ipu_csc_set_property(GObject *object, guint prop_id,
//                                         const GValue *value, GParamSpec *pspec);
//static void	mfw_gst_ipu_csc_get_property(GObject *object, guint prop_id,
//                                         GValue *value, GParamSpec *pspec);

static gboolean mfw_gst_ipu_csc_set_caps (GstBaseTransform * btrans,
    GstCaps * incaps, GstCaps * outcaps);
static gboolean mfw_gst_ipu_csc_get_unit_size (GstBaseTransform * btrans,
    GstCaps * caps, guint * size);
static GstFlowReturn mfw_gst_ipu_csc_transform (GstBaseTransform * btrans,
    GstBuffer * inbuf, GstBuffer * outbuf);
#if 0
static GstFlowReturn mfw_gst_ipu_csc_transform_ip (GstBaseTransform * btrans,
    GstBuffer * inbuf);
#endif
static GstFlowReturn mfw_gst_ipu_csc_chain (GstPad * pad, GstBuffer * buffer);

static void mfw_gst_ipu_csc_fixate_caps (GstBaseTransform * base,
    GstPadDirection direction, GstCaps * caps, GstCaps * othercaps);

/*=============================================================================
                            GLOBAL VARIABLES
=============================================================================*/
/* elementfactory information */
static GstElementDetails mfw_ipu_csc_details =
    GST_ELEMENT_DETAILS ("Freescale IPU Color Space Converter",
    "Filter/Effect/Video",
    "IPU Color Space Converter",
    FSL_GST_MM_PLUGIN_AUTHOR) ;

static GstPadTemplate *sinktempl, *srctempl;
static GstElementClass *parent_class = NULL;


/* copies the given caps */
static GstCaps *
mfw_gst_ipu_csc_caps_remove_format_info (GstPadDirection direction, GstCaps * caps)
{
  int i;
  GstStructure *structure;
  GstCaps *rgbcaps;
  GstCaps *graycaps;

  caps = gst_caps_copy (caps);

  for (i = 0; i < gst_caps_get_size (caps); i++) {
    structure = gst_caps_get_structure (caps, i);

    gst_structure_set_name (structure, "video/x-raw-yuv");
    gst_structure_remove_field (structure, "format");
    gst_structure_remove_field (structure, "endianness");
    gst_structure_remove_field (structure, "depth");
    gst_structure_remove_field (structure, "bpp");
    gst_structure_remove_field (structure, "red_mask");
    gst_structure_remove_field (structure, "green_mask");
    gst_structure_remove_field (structure, "blue_mask");
    gst_structure_remove_field (structure, "alpha_mask");
    gst_structure_remove_field (structure, "palette_data");
    /* Remove width and height to let ipucsc plugin support resize function */
    gst_structure_remove_field (structure, "width");
    gst_structure_remove_field (structure, "height");
  }

  gst_caps_do_simplify (caps);
  rgbcaps = gst_caps_copy (caps);

  for (i = 0; i < gst_caps_get_size (rgbcaps); i++) {
    structure = gst_caps_get_structure (rgbcaps, i);

    gst_structure_set_name (structure, "video/x-raw-rgb");
  }
  graycaps = gst_caps_copy (caps);

  for (i = 0; i < gst_caps_get_size (graycaps); i++) {
    structure = gst_caps_get_structure (graycaps, i);

    gst_structure_set_name (structure, "video/x-raw-gray");
  }

  gst_caps_append (caps, graycaps);
  gst_caps_append (caps, rgbcaps);

  return caps;
}

/* Convert a fourcc
 * to a GstCaps. If the context is ommitted, no fixed values
 * for video/audio size will be included in the GstCaps
 */
GstCaps *
mfw_gst_ipu_csc_codectype_to_caps (void)
{
  GstCaps *caps;
  GstCaps *temp;
  guint i;
  guint32 yuv_formats[] = {
      GST_MAKE_FOURCC('I', '4', '2', '0'),
      GST_MAKE_FOURCC('Y', 'V', '1', '2'),
      GST_MAKE_FOURCC('Y', 'U', 'Y', '2'),
      GST_MAKE_FOURCC('N', 'V', '1', '2')
  };
  guint32 rgb_formats[] = {
      GST_MAKE_FOURCC('R', 'G', 'B', 'P')
  };

  caps = gst_caps_new_empty ();
  for (i = 0; i < G_N_ELEMENTS(yuv_formats); i++) {
      temp = gst_caps_new_simple ("video/x-raw-yuv",
          "format", GST_TYPE_FOURCC,yuv_formats[i],
          "width", GST_TYPE_INT_RANGE, 1, G_MAXINT,
          "height", GST_TYPE_INT_RANGE, 1, G_MAXINT,
          "framerate", GST_TYPE_FRACTION_RANGE, 0, 1, G_MAXINT, 1, NULL);
      if (temp != NULL) {
          gst_caps_append (caps, temp);
      }
  }

  for (i = 0; i < G_N_ELEMENTS(rgb_formats); i++) {
      int bpp = 0, depth = 0, endianness = 0;
      gulong g_mask = 0, r_mask = 0, b_mask = 0, a_mask = 0;
      guint32 fmt = 0;
      switch(rgb_formats[i]){
      case GST_MAKE_FOURCC ('R', 'G', 'B', 'P'):
          bpp = depth = 16;
          endianness = G_BYTE_ORDER;
          r_mask = 0xf800;
          g_mask = 0x07e0;
          b_mask = 0x001f;
          break;

      default:
          break;
      }

      if (a_mask != 0) {
          temp = gst_caps_new_simple ("video/x-raw-rgb",
            "format", GST_TYPE_FOURCC,rgb_formats[i],
            "width", GST_TYPE_INT_RANGE, 1, G_MAXINT,
            "height", GST_TYPE_INT_RANGE, 1, G_MAXINT,
            "framerate", GST_TYPE_FRACTION_RANGE, 0, 1, G_MAXINT, 1,
            "bpp", G_TYPE_INT, bpp,
            "depth", G_TYPE_INT, depth,
            "red_mask", G_TYPE_INT, r_mask,
            "green_mask", G_TYPE_INT, g_mask,
            "blue_mask", G_TYPE_INT, b_mask,
            "alpha_mask", G_TYPE_INT, a_mask,
            "endianness", G_TYPE_INT, endianness, NULL);
      } else if (r_mask != 0) {
          temp = gst_caps_new_simple ("video/x-raw-rgb",
            "format", GST_TYPE_FOURCC,rgb_formats[i],
            "width", GST_TYPE_INT_RANGE, 1, G_MAXINT,
            "height", GST_TYPE_INT_RANGE, 1, G_MAXINT,
            "framerate", GST_TYPE_FRACTION_RANGE, 0, 1, G_MAXINT, 1,
            "bpp", G_TYPE_INT, bpp,
            "depth", G_TYPE_INT, depth,
            "red_mask", G_TYPE_INT, r_mask,
            "green_mask", G_TYPE_INT, g_mask,
            "blue_mask", G_TYPE_INT, b_mask,
            "endianness", G_TYPE_INT, endianness, NULL);
      } else {
          temp = gst_caps_new_simple ("video/x-raw-rgb",
            "format", GST_TYPE_FOURCC,rgb_formats[i],
            "width", GST_TYPE_INT_RANGE, 1, G_MAXINT,
            "height", GST_TYPE_INT_RANGE, 1, G_MAXINT,
            "framerate", GST_TYPE_FRACTION_RANGE, 0, 1, G_MAXINT, 1,
            "bpp", G_TYPE_INT, bpp,
            "depth", G_TYPE_INT, depth,
            "endianness", G_TYPE_INT, endianness, NULL);
      }

      if (temp != NULL) {
          gst_caps_append (caps, temp);
      }
  }

  return caps;
}

/* The caps can be transformed into any other caps with format info removed.
 * However, we should prefer passthrough, so if passthrough is possible,
 * put it first in the list. */
static GstCaps *
mfw_gst_ipu_csc_transform_caps (GstBaseTransform * btrans,
    GstPadDirection direction, GstCaps * caps)
{
  GstCaps *template;
  GstCaps *result;

  template = mfw_gst_ipu_csc_codectype_to_caps ();
  result = gst_caps_intersect (caps, template);
  gst_caps_unref (template);

  gst_caps_append (result, mfw_gst_ipu_csc_caps_remove_format_info (direction,caps));

  GST_DEBUG_OBJECT (btrans, "transformed %" GST_PTR_FORMAT " into %"
      GST_PTR_FORMAT, caps, result);

  return result;
}

gboolean core_ipu_library_init_io_parameter(MfwGstIPUCSC* filter, guint in_format, guint out_format, gint in_width, gint in_height, gint out_width, gint out_height)
{
    memset(&(filter->input), 0, sizeof(ipu_lib_input_param_t));
    memset(&(filter->output), 0, sizeof(ipu_lib_output_param_t));
    filter->mode = OP_NORMAL_MODE;
    memset(&(filter->ipu_handle), 0, sizeof(ipu_lib_handle_t));

    filter->input.width = in_width;
    filter->input.height = in_height;
    filter->input.fmt = in_format;
#ifdef WALKAROUND_YUY2_YUYV
    if( 0x32595559 == filter->input.fmt )
    {
        filter->input.fmt = 0x56595559;
    }
#endif
    filter->input.input_crop_win.pos.x = 0;
    filter->input.input_crop_win.pos.y = 0;
    filter->input.input_crop_win.win_w = filter->input.width;
    filter->input.input_crop_win.win_h = filter->input.height;

    filter->output.width = out_width;//filter->input.width;
    filter->output.height = out_height;//filter->input.height;
    filter->output.fmt = out_format;
    filter->output.rot = 0;
    filter->output.show_to_fb = 0; // Direct Rendering
    filter->output.fb_disp.pos.x = 0;
    filter->output.fb_disp.pos.y = 0;
    filter->output.fb_disp.fb_num = 0; // Must be 2???
    filter->output.output_win.pos.x = 0;
    filter->output.output_win.pos.y = 0;
    filter->output.output_win.win_w = filter->output.width;
    filter->output.output_win.win_h = filter->output.height;

    g_print("in_format=0x%x, in_width=%d, in_height=%d\n",in_format, in_width, in_height);
    g_print("out_format=0x%x, out_width=%d, out_height=%d\n",out_format, out_width, out_height);

    return TRUE;
}

static gboolean
mfw_gst_ipu_csc_set_caps (GstBaseTransform * btrans, GstCaps * incaps,
    GstCaps * outcaps)
{
  MfwGstIPUCSC *space;
  GstStructure *structure;
  gint in_height, in_width;
  gint out_height, out_width;
  const GValue *in_framerate = NULL;
  const GValue *out_framerate = NULL;
  const GValue *in_par = NULL;
  const GValue *out_par = NULL;
  gboolean res;
  guint in_format, out_format;
  gint crop_left = 0, crop_right = 0, crop_top = 0, crop_bottom = 0;

  g_print(RED_STR("incaps %s\n",gst_caps_to_string(incaps)));
  g_print(RED_STR("outcaps %s\n",gst_caps_to_string(outcaps)));

  space = MFW_GST_IPU_CSC (btrans);

  /* parse in and output values */
  structure = gst_caps_get_structure (incaps, 0);

  gst_structure_get_fourcc(structure, "format", &in_format);

  /* we have to have width and height */
  res = gst_structure_get_int (structure, "width", &in_width);
  res &= gst_structure_get_int (structure, "height", &in_height);
  if (!res)
    goto no_width_height;

  gst_structure_get_int (structure, "crop-left-by-pixel", &crop_left);
  gst_structure_get_int (structure, "crop-right-by-pixel", &crop_right);
  gst_structure_get_int (structure, "crop-top-by-pixel", &crop_top);
  gst_structure_get_int (structure, "crop-bottom-by-pixel", &crop_bottom);

  /* and framerate */
  in_framerate = gst_structure_get_value (structure, "framerate");
  if (in_framerate == NULL || !GST_VALUE_HOLDS_FRACTION (in_framerate))
    goto no_framerate;

  /* this is optional */
  in_par = gst_structure_get_value (structure, "pixel-aspect-ratio");

  structure = gst_caps_get_structure (outcaps, 0);

  gst_structure_get_fourcc(structure, "format", &out_format);

  /* we have to have width and height */
  res = gst_structure_get_int (structure, "width", &out_width);
  res &= gst_structure_get_int (structure, "height", &out_height);
  if (!res)
    goto no_width_height;

  /* and framerate */
  out_framerate = gst_structure_get_value (structure, "framerate");
  if (out_framerate == NULL || !GST_VALUE_HOLDS_FRACTION (out_framerate))
    goto no_framerate;

  /* this is optional */
  out_par = gst_structure_get_value (structure, "pixel-aspect-ratio");

  /* these must match */
  if (/*in_width != out_width || in_height != out_height ||*/
      gst_value_compare (in_framerate, out_framerate) != GST_VALUE_EQUAL)
    goto format_mismatch;

  /* if present, these must match too */
  if (in_par && out_par
      && gst_value_compare (in_par, out_par) != GST_VALUE_EQUAL)
    goto format_mismatch;

  space->input_width = in_width;
  space->input_height = in_height;

  space->interlaced = FALSE;
  gst_structure_get_boolean (structure, "interlaced", &space->interlaced);

  // Initial core ipu library;
  res = core_ipu_library_init_io_parameter(space, in_format, out_format, in_width+crop_left+crop_right,
                                    in_height+crop_top+crop_bottom, out_width, out_height);
  if (!res)
    goto core_ipu_library_init_io_parameter_failed;

  GST_DEBUG ("reconfigured %d %d", space->input_format, space->output_format);

  return TRUE;

  /* ERRORS */
no_width_height:
  {
    GST_DEBUG_OBJECT (space, "did not specify width or height");
    return FALSE;
  }
no_framerate:
  {
    GST_DEBUG_OBJECT (space, "did not specify framerate");
    return FALSE;
  }
format_mismatch:
  {
    GST_DEBUG_OBJECT (space, "input and output formats do not match");
    return FALSE;
  }
invalid_in_caps:
  {
    GST_DEBUG_OBJECT (space, "could not configure context for input format");
    return FALSE;
  }
invalid_out_caps:
  {
    GST_DEBUG_OBJECT (space, "could not configure context for output format");
    return FALSE;
  }
core_ipu_library_init_io_parameter_failed:
  {
    GST_DEBUG_OBJECT (space, "could not initialize the core ipu library IO parameter");
    return FALSE;
  }
}

GType
mfw_gst_ipu_csc_get_type (void)
{
  static GType mfw_ipu_csc_type = 0;

  if (!mfw_ipu_csc_type) {
    static const GTypeInfo mfw_ipu_csc_info = {
      sizeof (MfwGstIPUCSCClass),
      (GBaseInitFunc) mfw_gst_ipu_csc_base_init,
      NULL,
      (GClassInitFunc) mfw_gst_ipu_csc_class_init,
      NULL,
      NULL,
      sizeof (MfwGstIPUCSC),
      0,
      (GInstanceInitFunc) mfw_gst_ipu_csc_init,
    };

    mfw_ipu_csc_type = g_type_register_static (GST_TYPE_BASE_TRANSFORM,
        "MfwGstIPUCSC", &mfw_ipu_csc_info, 0);
  }

  return mfw_ipu_csc_type;
}

static void
mfw_gst_ipu_csc_base_init (MfwGstIPUCSCClass * klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

  gst_element_class_add_pad_template (element_class, srctempl);
  gst_element_class_add_pad_template (element_class, sinktempl);
  gst_element_class_set_details (element_class, &mfw_ipu_csc_details);
}

static void
mfw_gst_ipu_csc_finalize (GObject * obj)
{
  MfwGstIPUCSC *space = MFW_GST_IPU_CSC (obj);

  G_OBJECT_CLASS (parent_class)->finalize (obj);
}

static void
mfw_gst_ipu_csc_class_init (MfwGstIPUCSCClass * klass)
{
  GObjectClass *gobject_class;
  GstBaseTransformClass *gstbasetransform_class;

  gobject_class = (GObjectClass *) klass;
  gstbasetransform_class = (GstBaseTransformClass *) klass;

  parent_class = g_type_class_peek_parent (klass);

  gobject_class->finalize = GST_DEBUG_FUNCPTR (mfw_gst_ipu_csc_finalize);

  gstbasetransform_class->transform_caps =
      GST_DEBUG_FUNCPTR (mfw_gst_ipu_csc_transform_caps);
  gstbasetransform_class->set_caps = GST_DEBUG_FUNCPTR (mfw_gst_ipu_csc_set_caps);
  gstbasetransform_class->get_unit_size =
      GST_DEBUG_FUNCPTR (mfw_gst_ipu_csc_get_unit_size);
  gstbasetransform_class->transform =
      GST_DEBUG_FUNCPTR (mfw_gst_ipu_csc_transform);
#if 0
  gstbasetransform_class->transform_ip =
      GST_DEBUG_FUNCPTR (mfw_gst_ipu_csc_transform_ip);
#endif
  gstbasetransform_class->fixate_caps = GST_DEBUG_FUNCPTR (mfw_gst_ipu_csc_fixate_caps);

  gstbasetransform_class->passthrough_on_same_caps = TRUE;

  GST_DEBUG_CATEGORY_INIT(mfw_gst_ipucsc_debug, "mfw_ipucsc",
              0, "FreeScale's IPU Color Space Converter Gst Plugin's Log");
}

static void
mfw_gst_ipu_csc_init (MfwGstIPUCSC * space)
{
  GstBaseTransform * trans = (GstBaseTransform*)space;
  //gst_pad_set_chain_function (trans->sinkpad,
  //    GST_DEBUG_FUNCPTR (mfw_gst_ipu_csc_transform));

  gst_base_transform_set_qos_enabled (GST_BASE_TRANSFORM (space), TRUE);

#define MFW_GST_IPU_CSC_PLUGIN VERSION
      PRINT_CORE_VERSION("IPU_CSC_CORE_LIBRARY_VERSION_INFOR_01.00");
      PRINT_PLUGIN_VERSION(MFW_GST_IPU_CSC_PLUGIN);
}

gint
picture_get_size (guint32 format, int width, int height)
{
    gint framesize = 0;
    switch(format){
    case GST_MAKE_FOURCC ('I', '4', '2', '0'):
    case GST_MAKE_FOURCC ('N', 'V', '1', '2'):
    case GST_MAKE_FOURCC ('Y', 'V', '1', '2'):
        framesize = width*height*3/2;
        break;
    case GST_MAKE_FOURCC ('Y', 'U', 'Y', '2'):
        framesize = width*height*2;
        break;
    case GST_MAKE_FOURCC ('R', 'G', 'B', 'P'):
        framesize = width*height*2;
        break;
    default:
        break;
    }
    return framesize;
}

static gboolean
mfw_gst_ipu_csc_get_unit_size (GstBaseTransform * btrans, GstCaps * caps,
    guint * size)
{
  GstStructure *structure = NULL;
  gboolean ret = TRUE;
  gint width, height;
  guint32 format = 0;
  gint crop_left = 0, crop_right = 0, crop_top = 0, crop_bottom = 0;

  MfwGstIPUCSC* filter = (MfwGstIPUCSC*)btrans;

  g_assert (size);

  structure = gst_caps_get_structure (caps, 0);
  gst_structure_get_int (structure, "width", &width);
  gst_structure_get_int (structure, "height", &height);
  gst_structure_get_int (structure, "crop-left-by-pixel", &crop_left);
  gst_structure_get_int (structure, "crop-right-by-pixel", &crop_right);
  gst_structure_get_int (structure, "crop-top-by-pixel", &crop_top);
  gst_structure_get_int (structure, "crop-bottom-by-pixel", &crop_bottom);

  gst_structure_get_fourcc(structure, "format", &format);
  *size = picture_get_size (format, width+crop_left+crop_right,
                            crop_top+height+crop_bottom);
  //filter->input.fmt = format;

  g_print(RED_STR("%s:size=%d\n", __FUNCTION__, *size));

  return ret;
}

#if 0
/* FIXME: Could use transform_ip to implement endianness swap type operations */
static GstFlowReturn
mfw_gst_ipu_csc_transform_ip (GstBaseTransform * btrans, GstBuffer * inbuf)
{
  /* do nothing */
  return GST_FLOW_OK;
}
#endif

void ipu_output_cb(void * arg, int index)
{
    return;
}

gint core_ipu_library_convert_InHard_OutHard (GstBaseTransform * btrans, GstBuffer * inbuf,
    GstBuffer * outbuf)
{
    MfwGstIPUCSC *filter;
    gint result;
    int ret_ipu_lib = 0;
    int next_update_idx = 0;

    static int times = 0;

    filter = MFW_GST_IPU_CSC (btrans);

    //filter->input.user_def_paddr[0] = (dma_addr_t)(GST_BUFFER_OFFSET(inbuf));
    //filter->output.user_def_paddr[0] = (int)(GST_BUFFER_OFFSET(outbuf));
    filter->input.user_def_paddr[0] = DMABLE_BUFFER_PHY_ADDR(inbuf);
    filter->output.user_def_paddr[0] = DMABLE_BUFFER_PHY_ADDR(outbuf);

    ret_ipu_lib = mxc_ipu_lib_task_init(&(filter->input), NULL, &(filter->output), filter->mode, &(filter->ipu_handle));
    if( 0 != ret_ipu_lib )
    {
        GST_ERROR("Failed mxc_ipu_lib_task_init(): ret_ipu_lib=%d\n", ret_ipu_lib);
        return GST_FLOW_ERROR;
    }
    next_update_idx = mxc_ipu_lib_task_buf_update(&(filter->ipu_handle), NULL, 0, 0, NULL, NULL);
    if (next_update_idx < 0)
    {
        GST_ERROR("Failed mxc_ipu_lib_task_buf_update(): next_update_idx=%d\n", next_update_idx);
        return GST_FLOW_ERROR;
    }
    mxc_ipu_lib_task_uninit( &(filter->ipu_handle) );

    return 0;
}

gint core_ipu_library_convert_InSoft_OutSoft (GstBaseTransform * btrans, GstBuffer * inbuf,
    GstBuffer * outbuf)
{
    MfwGstIPUCSC *filter;
    gint result;
    int ret_ipu_lib = 0;
    int next_update_idx = 0;

    filter = MFW_GST_IPU_CSC (btrans);

    filter->input.user_def_paddr[0]= 0;
    filter->output.user_def_paddr[0] = 0;
    ret_ipu_lib = mxc_ipu_lib_task_init(&(filter->input), NULL, &(filter->output), filter->mode, &(filter->ipu_handle));
    if( 0 != ret_ipu_lib )
    {
        GST_ERROR("Failed mxc_ipu_lib_task_init(): ret_ipu_lib=%d\n", ret_ipu_lib);
        return GST_FLOW_ERROR;
    }

    memcpy(filter->ipu_handle.inbuf_start[0], GST_BUFFER_DATA(inbuf), GST_BUFFER_SIZE(inbuf));
    next_update_idx = mxc_ipu_lib_task_buf_update(&(filter->ipu_handle), filter->input.user_def_paddr[0], 0, 0, NULL, NULL);
    if (next_update_idx < 0)
    {
        GST_ERROR("Failed mxc_ipu_lib_task_buf_update(): next_update_idx=%d\n", next_update_idx);
        return GST_FLOW_ERROR;
    }
    memcpy(GST_BUFFER_DATA(outbuf), filter->ipu_handle.outbuf_start[0], filter->ipu_handle.ofr_size);

    mxc_ipu_lib_task_uninit( &(filter->ipu_handle) );

    return 0;
}

gint core_ipu_library_convert_InSoft_OutHard (GstBaseTransform * btrans, GstBuffer * inbuf,
    GstBuffer * outbuf)
{
    MfwGstIPUCSC *filter;
    gint result;
    int ret_ipu_lib = 0;
    int next_update_idx = 0;

    filter = MFW_GST_IPU_CSC (btrans);

    // FIXME
    filter->input.user_def_paddr[0]= 0;
    filter->output.user_def_paddr[0] = DMABLE_BUFFER_PHY_ADDR(outbuf);

    ret_ipu_lib = mxc_ipu_lib_task_init(&(filter->input), NULL, &(filter->output), filter->mode, &(filter->ipu_handle));
    if( 0 != ret_ipu_lib )
    {
        GST_ERROR("Failed mxc_ipu_lib_task_init(): ret_ipu_lib=%d\n", ret_ipu_lib);
        return GST_FLOW_ERROR;
    }

    memcpy(filter->ipu_handle.inbuf_start[0], GST_BUFFER_DATA(inbuf), GST_BUFFER_SIZE(inbuf));
    next_update_idx = mxc_ipu_lib_task_buf_update(&(filter->ipu_handle), filter->input.user_def_paddr[0], 0, 0, NULL, NULL);
    if (next_update_idx < 0)
    {
        GST_ERROR("Failed mxc_ipu_lib_task_buf_update(): next_update_idx=%d\n", next_update_idx);
        return GST_FLOW_ERROR;
    }

    mxc_ipu_lib_task_uninit( &(filter->ipu_handle) );

    return 0;
}

gint core_ipu_library_convert_InHard_OutSoft (GstBaseTransform * btrans, GstBuffer * inbuf,
    GstBuffer * outbuf)
{
    MfwGstIPUCSC *filter;
    gint result;
    int ret_ipu_lib = 0;
    int next_update_idx = 0;

    filter = MFW_GST_IPU_CSC (btrans);

    //filter->input.user_def_paddr[0] = (dma_addr_t)(GST_BUFFER_OFFSET(inbuf));
    filter->input.user_def_paddr[0] = DMABLE_BUFFER_PHY_ADDR(inbuf);
    filter->output.user_def_paddr[0] = 0;
    ret_ipu_lib = mxc_ipu_lib_task_init(&(filter->input), NULL, &(filter->output), filter->mode, &(filter->ipu_handle));
    if( 0 != ret_ipu_lib )
    {
        GST_ERROR("Failed mxc_ipu_lib_task_init(): ret_ipu_lib=%d\n", ret_ipu_lib);
        return GST_FLOW_ERROR;
    }

    next_update_idx = mxc_ipu_lib_task_buf_update(&(filter->ipu_handle), filter->input.user_def_paddr[0], 0, 0, NULL, NULL);
    if (next_update_idx < 0)
    {
        GST_ERROR("Failed mxc_ipu_lib_task_buf_update(): next_update_idx=%d\n", next_update_idx);
        return GST_FLOW_ERROR;
    }
    memcpy(GST_BUFFER_DATA(outbuf), filter->ipu_handle.outbuf_start[0], filter->ipu_handle.ofr_size);

    ipu_output_cb((void*)filter, 0);

    mxc_ipu_lib_task_uninit( &(filter->ipu_handle) );

    return 0;
}

static GstFlowReturn
mfw_gst_ipu_csc_transform (GstBaseTransform * btrans, GstBuffer * inbuf,
    GstBuffer * outbuf)
{
  MfwGstIPUCSC *filter;
  gint result;
  guint input_buffer_flag = IS_DMABLE_BUFFER(inbuf);
  guint output_buffer_flag = IS_DMABLE_BUFFER(outbuf);
  int ret_ipu_lib = 0;
  int next_update_idx = 0;

  filter = MFW_GST_IPU_CSC (btrans);
  GST_DEBUG("csc method:%d x %d\n",input_buffer_flag, output_buffer_flag);
  if( (0==input_buffer_flag) && (0==output_buffer_flag) )
  {// in software buffer, out software buffer
      result = core_ipu_library_convert_InSoft_OutSoft(btrans, inbuf, outbuf);
      if (result == -1)
        goto not_supported;
  }
  else if( (0!=input_buffer_flag) && (0!=output_buffer_flag) )
  {// in hardware buffer, out hardware buffer
      result = core_ipu_library_convert_InHard_OutHard(btrans, inbuf, outbuf);
      if (result == -1)
        goto not_supported;
  }
  else if( (0==input_buffer_flag) && (0!=output_buffer_flag) )
  {// in software buffer, out hardware buffer
      result = core_ipu_library_convert_InSoft_OutHard(btrans, inbuf, outbuf);
      if (result == -1)
        goto not_supported;
  }
  else if( (0!=input_buffer_flag) && (0==output_buffer_flag) )
  {// in hardware buffer, out software buffer
      result = core_ipu_library_convert_InSoft_OutSoft(btrans, inbuf, outbuf);
      if (result == -1)
        goto not_supported;
  }

  /* baseclass copies timestamps */
  GST_DEBUG ("from %d -> to %d done", filter->input_format, filter->output_format);

  return GST_FLOW_OK;

  /* ERRORS */
unknown_format:
  {
    GST_ELEMENT_ERROR (filter, CORE, NOT_IMPLEMENTED, (NULL),
        ("attempting to convert colorspaces between unknown formats"));
    return GST_FLOW_NOT_NEGOTIATED;
  }
not_supported:
  {
    GST_ELEMENT_ERROR (filter, CORE, NOT_IMPLEMENTED, (NULL),
        ("cannot convert between formats"));
    return GST_FLOW_NOT_SUPPORTED;
  }
}

struct _GstBaseTransformPrivate
{
  /* QoS *//* with LOCK */
  gboolean qos_enabled;
  gdouble proportion;
  GstClockTime earliest_time;
  /* previous buffer had a discont */
  gboolean discont;

  GstActivateMode pad_mode;

  gboolean gap_aware;

  /* caps used for allocating buffers */
  gboolean proxy_alloc;
  GstCaps *sink_alloc;
  GstCaps *src_alloc;
  /* upstream caps and size suggestions */
  GstCaps *sink_suggest;
  guint size_suggest;
  gboolean suggest_pending;

  gboolean reconfigure;
};

static GstFlowReturn
mfw_gst_ipu_csc_chain (GstPad * pad, GstBuffer * buffer)
{
  GstBaseTransform *trans;
  GstBaseTransformClass *klass;
  GstFlowReturn ret;
  GstClockTime last_stop = GST_CLOCK_TIME_NONE;
  GstBuffer *outbuf = NULL;

  trans = GST_BASE_TRANSFORM (GST_OBJECT_PARENT (pad));

  g_print (RED_STR("%s\n", __FUNCTION__));

  /* calculate end position of the incoming buffer */
  if (GST_BUFFER_TIMESTAMP (buffer) != GST_CLOCK_TIME_NONE) {
    if (GST_BUFFER_DURATION (buffer) != GST_CLOCK_TIME_NONE)
      last_stop = GST_BUFFER_TIMESTAMP (buffer) + GST_BUFFER_DURATION (buffer);
    else
      last_stop = GST_BUFFER_TIMESTAMP (buffer);
  }

  klass = GST_BASE_TRANSFORM_GET_CLASS (trans);
  if (klass->before_transform)
    klass->before_transform (trans, buffer);

  /* protect transform method and concurrent buffer alloc */
  GST_BASE_TRANSFORM_LOCK (trans);
  ret = gst_base_transform_handle_buffer (trans, buffer, &outbuf);
  GST_BASE_TRANSFORM_UNLOCK (trans);

  /* outbuf can be NULL, this means a dropped buffer, if we have a buffer but
   * GST_BASE_TRANSFORM_FLOW_DROPPED we will not push either. */
  if (outbuf != NULL) {
    if ((ret == GST_FLOW_OK)) {
      /* Remember last stop position */
      if ((last_stop != GST_CLOCK_TIME_NONE) &&
          (trans->segment.format == GST_FORMAT_TIME))
        gst_segment_set_last_stop (&trans->segment, GST_FORMAT_TIME, last_stop);

      /* apply DISCONT flag if the buffer is not yet marked as such */
      if (trans->priv->discont) {
        if (!GST_BUFFER_IS_DISCONT (outbuf)) {
          outbuf = gst_buffer_make_metadata_writable (outbuf);
          GST_BUFFER_FLAG_SET (outbuf, GST_BUFFER_FLAG_DISCONT);
        }
        trans->priv->discont = FALSE;
      }
      ret = gst_pad_push (trans->srcpad, outbuf);

      // for ipu csc library.
      {
          MfwGstIPUCSC *filter;
          filter = MFW_GST_IPU_CSC (trans);
          //mxc_ipu_lib_task_uninit( &(filter->ipu_handle) );
      }
    } else
      gst_buffer_unref (outbuf);
  }

  /* convert internal flow to OK and mark discont for the next buffer. */
  if (ret == GST_BASE_TRANSFORM_FLOW_DROPPED) {
    trans->priv->discont = TRUE;
    ret = GST_FLOW_OK;
  }

  return ret;
}

gboolean
gst_video_calculate_display_ratio_csc (guint * dar_n, guint * dar_d,
    guint video_width, guint video_height,
    guint video_par_n, guint video_par_d,
    guint display_par_n, guint display_par_d)
{
  gint num, den;

  GValue display_ratio = { 0, };
  GValue tmp = { 0, };
  GValue tmp2 = { 0, };

  g_return_val_if_fail (dar_n != NULL, FALSE);
  g_return_val_if_fail (dar_d != NULL, FALSE);

  g_value_init (&display_ratio, GST_TYPE_FRACTION);
  g_value_init (&tmp, GST_TYPE_FRACTION);
  g_value_init (&tmp2, GST_TYPE_FRACTION);

  /* Calculate (video_width * video_par_n * display_par_d) /
   * (video_height * video_par_d * display_par_n) */
  gst_value_set_fraction (&display_ratio, video_width, video_height);
  gst_value_set_fraction (&tmp, video_par_n, video_par_d);

  if (!gst_value_fraction_multiply (&tmp2, &display_ratio, &tmp))
    goto error_overflow;

  gst_value_set_fraction (&tmp, display_par_d, display_par_n);

  if (!gst_value_fraction_multiply (&display_ratio, &tmp2, &tmp))
    goto error_overflow;

  num = gst_value_get_fraction_numerator (&display_ratio);
  den = gst_value_get_fraction_denominator (&display_ratio);

  g_value_unset (&display_ratio);
  g_value_unset (&tmp);
  g_value_unset (&tmp2);

  g_return_val_if_fail (num > 0, FALSE);
  g_return_val_if_fail (den > 0, FALSE);

  *dar_n = num;
  *dar_d = den;

  return TRUE;
error_overflow:
  g_value_unset (&display_ratio);
  g_value_unset (&tmp);
  g_value_unset (&tmp2);
  return FALSE;
}

static void
mfw_gst_ipu_csc_fixate_caps (GstBaseTransform * base, GstPadDirection direction,
    GstCaps * caps, GstCaps * othercaps)
{
  GstStructure *ins, *outs;
  const GValue *from_par, *to_par;

  g_return_if_fail (gst_caps_is_fixed (caps));

  GST_DEBUG_OBJECT (base, "trying to fixate othercaps %" GST_PTR_FORMAT
      " based on caps %" GST_PTR_FORMAT, othercaps, caps);

  ins = gst_caps_get_structure (caps, 0);
  outs = gst_caps_get_structure (othercaps, 0);

  from_par = gst_structure_get_value (ins, "pixel-aspect-ratio");
  to_par = gst_structure_get_value (outs, "pixel-aspect-ratio");

  /* we have both PAR but they might not be fixated */
  if (from_par && to_par) {
    gint from_w, from_h, from_par_n, from_par_d, to_par_n, to_par_d;

    gint count = 0, w = 0, h = 0;

    guint num, den;

    /* from_par should be fixed */
    g_return_if_fail (gst_value_is_fixed (from_par));

    from_par_n = gst_value_get_fraction_numerator (from_par);
    from_par_d = gst_value_get_fraction_denominator (from_par);

    /* fixate the out PAR */
    if (!gst_value_is_fixed (to_par)) {
      GST_DEBUG_OBJECT (base, "fixating to_par to %dx%d", from_par_n,
          from_par_d);
      gst_structure_fixate_field_nearest_fraction (outs, "pixel-aspect-ratio",
          from_par_n, from_par_d);
    }

    to_par_n = gst_value_get_fraction_numerator (to_par);
    to_par_d = gst_value_get_fraction_denominator (to_par);

    /* if both width and height are already fixed, we can't do anything
     * about it anymore */
    if (gst_structure_get_int (outs, "width", &w))
      ++count;
    if (gst_structure_get_int (outs, "height", &h))
      ++count;
    if (count == 2) {
      GST_DEBUG_OBJECT (base, "dimensions already set to %dx%d, not fixating",
          w, h);
      return;
    }

    gst_structure_get_int (ins, "width", &from_w);
    gst_structure_get_int (ins, "height", &from_h);

    if (!gst_video_calculate_display_ratio_csc (&num, &den, from_w, from_h,
            from_par_n, from_par_d, to_par_n, to_par_d)) {
      GST_ELEMENT_ERROR (base, CORE, NEGOTIATION, (NULL),
          ("Error calculating the output scaled size - integer overflow"));
      return;
    }

    GST_DEBUG_OBJECT (base,
        "scaling input with %dx%d and PAR %d/%d to output PAR %d/%d",
        from_w, from_h, from_par_n, from_par_d, to_par_n, to_par_d);
    GST_DEBUG_OBJECT (base, "resulting output should respect ratio of %d/%d",
        num, den);

    /* now find a width x height that respects this display ratio.
     * prefer those that have one of w/h the same as the incoming video
     * using wd / hd = num / den */

    /* if one of the output width or height is fixed, we work from there */
    if (h) {
      GST_DEBUG_OBJECT (base, "height is fixed,scaling width");
      w = (guint) gst_util_uint64_scale_int (h, num, den);
    } else if (w) {
      GST_DEBUG_OBJECT (base, "width is fixed, scaling height");
      h = (guint) gst_util_uint64_scale_int (w, den, num);
    } else {
      /* none of width or height is fixed, figure out both of them based only on
       * the input width and height */
      /* check hd / den is an integer scale factor, and scale wd with the PAR */
      if (from_h % den == 0) {
        GST_DEBUG_OBJECT (base, "keeping video height");
        h = from_h;
        w = (guint) gst_util_uint64_scale_int (h, num, den);
      } else if (from_w % num == 0) {
        GST_DEBUG_OBJECT (base, "keeping video width");
        w = from_w;
        h = (guint) gst_util_uint64_scale_int (w, den, num);
      } else {
        GST_DEBUG_OBJECT (base, "approximating but keeping video height");
        h = from_h;
        w = (guint) gst_util_uint64_scale_int (h, num, den);
      }
    }
    GST_DEBUG_OBJECT (base, "scaling to %dx%d", w, h);

    /* now fixate */
    gst_structure_fixate_field_nearest_int (outs, "width", w);
    gst_structure_fixate_field_nearest_int (outs, "height", h);
  } else {
    gint width, height;

    if (gst_structure_get_int (ins, "width", &width)) {
      if (gst_structure_has_field (outs, "width")) {
        gst_structure_fixate_field_nearest_int (outs, "width", width);
      }
    }
    if (gst_structure_get_int (ins, "height", &height)) {
      if (gst_structure_has_field (outs, "height")) {
        gst_structure_fixate_field_nearest_int (outs, "height", height);
      }
    }
  }

  GST_DEBUG_OBJECT (base, "fixated othercaps to %" GST_PTR_FORMAT, othercaps);
}

static gboolean
plugin_init (GstPlugin *plugin)
{
    GstCaps *caps;

    /* template caps */
    caps = mfw_gst_ipu_csc_codectype_to_caps ();

    /* build templates */
    srctempl = gst_pad_template_new ("src",
        GST_PAD_SRC, GST_PAD_ALWAYS, gst_caps_copy (caps));

    /* the sink template will do palette handling as well... */
    sinktempl = gst_pad_template_new ("sink", GST_PAD_SINK, GST_PAD_ALWAYS, caps);

    return gst_element_register (plugin, "mfw_ipucsc",
                GST_RANK_PRIMARY,
                MFW_GST_TYPE_IPU_CSC);
}

GST_PLUGIN_DEFINE (
    GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    "mfw_ipucsc",
    "Freescale IPU Color Space Converter",
    plugin_init,
    VERSION,
    GST_LICENSE_UNKNOWN,
    FSL_GST_MM_PLUGIN_PACKAGE_NAME, FSL_GST_MM_PLUGIN_PACKAGE_ORIG)

