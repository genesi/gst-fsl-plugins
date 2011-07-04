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
 * Module Name:    mfw_gst_audio_src.c
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
#include <gst/gst.h>
#include <string.h>
#include "mfw_gst_utils.h"
#ifdef MEMORY_DEBUG
#include "mfw_gst_debug.h"
#endif
#include "src_ppp_interface.h"  /*fsl src ppp*/
#include <mxc_asrc.h>		/* asrc  */
#include <fcntl.h>              /* fcntl */
#include <sys/mman.h>           /* mmap  */
#include <sys/ioctl.h>          /* ioctl */      

#include "mfw_gst_audio_src.h" 
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
#define MFW_GST_AUDIO_SRC_CAPS    \
        "audio/x-raw-int"                  
    
#ifdef MEMORY_DEBUG
    static Mem_Mgr mem_mgr = {0};
    
#define AUDIO_SRC_MALLOC( size)\
        dbg_malloc((&mem_mgr),(size), "line" STR(__LINE__) "of" STR(__FUNCTION__) )
#define AUDIO_SRC_FREE( ptr)\
        dbg_free(&mem_mgr, (ptr), "line" STR(__LINE__) "of" STR(__FUNCTION__) )
    
#else
#define AUDIO_SRC_MALLOC(size)\
        g_malloc((size))
#define AUDIO_SRC_FREE( ptr)\
        g_free((ptr))
    
#endif
    
#define AUDIO_SRC_FATAL_ERROR(...) g_print(RED_STR(__VA_ARGS__))
#define AUDIO_SRC_FLOW(...) g_print(BLUE_STR(__VA_ARGS__))

#define DEFAULT_BITWIDTH 16
#define DEFAULT_BITDEPTH 16
#define DEFAULT_CHANNELS 2
#define DEFAULT_OUTPUT_BITSPERSAMPLE 16
#define DEFAULT_OUTPUT_SAMPLE_RATE 44100
#define DEFAULT_OUTPUT_PACKED_24BITS 0
#define DEFAULT_SRC_MODE  0
#define DEFAULT_USE_ASRC  0
#define DEFAULT_SAMPLERATE 44100
#define DEFAULT_CHANNELS 2
#define DEFAULT_INPUT_BLOCKSIZE SRC_INPUT_BLOCKSIZE
#define DEFAULT_DMA_BUFFER_SIZE 16384
#define DEFAULT_DMA_BUFFER_NUM  4
#define DEFAULT_IN_CLK 0
#define DEFAULT_OUT_CLK 0
#define	GST_CAT_DEFAULT	mfw_gst_audiosrc_debug

/*=============================================================================
                                      LOCAL VARIABLES
=============================================================================*/
static GstStaticPadTemplate mfw_audio_src_sink_factory =
    GST_STATIC_PAD_TEMPLATE (
    "sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS(MFW_GST_AUDIO_SRC_CAPS)
);

/*=============================================================================
                        LOCAL FUNCTION PROTOTYPES
=============================================================================*/
GST_DEBUG_CATEGORY_STATIC(mfw_gst_audiosrc_debug);

static void	mfw_gst_audio_src_class_init(gpointer klass);
static void	mfw_gst_audio_src_base_init(gpointer klass);
static void	mfw_gst_audio_src_init(MfwGstAudioSrc *filter, gpointer gclass);

static void	mfw_gst_audio_src_set_property(GObject *object, guint prop_id,
                                         const GValue *value, GParamSpec *pspec);
static void	mfw_gst_audio_src_get_property(GObject *object, guint prop_id,
                                         GValue *value, GParamSpec *pspec);

static gboolean mfw_gst_audio_src_set_caps(GstPad *pad, GstCaps *caps);
static gboolean mfw_gst_audio_src_sink_event(GstPad * pad, GstEvent * event);

static GstFlowReturn mfw_gst_audio_src_chain (GstPad *pad, GstBuffer *buf);

static gint mfw_gst_asrc_get_output_buffer_size( gint input_buffer_size, 
		            gint input_sample_rate, gint output_sample_rate);
static gint mfw_gst_asrc_configure_asrc_channel(GObject *object, MfwGstAudioASrcInfo *pAudioASrcInfo, MfwGstAudioASrcBuf *pAduioASrcBufIn, MfwGstAudioASrcBuf *pAduioASrcBufOut);
static void     mfw_gst_audio_asrc_bitshift(gint * src, gint * dst, MfwGstAudioASrcInfo *info, gint size);
static void     mfw_gst_audio_asrc_convert_data(gint *src, gint *dst, MfwGstAudioASrcInfo *info, gint size);
static GstFlowReturn mfw_gst_audio_asrc_process_frame(MfwGstAudioSrc *filter, GstBuffer *buf, gboolean isEOS);
static GstFlowReturn mfw_gst_audio_src_process_frame(MfwGstAudioSrc *filter, GstBuffer *buf, gboolean isEOS);
 
/*=============================================================================
                            GLOBAL VARIABLES
=============================================================================*/
static GstElementClass *parent_class = NULL;

/*=============================================================================
                            LOCAL FUNCTIONS
=============================================================================*/

/*=============================================================================
 FUNCTION:          mfw_gst_asrc_configure_asrc_channel
 DESCRIPTION:       Configure the channel info and get the input and output buffer pointer.    
 ARGUMENTS PASSED:
        objdect                  -    GObject handle
	pAudioASrcInfo           -    audio info
	pAduioASrcBufIn          -    input dma buffer info
	pAduioASrcBufOut         -    output dma buffer info
 RETURN VALUE:
        0                        -    succeed
	< 0                      -    fail
 IMPORTANT NOTES:   None
=============================================================================*/
static gint mfw_gst_asrc_configure_asrc_channel(GObject *object, MfwGstAudioASrcInfo *pAudioASrcInfo, MfwGstAudioASrcBuf *pAduioASrcBufIn, MfwGstAudioASrcBuf *pAduioASrcBufOut)
{
        MfwGstAudioSrc *filter = MFW_GST_AUDIO_SRC (object);
	gint err = 0;
	gint i = 0;
	struct asrc_req req;
	struct asrc_config config;
	struct asrc_querybuf buf_info;

	req.chn_num = pAudioASrcInfo->channel;
	if ((err = ioctl(filter->fd_asrc, ASRC_REQ_PAIR, &req)) < 0) {
		return err;
	}
	GST_DEBUG("req.index=%d\n",req.index);
	config.pair = req.index;
	config.channel_num = req.chn_num;
	config.dma_buffer_size = filter->dma_buffer_size;
	config.input_sample_rate = pAudioASrcInfo->input_sample_rate;
	config.output_sample_rate = pAudioASrcInfo->output_sample_rate;
	config.buffer_num = filter->dma_buffer_num;
	config.word_width = pAudioASrcInfo->output_bitwidth; 
	switch (filter->inclk)
	{
		case 0:
			config.inclk = INCLK_NONE;
			break;
		case 1:
			config.inclk = INCLK_SSI1_RX;
			break;
		case 2:
			config.inclk = INCLK_SPDIF_RX;
			break;
		case 3:
			config.inclk = INCLK_ESAI_RX;
			break;
		default:
			GST_WARNING("inclk invalid");
			break;
			
	}
	switch (filter->outclk)
	{
		case 0:
			config.outclk = OUTCLK_ASRCK1_CLK;
			break;
		case 1:
			config.outclk = OUTCLK_SSI1_TX;
			break;
		case 2:
			config.outclk = OUTCLK_SPDIF_TX;
			break;
		case 3:
			config.outclk = OUTCLK_ESAI_TX;
			break;
		default:
			GST_WARNING("inclk invalid");
			break;
			
	}
	GST_DEBUG("inclk=%d,outclk=%d,word_width=%d\n",config.inclk, config.outclk, config.word_width);  
	filter->pair_index = req.index;
	
	
	if ((err = ioctl(filter->fd_asrc, ASRC_CONFIG_PAIR, &config)) < 0)
		return err;
	for (i = 0; i < config.buffer_num; i++) {

		buf_info.buffer_index = i;

		if ((err = ioctl(filter->fd_asrc, ASRC_QUERYBUF, &buf_info)) < 0)
			return err;
		pAduioASrcBufIn[i].start =
		    mmap(NULL, buf_info.input_length, PROT_READ | PROT_WRITE,
			 MAP_SHARED, filter->fd_asrc, buf_info.input_offset);
		pAduioASrcBufIn[i].max_len = buf_info.input_length;
		pAduioASrcBufOut[i].start =
		    mmap(NULL, buf_info.output_length, PROT_READ | PROT_WRITE,
			 MAP_SHARED, filter->fd_asrc, buf_info.output_offset);
		pAduioASrcBufOut[i].max_len = buf_info.output_length;
		
	}

	return 0;
}

