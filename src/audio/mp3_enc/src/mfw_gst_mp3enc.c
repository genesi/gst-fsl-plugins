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
 * Module Name:    mfw_gst_mp3enc.c
 *
 * Description:    Implementation of mp3 plugin for Gstreamer using push
 *                 based method.
 *
 * Portability:    This code is written for Linux OS and Gstreamer
 */  
 
/*
 * Changelog: 
 *
 */


/*=======================================================================================
                            INCLUDE FILES
========================================================================================*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/gst.h>
#include <string.h>

#include "mfw_gst_mp3enc.h"

#include "mfw_gst_utils.h"

/*==================================================================================================
                                        LOCAL CONSTANTS
==================================================================================================*/

/* None. */

/*==================================================================================================
                          STATIC TYPEDEFS (STRUCTURES, UNIONS, ENUMS)
==================================================================================================*/
/* Filter signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

enum
{
  ARG_0,
  ARG_SAMPLERATE,
  ARG_BITRATE,
  ARG_CHANNEL,
  ARG_INPUTFMT,
  ARG_OPTMOD
};

static GstStaticPadTemplate mfw_gst_mp3enc_sink_factory =
GST_STATIC_PAD_TEMPLATE ("sink",
			 GST_PAD_SINK,
			 GST_PAD_ALWAYS,
			 GST_STATIC_CAPS ("audio/x-raw-int,"
					  "endianness = (int) { "
					  G_STRINGIFY (G_BYTE_ORDER) " }, "
					  "signed = (boolean) { TRUE, FALSE }, "
					  "width = (int) 16, "
					  "depth = (int) 16, "
					  "rate = (int) { 32000,44100,48000}, "
					  "channels = (int) [ 1, 2 ] "));

static GstStaticPadTemplate mfw_gst_mp3enc_src_factory =
GST_STATIC_PAD_TEMPLATE ("src",
			 GST_PAD_SRC,
			 GST_PAD_ALWAYS,
			 GST_STATIC_CAPS ("audio/mpeg, "
					  "mpegversion = (int) 1, "
					  "layer = (int)3, "
					  "rate = (int) {32000, 44100, 48000 }, "
					  "channels = (int)2"));

/*==================================================================================================
                                        LOCAL MACROS
==================================================================================================*/

#define DEFAULT_SAMPLERATE	44100
#define DEFAULT_BITRATE		128
#define DEFAULT_CHANNELS	2

#define FMT_INTERLEAVED		0
#define FMT_CONTIGUOUS		1
#define DEFAULT_INPUTFMT	FMT_INTERLEAVED
#define OPTSPEED		0
#define OPTQUALITY		1
#define DEFAULT_OPTMOD		OPTQUALITY

#define NUM_SAMPLES 1152	/* 1152 samples per channel */



GST_DEBUG_CATEGORY_STATIC (mfw_gst_mp3enc_debug);
#define GST_CAT_DEFAULT mfw_gst_mp3enc_debug


/*==================================================================================================
                                      STATIC VARIABLES
==================================================================================================*/

/*==================================================================================================
                                 STATIC FUNCTION PROTOTYPES
==================================================================================================*/

GST_BOILERPLATE (MfwGstMp3EncInfo, mfw_gst_mp3enc, GstElement,
		 GST_TYPE_ELEMENT);



static void mfw_gst_mp3enc_set_property (GObject * object, guint prop_id,
					 const GValue * value,
					 GParamSpec * pspec);
static void mfw_gst_mp3enc_get_property (GObject * object, guint prop_id,
					 GValue * value, GParamSpec * pspec);

static gboolean mfw_gst_mp3enc_set_caps (GstPad * pad, GstCaps * caps);
static gboolean mfw_gst_mp3enc_sink_event (GstPad * pad, GstEvent * event);
static GstStateChangeReturn mfw_gst_mp3enc_change_state (GstElement * element,
							 GstStateChange
							 transition);
static GstFlowReturn mfw_gst_mp3enc_encode_frame (MfwGstMp3EncInfo * mp3enc,
						  gboolean eos);
static GstFlowReturn mfw_gst_mp3enc_chain (GstPad * pad, GstBuffer * buf);

