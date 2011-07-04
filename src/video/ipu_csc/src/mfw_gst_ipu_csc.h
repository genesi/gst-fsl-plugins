/*
 * Copyright (C) 2010 Freescale Semiconductor, Inc. All rights reserved.
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
 * Module Name:    mfw_gst_ipu_csc.h
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
#ifndef __MFW_GST_IPU_CSC_H__
#define __MFW_GST_IPU_CSC_H__

#include <gst/gst.h>
#include <gst/base/gstadapter.h>
#include <gst/base/gstbasetransform.h>

/*=============================================================================
                                           CONSTANTS
=============================================================================*/

/*=============================================================================
                                             ENUMS
=============================================================================*/
/* plugin property ID */
enum{
    CSTYPE_YUV = 1,
    CSTYPE_RGB = 2,
    CSTYPE_GRAY = 3
};

enum{
    PROPER_ID_OUTPUT_WIDTH = 1,
    PROPER_ID_OUTPUT_HEIGHT = 2,
    PROPER_ID_OUTPUT_FORMAT = 3,
    PROPER_ID_OUTPUT_CSTYPE = 4
};

/*=============================================================================
                                            MACROS
=============================================================================*/

G_BEGIN_DECLS

#define MFW_GST_TYPE_IPU_CSC \
    (mfw_gst_ipu_csc_get_type())
#define MFW_GST_IPU_CSC(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj),MFW_GST_TYPE_IPU_CSC, MfwGstIPUCSC))
#define MFW_GST_IPU_CSC_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass),MFW_GST_TYPE_IPU_CSC, MfwGstIPUCSCClass))
#define MFW_GST_IS_IPU_CSC(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj),MFW_GST_TYPE_IPU_CSC))
#define MFW_GST_IS_IPU_CSC_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass),MFW_GST_TYPE_IPU_CSC))

/*=============================================================================
                                 STRUCTURES AND OTHER TYPEDEFS
=============================================================================*/

typedef struct _MfwGstIPUCSC
{
    GstBaseTransform element;

    // Add members for csc
    gint fd_ipu;
    ipu_lib_input_param_t input;
    ipu_lib_output_param_t output;
    int mode;
    ipu_lib_handle_t ipu_handle;

    ipu_mem_info output_ipu_mem;

    guint input_framesize;
    gint input_width;
    gint input_height;
    guint input_format;
    gint input_cstype;
    
    guint output_framesize;
    gint output_width;
    gint output_height;
    guint output_format;
    gint output_cstype;

    gboolean interlaced;

    gboolean bpassthrough;

}MfwGstIPUCSC;

typedef struct _MfwGstIPUCSCClass 
{
    GstBaseTransformClass parent_class;
}MfwGstIPUCSCClass;

/*=============================================================================
                                 GLOBAL VARIABLE DECLARATIONS
=============================================================================*/
/* None. */

/*=============================================================================
                                     FUNCTION PROTOTYPES
=============================================================================*/
GType mfw_gst_ipu_csc_get_type (void);

G_END_DECLS

#endif /* __MFW_GST_IPU_CSC_H__ */