/*=============================================================================
 FUNCTION:          mfw_gst_asrc_get_output_buffer_size
 DESCRIPTION:       Calculate the output buffer size of ASRC.    
 ARGUMENTS PASSED:
	input_buffer_size        -    size of SRC input buffer
        input_sample_rate        -    source sample rate
	output_sample_rate       -    destination sample rate
 RETURN VALUE:
        outbuffer_size           -    size of SRC output buffer
 IMPORTANT NOTES:   None
=============================================================================*/
static gint mfw_gst_asrc_get_output_buffer_size( gint input_buffer_size,
				gint input_sample_rate, gint output_sample_rate)
{
	gint i = 0;
	gint outbuffer_size = 0;
	gint outsample = output_sample_rate;
	while (outsample >= input_sample_rate) {
		++i;
		outsample -= input_sample_rate;
	}
	outbuffer_size = i * input_buffer_size;
	i = 1;
	while (((input_buffer_size >> i) > 2) && (outsample != 0)) {
		if (((outsample << 1) - input_sample_rate) >= 0) {
			outsample = (outsample << 1) - input_sample_rate;
			outbuffer_size += (input_buffer_size >> i);

		} else {
			outsample = outsample << 1;
		}
		i++;
	}
	outbuffer_size = (outbuffer_size >> 3) << 3;

	return outbuffer_size;

}

/*=============================================================================
 FUNCTION:          src_templ
 DESCRIPTION:       Generate the source pad template.    
 IMPORTANT NOTES:   None
=============================================================================*/
static GstPadTemplate *
src_templ(void)
{
    static GstPadTemplate *templ = NULL;
    if (!templ) {
        GstCaps *caps;
        
        caps = gst_caps_new_simple("audio/x-raw-int", NULL);
        templ = gst_pad_template_new("src", GST_PAD_SRC, GST_PAD_ALWAYS, caps);
    }
    return templ;
}