/*==================================================================================================
                                     GLOBAL VARIABLES
==================================================================================================*/


/*==================================================================================================
                                     LOCAL FUNCTIONS
==================================================================================================*/





void encoder_mem_info_alloc (MfwGstMp3EncInfo * mp3enc)
{

    MP3E_Encoder_Config *enc_config = &mp3enc->enc_config;
    int instance_id = enc_config->instance_id;

    mp3enc->W1[instance_id] = (char *)g_malloc(sizeof(char)*enc_config->mem_info[0].size);       
    enc_config->mem_info[0].ptr =  
        (int*)((unsigned int )(&mp3enc->W1[instance_id][0] + 
                   enc_config->mem_info[0].align - 1 )
                   & (0xffffffff ^ (enc_config->mem_info[0].align - 1 )));

    mp3enc->W2[instance_id] = (char *)g_malloc(sizeof(char)*enc_config->mem_info[1].size);
    enc_config->mem_info[1].ptr =  
        (int *)((unsigned int )(&mp3enc->W2[instance_id][0] 
                                + enc_config->mem_info[1].align - 1 )
    			& (0xffffffff ^ (enc_config->mem_info[1].align - 1 )));

    mp3enc->W3[instance_id] = (char *)g_malloc(sizeof(char)*enc_config->mem_info[2].size);
    enc_config->mem_info[2].ptr =  
        (int *)((unsigned int )(&mp3enc->W3[instance_id][0] 
                                + enc_config->mem_info[2].align - 1 ) 
    			& (0xffffffff ^ (enc_config->mem_info[2].align - 1 )));

    mp3enc->W4[instance_id] = (char *)g_malloc(sizeof(char)*enc_config->mem_info[3].size);
    enc_config->mem_info[3].ptr =  
        (int *)((unsigned int )(&mp3enc->W4[instance_id][0] 
                                + enc_config->mem_info[3].align - 1 )
    			& (0xffffffff ^ (enc_config->mem_info[3].align - 1 )));

    mp3enc->W5[instance_id] = (char *)g_malloc(sizeof(char)*enc_config->mem_info[4].size);
    enc_config->mem_info[4].ptr =  
        (int *)((unsigned int )(&mp3enc->W5[instance_id][0] 
                                + enc_config->mem_info[4].align - 1 )
    			& (0xffffffff ^ (enc_config->mem_info[4].align - 1 )));

    mp3enc->W6[instance_id] = (char *)g_malloc(sizeof(char)*enc_config->mem_info[5].size);
    enc_config->mem_info[5].ptr =  
        (int *)((unsigned int )(&mp3enc->W6[instance_id][0] 
                                + enc_config->mem_info[5].align - 1 ) 
    			& (0xffffffff ^ (enc_config->mem_info[5].align - 1 )));

}

void encoder_mem_info_free (MfwGstMp3EncInfo * mp3enc)
{

    MP3E_Encoder_Config *enc_config = &mp3enc->enc_config;
    int instance_id = enc_config->instance_id;

    g_free(mp3enc->W1[instance_id]);
    g_free(mp3enc->W2[instance_id]);
    g_free(mp3enc->W3[instance_id]);
    g_free(mp3enc->W4[instance_id]);
    g_free(mp3enc->W5[instance_id]);
    g_free(mp3enc->W6[instance_id]);
}


static void
mfw_gst_mp3enc_base_init (gpointer gclass)
{
  static GstElementDetails element_details = {
    "freescale mp3 encoder",
    "codec/Encoder/audio",
    "Freescale mp3 encoder Element",
    FSL_GST_MM_PLUGIN_AUTHOR
  };
  GstElementClass *element_class = GST_ELEMENT_CLASS (gclass);

  gst_element_class_add_pad_template (element_class,
				      gst_static_pad_template_get
				      (&mfw_gst_mp3enc_src_factory));
  gst_element_class_add_pad_template (element_class,
				      gst_static_pad_template_get
				      (&mfw_gst_mp3enc_sink_factory));
  gst_element_class_set_details (element_class, &element_details);
}