static void
mfw_gst_audio_src_set_property(GObject *object, guint prop_id,
                             const GValue *value, GParamSpec *pspec)
{
    MfwGstAudioSrc *filter = MFW_GST_AUDIO_SRC (object);
    switch (prop_id)
    {
    case PROPER_ID_OUTPUT_SAMPLE_RATE:
	filter->OutSampleRate = g_value_get_int(value);
        break;
    case PROPER_ID_OUTPUT_BITS_PER_SAMPLE:
	filter->OutBitsPerSample = g_value_get_int(value);
        break;
    case PROPER_ID_OUTPUT_PACKED_24BITS:
	filter->Packed_24Bit_out = g_value_get_int(value);
        break;
    case PROPER_ID_FAST_SRC_MODE:
	filter->FastSrcMode = g_value_get_int(value);
        break;
    case PROPER_ID_USE_ASRC:
	filter->UseASRC = g_value_get_int(value);
        break;
    case PROPER_ID_DMA_BUFFER_SIZE_ASRC:
	filter->dma_buffer_size = g_value_get_int(value);
        break;
    case PROPER_ID_ASRC_IN_CLK:
	filter->inclk = g_value_get_int(value);
        break;
    case PROPER_ID_ASRC_OUT_CLK:
	filter->outclk = g_value_get_int(value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
mfw_gst_audio_src_get_property(GObject *object, guint prop_id,
                             GValue *value, GParamSpec *pspec)
{
    MfwGstAudioSrc *filter = MFW_GST_AUDIO_SRC (object);
    switch (prop_id)
    {
    case PROPER_ID_OUTPUT_SAMPLE_RATE:
        g_value_set_int(value, filter->OutSampleRate);
        break;
    case PROPER_ID_OUTPUT_BITS_PER_SAMPLE:
        g_value_set_int(value, filter->OutBitsPerSample);
        break;
    case PROPER_ID_OUTPUT_PACKED_24BITS:
        g_value_set_int(value, filter->Packed_24Bit_out);
        break;
    case PROPER_ID_FAST_SRC_MODE:
        g_value_set_int(value, filter->FastSrcMode);
        break;
    case PROPER_ID_USE_ASRC:
        g_value_set_int(value, filter->UseASRC);
        break;
    case PROPER_ID_DMA_BUFFER_SIZE_ASRC:
        g_value_set_int(value, filter->dma_buffer_size);
        break;
    case PROPER_ID_ASRC_IN_CLK:
        g_value_set_int(value, filter->inclk);
        break;
    case PROPER_ID_ASRC_OUT_CLK:
        g_value_set_int(value, filter->outclk);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}




/*=============================================================================
 FUNCTION:      mfw_gst_audio_src_sink_event
 DESCRIPTION:       Handles an event on the sink pad.
 ARGUMENTS PASSED:
        pad        -    pointer to pad
        event      -    pointer to event
 RETURN VALUE:
        TRUE       -	if event is sent to sink properly
        FALSE	   -	if event is not sent to sink properly
 PRE-CONDITIONS:    None
 POST-CONDITIONS:   None
 IMPORTANT NOTES:   None
=============================================================================*/
static gboolean 
mfw_gst_audio_src_sink_event(GstPad * pad, GstEvent * event)
{
    MfwGstAudioSrc *filter = MFW_GST_AUDIO_SRC (GST_PAD_PARENT(pad));
    gboolean result = TRUE;
    switch (GST_EVENT_TYPE(event)) {
    case GST_EVENT_NEWSEGMENT:
    {
        GstFormat format;
        gst_event_parse_new_segment(event, NULL, NULL, &format, NULL,
        			NULL, NULL);
        if (format == GST_FORMAT_TIME) {
        	GST_WARNING("\nCame to the FORMAT_TIME call\n");
        } else {
            GST_WARNING("Dropping newsegment event in format %s",
                      gst_format_get_name(format));
            result = TRUE;
        }
        result = gst_pad_event_default(pad, event);
        break;
    }

    case GST_EVENT_FLUSH_STOP:
    {
	if(gst_adapter_available(filter->pAdapter_in)>0)
		gst_adapter_flush(filter->pAdapter_in,gst_adapter_available(filter->pAdapter_in));

	if(filter->ASRC_START==TRUE)
	{
        	filter->ASRC_START = FALSE;
		GST_DEBUG("FLUSH START\n");
	
		result=ioctl(filter->fd_asrc, ASRC_STOP_CONV, &filter->pair_index);
			if(result<0)
			{
				GST_ERROR("Stop ASRC failed\n");
				break;
		}
		result=ioctl(filter->fd_asrc, ASRC_FLUSH, &filter->pair_index);
		if(result<0)
		{
			GST_ERROR("Flush ASRC failed\n");
			break;
		}
	}
        result = gst_pad_event_default(pad, event);	
    	break;
    
    }
    case GST_EVENT_EOS:
    {
        GST_WARNING("\nAudio post processor: Get EOS event\n");
       
	if (gst_adapter_available(filter->pAdapter_in) >0 )
	{
		if (filter->UseASRC ==0)
		{	
			result = mfw_gst_audio_src_process_frame(filter,NULL, TRUE);
			if (result!=GST_FLOW_OK){
        		GST_WARNING("mfw_gst_audio_src_process_frame failed with result=%d\n", result);
			}
		}
		else
		{
			result = mfw_gst_audio_asrc_process_frame(filter,NULL, TRUE);
			if (result!=GST_FLOW_OK){
	        		GST_WARNING("mfw_gst_audio_asrc_process_frame failed with result=%d\n", result);
			}
		}
	
	}

	result = gst_pad_event_default(pad, event);
 	if (result != TRUE) {
      		GST_ERROR("\n Error in pushing the event, result is %d\n",result);
     	}
        gst_adapter_clear(filter->pAdapter_in);
	if(filter->UseASRC)
	{
		result=ioctl(filter->fd_asrc, ASRC_STOP_CONV, &filter->pair_index);
		result=ioctl(filter->fd_asrc, ASRC_RELEASE_PAIR, &filter->pair_index);
		GST_DEBUG("STOP ASRC CONV");
	}
        break;
    }
    default:
	{
	    result = gst_pad_event_default(pad, event);
	    break;
	}

    }
    return result;
}

static void 
mfw_gst_audio_src_free_mem(SRC_Mem_Alloc_Info * meminfo)
{
    int i;
    void * buf;

    for (i=0;i<meminfo->src_num_reqs;i++){
        if (buf=meminfo->mem_info_sub[i].app_base_ptr){
            AUDIO_SRC_FREE(buf);
            meminfo->mem_info_sub[i].app_base_ptr = NULL;
        }
    }

}


static GstFlowReturn
mfw_gst_audio_src_alloc_mem(SRC_Mem_Alloc_Info * meminfo)
{
    int i;
    void * buf;
    
    for (i=0;i<meminfo->src_num_reqs;i++){
        buf = AUDIO_SRC_MALLOC(meminfo->mem_info_sub[i].src_size);
        if (buf==NULL)
            goto allocErr;
        meminfo->mem_info_sub[i].app_base_ptr = buf;
    }

    return GST_FLOW_OK;
    
allocErr:
    mfw_gst_audio_src_free_mem(meminfo);
    return GST_FLOW_ERROR;
    
}


/*=============================================================================
FUNCTION:   mfw_gst_audio_src_change_state

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
 GstStateChangeReturn
mfw_gst_audio_src_change_state(GstElement * element,
				GstStateChange transition)
{
    GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
    MfwGstAudioSrc *filter = MFW_GST_AUDIO_SRC (element);
    
    gboolean res=0;


    switch (transition) {

    	case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
	{
	    if(filter->paused==TRUE)
	    {
	 	if(filter->ASRC_START==TRUE)
	 	{
			filter->paused = FALSE;
		 	GST_DEBUG("PAUSED to PLAYING ASRC\n");

			res = ioctl(filter->fd_asrc, ASRC_START_CONV, &filter->pair_index);
		}	  
       	 }
	    break;
	   
	}
    default:
	    break;
    }

    ret = parent_class->change_state(element, transition);

    switch (transition) {
    	case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
	{
 
	if(filter->paused==FALSE)
	    {
         	if(filter->ASRC_START==TRUE)
	 	{
		 GST_DEBUG("PLAYING to PAUSED ASRC\n");
		 filter->paused = TRUE;
		
	 	}	  
       	    }
	if(filter->UseASRC)
    	    res = ioctl(filter->fd_asrc, ASRC_STOP_CONV, &filter->pair_index);

	    break;
    	}
	case GST_STATE_CHANGE_READY_TO_NULL:
	{
	    if(filter->UseASRC==0)
	    {
		    if(filter->pAudioSrcConfig!=NULL)
		    {
			mfw_gst_audio_src_free_mem(&filter->pAudioSrcConfig->src_mem_info);
			AUDIO_SRC_FREE(filter->pAudioSrcConfig);	
		    }
		    if(filter->pAudioSrcParams!=NULL)
			AUDIO_SRC_FREE(filter->pAudioSrcParams);
	    }
	    else
	    {
		    if (filter->pAudioASrcInfo!=NULL)
			AUDIO_SRC_FREE(filter->pAudioASrcInfo);
	    } 
	    break;
	}
	default:
	    break;
    }

    if (res<0)
	ret = GST_STATE_CHANGE_FAILURE;
    return ret;

}

static GstFlowReturn
mfw_gst_audio_src_core_init(MfwGstAudioSrc *filter, GstBuffer *buf)
{
    GstCaps * caps;
    GstStructure * s;
    SRC_RET_TYPE iStatus =SRC_OK;
    Src_Config *psPPPConfig=NULL; 
    Src_Params sPPParams;
    Src_Params *psPPParams=NULL;
    MfwGstAudioASrcInfo *pAudioASrcInfo=NULL;
    GstAdapter *pAdapter = NULL;
    gboolean ret;
    gint dma_out_size;

    if(filter->UseASRC == 0)
    {
	//Using FSL software SRC 
    	psPPPConfig = AUDIO_SRC_MALLOC(sizeof(Src_Config));
    	psPPParams = AUDIO_SRC_MALLOC(sizeof(Src_Params));

    	if (psPPPConfig==NULL||psPPParams==NULL)
        	goto InitErr;

    	memset(psPPPConfig, 0, sizeof(Src_Config));
	memset(psPPParams, 0, sizeof(Src_Params));   

        psPPParams->wBitsPerSample_in= DEFAULT_BITDEPTH;
        psPPParams->nSampleRate_in = DEFAULT_SAMPLERATE;
        psPPParams->nChannels = DEFAULT_CHANNELS;
        psPPPConfig->InputBlockSize = DEFAULT_INPUT_BLOCKSIZE;
    	caps = gst_buffer_get_caps(buf);
    	s = gst_caps_get_structure(caps, 0);

    	gst_structure_get_int(s, "depth", &psPPParams->wBitsPerSample_in);
    	gst_structure_get_int(s, "width", &psPPParams->wBitsPerSample_in);
    	gst_structure_get_int(s, "rate", &psPPParams->nSampleRate_in);
    	gst_structure_get_int(s, "channels", &psPPParams->nChannels);
	
    	psPPParams->wBitsPerSample_out = filter->OutBitsPerSample;
    	psPPParams->nSampleRate_out = filter->OutSampleRate;
    	psPPParams->Packed_24Bit_out = filter->Packed_24Bit_out;
    	psPPParams->FastSrcMode = filter->FastSrcMode;

    	GST_INFO("SampleRate In %d",psPPParams->nSampleRate_in);
    	GST_INFO("SampleRate Out %d",psPPParams->nSampleRate_out);
      	GST_INFO("BitWidth In %d",psPPParams->wBitsPerSample_in);
    	GST_INFO("BitWidth Out %d",psPPParams->wBitsPerSample_out);
    	GST_INFO("FastSrcMode  %d",psPPParams->FastSrcMode);
    	GST_INFO("Packed_24Bit_out %d",psPPParams->Packed_24Bit_out);
    	GST_INFO("nChannels %d",psPPParams->nChannels);
    
    	memcpy(&sPPParams,psPPParams, sizeof(Src_Params));
     
    	iStatus = src_query_ppp_mem(psPPPConfig,sPPParams);
    	if (iStatus!=SRC_OK)
        	goto InitErr;

    	ret = mfw_gst_audio_src_alloc_mem(&psPPPConfig->src_mem_info);
    	if (ret!=GST_FLOW_OK)
        	goto InitErr;

    	iStatus = src_ppp_init( psPPPConfig, sPPParams);
    	if(iStatus != SRC_OK)
    	{
       		mfw_gst_audio_src_free_mem(&psPPPConfig->src_mem_info); 
       		goto InitErr;
    	}
    	filter->pAudioSrcConfig = psPPPConfig;
    	filter->pAudioSrcParams = psPPParams;
    }
    else
    {

	//Using Asynchronous Sample Rate Converter (ASRC)
	filter->fd_asrc = open("/dev/mxc_asrc", O_RDWR);
	if (filter->fd_asrc < 0)
		goto InitErr;

	pAudioASrcInfo   = AUDIO_SRC_MALLOC(sizeof(MfwGstAudioASrcInfo));
	if(pAudioASrcInfo==NULL)
		goto InitErr;
	memset(pAudioASrcInfo, 0, sizeof(MfwGstAudioASrcInfo));

	memset(filter->sAduioASrcBufIn, 0, sizeof(MfwGstAudioASrcBuf)*BUF_NUM);
	memset(filter->sAduioASrcBufOut, 0, sizeof(MfwGstAudioASrcBuf)*BUF_NUM);

	caps = gst_buffer_get_caps(buf);
    	s = gst_caps_get_structure(caps, 0);

    	gst_structure_get_int(s, "depth", &pAudioASrcInfo->bitwidth);
    	gst_structure_get_int(s, "width", &pAudioASrcInfo->bitwidth);
    	gst_structure_get_int(s, "rate", &pAudioASrcInfo->input_sample_rate);
    	gst_structure_get_int(s, "channels", &pAudioASrcInfo->channel);

	pAudioASrcInfo->output_sample_rate = filter->OutSampleRate;
	if (filter->OutBitsPerSample==24)
	{
		if(!filter->Packed_24Bit_out)
		{
			filter->OutBitsPerSample=32;	
		}
	}
	pAudioASrcInfo->output_bitwidth = filter->OutBitsPerSample;
	dma_out_size = mfw_gst_asrc_get_output_buffer_size( filter->dma_buffer_size, pAudioASrcInfo->input_sample_rate, pAudioASrcInfo->output_sample_rate);
	        
	ret = mfw_gst_asrc_configure_asrc_channel(filter, pAudioASrcInfo, filter->sAduioASrcBufIn, filter->sAduioASrcBufOut);
	if (ret < 0)
		goto InitErr;

	filter->pAudioASrcInfo = pAudioASrcInfo;
    }

    	filter->inptr = NULL;
    	filter->outptr = NULL;
	pAdapter = (GstAdapter *)gst_adapter_new();
    	filter->pAdapter_in = pAdapter;
 
    	return GST_FLOW_OK;

    
InitErr:
	
    if (psPPPConfig)
        AUDIO_SRC_FREE(psPPPConfig);
    if (psPPParams)
        AUDIO_SRC_FREE(psPPParams);
    if (pAudioASrcInfo)
        AUDIO_SRC_FREE(pAudioASrcInfo);
  
    
    return GST_FLOW_ERROR;
    
}
// SRC Software

static GstFlowReturn
mfw_gst_audio_src_process_frame(MfwGstAudioSrc *filter, GstBuffer *buf, gboolean isEOS)
{
    Src_Config * psPPPConfig = filter->pAudioSrcConfig;
    Src_Params *psPPParams = filter->pAudioSrcParams; 
    GstBuffer * outb;
    gint insize;
    gint default_insize;
    gint outsize;
    gint outBytesNum=0;
    GstFlowReturn ret;
    SRC_RET_TYPE iStatus;
    
    if (G_UNLIKELY(filter->capsSet==FALSE)){
        GstCaps * caps;
        //Set caps for the srcpad 
        caps = gst_caps_new_simple("audio/x-raw-int",
            				       "endianness", G_TYPE_INT, G_BYTE_ORDER,
            				       "signed", G_TYPE_BOOLEAN, TRUE,
            				       "width", G_TYPE_INT, psPPParams->wBitsPerSample_out, 
            				       "depth", G_TYPE_INT, psPPParams->wBitsPerSample_out,
            				       "rate", G_TYPE_INT, psPPParams->nSampleRate_out,
            				       "channels", G_TYPE_INT, psPPParams->nChannels,
            				       NULL); 
        gst_pad_set_caps(filter->srcpad, caps);
        filter->capsSet = TRUE;
    }
    // Push the input data to the adapter.
    if( !isEOS)
    {
    	gst_adapter_push(filter->pAdapter_in,buf); 
    // Allocate the output buffer of the src pad to hold the output data. 
    	outsize = (int)(((int)( GST_BUFFER_SIZE(buf) * ((float)psPPParams->nSampleRate_out/(float)psPPParams->nSampleRate_in +0.5) ))*psPPParams->wBitsPerSample_out/psPPParams->wBitsPerSample_in*1.5); 
    }
    else
    {
	outsize = (int)(((int)( gst_adapter_available(filter->pAdapter_in) * ((float)psPPParams->nSampleRate_out/(float)psPPParams->nSampleRate_in +0.5) ))*psPPParams->wBitsPerSample_out/psPPParams->wBitsPerSample_in*1.5);
    }

    ret = gst_pad_alloc_buffer(filter->srcpad, 0, outsize, GST_PAD_CAPS(filter->srcpad), &outb);
    if (ret!=GST_FLOW_OK)
        return ret;

    filter->outptr=GST_BUFFER_DATA(outb);
    default_insize = DEFAULT_INPUT_BLOCKSIZE*(psPPParams->wBitsPerSample_in/8)*psPPParams->nChannels;
    //Process the input data at the alignment of default_insize.
    while(gst_adapter_available(filter->pAdapter_in)>=default_insize)
    {
    	psPPPConfig->InputBlockSize =DEFAULT_INPUT_BLOCKSIZE;
        filter->inptr = gst_adapter_peek(filter->pAdapter_in,default_insize);  
	iStatus = src_ppp_process(  psPPPConfig, filter->inptr, filter->outptr);
	if (iStatus!=SRC_OK){
            AUDIO_SRC_FATAL_ERROR("src_ppp_process failed with ret=%d\n", iStatus);
            goto processErr;
        }
	filter->outptr += psPPPConfig->OutputByteNum;
	outBytesNum += psPPPConfig->OutputByteNum; 
        gst_adapter_flush(filter->pAdapter_in, default_insize);
    }
    //Process the residual input data if exists.
    if(gst_adapter_available(filter->pAdapter_in)>0)
    {
    	
	insize = gst_adapter_available(filter->pAdapter_in);
	psPPPConfig->InputBlockSize =insize/((psPPParams->wBitsPerSample_in/8)*psPPParams->nChannels);
        filter->inptr = gst_adapter_peek(filter->pAdapter_in,insize);  

	iStatus = src_ppp_process(  psPPPConfig, filter->inptr, filter->outptr);
	if (iStatus!=SRC_OK){
            AUDIO_SRC_FATAL_ERROR("src_ppp_process failed with ret=%d\n", iStatus);
            goto processErr;
        }
	filter->outptr += psPPPConfig->OutputByteNum;
	outBytesNum += psPPPConfig->OutputByteNum; 
        gst_adapter_flush(filter->pAdapter_in, insize);
    }
    //Push the output data to the src pad.
    GST_BUFFER_SIZE(outb) = outBytesNum;
    if (GST_BUFFER_SIZE(outb)%(psPPParams->nChannels*psPPParams->wBitsPerSample_out/8)!=0)
	GST_BUFFER_SIZE(outb) -= GST_BUFFER_SIZE(outb)%(psPPParams->nChannels*psPPParams->wBitsPerSample_out/8);

    ret = gst_pad_push(filter->srcpad, outb);
    return ret;

processErr:
    if(buf)
    	gst_buffer_unref(buf);
    if(outb)
    	gst_buffer_unref(outb);
    return GST_FLOW_ERROR;
    
}
// ASRC

static void mfw_gst_audio_asrc_bitshift(gint * src, gint * dst, MfwGstAudioASrcInfo *info, gint size)
{

	guint data;
	guint zero;
	gint nleft;
	
	if (info->bitwidth == 8) {
		guint8  *ptr;
		ptr = (guint8 *)src;
		nleft = size>>2;
		do {
			data = *ptr++;
			data -= 0x7F;
			zero = ((data << 16) & 0xFFFF00);
			*dst++ = (guint)zero;
		} while (--nleft);
		return;
	}
	if  (info->bitwidth == 16)
	{
		gint16 *ptr;
		ptr = (gint16 *)src;
		nleft = size>>2;
		do {
			data = *ptr++;
			zero = ((data << 8) & 0xFFFF00);
			*dst++ = zero;
		} while (--nleft);
		return;
	}
	if  (info->bitwidth == 24)
	{
		guint8 *ptr;
		guint val1,val2,val3;
		ptr = (guint8 *)src;
		nleft = size>>2;
		do {
			val1 = *ptr++;
			val2 = *ptr++;
			val3 = *ptr++;
			data = (((val3<<16)|(val2<<8)|val1) & 0xFFFFFF);
			*dst++ = data;
		} while (--nleft);
		return;
	}
	if  (info->bitwidth == 32)
	{
		nleft = size >>2;
		do {
			data = *src++;
			zero = ((data >> 8) & 0xFFFFFF);
			*dst++ = zero;
		} while (--nleft);
	}
}

static void mfw_gst_audio_asrc_convert_data(gint *src, gint *dst, MfwGstAudioASrcInfo *info, gint size)
{
	guint data;
	if (info->output_bitwidth == 8) 
	{
		guint8 *ptr;
		ptr = (guint8 *)dst;
		guint val;
		do {
			data = *src++;
			val = (data>>16) & 0xFF;
			*ptr++  = val;
			size -= 4;
		} while (size > 0);
		return;
	}
	if (info->output_bitwidth == 16) 
	{
		guint16 *ptr;
		ptr = (guint16 *)dst;
		guint16 val;
		do {
			data = *src++;
			val = (data>>8) & 0xFFFF;
			*ptr++  = val;			
			size -= 4;
		} while (size > 0);
		return;
	}
	if (info->output_bitwidth == 24) 
	{
		guint8 *ptr;
		ptr = (guint8 *)dst;
		guint8 val1,val2,val3;
		do {
			data = *src++;
			val1 = (data & 0xFF);
			val2 = ((data>>8) & 0xFF);
			val3 = ((data>>16) & 0xFF);
			*ptr++  = val1;
			*ptr++  = val2;
			*ptr++  = val3;
			size -= 4;
		} while (size > 0);
		return;
	}
	if (info->output_bitwidth == 32) 
	{
		do {
			data = *src++;
			data = (data<<8) & 0xFFFFFF00;
			*dst++ = data;
			size -= 4;
		} while (size > 0);
	}
}


static GstFlowReturn
mfw_gst_audio_asrc_process_frame(MfwGstAudioSrc *filter, GstBuffer *buf, gboolean isEOS)
{
    MfwGstAudioASrcInfo *pAudioASrcInfo  = filter->pAudioASrcInfo;
    MfwGstAudioASrcBuf *pAduioASrcBufIn  = filter->sAduioASrcBufIn;
    MfwGstAudioASrcBuf *pAduioASrcBufOut = filter->sAduioASrcBufOut;
    struct asrc_buffer inbuf, outbuf;
    gint i = 0;
    gint err;
    gint nleft, nwritten;
    gint8 *p;
    GstBuffer * outb;
    gint insize;
    gint insize_local;
    gint default_insize;
    gint outsize;
    gint output_dma_size;
    gint outBytesNum=0;
    GstFlowReturn ret;
    SRC_RET_TYPE iStatus;
    if (G_UNLIKELY(filter->capsSet==FALSE)){
        GstCaps * caps;
        //Set caps for the srcpad
        caps = gst_caps_new_simple("audio/x-raw-int",
            				       "endianness", G_TYPE_INT, G_BYTE_ORDER,
            				       "signed", G_TYPE_BOOLEAN, TRUE,
            				       "width", G_TYPE_INT, pAudioASrcInfo->output_bitwidth, 
            				       "depth", G_TYPE_INT, pAudioASrcInfo->output_bitwidth,
            				       "rate", G_TYPE_INT, pAudioASrcInfo->output_sample_rate,
            				       "channels", G_TYPE_INT, pAudioASrcInfo->channel,
            				        NULL); 
        gst_pad_set_caps(filter->srcpad, caps);
        filter->capsSet = TRUE;
	if(GST_BUFFER_SIZE(buf)*4/(pAudioASrcInfo->bitwidth/8)>=(filter->dma_buffer_num-1)*filter->dma_buffer_size && isEOS == FALSE  )
   	{
		//Need to set SSI clock first;
		gst_adapter_push(filter->pAdapter_in,buf);
		ret = gst_pad_alloc_buffer(filter->srcpad, 0, 4*pAudioASrcInfo->channel, GST_PAD_CAPS(filter->srcpad), &outb);
    		if (ret!=GST_FLOW_OK)
 			return ret;
		GST_BUFFER_SIZE(outb)=4*pAudioASrcInfo->channel;
		memset(GST_BUFFER_DATA(outb),0,GST_BUFFER_SIZE(outb));
    		ret = gst_pad_push(filter->srcpad, outb);
    			return ret;	
    	}
    }
    
    // Push the input data to the adapter.
    if (isEOS == FALSE)
    	gst_adapter_push(filter->pAdapter_in,buf);
    

    if(filter->ASRC_START == TRUE && (gst_adapter_available(filter->pAdapter_in)*4/(pAudioASrcInfo->bitwidth/8))<filter->dma_buffer_size && isEOS == FALSE )
    {
	   return  GST_FLOW_OK;
    }
     
    // Allocate the output buffer of the src pad to hold the output data.
    if(isEOS==TRUE)
    {
	insize = (filter->nInBufQueued-filter->nOutBufDequeued)*filter->dma_buffer_size/4*(pAudioASrcInfo->bitwidth/8)+gst_adapter_available(filter->pAdapter_in);
	outsize = mfw_gst_asrc_get_output_buffer_size( insize/(pAudioASrcInfo->bitwidth/8), pAudioASrcInfo->input_sample_rate, pAudioASrcInfo->output_sample_rate)*(pAudioASrcInfo->output_bitwidth/8)+64;
    	ret = gst_pad_alloc_buffer(filter->srcpad, 0, outsize, GST_PAD_CAPS(filter->srcpad), &outb);
 	if (ret!=GST_FLOW_OK)
	{
		GST_ERROR("gst_pad_alloc_buffer failed, insize=%d, outsize=%d\n",insize,outsize);	

        	return ret;
	}
	pAudioASrcInfo->data_len = gst_adapter_available(filter->pAdapter_in);
    	pAudioASrcInfo->output_data_len = mfw_gst_asrc_get_output_buffer_size( insize*4/(pAudioASrcInfo->bitwidth/8),
					pAudioASrcInfo->input_sample_rate,
					 pAudioASrcInfo->output_sample_rate);
    }
    else
    { 
     	insize = gst_adapter_available(filter->pAdapter_in);
   	outsize = mfw_gst_asrc_get_output_buffer_size( insize/(pAudioASrcInfo->bitwidth/8), pAudioASrcInfo->input_sample_rate, pAudioASrcInfo->output_sample_rate)*(pAudioASrcInfo->output_bitwidth/8)+64;
   	ret = gst_pad_alloc_buffer(filter->srcpad, 0, outsize, GST_PAD_CAPS(filter->srcpad), &outb);
   	if (ret!=GST_FLOW_OK)
	{
		GST_ERROR("gst_pad_alloc_buffer failed, insize=%d, outsize=%d\n",insize,outsize);	

        	return ret;
	}
	pAudioASrcInfo->data_len = gst_adapter_available(filter->pAdapter_in);
    	pAudioASrcInfo->output_data_len = mfw_gst_asrc_get_output_buffer_size( pAudioASrcInfo->data_len*4/(pAudioASrcInfo->bitwidth/8),
					pAudioASrcInfo->input_sample_rate,
					 pAudioASrcInfo->output_sample_rate);
     }    
    filter->outptr=GST_BUFFER_DATA(outb);
    default_insize = filter->dma_buffer_size;
    output_dma_size = mfw_gst_asrc_get_output_buffer_size( default_insize, pAudioASrcInfo->input_sample_rate, pAudioASrcInfo->output_sample_rate);

    insize = filter->dma_buffer_size/4*(pAudioASrcInfo->bitwidth/8);
     

    if(filter->ASRC_START!=TRUE)
    {

	if(gst_adapter_available(filter->pAdapter_in)>=(filter->dma_buffer_num-1)*insize)
	{
		memset(pAduioASrcBufIn[i].start, 0, filter->dma_buffer_size);
		inbuf.length = filter->dma_buffer_size;
		inbuf.index = i;
		if ((err = ioctl(filter->fd_asrc, ASRC_Q_INBUF, &inbuf)) < 0)
			goto processErr;
		filter->nInBufQueued++;
		outbuf.index = i;
   		outbuf.length = output_dma_size;
    		if ((err = ioctl(filter->fd_asrc, ASRC_Q_OUTBUF, &outbuf)) < 0)
			goto processErr;
		i++;

		while (i < filter->dma_buffer_num) {
			filter->inptr = gst_adapter_peek(filter->pAdapter_in, insize);

			mfw_gst_audio_asrc_bitshift(filter->inptr,pAduioASrcBufIn[i].start,pAudioASrcInfo,filter->dma_buffer_size);	
			inbuf.length = filter->dma_buffer_size;
			inbuf.index = i;
			pAudioASrcInfo->data_len -= insize;

			if ((err = ioctl(filter->fd_asrc, ASRC_Q_INBUF, &inbuf)) < 0)
				goto processErr;
			filter->nInBufQueued++;
			outbuf.index = i;
			outbuf.length = output_dma_size;

			if ((err = ioctl(filter->fd_asrc, ASRC_Q_OUTBUF, &outbuf)) < 0)
				goto processErr;
			i++;

			gst_adapter_flush(filter->pAdapter_in,  insize);

		}
		filter->ASRC_START=TRUE;
		GST_DEBUG("START ASRC CONV,pairindex=%d\n",filter->pair_index);

    		if ((err = ioctl(filter->fd_asrc, ASRC_START_CONV, &filter->pair_index)) < 0)
			goto processErr;
	  	if ((err = ioctl(filter->fd_asrc, ASRC_DQ_OUTBUF, &outbuf)) < 0)
		{
			if(outbuf.buf_valid==ASRC_BUF_NA)
			{
		     	        return GST_FLOW_OK;
			}
			goto processErr;
		}
		
		filter->nOutBufDequeued++; 
	    	if ((err = ioctl(filter->fd_asrc, ASRC_Q_OUTBUF, &outbuf)) < 0)
			goto processErr;
		return GST_FLOW_OK;

	
	}
	else
	{
			GST_BUFFER_SIZE(outb)=4*pAudioASrcInfo->channel;
			memset(GST_BUFFER_DATA(outb),0,GST_BUFFER_SIZE(outb));
    			ret = gst_pad_push(filter->srcpad, outb);
    			return ret;
   	}
    }
    if(filter->ASRC_START==TRUE)
    {
             	if (pAudioASrcInfo->data_len >= insize || pAudioASrcInfo->data_len>0 && isEOS==TRUE) {

			
 			if ((err = ioctl(filter->fd_asrc, ASRC_DQ_INBUF, &inbuf)) < 0)
			{
			
				if(inbuf.buf_valid==ASRC_BUF_NA)
				{
					GST_DEBUG("DQ_INBUF after ASRC stopped\n");
					GST_BUFFER_SIZE(outb)=4*pAudioASrcInfo->channel;
					memset(GST_BUFFER_DATA(outb),0,GST_BUFFER_SIZE(outb));
    					ret = gst_pad_push(filter->srcpad, outb);
    					return ret;
				}
				goto processErr;

			}
			inbuf.length = (pAudioASrcInfo->data_len > insize) ? filter->dma_buffer_size : pAudioASrcInfo->data_len*4/(pAudioASrcInfo->bitwidth/8);
			insize_local = inbuf.length/4*(pAudioASrcInfo->bitwidth/8);
 			filter->inptr = gst_adapter_peek(filter->pAdapter_in, insize_local);

		 	mfw_gst_audio_asrc_bitshift(filter->inptr,pAduioASrcBufIn[inbuf.index].start,pAudioASrcInfo,inbuf.length);
		
		 	gst_adapter_flush(filter->pAdapter_in,  insize_local);
	        	pAudioASrcInfo->data_len -= insize_local;
			GST_DEBUG("Q_INBUF size=%d\n",inbuf.length);	

			if ((err = ioctl(filter->fd_asrc, ASRC_Q_INBUF, &inbuf)) < 0)
				goto processErr;
			filter->nInBufQueued++;	
	
 
			while ((pAudioASrcInfo->output_data_len > 0 && isEOS==TRUE)||pAudioASrcInfo->output_data_len >= output_dma_size)
			{
   		        	if ((err = ioctl(filter->fd_asrc, ASRC_DQ_OUTBUF, &outbuf)) < 0)
				{	
					
					if(outbuf.buf_valid==ASRC_BUF_NA)
					{
						GST_DEBUG("DQ_OUTBUF after ASRC stopped\n");
						GST_BUFFER_SIZE(outb)=4*pAudioASrcInfo->channel;
						memset(GST_BUFFER_DATA(outb),0,GST_BUFFER_SIZE(outb));
		    				ret = gst_pad_push(filter->srcpad, outb);
    						return ret;
					}
					goto processErr;
				
				}
				filter->nOutBufDequeued++;
				GST_ERROR("DQ_OUTBUF size=%d\n",outbuf.length);	

				if (isEOS==TRUE)
					outbuf.length = (pAudioASrcInfo->output_data_len > outbuf.length) ? outbuf.length : pAudioASrcInfo->output_data_len;	
		       		nleft = outbuf.length;
		        	p = pAduioASrcBufOut[outbuf.index].start;
				mfw_gst_audio_asrc_convert_data(p, filter->outptr, pAudioASrcInfo, nleft);	
				filter->outptr += (nleft>>2)*(pAudioASrcInfo->output_bitwidth/8);
				outBytesNum +=  (nleft>>2)*(pAudioASrcInfo->output_bitwidth/8);
    	     			pAudioASrcInfo->output_data_len -= outbuf.length;
    	     			outbuf.length = output_dma_size;
	   			if ((err = ioctl(filter->fd_asrc, ASRC_Q_OUTBUF, &outbuf)) < 0)
    					goto processErr;
		
				if (isEOS==TRUE)
				{
					//these code is used to solve the problem of ASRC overload error at the EOS event
					if ((err = ioctl(filter->fd_asrc, ASRC_DQ_INBUF, &inbuf)) < 0)
					{						
					
						if(inbuf.buf_valid==ASRC_BUF_NA)
						{
							GST_DEBUG("DQ_INBUF after ASRC stopped\n");
							GST_BUFFER_SIZE(outb)=4*pAudioASrcInfo->channel;
							memset(GST_BUFFER_DATA(outb),0,GST_BUFFER_SIZE(outb));
	    						ret = gst_pad_push(filter->srcpad, outb);
    							return ret;
						}
						goto processErr;

					}
					memset(pAduioASrcBufIn[inbuf.index].start, 0, filter->dma_buffer_size);
					inbuf.length = filter->dma_buffer_size;
					if ((err = ioctl(filter->fd_asrc, ASRC_Q_INBUF, &inbuf)) < 0)
						goto processErr;

				}
				

				
       			} 
		}

    		GST_BUFFER_SIZE(outb) = outBytesNum;
		GST_DEBUG("residual data size in adapter=%d,outBytesNum=%d\n",gst_adapter_available(filter->pAdapter_in),outBytesNum);	
		ret = gst_pad_push(filter->srcpad, outb);	
    		return ret;
    }

  

processErr:
    GST_ERROR("Process Err\n");
    if(buf)
    	gst_buffer_unref(buf);
    if(outb)
	gst_buffer_unref(outb);
    if(filter->ASRC_START==TRUE)
    {
    	    ioctl(filter->fd_asrc, ASRC_STOP_CONV, &filter->pair_index);
	    ioctl(filter->fd_asrc, ASRC_RELEASE_PAIR, &filter->pair_index);
    }
    return GST_FLOW_ERROR;
    
}

static GstFlowReturn
mfw_gst_audio_src_chain (GstPad *pad, GstBuffer *buf)
{
    MfwGstAudioSrc *filter;
    GstFlowReturn ret;
    
    g_return_if_fail (pad != NULL);
    g_return_if_fail (buf != NULL);
    
    filter = MFW_GST_AUDIO_SRC(GST_OBJECT_PARENT (pad));   

    if (G_UNLIKELY(filter->init==FALSE)) {
        
    	ret = mfw_gst_audio_src_core_init(filter, buf);
        if (ret!=GST_FLOW_OK){
            gst_buffer_unref(buf);
            AUDIO_SRC_FATAL_ERROR("mfw_gst_audio_src_core_init failed with return %d\n", ret);
            return ret;
        }
        filter->init = TRUE;
    }

    if (filter->UseASRC ==0)
    {
	// Using FSL software SRC 
    	ret = mfw_gst_audio_src_process_frame(filter, buf, FALSE);
    	if (ret!=GST_FLOW_OK){
        	GST_WARNING("mfw_gst_audio_src_process_frame failed with ret=%d\n", ret);
    	}
    }
    else
    {
	// Using hardware ASRC module
	ret = mfw_gst_audio_asrc_process_frame(filter,buf, FALSE);
	if (ret!=GST_FLOW_OK){
        	GST_WARNING("mfw_gst_audio_asrc_process_frame failed with ret=%d\n", ret);
    	}
    }

    return ret;
}

/*=============================================================================
FUNCTION:    mfw_gst_audio_src_get_type
        
DESCRIPTION:    

ARGUMENTS PASSED:
        
  
RETURN VALUE:
        

PRE-CONDITIONS:
   	    None
 
POST-CONDITIONS:
   	    None

IMPORTANT NOTES:
   	    None
=============================================================================*/

GType
mfw_gst_audio_src_get_type(void)
{
    static GType mfw_audio_src_type = 0;

    if (!mfw_audio_src_type)
    {
        static const GTypeInfo mfw_audio_src_info =
        {
            sizeof (MfwGstAudioSrcClass),
            (GBaseInitFunc) mfw_gst_audio_src_base_init,
            NULL,
            (GClassInitFunc) mfw_gst_audio_src_class_init,
            NULL,
            NULL,
            sizeof (MfwGstAudioSrc),
            0,
            (GInstanceInitFunc) mfw_gst_audio_src_init,
        };
        
        mfw_audio_src_type = g_type_register_static (GST_TYPE_ELEMENT,
            "MfwGstAudioSrc",
            &mfw_audio_src_info, 
            0
        );
    }
    GST_DEBUG_CATEGORY_INIT(mfw_gst_audiosrc_debug, "mfw_audiosrc",
			    0, "FreeScale's Audio SampleRate Convertor Gst Plugin's Log");
    return mfw_audio_src_type;
}

static void
mfw_gst_audio_src_base_init (gpointer klass)
{
    static GstElementDetails mfw_audio_src_details = 
        GST_ELEMENT_DETAILS ("Freescale Audio Sampling Rate Converter",
        "Filter/Effect/Audio",
        "Audio SamplingRate-Converter",
        FSL_GST_MM_PLUGIN_AUTHOR) ;

    GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

    gst_element_class_add_pad_template (
        element_class,
        src_templ()
    );
    
    gst_element_class_add_pad_template (element_class,
    gst_static_pad_template_get (&mfw_audio_src_sink_factory));
    gst_element_class_set_details (element_class, &mfw_audio_src_details);
    
    return;
}


static void
mfw_gst_audio_src_class_init (gpointer klass)
{
    GObjectClass *gobject_class;
    GstElementClass *gstelement_class;
    int i;

    gobject_class = (GObjectClass*) klass;
    gstelement_class = (GstElementClass*) klass;

    parent_class = g_type_class_peek_parent (klass);

    gobject_class->set_property = mfw_gst_audio_src_set_property;
    gobject_class->get_property = mfw_gst_audio_src_get_property;
    gstelement_class->change_state = mfw_gst_audio_src_change_state;

    g_object_class_install_property (
        G_OBJECT_CLASS (klass), 
        PROPER_ID_OUTPUT_SAMPLE_RATE, 
        g_param_spec_int ("out_rate", "output sample rates", 
        "Output Sampling Rates", 
        8000, 96000,
        DEFAULT_OUTPUT_SAMPLE_RATE, G_PARAM_READWRITE)
    ); 
    g_object_class_install_property (
        G_OBJECT_CLASS (klass), 
        PROPER_ID_OUTPUT_BITS_PER_SAMPLE, 
        g_param_spec_int ("out_bitwidth", "output bits per sample", 
        "Output Bits Per Sample", 
        8, 32,
        DEFAULT_OUTPUT_BITSPERSAMPLE, G_PARAM_READWRITE)
    );

    g_object_class_install_property (
        G_OBJECT_CLASS (klass), 
        PROPER_ID_OUTPUT_PACKED_24BITS, 
        g_param_spec_int ("out_in_packed24", "output samples format for 24bits", 
        "Whether Output Samples in Packed 24bit Format, need to use together with out-bitwidth", 
        0, 1,
        DEFAULT_OUTPUT_PACKED_24BITS, G_PARAM_READWRITE)
    );

    g_object_class_install_property (
        G_OBJECT_CLASS (klass), 
        PROPER_ID_FAST_SRC_MODE, 
        g_param_spec_int ("fastmode", "fast src mode", 
        "Use Fastmode of FSL SRC", 
        0, 1,
        DEFAULT_SRC_MODE, G_PARAM_READWRITE)
    );

    g_object_class_install_property (
        G_OBJECT_CLASS (klass), 
        PROPER_ID_USE_ASRC, 
        g_param_spec_int ("use_ASRC", "use ASRC module", 
        "Use ASRC module ", 
        0, 1,
        DEFAULT_USE_ASRC, G_PARAM_READWRITE)
    );

    g_object_class_install_property (
        G_OBJECT_CLASS (klass), 
        PROPER_ID_DMA_BUFFER_SIZE_ASRC, 
        g_param_spec_int ("dma_buf_size", "ASRC DMA buffer size", 
        "ASRC DMA buffer size ", 
        0, 65536,
        DEFAULT_DMA_BUFFER_SIZE, G_PARAM_READWRITE)
    );

    g_object_class_install_property (
        G_OBJECT_CLASS (klass), 
        PROPER_ID_ASRC_IN_CLK, 
        g_param_spec_int ("asrc_inclk", "ASRC input clock selection", 
        "ASRC input clock number 0: INCLK_NONE 1:INCLK_SSI1_RX 2:INCLK_SPDIF_RX 3:INCLK_ESAI_RX (Default 0)", 
        0, 3,
        0, G_PARAM_READWRITE)
    );

    g_object_class_install_property (
        G_OBJECT_CLASS (klass), 
        PROPER_ID_ASRC_OUT_CLK, 
        g_param_spec_int ("asrc_outclk", "ASRC output clock selection", 
        "ASRC output clock number 0: OUTCLK_ASRCK1_CLK 1:OUTCLK_SSI1_TX 2:OUTCLK_SPDIF_TX 3:OUTCLK_ESAI_TX (Default 0)", 
        0, 3,
        0, G_PARAM_READWRITE) 
    );


    return;
}

static gboolean
mfw_gst_audio_src_set_caps (GstPad *pad, GstCaps *caps)
{
    MfwGstAudioSrc *filter;
    GstPad *otherpad;
    
    filter = MFW_GST_AUDIO_SRC(gst_pad_get_parent (pad));

    gst_object_unref(filter);

    return TRUE;
}



static void 
mfw_gst_audio_src_init	 (MfwGstAudioSrc *filter,
    gpointer gclass)

{

    filter->sinkpad = gst_pad_new_from_template (
    gst_static_pad_template_get (
        &mfw_audio_src_sink_factory), 
        "sink");
    gst_pad_set_setcaps_function (filter->sinkpad, mfw_gst_audio_src_set_caps);

    filter->srcpad = gst_pad_new_from_template(src_templ(), "src");

    gst_pad_set_setcaps_function (filter->srcpad, mfw_gst_audio_src_set_caps);

    gst_element_add_pad (GST_ELEMENT (filter), filter->sinkpad);
    gst_element_add_pad (GST_ELEMENT (filter), filter->srcpad);

    gst_pad_set_chain_function (filter->sinkpad, mfw_gst_audio_src_chain);

    filter->capsSet = FALSE;
    filter->init = FALSE;

    gst_pad_set_event_function(filter->sinkpad,
			       GST_DEBUG_FUNCPTR
			       (mfw_gst_audio_src_sink_event));

    filter->OutSampleRate = DEFAULT_OUTPUT_SAMPLE_RATE;
    filter->OutBitsPerSample = DEFAULT_OUTPUT_BITSPERSAMPLE;
    filter->Packed_24Bit_out = DEFAULT_OUTPUT_PACKED_24BITS;
    filter->FastSrcMode = DEFAULT_SRC_MODE;
    filter->UseASRC = DEFAULT_USE_ASRC; 
    filter->dma_buffer_size = DEFAULT_DMA_BUFFER_SIZE; 
    filter->dma_buffer_num = DEFAULT_DMA_BUFFER_NUM;
    filter->pAudioASrcInfo = NULL;
    filter->pAudioSrcConfig = NULL;
    filter->pAudioSrcParams = NULL;
    filter->ASRC_START = FALSE;
    filter->paused = FALSE;
    filter->inclk = DEFAULT_IN_CLK;
    filter->outclk = DEFAULT_OUT_CLK;
    filter->nInBufQueued = 0;
    filter->nOutBufDequeued = 0;
 
#define MFW_GST_AUDIO_SRC_PLUGIN VERSION
    PRINT_CORE_VERSION(src_ppp_versionInfo());
    PRINT_PLUGIN_VERSION(MFW_GST_AUDIO_SRC_PLUGIN);
    
    return;
}



static gboolean
plugin_init (GstPlugin *plugin)
{
    
    return gst_element_register (plugin, "mfw_audiosrc",
                GST_RANK_PRIMARY,
                MFW_GST_TYPE_AUDIO_SRC);
}



GST_PLUGIN_DEFINE (
    GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    "mfw_audiosrc",
    "Freescale Audio SamplingRate-Converter",
    plugin_init, 
    VERSION, 
    GST_LICENSE_UNKNOWN, 
    FSL_GST_MM_PLUGIN_PACKAGE_NAME, FSL_GST_MM_PLUGIN_PACKAGE_ORIG)