/* initialize the plugin's class */
static void
mfw_gst_mp3enc_class_init (MfwGstMp3EncInfoClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  gobject_class->set_property = mfw_gst_mp3enc_set_property;
  gobject_class->get_property = mfw_gst_mp3enc_get_property;
  gstelement_class->change_state = mfw_gst_mp3enc_change_state;

  parent_class = g_type_class_ref (GST_TYPE_ELEMENT);

  /*ENGR39447:set caps when data come but not by property */
  g_object_class_install_property (gobject_class, ARG_SAMPLERATE,
				   g_param_spec_int ("sample_rate",
						     "Sample_rate",
						     "Input sample rate(default 16000)",
						     16000, 48000,
						     DEFAULT_SAMPLERATE,
						     G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, ARG_BITRATE,
				   g_param_spec_int ("bitrate", "Bitrate",
						     "Encode bitrate in kbps",
						     32, 320, DEFAULT_BITRATE,
						     G_PARAM_READWRITE));
  /*ENGR39447:set caps when data come but not by property */
  g_object_class_install_property (gobject_class, ARG_CHANNEL,
				   g_param_spec_int ("channels", "Channels",
						     "Input channels(default 2)",
						     1, 2, DEFAULT_CHANNELS,
						     G_PARAM_READABLE));
  g_object_class_install_property (gobject_class, ARG_INPUTFMT,
				   g_param_spec_int ("inputfmt", "Inputfmt",
						     "Input data format, L/R interleaved or contiguous(default interleaved)",
						     0, 1, DEFAULT_INPUTFMT,
						     G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, ARG_OPTMOD,
				   g_param_spec_int ("optmod", "Optmod",
						     "Encoder optimized mode, for speed or quality(default quality)",
						     0, 1, DEFAULT_OPTMOD,
						     G_PARAM_READWRITE));
}

/* initialize the new element
 * instantiate pads and add them to element
 * set functions
 * initialize structure
 */
static void
mfw_gst_mp3enc_init (MfwGstMp3EncInfo * mp3enc,
		     MfwGstMp3EncInfoClass * gclass)
{
  GstElementClass *klass = GST_ELEMENT_GET_CLASS (mp3enc);

  mp3enc->sinkpad =
    gst_pad_new_from_template (gst_element_class_get_pad_template (klass,
								   "sink"),
			       "sink");
  gst_pad_set_setcaps_function (mp3enc->sinkpad, mfw_gst_mp3enc_set_caps);

/*ENGR39447:no need get caps func*/

  mp3enc->srcpad =
    gst_pad_new_from_template (gst_element_class_get_pad_template (klass,
								   "src"),
			       "src");
  gst_pad_set_setcaps_function (mp3enc->srcpad, mfw_gst_mp3enc_set_caps);

  gst_element_add_pad (GST_ELEMENT (mp3enc), mp3enc->sinkpad);
  gst_element_add_pad (GST_ELEMENT (mp3enc), mp3enc->srcpad);
  gst_pad_set_chain_function (mp3enc->sinkpad, mfw_gst_mp3enc_chain);
  gst_pad_set_event_function (mp3enc->sinkpad,
			      GST_DEBUG_FUNCPTR (mfw_gst_mp3enc_sink_event));

  mp3enc->params.app_sampling_rate = DEFAULT_SAMPLERATE;
  mp3enc->params.app_bit_rate = DEFAULT_BITRATE;
  mp3enc->channels = DEFAULT_CHANNELS;
  mp3enc->inputfmt = DEFAULT_INPUTFMT;
  mp3enc->optmod = DEFAULT_OPTMOD;
  mp3enc->params.app_mode =
    ((mp3enc->channels % 2) & 0x3) + ((mp3enc->inputfmt & 0x3) << 8) +
    ((mp3enc->optmod & 0x3) << 16);

  /*ENGR39447:set caps when data come but not by property */
  mp3enc->encinit = FALSE;

#define MFW_GST_MP3_ENCODER_PLUGIN VERSION    
  PRINT_CORE_VERSION(MP3ECodecVersionInfo());
  PRINT_PLUGIN_VERSION(MFW_GST_MP3_ENCODER_PLUGIN);
  INIT_DEMO_MODE(MP3ECodecVersionInfo(), mp3enc->demo_mode);

}

static void
mfw_gst_mp3enc_set_property (GObject * object, guint prop_id,
			     const GValue * value, GParamSpec * pspec)
{
  MfwGstMp3EncInfo *mp3enc = MFW_GST_MP3ENC (object);

  switch (prop_id)
    {
      /*ENGR39447:set caps when data come but not by property */
    case ARG_BITRATE:
      mp3enc->params.app_bit_rate = g_value_get_int (value);
      break;
      /*ENGR39447:set caps when data come but not by property */
    case ARG_INPUTFMT:
      mp3enc->inputfmt = g_value_get_int (value);
      break;
    case ARG_OPTMOD:
      mp3enc->optmod = g_value_get_int (value);
      break;
    case ARG_SAMPLERATE:
      mp3enc->params.app_sampling_rate = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
mfw_gst_mp3enc_get_property (GObject * object, guint prop_id,
			     GValue * value, GParamSpec * pspec)
{
  MfwGstMp3EncInfo *mp3enc = MFW_GST_MP3ENC (object);

  switch (prop_id)
    {
    case ARG_SAMPLERATE:
      g_value_set_int (value, mp3enc->params.app_sampling_rate);
      break;
    case ARG_BITRATE:
      g_value_set_int (value, mp3enc->params.app_bit_rate);
      break;
    case ARG_CHANNEL:
      g_value_set_int (value, mp3enc->channels);
      break;
    case ARG_INPUTFMT:
      g_value_set_int (value, mp3enc->inputfmt);
      break;
    case ARG_OPTMOD:
      g_value_set_int (value, mp3enc->optmod);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

/* GstElement vmethod implementations */

/* this function handles the link with other elements */
static gboolean
mfw_gst_mp3enc_set_caps (GstPad * pad, GstCaps * caps)
{
  MfwGstMp3EncInfo *mp3enc;

  mp3enc = MFW_GST_MP3ENC (GST_OBJECT_PARENT (pad));

  return gst_pad_set_caps (pad, caps);
}

/*ENGR39447:no need get caps func*/

static gboolean
mfw_gst_mp3enc_sink_event (GstPad * pad, GstEvent * event)
{
  MfwGstMp3EncInfo *mp3enc;
  gboolean ret = TRUE;
  GstBuffer *outbuf = NULL;
  guint8 *outdata = NULL;
  mp3enc = MFW_GST_MP3ENC (GST_OBJECT_PARENT (pad));

  switch (GST_EVENT_TYPE (event))
    {
    case GST_EVENT_EOS:
      {
	while (mp3enc->encbuf != NULL)
	  {
        if (mp3enc->demo_mode == 2)
            return GST_FLOW_ERROR;
	    mfw_gst_mp3enc_encode_frame (mp3enc, TRUE);
	  }
    if (mp3enc->demo_mode == 2)
        return GST_FLOW_ERROR;
	ret =
	  gst_pad_alloc_buffer_and_set_caps (mp3enc->srcpad,
					     GST_BUFFER_OFFSET_NONE,
					     mp3enc->params.mp3e_outbuf_size,
					     GST_PAD_CAPS (mp3enc->srcpad),
					     &outbuf);
	if (ret != GST_FLOW_OK)
	  {
	    GST_WARNING ("Can not allocate buffer from next element!\n");
	    return GST_FLOW_ERROR;
	  }
	outdata = GST_BUFFER_DATA (outbuf);

	mp3e_flush_bitstream (&mp3enc->enc_config, outdata);
	GST_BUFFER_TIMESTAMP (outbuf) = mp3enc->fr_pts;
	GST_DEBUG ("time stamp is %lld\n", mp3enc->fr_pts);
	gst_pad_push (mp3enc->srcpad, outbuf);

	gst_pad_push_event (mp3enc->srcpad, event);
	GST_DEBUG ("Mp3enc EOS event pushed!\n");
	break;
      }
    default:
      {
	ret = gst_pad_event_default (pad, event);
	break;
      }
    }

  return ret;
}


static GstStateChangeReturn
mfw_gst_mp3enc_change_state (GstElement * element, GstStateChange transition)
{
  GstStateChangeReturn ret;
  MfwGstMp3EncInfo *mp3enc = MFW_GST_MP3ENC (element);

  switch (transition)
    {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      {
	/*ENGR39447:set caps when data come but not by property */
	break;
      }
    }

  ret = parent_class->change_state (element, transition);

  switch (transition)
    {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
	{
        encoder_mem_info_free(mp3enc);
        mp3enc->encinit = FALSE;
        break;
    }
    case GST_STATE_CHANGE_READY_TO_NULL:
      break;
    }

  return ret;
}


static GstFlowReturn
mfw_gst_mp3enc_encode_frame (MfwGstMp3EncInfo * mp3enc, gboolean eos)
{
  GstBuffer *outbuf;
  GstFlowReturn ret;
  guint8 *outdata;
  guint64 duration;
  GstCaps *srccaps;

  srccaps = GST_PAD_CAPS (mp3enc->srcpad);

  mp3enc->fr_pts = GST_BUFFER_TIMESTAMP (mp3enc->encbuf);
  /* push the filled output buf  to src pad */
  ret =
    gst_pad_alloc_buffer_and_set_caps (mp3enc->srcpad, GST_BUFFER_OFFSET_NONE,
				       mp3enc->params.mp3e_outbuf_size,
				       srccaps, &outbuf);
  if (ret != GST_FLOW_OK)
    {
      GST_WARNING ("Can not allocate buffer from next element!\n");
      return GST_FLOW_ERROR;
    }
  outdata = GST_BUFFER_DATA (outbuf);
  duration =
    gst_util_uint64_scale_int (NUM_SAMPLES, GST_SECOND,
			       mp3enc->params.app_sampling_rate);

    DEMO_LIVE_CHECK(mp3enc->demo_mode, 
        mp3enc->fr_pts, 
        mp3enc->srcpad);
    if (mp3enc->demo_mode == 2)
            return GST_FLOW_OK;

  if (mp3enc->channels == 1)	//mono,a frame is 16bit*NUM_SAMPLES
{
      MP3E_INT16 *inbuf;

      if (GST_BUFFER_SIZE (mp3enc->encbuf) >= NUM_SAMPLES * 2)	//Fill right channel with zero and to encode
	{
	  inbuf = (MP3E_INT16 *) (GST_BUFFER_DATA (mp3enc->encbuf));
	  mp3e_encode_frame (inbuf, &mp3enc->enc_config, outdata);
	  GST_BUFFER_TIMESTAMP (outbuf) = mp3enc->fr_pts;
	  GST_BUFFER_DURATION (outbuf) = duration;
	  GST_BUFFER_SIZE (outbuf) = mp3enc->enc_config.num_bytes;

	  GST_DEBUG ("time stamp is %lld, output bytes=%d\n", mp3enc->fr_pts,GST_BUFFER_SIZE(outbuf));
	  if (GST_BUFFER_SIZE (outbuf) > 0)
	    gst_pad_push (mp3enc->srcpad, outbuf);
	  else
	    gst_buffer_unref (outbuf);

	  if (GST_BUFFER_SIZE (mp3enc->encbuf) > NUM_SAMPLES * 2)
	    {
	      GstBuffer *tmpbuf;
	      tmpbuf =
		gst_buffer_create_sub (mp3enc->encbuf, NUM_SAMPLES * 2,
				       GST_BUFFER_SIZE (mp3enc->encbuf) -
				       NUM_SAMPLES * 2);
	      gst_buffer_unref (mp3enc->encbuf);
	      mp3enc->encbuf = tmpbuf;
	      GST_BUFFER_TIMESTAMP (mp3enc->encbuf) =
		mp3enc->fr_pts + duration;
	    }
	  else
	    {
	      gst_buffer_unref (mp3enc->encbuf);
	      mp3enc->encbuf = NULL;
	    }
	}
      else
	{
	  if (eos)
	    {
	      gint i;
	      MP3E_INT16 inbuf[NUM_SAMPLES * 2] = { 0 };

	      for (i = 0; i < GST_BUFFER_SIZE (mp3enc->encbuf) / 2; i++)
		{
		  inbuf[i] =
		    ((MP3E_INT16 *) GST_BUFFER_DATA (mp3enc->encbuf))[i];
		}

	      mp3e_encode_frame (inbuf, &mp3enc->enc_config, outdata);
	      GST_BUFFER_TIMESTAMP (outbuf) = mp3enc->fr_pts;
	      GST_BUFFER_DURATION (outbuf) = duration;
              GST_BUFFER_SIZE (outbuf) = mp3enc->enc_config.num_bytes;
              GST_DEBUG ("time stamp is %lld, output bytes=%d\n", mp3enc->fr_pts,GST_BUFFER_SIZE(outbuf));
	      if (GST_BUFFER_SIZE (outbuf) > 0)
		gst_pad_push (mp3enc->srcpad, outbuf);
	      else
		gst_buffer_unref (outbuf);

	      gst_buffer_unref (mp3enc->encbuf);
	      mp3enc->encbuf = NULL;
	    }
	}
    }
  else				//stereo, a frame is 16bit*2*NUM_SAMPLES
    {
      MP3E_INT16 *inbuf;

      if (GST_BUFFER_SIZE (mp3enc->encbuf) >= NUM_SAMPLES * 2 * 2)	//To encode directly
	{
	  inbuf = (MP3E_INT16 *) (GST_BUFFER_DATA (mp3enc->encbuf));
	  mp3e_encode_frame (inbuf, &mp3enc->enc_config, outdata);
	  GST_BUFFER_TIMESTAMP (outbuf) = mp3enc->fr_pts;
	  GST_BUFFER_DURATION (outbuf) = duration;
	  GST_BUFFER_SIZE (outbuf) = mp3enc->enc_config.num_bytes;

	  GST_DEBUG ("time stamp is %lld,size=%d\n", mp3enc->fr_pts,
		     GST_BUFFER_SIZE (outbuf));
	  if (GST_BUFFER_SIZE (outbuf) > 0)
	    gst_pad_push (mp3enc->srcpad, outbuf);
	  else
	    gst_buffer_unref (outbuf);


	  if (GST_BUFFER_SIZE (mp3enc->encbuf) > NUM_SAMPLES * 2 * 2)
	    {
	      GstBuffer *tmpbuf;
	      tmpbuf =
		gst_buffer_create_sub (mp3enc->encbuf, NUM_SAMPLES * 2 * 2,
				       GST_BUFFER_SIZE (mp3enc->encbuf) -
				       NUM_SAMPLES * 2 * 2);
	      gst_buffer_unref (mp3enc->encbuf);
	      mp3enc->encbuf = tmpbuf;
	      GST_BUFFER_TIMESTAMP (mp3enc->encbuf) =
		mp3enc->fr_pts + duration;
	    }
	  else
	    {
	      gst_buffer_unref (mp3enc->encbuf);
	      mp3enc->encbuf = NULL;
	    }
	}
      else
	{
	  if (eos)
	    {
	      gint i;
	      MP3E_INT16 inbuf[NUM_SAMPLES * 2] = { 0 };

	      for (i = 0; i < GST_BUFFER_SIZE (mp3enc->encbuf) / 2; i++)
		{
		  inbuf[i] =
		    ((MP3E_INT16 *) GST_BUFFER_DATA (mp3enc->encbuf))[i];
		}

	      mp3e_encode_frame (inbuf, &mp3enc->enc_config, outdata);
	      GST_BUFFER_TIMESTAMP (outbuf) = mp3enc->fr_pts;
	      GST_BUFFER_DURATION (outbuf) = duration;
              GST_BUFFER_SIZE (outbuf) = mp3enc->enc_config.num_bytes;

	      GST_DEBUG ("time stamp is %lld,size=%d\n", mp3enc->fr_pts,
			 GST_BUFFER_SIZE (outbuf));
	      if (GST_BUFFER_SIZE (outbuf) > 0)
		gst_pad_push (mp3enc->srcpad, outbuf);
	      else
		gst_buffer_unref (outbuf);

	      gst_buffer_unref (mp3enc->encbuf);
	      mp3enc->encbuf = NULL;
	    }
	}
    }

  return GST_FLOW_OK;
}

/* chain function
 * this function does the actual processing
 */

static GstFlowReturn
mfw_gst_mp3enc_chain (GstPad * pad, GstBuffer * buf)
{
  MfwGstMp3EncInfo *mp3enc;

  mp3enc = MFW_GST_MP3ENC (GST_OBJECT_PARENT (pad));

  if (mp3enc->demo_mode == 2)
      return GST_FLOW_ERROR;


  /*ENGR39447:set caps when data come but not by property */
  if (FALSE == mp3enc->encinit)
    {
      GstCaps *caps;
      MP3E_RET_VAL val;
      GstStructure *structure = NULL;

      /*Set src pad caps according to buffer's setting */

      caps = GST_BUFFER_CAPS (buf);
      structure = gst_caps_get_structure (caps, 0);
      gst_structure_get_int (structure, "rate",
			     &mp3enc->params.app_sampling_rate);

      gst_structure_get_int (structure, "channels", &mp3enc->channels);	//input channels
      caps = gst_caps_new_simple ("audio/mpeg", "mpegversion", G_TYPE_INT, 1, "layer", G_TYPE_INT, 3, "rate", G_TYPE_INT, mp3enc->params.app_sampling_rate, "channels", G_TYPE_INT, 2, NULL);	//output channels must be stereo

      gst_pad_set_caps (mp3enc->srcpad, caps);
      gst_caps_unref (caps);

      mp3enc->enc_config.instance_id = 0;

      /* memory setup */
      val = mp3e_query_mem (&mp3enc->enc_config);
      if (val != MP3E_SUCCESS)
	{
	  GST_ERROR ("Query memory failed\n");
	  return GST_FLOW_ERROR;
	}
      encoder_mem_info_alloc (mp3enc);

      /*update the app mode */
      mp3enc->params.app_mode = ((mp3enc->channels % 2) & 0x3) +
                            ((mp3enc->inputfmt & 0x3) << 8) + 
	                        ((mp3enc->optmod & 0x3) << 16);

      GST_DEBUG("app_mode = 0x%x \n",mp3enc->params.app_mode);
      GST_DEBUG("sample rate:%d.\n",mp3enc->params.app_sampling_rate);
      /* Initialize the encoder */
      val = mp3e_encode_init (&mp3enc->params, &mp3enc->enc_config);

      if (val != MP3E_SUCCESS)
	{
	  GST_ERROR ("Initalization failed,ret = %d\n", val);
	  return GST_FLOW_ERROR;
	}
      GST_DEBUG ("encoder sample_rate = %d,bitrate = %d,channels = %d\n",
		 mp3enc->params.app_sampling_rate,
		 mp3enc->params.app_bit_rate, mp3enc->channels);

      mp3enc->encinit = TRUE;
    }

  if (mp3enc->encbuf == NULL)
    mp3enc->encbuf = buf;
  else
    mp3enc->encbuf = gst_buffer_join (mp3enc->encbuf, buf);

  return mfw_gst_mp3enc_encode_frame (mp3enc, FALSE);
}


/* entry point to initialize the plug-in
 * initialize the plug-in itself
 * register the element factories and pad templates
 * register the features
 *
 * exchange the string 'plugin' with your elemnt name
 */
static gboolean
plugin_init (GstPlugin * plugin)
{
  /* exchange the strings 'plugin' and 'Template plugin' with your
   * plugin name and description */
  GST_DEBUG_CATEGORY_INIT (mfw_gst_mp3enc_debug, "mfw_mp3encoder",
			   0, "FreeScale's MP3 Encoder's Log");

  return gst_element_register (plugin, "mfw_mp3encoder",
			       GST_RANK_PRIMARY, MFW_GST_TYPE_MP3ENC);
}

/* this is the structure that gstreamer looks for to register plugins
 *
 * exchange the strings 'plugin' and 'Template plugin' with you plugin name and
 * description
 */
GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
		   GST_VERSION_MINOR,
		   "mfw_mp3encoder",
		   "encode to MP3 format",
		   plugin_init, VERSION, GST_LICENSE_UNKNOWN,
		   FSL_GST_MM_PLUGIN_PACKAGE_NAME, FSL_GST_MM_PLUGIN_PACKAGE_ORIG)