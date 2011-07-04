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

/*=============================================================================
                                                                               
    Module Name:            mfw_gst_bsacdec.c

    General Description:    Gstreamer plugin for AAC + LC decoder
                            capable of decoding AAC (with both ADIF and ADTS 
                            header).
                            
===============================================================================
Portability: This file is ported for Linux and GStreamer.

===============================================================================
                            INCLUDE FILES
=============================================================================*/

#include <gst/gst.h>
#ifdef PUSH_MODE
#include <gst/base/gstadapter.h>
#endif
#include <string.h>
#include "bsacd_dec_interface.h"
#include "mfw_gst_bsacdec.h"
#include "mfw_gst_utils.h"

/*=============================================================================
                            LOCAL CONSTANTS
=============================================================================*/
#define MULT_FACTOR 4

/* the below macros are used to calculate the 
Bitrate by parsing the ADTS header */
#define SAMPLING_FREQ_IDX_MASk  0x3c
#define BITSPERFRAME_MASK 0x3ffe000
#define ADTS_HEADER_LENGTH 7 
#ifdef PUSH_MODE
#define BS_BUF_SIZE AACD_INPUT_BUFFER_SIZE
#define TIMESTAMP_DIFFRENCE_MAX_IN_NS 200000000
#endif

FILE *pfOutput;

/*=============================================================================
                LOCAL TYPEDEFS (STRUCTURES, UNIONS, ENUMS)
=============================================================================*/

static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE("sink",
								   GST_PAD_SINK,
								   GST_PAD_ALWAYS,
								   GST_STATIC_CAPS
								   ("audio/mpeg, "
								    "mpegversion	= (gint){2, 4}, "
								    "bitrate = [8000 ,	384000],"
								    "channels =	(gint) [	1, 2 ]"));

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE("src",
								  GST_PAD_SRC,
								  GST_PAD_ALWAYS,
								  GST_STATIC_CAPS("audio/x-raw-int, \
                                    " "endianness	= (gint)	" G_STRINGIFY(G_BYTE_ORDER) ",\
                                    " "signed	= (boolean)	true, \
                                    " "width = (gint) 16,\
                                    " "depth = (gint) 16, \
                                    " "rate =	(gint) {8000, 11025, 12000,	16000,\
                                    22050, 24000, 32000,44100, 48000 },	\
                                    " "channels =	(gint) [	1, 2 ]"));


/*=============================================================================
                                LOCAL MACROS
=============================================================================*/
#define LONG_BOUNDARY 4
#define	GST_CAT_DEFAULT    mfw_gst_bsacdec_debug

#define BS_BUF_SIZE AACD_INPUT_BUFFER_SIZE
#define MAX_ENC_BUF_SIZE 400*BS_BUF_SIZE
#define LONG_BOUNDARY 4
#define FRAMESIZE 1024

int sampling_frequency[] = { 96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050, 16000, 12000, 11025, 8000, 0, 0, 0, 0};
enum { BITSPERFRAME_SCALE = 4 } ;
unsigned int lastGoodBufferFullness,lastGoodBufferFullness,firstBufferFullness;
 
BitstreamParam App_bs_param;
 //int bitstream_count;
 char bitstream_buf[MAX_ENC_BUF_SIZE];
// int bitstream_buf_index;
// int in_buf_done;
 int bytes_supplied;
 unsigned char App_ibs_buf[INTERNAL_BS_BUFSIZE];



/*=============================================================================
                        STATIC FUNCTION PROTOTYPES
=============================================================================*/
GST_DEBUG_CATEGORY_STATIC(mfw_gst_bsacdec_debug);
static void mfw_gst_bsacdec_class_init(MFW_GST_BSACDEC_CLASS_T *
					  klass);
static void mfw_gst_bsacdec_base_init(MFW_GST_BSACDEC_CLASS_T *
					 klass);
static void mfw_gst_bsacdec_init(MFW_GST_BSACDEC_INFO_T *
				    bsacdec_info);
static void mfw_gst_bsacdec_set_property(GObject * object,
					    guint prop_id,
					    const GValue * value,
					    GParamSpec * pspec);
static void mfw_gst_bsacdec_get_property(GObject * object,
					    guint prop_id, GValue * value,
					    GParamSpec * pspec);
static gboolean mfw_gst_bsacdec_set_caps(GstPad * pad, GstCaps * caps);
static GstFlowReturn mfw_gst_bsacdec_chain(GstPad * pad,
					      GstBuffer * buf);
static gboolean mfw_gst_bsacdec_sink_event(GstPad *, GstEvent *);
static gboolean mfw_gst_bsacdec_src_event(GstPad *, GstEvent *);
static gboolean plugin_init(GstPlugin * plugin);
static gboolean mfw_gst_bsacdec_seek(MFW_GST_BSACDEC_INFO_T *,
					GstPad *, GstEvent *);
static gboolean mfw_gst_bsacdec_convert_src(GstPad * pad,
					       GstFormat src_format,
					       gint64 src_value,
					       GstFormat * dest_format,
					       gint64 * dest_value);
static gboolean mfw_gst_bsacdec_convert_sink(GstPad * pad,
						GstFormat src_format,
						gint64 src_value,
						GstFormat * dest_format,
						gint64 * dest_value);
static gboolean mfw_gst_bsacdec_src_query(GstPad *, GstQuery *);
static const GstQueryType *mfw_gst_bsacdec_get_query_types(GstPad *
							      pad);
static gboolean mfw_gst_bsacdec_mem_flush(MFW_GST_BSACDEC_INFO_T *);

gint    App_get_adts_header(AACD_Block_Params * params,
                           MFW_GST_BSACDEC_INFO_T *bsacdec_info);
gulong  App_bs_look_bits (gint nbits,MFW_GST_BSACDEC_INFO_T *bsacdec_info);
gulong  App_bs_read_bits (gint nbits,MFW_GST_BSACDEC_INFO_T *bsacdec_info);
gint    App_bs_byte_align(MFW_GST_BSACDEC_INFO_T *bsacdec_info);
gint    App_bs_refill_buffer(MFW_GST_BSACDEC_INFO_T *bsacdec_info);
void    App_bs_readinit(gchar *buf, gint bytes,
                        MFW_GST_BSACDEC_INFO_T *bsacdec_info);
gint    App_FindFileType(gint val,MFW_GST_BSACDEC_INFO_T *bsacdec_info);
gint    App_get_prog_config(AACD_ProgConfig * p,
                            MFW_GST_BSACDEC_INFO_T *bsacdec_info);
gint    App_get_adif_header(AACD_Block_Params * params,MFW_GST_BSACDEC_INFO_T *bsacdec_info);
int App_init_raw(AACD_Block_Params * params,int channel_config,int sampling_frequency);
int ADTSBitrate(AACD_Decoder_info *dec_info,MFW_GST_BSACDEC_INFO_T * bsacdec_info);
void trnsptAdjustBitrate(unsigned int offset, unsigned int frameSize,unsigned int bufferFullness,MFW_GST_BSACDEC_INFO_T * bsacdec_info);
 void *aacd_alloc_fast(gint size);

/*=============================================================================
                            STATIC VARIABLES
=============================================================================*/
static GstElementClass *parent_class_aac = NULL;
static MFW_GST_BSACDEC_INFO_T *bsacdec_global_ptr = NULL;

/*=============================================================================
                            LOCAL FUNCTIONS
=============================================================================*/



BitstreamParam App_bs_param;
 //int bitstream_count;
 char bitstream_buf[MAX_ENC_BUF_SIZE];
// int bitstream_buf_index;
// int in_buf_done;
 int bytes_supplied;
 unsigned char App_ibs_buf[INTERNAL_BS_BUFSIZE];
 

 
/***************************************************************
 *  FUNCTION NAME - App_bs_look_bits
 *
 *  DESCRIPTION
 *      Get the required number of bits from the bit-register
 *
 *  ARGUMENTS
 *      nbits - Number of bits required
 *
 *  RETURN VALUE
 *      Required bits
 *
 **************************************************************/
unsigned long App_bs_look_bits (int nbits,MFW_GST_BSACDEC_INFO_T *bsacdec_info)
{
   BitstreamParam *b = &(bsacdec_info->app_params.bs_param);
    return b->bit_register;
}


/***************************************************************
 *  FUNCTION NAME - App_FindFileType
 *
 *  DESCRIPTION
 *      Find if the file is of type, ADIF or ADTS
 *
 *  ARGUMENTS
 *      val     -   First 4 bytes in the stream
 *
 *  RETURN VALUE
 *      0 - Success
 *     -1 - Error
 *
 **************************************************************/
int App_FindFileType(int val,MFW_GST_BSACDEC_INFO_T *bsacdec_info)
{

    
  if (val == 0x41444946)
    {
      bsacdec_info->app_params.App_adif_header_present = TRUE;
    }
  else
    {
      if ((val & 0xFFF00000) == 0xFFF00000)
	    bsacdec_info->app_params.App_adts_header_present = TRUE;
      else
	return(-1);
    }
  return(0);
}

int ADTSBitrate(AACD_Decoder_info *dec_info,
                MFW_GST_BSACDEC_INFO_T *bsacdec_info)
{
    AACD_Block_Params * params;
    params = &bsacdec_info->app_params.BlockParams;
    unsigned int unSampleRate = sampling_frequency[bsacdec_info->SampFreqIdx] ;
    GST_DEBUG("Sampling Frequency=%d\n",unSampleRate);
    const unsigned int unFrameSamples = FRAMESIZE ;
    dec_info->aacd_bit_rate =
        ((gfloat)bsacdec_info->bitsPerFrame * unSampleRate)/unFrameSamples;
    GST_DEBUG("Bitrate=%d\n",dec_info->aacd_bit_rate);
    return 1 ;
}





/***************************************************************
 *  FUNCTION NAME - App_bs_refill_buffer
 *
 *  DESCRIPTION
 *      Fill the bitstream buffer with new buffer.
 *
 *  ARGUMENTS
 *      None
 *
 *  RETURN VALUE
 *      0  - success
 *      -1 - error
 *
 **************************************************************/
int App_bs_refill_buffer (MFW_GST_BSACDEC_INFO_T *bsacdec_info)
{
    BitstreamParam *b = &(App_bs_param);
    int bytes_to_copy;
    unsigned int len;


    bytes_to_copy = b->bs_end_ext - b->bs_curr_ext;
    if (bytes_to_copy <= 0)
    {
      if (bsacdec_info->bitstream_count <=0)
	return(-1);
      else
	{
	  len = (bsacdec_info->bitstream_count > BS_BUF_SIZE) ? BS_BUF_SIZE : bsacdec_info->bitstream_count;

	  b->bs_curr_ext = (unsigned char *)(bitstream_buf + bsacdec_info->bitstream_buf_index);
	  b->bs_end_ext = b->bs_curr_ext + len;
	  bytes_to_copy = len;

	  bsacdec_info->bitstream_buf_index += len;
	  bsacdec_info->bitstream_count -= len;
	  bsacdec_info->in_buf_done    += len;
	  bytes_supplied = len;

	  /* Set only if previous Seeking is done */
	  /*
	    if (b->bs_seek_needed == 0)
            b->bs_seek_needed = SeekFlag;
	 */ 
	}

    }

    if (bytes_to_copy > INTERNAL_BS_BUFSIZE)
      bytes_to_copy = INTERNAL_BS_BUFSIZE;

    b->bs_curr = App_ibs_buf;
    memcpy(b->bs_curr, b->bs_curr_ext, bytes_to_copy);
    b->bs_curr_ext += bytes_to_copy;
    b->bs_end = b->bs_curr + bytes_to_copy;

    return 0;

}


/***********************************************************************
 *
 *   FUNCTION NAME - App_bs_readinit
 *
 *   DESCRIPTION
 *      This module initializes the BitStream Parameteres.
 *
 *   ARGUMENTS
 *       buf       -  Buffer from which, data is to be copied
 *                    into internal-buffer and bit-register
 *       bytes     -  Size of the above buffer in bytes.
 *
 *   RETURN VALUE
 *      None
 **********************************************************************/
void App_bs_readinit(char *buf, int bytes,MFW_GST_BSACDEC_INFO_T *bsacdec_info)
{

    BitstreamParam *b = &(bsacdec_info->app_params.bs_param);
    unsigned int temp;
    int ret;

    b->bs_curr = (unsigned char *)buf;
    b->bs_end = b->bs_curr + bytes;
    b->bs_eof = 0;
    b->bs_seek_needed = 0;
    b->bit_register = 0;
    b->bit_counter = BIT_COUNTER_INIT;

    while (b->bit_counter >= 0)
    {
       if (b->bs_curr >= b->bs_end)
        {
            ret = App_bs_refill_buffer(bsacdec_info);
            if (ret < 0)
                break;
        }

		
        temp = *b->bs_curr++;
        b->bit_register = b->bit_register | (temp << b->bit_counter);
        b->bit_counter -= 8;
    }

}

/***************************************************************
 *  FUNCTION NAME - App_bs_read_bits
 *
 *  DESCRIPTION
 *      Reads the given number of bits from the bit-register
 *
 *  ARGUMENTS
 *      nbits - Number of bits required
 *
 *  RETURN VALUE
 *      - Required bits
 *      - -1 in case of end of bitstream
 *
 **************************************************************/
unsigned long App_bs_read_bits (int nbits,MFW_GST_BSACDEC_INFO_T *bsacdec_info)
{
    BitstreamParam *b = &(bsacdec_info->app_params.bs_param);
    unsigned long temp,temp1,temp_bit_register;
    long temp_bit_counter;
    int ret;

    bsacdec_info->app_params.BitsInHeader += nbits;


    temp_bit_counter = b->bit_counter;
    temp_bit_register = b->bit_register;

    /* If more than available bits are requested,
     * return error
     */
    if ((MIN_REQD_BITS - temp_bit_counter) < nbits)
        return 0;


    temp = temp_bit_register >> (32 - nbits);
    temp_bit_register <<= nbits;
    temp_bit_counter += nbits;

    while (temp_bit_counter >= 0)
    {
        if (b->bs_curr >= b->bs_end)
        {
            ret = App_bs_refill_buffer(bsacdec_info);
            if (ret < 0)
            {
                b->bit_register = temp_bit_register;
                b->bit_counter = temp_bit_counter;

                return(temp);
            }
        }

        temp1 = *b->bs_curr++;
        temp_bit_register = temp_bit_register | (temp1 << temp_bit_counter);
        temp_bit_counter -= 8;
     }

     b->bit_register = temp_bit_register;
     b->bit_counter = temp_bit_counter;


    return(temp);
}

/*******************************************************************************
 *
 *   FUNCTION NAME - App_bs_byte_align
 *
 *   DESCRIPTION
 *       This function makes the number of bits in the bit register
 *       to be a multiple of 8.
 *
 *   ARGUMENTS
 *       None
 *
 *   RETURN VALUE
 *       number of bits discarded.
*******************************************************************************/
int App_bs_byte_align(MFW_GST_BSACDEC_INFO_T *bsacdec_info)
{
    BitstreamParam *b = &(bsacdec_info->app_params.bs_param);
    int             nbits;

    nbits = MIN_REQD_BITS - b->bit_counter;
    nbits = nbits & 0x7;    /* LSB 3 bits */

    bsacdec_info->app_params.BitsInHeader += nbits;


    b->bit_register <<= nbits;
    b->bit_counter += nbits;


    return (nbits);
}


/*******************************************************************************
 *
 *   FUNCTION NAME - App_get_ele_list
 *
 *   DESCRIPTION
 *       Gets a list of elements present in the current data block
 *
 *   ARGUMENTS
 *       p           -  Pointer to array of Elements.
 *       enable_cpe  _  Flag to indicate whether channel paired element is
 *                      present.
 *
 *   RETURN VALUE
 *       None
*******************************************************************************/
static void App_get_ele_list(AACD_EleList * p, int enable_cpe,MFW_GST_BSACDEC_INFO_T *bsacdec_info)
{
    int             i,
                    j;

    for (i = 0, j = p->num_ele; i < j; i++)
    {
        if (enable_cpe)
            p->ele_is_cpe[i] = App_bs_read_bits(LEN_ELE_IS_CPE,bsacdec_info);
        else
            p->ele_is_cpe[i] = 0;
        p->ele_tag[i] = App_bs_read_bits(LEN_TAG,bsacdec_info);
    }

}



/*******************************************************************************
 *
 *   FUNCTION NAME - App_get_prog_config
 *
 *   DESCRIPTION
 *       Read the program configuration element from the data block
 *
 *   ARGUMENTS
 *       p           -  Pointer to a structure to store the new program
 *                       configuration.
 *
 *   RETURN VALUE
 *         Success :  0
 *
 *         Error   : -1
 *
*******************************************************************************/
int App_get_prog_config(AACD_ProgConfig * p,MFW_GST_BSACDEC_INFO_T *bsacdec_info)
{
    int             i,
                    j;


    p->tag = App_bs_read_bits(LEN_TAG,bsacdec_info);

    p->profile = App_bs_read_bits(LEN_PROFILE,bsacdec_info);
    if (p->profile != 1)
    {
        return -1;
    }
    p->sampling_rate_idx = App_bs_read_bits(LEN_SAMP_IDX,bsacdec_info);
    if (p->sampling_rate_idx >= 0xc)
    {
        return -1;
    }
    p->front.num_ele = App_bs_read_bits(LEN_NUM_ELE,bsacdec_info);
    if (p->front.num_ele > FCHANS)
    {
        return -1;
    }
    p->side.num_ele = App_bs_read_bits(LEN_NUM_ELE,bsacdec_info);
    if (p->side.num_ele > SCHANS)
    {
        return -1;
    }
    p->back.num_ele = App_bs_read_bits(LEN_NUM_ELE,bsacdec_info);
    if (p->back.num_ele > BCHANS)
    {
        return -1;
    }
    p->lfe.num_ele = App_bs_read_bits(LEN_NUM_LFE,bsacdec_info);
    if (p->lfe.num_ele > LCHANS)
    {
        return -1;
    }
    p->data.num_ele = App_bs_read_bits(LEN_NUM_DAT,bsacdec_info);
    p->coupling.num_ele = App_bs_read_bits(LEN_NUM_CCE,bsacdec_info);
    if (p->coupling.num_ele > CCHANS)
    {
        return -1;
    }
    if ((p->mono_mix.present = App_bs_read_bits(LEN_MIX_PRES,bsacdec_info)) == 1)
        p->mono_mix.ele_tag = App_bs_read_bits(LEN_TAG,bsacdec_info);
    if ((p->stereo_mix.present = App_bs_read_bits(LEN_MIX_PRES,bsacdec_info)) == 1)
        p->stereo_mix.ele_tag = App_bs_read_bits(LEN_TAG,bsacdec_info);
    if ((p->matrix_mix.present = App_bs_read_bits(LEN_MIX_PRES,bsacdec_info)) == 1)
    {
        p->matrix_mix.ele_tag = App_bs_read_bits(LEN_MMIX_IDX,bsacdec_info);
        p->matrix_mix.pseudo_enab = App_bs_read_bits(LEN_PSUR_ENAB,bsacdec_info);
    }
    App_get_ele_list(&p->front, 1,bsacdec_info);
    App_get_ele_list(&p->side, 1,bsacdec_info);
    App_get_ele_list(&p->back, 1,bsacdec_info);
    App_get_ele_list(&p->lfe, 0,bsacdec_info);
    App_get_ele_list(&p->data, 0,bsacdec_info);
    App_get_ele_list(&p->coupling, 1,bsacdec_info);

    App_bs_byte_align(bsacdec_info);

    j = App_bs_read_bits(LEN_COMMENT_BYTES,bsacdec_info);


    /*
     * The comment bytes are overwritten onto the same location, to
     * save memory.
     */

    for (i = 0; i < j; i++)
        p->comments[0] = App_bs_read_bits(LEN_BYTE,bsacdec_info);
    /* null terminator for string */
    p->comments[0] = 0;

    return 0;

}



/*******************************************************************************
 *
 *   FUNCTION NAME - App_get_adif_header
 *
 *   DESCRIPTION
 *       Gets ADIF header from the input bitstream.
 *
 *   ARGUMENTS
 *         params  -  place to store the adif-header data
 *
 *   RETURN VALUE
 *         Success :  1
 *         Error   : -1
*******************************************************************************/
int App_get_adif_header(AACD_Block_Params * params,MFW_GST_BSACDEC_INFO_T *bsacdec_info)
{
    int             i,
                    n,
                    select_status;
    AACD_ProgConfig      *tmp_config;
    ADIF_Header     temp_adif_header;
    ADIF_Header*    p = &(temp_adif_header);

    /* adif header */
    for (i = 0; i < LEN_ADIF_ID; i++)
         p->adif_id[i] = App_bs_read_bits(LEN_BYTE,bsacdec_info);
    p->adif_id[i] = 0;  /* null terminated string */

#ifdef UNIX
    /* test for id */
    if (strncmp(p->adif_id, "ADIF", 4) != 0)
        return -1;  /* bad id */
#else
    /* test for id */
    if (*((unsigned long *) p->adif_id) != *((unsigned long *) "ADIF"))
        return -1;  /* bad id */
#endif

    /* copyright string */
    if ((p->copy_id_present = App_bs_read_bits(LEN_COPYRT_PRES,bsacdec_info)) == 1)
    {
        for (i = 0; i < LEN_COPYRT_ID; i++)
            p->copy_id[i] = (char)App_bs_read_bits(LEN_BYTE,bsacdec_info);

        /* null terminated string */
        p->copy_id[i] = 0;
    }
    p->original_copy = App_bs_read_bits(LEN_ORIG,bsacdec_info);
    p->home = App_bs_read_bits(LEN_HOME,bsacdec_info);
    p->bitstream_type = App_bs_read_bits(LEN_BS_TYPE,bsacdec_info);
    p->bitrate = App_bs_read_bits(LEN_BIT_RATE,bsacdec_info);

    /* program config elements */
    select_status = -1;
    n = App_bs_read_bits(LEN_NUM_PCE,bsacdec_info) + 1;

    tmp_config = (AACD_ProgConfig*) aacd_alloc_fast (n*sizeof(AACD_ProgConfig));

    for (i = 0; i < n; i++)
    {
        tmp_config[i].buffer_fullness =
            (p->bitstream_type == 0) ? App_bs_read_bits(LEN_ADIF_BF,bsacdec_info) : 0;


        if (App_get_prog_config(&tmp_config[i],bsacdec_info))
        {
            return -1;
        }

	select_status = 1;
    }

    App_bs_byte_align(bsacdec_info);

    /* Fill in the AACD_Block_Params struct now */
    
    params->num_pce = n;
    params->pce     = tmp_config;
    params->BitstreamType = p->bitstream_type;
    params->BitRate       = p->bitrate;
    params->ProtectionAbsent = 0;
    return select_status;
}




/*******************************************************************************
 *
 *   FUNCTION NAME - App_get_adts_header
 *
 *   DESCRIPTION
 *       Searches and syncs to ADTS header from the input bitstream. It also
 *       gets the full ADTS header once sync is obtained.
 *
 *   ARGUMENTS
 *       params   - Place to store the header data
 *
 *   RETURN VALUE
 *       Success : 0
 *         Error : -1
*******************************************************************************/
int App_get_adts_header(AACD_Block_Params * params,MFW_GST_BSACDEC_INFO_T *bsacdec_info)
{
    ADTS_Header App_adts_header;
    ADTS_Header *p = &(App_adts_header);

    int bits_used = 0;
	//// bitrate support for adts header - tlsbo79743
	int start_bytes=0;
	int bits_consumed=0;
		unsigned int unSampleRate;
	const unsigned int unFrameSamples = 1024 ;
	unsigned int bufferFullness;
	const unsigned char channelConfig2NCC[8] = {0,1,2,3,4,5,6,7};
#ifdef OLD_FORMAT_ADTS_HEADER
    int emphasis;
#endif


    while (1)
   {
        /*
         * If we have used up more than maximum possible frame for finding
         * the ADTS header, then something is wrong, so exit.
         */
        if (bits_used > (LEN_BYTE * ADTS_FRAME_MAX_SIZE))
            return -1;

        /* ADTS header is assumed to be byte aligned */
        bits_used += App_bs_byte_align(bsacdec_info);

#ifdef CRC_CHECK
        /* Add header bits to CRC check */
        UpdateCRCStructBegin();
#endif

        p->syncword = App_bs_read_bits ((LEN_SYNCWORD - LEN_BYTE),bsacdec_info);
        bits_used += LEN_SYNCWORD - LEN_BYTE;

        /* Search for syncword */
        while (p->syncword != ((1 << LEN_SYNCWORD) - 1))
        {
            p->syncword = ((p->syncword << LEN_BYTE) | App_bs_read_bits(LEN_BYTE,bsacdec_info));
            p->syncword &= (1 << LEN_SYNCWORD) - 1;
            bits_used += LEN_BYTE;
            /*
             * If we have used up more than maximum possible frame for finding
             * the ADTS header, then something is wrong, so exit.
             */
            if (bits_used > (LEN_BYTE * ADTS_FRAME_MAX_SIZE))
                return -1;
	}

          bits_consumed = bits_used -  LEN_SYNCWORD; // bitrate support for adts header - tlsbo79743
	
         p->id = App_bs_read_bits(LEN_ID,bsacdec_info); 
	bits_used += LEN_ID;

	/*
	  Disabled version check in allow MPEG2/4 streams
	  0 - MPEG4
	  1 - MPEG2
	  if (!p->id)
	  {
	  continue;
	  }
	*/

        p->layer = App_bs_read_bits(LEN_LAYER,bsacdec_info);
	bits_used += LEN_LAYER;
	if (p->layer != 0)
	  {
	    continue;
	  }

	p->protection_abs = App_bs_read_bits(LEN_PROTECT_ABS,bsacdec_info);
	bits_used += LEN_PROTECT_ABS;

	p->profile = App_bs_read_bits(LEN_PROFILE,bsacdec_info);
        bits_used += LEN_PROFILE;
	if (p->profile != 1)
	  {
            continue;
	  }

	p->sampling_freq_idx = App_bs_read_bits(LEN_SAMP_IDX,bsacdec_info);
        bits_used += LEN_SAMP_IDX;
	if (p->sampling_freq_idx >= 0xc)
	  {
            continue;
	  }

	p->private_bit = App_bs_read_bits(LEN_PRIVTE_BIT,bsacdec_info);
        bits_used += LEN_PRIVTE_BIT;

        //temp_channel_config = p->channel_config;
	p->channel_config = App_bs_read_bits(LEN_CHANNEL_CONF,bsacdec_info);
        bits_used += LEN_CHANNEL_CONF;
        ///* Audio mode has changed, so config has to be built up again */
        //if (temp_channel_config != p->channel_config)
	//ptr->AACD_default_config = 1;
	p->original_copy = App_bs_read_bits(LEN_ORIG,bsacdec_info);
        bits_used += LEN_ORIG;

	p->home = App_bs_read_bits(LEN_HOME,bsacdec_info);
        bits_used += LEN_HOME;

#ifdef OLD_FORMAT_ADTS_HEADER
	params->Flush_LEN_EMPHASIS_Bits = 0;
	if (p->id == 0)
	  {
	    emphasis = App_bs_read_bits(LEN_EMPHASIS,bsacdec_info);
	    bits_used += LEN_EMPHASIS;
	    params->Flush_LEN_EMPHASIS_Bits = 1;
	  }

#endif

	p->copyright_id_bit = App_bs_read_bits(LEN_COPYRT_ID_ADTS,bsacdec_info);
        bits_used += LEN_COPYRT_ID_ADTS;

	p->copyright_id_start = App_bs_read_bits(LEN_COPYRT_START,bsacdec_info);
        bits_used += LEN_COPYRT_START;

	p->frame_length = App_bs_read_bits(LEN_FRAME_LEN,bsacdec_info);
        bits_used += LEN_FRAME_LEN;

	p->adts_buffer_fullness = App_bs_read_bits(LEN_ADTS_BUF_FULLNESS,bsacdec_info);
        bits_used += LEN_ADTS_BUF_FULLNESS;

	p->num_of_rdb = App_bs_read_bits(LEN_NUM_OF_RDB,bsacdec_info);
        bits_used += LEN_NUM_OF_RDB;

   /*
     Removed, constraint: num_of_rdb == 0 because we can support more than
     one raw data block in 1 adts frame
     if (p->num_of_rdb != 0)
     {
     continue;
     }
   */

        /*
         * If this point is reached, then the ADTS header has been found and
         * CRC structure can be updated.
         */
#ifdef CRC_CHECK
        /*
         * Finish adding header bits to CRC check. All bits to be CRC
         * protected.
         */
        UpdateCRCStructEnd(0);
#endif
        /*
         * Adjust the received frame length to add the bytes used up to
         * find the ADTS header.
         */
        p->frame_length += (bits_used / LEN_BYTE) - ADTS_FRAME_HEADER_SIZE;

        if (p->protection_abs == 0)
	  {
	    p->crc_check = App_bs_read_bits(LEN_CRC,bsacdec_info);
            bits_used += LEN_CRC;
	  }

        /* Full header successfully obtained, so get out of the search */
        break;
   }

#ifndef OLD_FORMAT_ADTS_HEADER
    bits_used += App_bs_byte_align(bsacdec_info);
#else
    if (p->id != 0) // MPEG-2 style : Emphasis field is absent
      {
	bits_used += App_bs_byte_align(bsacdec_info);
      }
    else //MPEG-4 style : Emphasis field is present; cancel its effect
      {
	bsacdec_info->app_params.BitsInHeader -= LEN_EMPHASIS;
      }
#endif

    /* Fill in the AACD_Block_Params struct now */

    params->num_pce = 0;
    params->ChannelConfig = p->channel_config;
    params->SamplingFreqIndex = p->sampling_freq_idx;
    params->BitstreamType = (p->adts_buffer_fullness == 0x7ff) ? 1 : 0;



    params->BitRate       = 0; /* Caution !*/

    /* The ADTS stream contains the value of buffer fullness in units
       of 32-bit words. But the application is supposed to deliver this
       in units of bits. Hence we do a left-shift */

    params->BufferFullness = (p->adts_buffer_fullness) << 5;
    params->ProtectionAbsent = p->protection_abs;
    params->CrcCheck = p->crc_check;

    /* Dexter add for test */
    params->frame_length  = p->frame_length;
    
    //ptr->AACD_mc_info.profile = p->profile;
    //ptr->AACD_mc_info.sampling_rate_idx = p->sampling_freq_idx;
    //AACD_infoinit(&(ptr->tbl_ptr_AACD_samp_rate_info[ptr->AACD_mc_info.sampling_rate_idx]), ptr);

	///////////////////////  bitrate support for adts header - tlsbo79743 /////////////////////////////
	
	bufferFullness = p->adts_buffer_fullness*32*channelConfig2NCC[p->channel_config];

	trnsptAdjustBitrate(bits_consumed,  8*p->frame_length, bufferFullness,bsacdec_info);

    return 0;

}

/*******************************************************************************
 *
 *   FUNCTION NAME - App_get_mp4forbsac_header
 *
 *   DESCRIPTION
 *       Gets mp4forbsac header from the input bitstream.
 *
 *   ARGUMENTS
 *         params  -  place to store the mp4forbsac-header data
 *
 *   RETURN VALUE
 *         Success :  1
 *         Error   : -1
*******************************************************************************/
int App_get_mp4forbsac_header(AACD_Block_Params * params, MFW_GST_BSACDEC_INFO_T * bsacdec_info)
{

    int byte1,byte2,byte3,byte4;

    byte1 = App_bs_read_bits(8, bsacdec_info);
    byte2 = App_bs_read_bits(8, bsacdec_info);
    byte3 = App_bs_read_bits(8, bsacdec_info);
    byte4 = App_bs_read_bits(8, bsacdec_info);
    params->scalOutObjectType = byte1 + (byte2 <<8) + (byte3 <<16) + (byte4 <<24);

    byte1 = App_bs_read_bits(8, bsacdec_info);
    byte2 = App_bs_read_bits(8, bsacdec_info);
    byte3 = App_bs_read_bits(8, bsacdec_info);
    byte4 = App_bs_read_bits(8, bsacdec_info);
    params->scalOutNumChannels = byte1 + (byte2 <<8) + (byte3 <<16) + (byte4 <<24);

    byte1 = App_bs_read_bits(8, bsacdec_info);
    byte2 = App_bs_read_bits(8, bsacdec_info);
    byte3 = App_bs_read_bits(8, bsacdec_info);
    byte4 = App_bs_read_bits(8, bsacdec_info);
    params->sampleRate = byte1 + (byte2 <<8) + (byte3 <<16) + (byte4 <<24);


    if(params->sampleRate == 96000) params->SamplingFreqIndex = 0;
    else if( params->sampleRate == 88200) params->SamplingFreqIndex = 1;
    else if( params->sampleRate == 64000) params->SamplingFreqIndex = 2;
    else if( params->sampleRate == 48000) params->SamplingFreqIndex = 3;
    else if( params->sampleRate == 44100) params->SamplingFreqIndex = 4;
    else if( params->sampleRate == 32000) params->SamplingFreqIndex = 5;
    else if( params->sampleRate == 24000) params->SamplingFreqIndex = 6;
    else if( params->sampleRate == 22050) params->SamplingFreqIndex = 7;
    else if( params->sampleRate == 16000) params->SamplingFreqIndex = 8;
    else if( params->sampleRate == 12000) params->SamplingFreqIndex = 9;
    else if( params->sampleRate == 11025) params->SamplingFreqIndex = 10;
    else if( params->sampleRate ==  8000) params->SamplingFreqIndex = 11;
    else if( params->sampleRate ==  7350) params->SamplingFreqIndex = 12;
    else params->SamplingFreqIndex = 13;




    byte1 = App_bs_read_bits(8, bsacdec_info);
    byte2 = App_bs_read_bits(8, bsacdec_info);
    byte3 = App_bs_read_bits(8, bsacdec_info);
    byte4 = App_bs_read_bits(8, bsacdec_info);
    params->framelengthflag = byte1 + (byte2 <<8) + (byte3 <<16) + (byte4 <<24);


    return 1;
}


void trnsptAdjustBitrate(unsigned int offset,
                         unsigned int frameSize,
                         unsigned int bufferFullness,MFW_GST_BSACDEC_INFO_T * bsacdec_info)
{
    if (!bsacdec_info->nFramesReceived) /* the first time around */
        firstBufferFullness = bufferFullness ;
    else if (bsacdec_info->nBitsReceived < (1UL<<(30-BITSPERFRAME_SCALE)))
        bsacdec_info->nBitsReceived += frameSize + offset ;

    if (!offset)
    {
        lastGoodBufferFullness = bufferFullness ;
        /* stop bitrate calculation before we have to do long divisions */
        if (bsacdec_info->nFramesReceived && bsacdec_info->nBitsReceived < (1UL<<(30-BITSPERFRAME_SCALE)))
            bsacdec_info->bitsPerFrame = ((bsacdec_info->nBitsReceived + bufferFullness - firstBufferFullness) <<BITSPERFRAME_SCALE) / bsacdec_info->nFramesReceived ;
        bsacdec_info->nFramesReceived ++ ;
        // fprintf(stderr,"bitsPerFrame = %d (received = %d)\n",h->bitsPerFrame>>BITSPERFRAME_SCALE,h->nFramesReceived);
    }
    else
    {
        /* if offset !=0, we skipped frames. Keep bitrate estimate and instead estimate
           number of frames skipped */
        if (bsacdec_info->bitsPerFrame) /* only if we have a br estimate already */
        {
            unsigned int nFramesSkipped = (((offset + bufferFullness - lastGoodBufferFullness) << BITSPERFRAME_SCALE) + (bsacdec_info->bitsPerFrame>>1)) / bsacdec_info->bitsPerFrame ;
            bsacdec_info->nFramesReceived += nFramesSkipped ;
//            fprintf(stderr,"bitsPerFrame = %d (received = %d, skipped = %d)\n",h->bitsPerFrame>>BITSPERFRAME_SCALE,h->nFramesReceived,nFramesSkipped);
        }
    }
}


#ifdef PUSH_MODE

#define COPY_BLOCK_TIMESTAMP(des, src) \
    do { \
        des->buflen = src->buflen; \
        des->timestamp = src->timestamp; \
    }while(0)

void init_tsmanager(Timestamp_Manager *tm)
{
    memset(tm, 0, sizeof(Timestamp_Manager));
}
    
void deinit_tsmanager(Timestamp_Manager *tm)
{
    if (tm->allocatedbuffer){
        g_free(tm->allocatedbuffer);
    }
    memset(tm, 0, sizeof(Timestamp_Manager));
}

void clear_tsmanager(Timestamp_Manager *tm)
{
    int i;
    Block_Timestamp * bt = tm->allocatedbuffer;
    tm->freelist = tm->head = tm->tail = NULL;
    for (i=0;i<tm->allocatednum;i++){
        bt->next = tm->freelist;
        tm->freelist = bt;
        bt++;
    }
}

Block_Timestamp * new_block_timestamp(Timestamp_Manager *tm)
{
    Block_Timestamp * newbuffer;
    if (tm->freelist){
        newbuffer = tm->freelist;
        tm->freelist = newbuffer->next;
        return newbuffer;
    }
    if (tm->allocatednum)
        tm->allocatednum <<=1;
    else
        tm->allocatednum = 4;
    if (newbuffer=g_malloc(sizeof(Block_Timestamp)*tm->allocatednum)) {
        Block_Timestamp *oldhead, *nb;
        int i = 0;
        
        oldhead = tm->head;
        nb = newbuffer;
        tm->freelist = tm->head = tm->tail = NULL;
        for (i=0;i<(tm->allocatednum-1);i++){
            if (oldhead){
                COPY_BLOCK_TIMESTAMP(nb, oldhead);
                nb->next = NULL;
                if (tm->tail){
                    (tm->tail)->next = nb;
                    tm->tail = nb;
                }else{
                    tm->head = tm->tail = nb;
                }
                oldhead = oldhead->next;
            }else{
                nb->next = tm->freelist;
                tm->freelist = nb;
            }
            nb++;
        }
        if (tm->allocatedbuffer){
            g_free(tm->allocatedbuffer);
        }
        tm->allocatedbuffer = newbuffer;
        return nb;
    }else{
        return newbuffer;
    }
}

gboolean push_block_with_timestamp(Timestamp_Manager *tm, guint blen, GstClockTime timestamp)
{
    Block_Timestamp * bt;
    if (bt = new_block_timestamp(tm)){
        bt->buflen = blen;
        bt->timestamp = timestamp;
        bt->next = NULL;
        if (tm->tail){
            (tm->tail)->next = bt;
            tm->tail = bt;
        }else{
            tm->head = tm->tail = bt;
        }
        return TRUE;
    }else{
        return FALSE;
    }
}

GstClockTime get_timestamp_with_length(Timestamp_Manager *tm, guint length)
{
    GstClockTime ts = -1;
    Block_Timestamp * bt = tm->head;
    if (bt){
        ts = bt->timestamp;
        while(length>=bt->buflen){
            length-=bt->buflen;
            if (bt==tm->tail){
                tm->tail=NULL;
            }
            tm->head = bt->next;
            bt->next = tm->freelist;
            tm->freelist = bt;
            bt = tm->head;
            if (!bt) break;
        }
        if (bt){
            bt->buflen-=length;
        }
    }
    return ts;
}

guint get_tsmanager_length(Timestamp_Manager *tm)
{
    guint len = 0;
    Block_Timestamp * bt = tm->head;
    while(bt){
        len+=bt->buflen;
        bt=bt->next;
    }
    return len;
}

#endif



/*=============================================================================
FUNCTION: mfw_gst_bsacdec_set_property

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
mfw_gst_bsacdec_set_property(GObject * object, guint prop_id,
				const GValue * value, GParamSpec * pspec)
{

    GST_DEBUG(" in mfw_gst_bsacdec_set_property routine \n");
    GST_DEBUG(" out of mfw_gst_bsacdec_set_property routine \n");
}

/*=============================================================================
FUNCTION: mfw_gst_bsacdec_set_property

DESCRIPTION: gets the property of the element

ARGUMENTS PASSED:
        object     - pointer to the elements object
        prop_id    - ID of the property;
        value      - value of the property got from the application
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
mfw_gst_bsacdec_get_property(GObject * object, guint prop_id,
				GValue * value, GParamSpec * pspec)
{
    GST_DEBUG(" in mfw_gst_bsacdec_get_property routine \n");
    GST_DEBUG(" out of mfw_gst_bsacdec_get_property routine \n");

}


/***************************************************************************
*
*   FUNCTION NAME - alloc_fast
*
*   DESCRIPTION
*          This function simulates to allocate memory in the internal
*          memory. This function when used by the application should
*          ensure that the memory address returned in always aligned
*          to long boundry.
*
*   ARGUMENTS
*       size              - Size of the memory requested.
*
*   RETURN VALUE
*       base address of the memory chunck allocated.
*
***************************************************************************/
 void *aacd_alloc_fast(gint size)
{
    void *ptr = NULL;
    ptr = (void *) g_malloc(size + 4);
    ptr =
	(void *) (((long) ptr + (long) (LONG_BOUNDARY - 1)) &
		  (long) (~(LONG_BOUNDARY - 1)));
    return ptr;

}
/***************************************************************************
*
*   FUNCTION NAME - app_swap_buffers_aac_dec
*
*   DESCRIPTION
*       This function is a callback by instance of AACD decoder requesting
*       the application for a new input buffer. It returns the used buffer
*       pointer to the application and expects a new buffer pointer and
*       buffer length in return. If buffer is not available it returns
*       a NULL pointer with zero length to the AACD decoder instance. This
*       is an example code.
*
*       The variable bytes_supplied keeps track of the total number
*       of bytes used by the decoder, since the last call to the decoder.
*
*       Also see, update_bitstream_status() function below.
*
*   ARGUMENTS
*       new_buf_ptr       - Pointer to pointer of used buffer
*       new_buf_len       - Pointer to the length of the input buffer var
*       instance_id       - AACD decoder Instance ID
*
*   RETURN VALUE
*          0              - If buffer allocation is successfull.
*         -1              - Indicates 'End of Bitstream'.
*
***************************************************************************/

AACD_INT8 app_swap_buffers_aac_dec (AACD_UCHAR **new_buf_ptr,
                               AACD_UINT32 *new_buf_len,
                               void *global_struct)
{

    gint len;
    if (!bsacdec_global_ptr->inbuffer2) {
	*new_buf_ptr = NULL;
	*new_buf_len = 0;
	return (AACD_INT8) - 1;
    }
    *new_buf_ptr =
	(AACD_UCHAR *) GST_BUFFER_DATA(bsacdec_global_ptr->inbuffer2);
    *new_buf_len = GST_BUFFER_SIZE(bsacdec_global_ptr->inbuffer2);

    return (0);
}

/***************************************************************************
*
*   FUNCTION NAME - alloc_slow
*
*   DESCRIPTION
*          This function simulates to allocate memory in the internal
*          memory. This function when used by the application should
*          ensure that the memory address returned in always aligned
*          to long boundry.
*
*   ARGUMENTS
*       size              - Size of the memory requested.
*
*   RETURN VALUE
*       base address of the memory chunck allocated.
*
***************************************************************************/


static void *aacd_alloc_slow(gint size)
{
    void *ptr = NULL;
    ptr = (void *) g_malloc(size);
    ptr =
	(void *) (((long) ptr + (long) LONG_BOUNDARY - 1) &
		  (long) (~(LONG_BOUNDARY - 1)));
    return ptr;
}

/***************************************************************************
*
*   FUNCTION NAME - aacd_free
*
*   DESCRIPTION     This function frees the memory allocated previously
*   ARGUMENTS
*       mem       - memory address to be freed
*
*   RETURN VALUE
*       None
*
***************************************************************************/

static void aacd_free(void *mem)
{
    g_free(mem);
    return;
}


/***************************************************************************
*
*   FUNCTION NAME - mfw_gst_interleave_samples
*
*   DESCRIPTION
*                   This function interleaves the decoded left and right 
*                   channel output PCM samples
*   ARGUMENTS
*       data_ch0    - pointer to the decoded left channel output PCM samples
*       data_ch1    - pointer to the decoded right channel output PCM samples
*       data_out    - pointer to store the interleaved output
*       frameSize   - output frame size
*       channels    - number of output channels
*
*   RETURN VALUE
*       None
*
***************************************************************************/
static void mfw_gst_interleave_samples(AACD_OutputFmtType * data_ch0,
				       AACD_OutputFmtType * data_ch1,
				       short * data_out,
				       gint frameSize, gint channels)
{
    gint i;
    AACD_OutputFmtType tmp;

    for (i = 0; i < frameSize; i++) {
	*data_out++ = *data_ch0++;

	if (channels == 2)
	    *data_out++ = *data_ch1++;
    }

}

static void mfw_gst_2channel_32_to_16sample(AACD_OutputFmtType * data_in,
				       short * data_out,
				       gint frameSize, gint actualchannel)
{
    gint i,j;
    short  tmp, *src;
    for (i = 0; i < frameSize; i++) {
        if (G_LIKELY(actualchannel>1)){
            *data_out++ = data_in[0];
            *data_out++ = data_in[1];
        }else{
            tmp = data_in[0];
            *data_out++ = tmp;
            /* FixME: Output as mono to pass the comformance test.*/
            *data_out++ = tmp;
        }
        data_in+=actualchannel;
    }
}

static void mfw_gst_interleave_to_16samples(AACD_OutputFmtType * data_in,
				       short * data_out,
				       gint frameSize, gint actualchannel)
{
    gint i,j;
    short  tmp;
    AACD_OutputFmtType * src;
	for(i=0;i<frameSize;i++) {	
       src = data_in + i;
       if (G_LIKELY(actualchannel>1)) {
            for(j=0;j<actualchannel;j++)               
            {
            /* Dump all the channel information to comformance test */
            /* if (j <= 2) */
            *data_out++ = *src;
            src = src + AAC_FRAME_SIZE;
            }
        } else {
            *data_out++ = *src;
            *data_out++ = *src;
        }
    }

}



/***************************************************************************
*
*   FUNCTION NAME - mfw_gst_bsacdec_data
*
*   DESCRIPTION
*                   This function decodes data in the input buffer and 
*                   pushes the decode pcm output to the next element in the 
*                   pipeline
*   ARGUMENTS
*       bsacdec_info    - pointer to the plugin context
*       inbuffsize         - pointer to the input buffer size
*
*   RETURN VALUE
*       TRUE               - decoding is succesful
*       FALSE              - error in decoding
***************************************************************************/
#ifndef PUSH_MODE
static gboolean
mfw_gst_bsacdec_data(MFW_GST_BSACDEC_INFO_T * bsacdec_info,
			gint * inbuffsize)
#else
static gint
mfw_gst_bsacdec_data(MFW_GST_BSACDEC_INFO_T * bsacdec_info,
			gint inbuffsize)
#endif

{

    AACD_RET_TYPE rc;
    gint rec_no = 0;
    GstCaps *src_caps = NULL;
    GstCaps *caps = NULL;
    GstBuffer *outbuffer = NULL;
    GstBuffer *pushbuffer = NULL;
    guint8 *inbuffer = NULL;
    GstFlowReturn res = GST_FLOW_ERROR;
    guint64 time_duration = 0;
    AACD_Decoder_Config *dec_config = NULL;
    AACD_Decoder_info dec_info;
#if 0 /* Non-interleaved frames */    
    AACD_OutputFmtType outbuf[CHANS][AAC_FRAME_SIZE];
    AACD_OutputFmtType * pushbuf;

#else
    AACD_OutputFmtType * outbuf;
#endif
    dec_config = bsacdec_info->app_params.dec_config;
    GstBuffer *residue=NULL;
#ifdef PUSH_MODE
    GstClockTime ts;
    gint consumelen = 0;
#endif
    guint framesinbuffer = 0;




#ifndef PUSH_MODE
    /* No buf data need to be decoded, just return */
    if (*inbuffsize == 0)
        return TRUE;

    do {
#endif
        	if (bsacdec_global_ptr == NULL) {
#ifndef PUSH_MODE
        	    return TRUE;
#else 
                return consumelen;
#endif
        	}

#ifdef PUSH_MODE
        inbuffer = gst_adapter_peek(bsacdec_info->pAdapter, inbuffsize);
#endif


#ifndef PUSH_MODE   
    	inbuffer = GST_BUFFER_DATA(bsacdec_info->inbuffer1) +
    	           GST_BUFFER_OFFSET(bsacdec_info->inbuffer1);
#else
        inbuffer += consumelen;
#endif
        GST_DEBUG(" Begin to decode BASC frame");
    	/* the decoder decodes the encoded data in the input buffer and outputs 
    	   a frame of PCM samples */
    	framesinbuffer++;
#ifndef PUSH_MODE       
        rc = aacd_decode_frame(dec_config,&dec_info,outbuf, inbuffer, *inbuffsize);
#else
#ifdef INTERLEAVED_FRAME
        src_caps = GST_PAD_CAPS(bsacdec_info->srcpad);
            
        /* multiplication factor is obtained as a multiple of Bytes per
	       sample and number of channels */
        
        res =
            gst_pad_alloc_buffer_and_set_caps(bsacdec_info->srcpad,
            0,
            CHANS*AAC_FRAME_SIZE*sizeof(AACD_OutputFmtType),
            src_caps, &outbuffer);
        
        if (res != GST_FLOW_OK) {
            GST_ERROR("Error in allocating output buffer");
            #ifndef PUSH_MODE
            return FALSE;
            #else
            return consumelen;
            #endif
        }

        outbuf = (AACD_OutputFmtType *)GST_BUFFER_DATA(outbuffer);
#else        
        src_caps = GST_PAD_CAPS(bsacdec_info->srcpad);
            
        /* multiplication factor is obtained as a multiple of Bytes per
	       sample and number of channels */
        
        res =
            gst_pad_alloc_buffer_and_set_caps(bsacdec_info->srcpad,
            0,
            CHANS*AAC_FRAME_SIZE*sizeof(AACD_OutputFmtType),
            src_caps, &outbuffer);
        
        if (res != GST_FLOW_OK) {
            GST_ERROR("Error in allocating output buffer");
            #ifndef PUSH_MODE
            return FALSE;
            #else
            goto error;
            #endif
        }


        outbuf = g_malloc(CHANS*AAC_FRAME_SIZE*sizeof(AACD_OutputFmtType));
#endif        
        rc = aacd_decode_frame(dec_config,&dec_info,outbuf, inbuffer, inbuffsize);
#endif
	    GST_DEBUG(" return val of decoder = %d\n", rc);
        if ((rc != AACD_ERROR_NO_ERROR && rc != AACD_ERROR_EOF)) {
            g_print("decoder error:%d.\n",rc);
            {
                bsacdec_info->flow_error = TRUE;
                goto error;
            }
            GST_ERROR("Error in decoding the frame error is %d\n",rc);
            #ifndef PUSH_MODE
            GST_DEBUG("inbuffsize = %d\n",*inbuffsize);
            #else
            GST_DEBUG("inbuffsize = %d\n",inbuffsize);
            #endif
	    }


	    bsacdec_info->sampling_freq = dec_info.aacd_sampling_frequency;
	    bsacdec_info->number_of_channels = dec_info.aacd_num_channels;
    
#ifndef PUSH_MODE
    	GST_BUFFER_OFFSET(bsacdec_info->inbuffer1) +=
    	    (dec_info.BitsInBlock / 8);
    	*inbuffsize -= (dec_info.BitsInBlock/8);
#else
        consumelen += (dec_info.BitsInBlock/8);
#endif

        /* Should not check the AACD_bno with conformance test */
#ifdef INTERLEAVED_FRAME    
        if(*(dec_config->AACD_bno) <= 2){
#ifndef PUSH_MODE       
            continue;
        }
#else
            get_timestamp_with_length(&bsacdec_info->tsMgr, consumelen);
            return consumelen;
        }
#endif
#endif    

#if 0
        {
            gint i,j;
            
            short * dst = GST_BUFFER_DATA(outbuffer);
            AACD_OutputFmtType * src;

			for(i=0;i<dec_info.aacd_len;i++) {	
               src = outbuf + i;  
                for(j=0;j<dec_info.aacd_num_channels;j++)               
                {
                *dst++ = *src;
                src = src + AAC_FRAME_SIZE;
                }   
            }
        }
#else
        {
            short * dst = GST_BUFFER_DATA(outbuffer);
            mfw_gst_interleave_to_16samples(outbuf, (short *)dst , dec_info.aacd_len,dec_info.aacd_num_channels);

        }
#endif

        if (dec_info.aacd_len != 0 && dec_info.aacd_num_channels != 0) {
        
        /* capabailites of the src pad are set in accordance with the next osssink
            element in the pipeline */
        if (!bsacdec_info->caps_set) {
            bsacdec_info->caps_set = TRUE;
            caps = gst_caps_new_simple("audio/x-raw-int",
                "endianness", G_TYPE_INT,
                G_BYTE_ORDER, "signed",
                G_TYPE_BOOLEAN, TRUE, "width",
                G_TYPE_INT, 16, "depth", G_TYPE_INT,
                16, "rate", G_TYPE_INT,
                dec_info.aacd_sampling_frequency,
                "channels", G_TYPE_INT, 2, NULL);
            gst_pad_set_caps(bsacdec_info->srcpad, caps);
            
            gst_buffer_set_caps(outbuffer, caps);
            gst_caps_unref(caps);

        }
        // mfw_gst_2channel_32_to_16sample(outbuf , (short *)outbuf, dec_info.aacd_len,dec_info.aacd_num_channels);
        /* Dump the output to file */
#ifdef DUMP_WAV

        {
            gint i,j;
            
            g_print("output:%d,channel:%d, frequence:%d.\n",dec_info.aacd_len,
               dec_info.aacd_num_channels, dec_info.aacd_sampling_frequency);



			for(i=0;i<dec_info.aacd_len;i++) {	
               AACD_OutputFmtType * temp;
               temp = outbuf+i;  
                for(j=0;j<dec_info.aacd_num_channels;j++)               
                {
                fwrite(temp, 2, 1, pfOutput);  
                temp = temp + AAC_FRAME_SIZE;
                }   
            }
        }

#endif
        GST_BUFFER_SIZE(outbuffer) = MULT_FACTOR * dec_info.aacd_len;
        time_duration = gst_util_uint64_scale_int(dec_info.aacd_len, GST_SECOND, dec_info.aacd_sampling_frequency);
        
        /* The timestamp in nanoseconds     of the data     in the buffer. */

#ifndef PUSH_MODE        
        if ((framesinbuffer==1) && (*inbuffsize==0) && (GST_CLOCK_TIME_IS_VALID(GST_BUFFER_TIMESTAMP(bsacdec_info->inbuffer1)))){
        //use parser timestamp. for some clips not a/v sync.
            bsacdec_info->time_offset = GST_BUFFER_TIMESTAMP(bsacdec_info->inbuffer1);
        }
#else
#if 0
        ts = get_timestamp_with_length(&bsacdec_info->tsMgr, consumelen);
        if (GST_CLOCK_TIME_IS_VALID(ts)){
            if ((ts>bsacdec_info->time_offset) && (ts-bsacdec_info->time_offset>TIMESTAMP_DIFFRENCE_MAX_IN_NS)){
                GST_ERROR("error timestamp\n");
                bsacdec_info->time_offset = ts;
            }
        }
#endif        
#endif

        DEMO_LIVE_CHECK(bsacdec_info->demo_mode, 
            bsacdec_info->time_offset, 
            bsacdec_info->srcpad);
        if (bsacdec_info->demo_mode == 2)
            return -1;

        GST_BUFFER_TIMESTAMP(outbuffer) = bsacdec_info->time_offset;
        GST_DEBUG("show time:%" GST_TIME_FORMAT "\n", GST_TIME_ARGS(bsacdec_info->time_offset));
        /* The duration     in nanoseconds of the data in the buffer */
        GST_BUFFER_DURATION(outbuffer) = time_duration;
        /* The offset in the source file of the     beginning of this buffer */
        GST_BUFFER_OFFSET(outbuffer) = 0;
        /*record next timestamp */       
        bsacdec_info->time_offset += time_duration; 
#ifndef INTERLEAVED_FRAME 
        g_free(outbuf);    
#endif
        /* Output PCM samples are pushed on to the next element in the pipeline */
        res = gst_pad_push(bsacdec_info->srcpad, outbuffer);
        if (res != GST_FLOW_OK) {
            GST_ERROR(" not able to push the data \n");
            #ifndef PUSH_MODE
            return FALSE;
            #else
            return consumelen;
            #endif
        }

        
    } else {
	    bsacdec_info->init_done = FALSE;
	    #ifndef PUSH_MODE
        return TRUE;
        #else
        return consumelen;
        #endif
	}

#ifndef PUSH_MODE
    } while (*inbuffsize > 0);
#endif

    if(bsacdec_info->corrupt_bs)
    {
        bsacdec_info->corrupt_bs = FALSE;
        if (GST_CLOCK_TIME_IS_VALID(bsacdec_info->buffer_time))
            bsacdec_info->time_offset = bsacdec_info->buffer_time;
    }
#ifndef PUSH_MODE
    return TRUE;
#else
    return consumelen;
#endif

error:

#ifndef INTERLEAVED_FRAME
    g_free(outbuf);
#endif

#ifndef PUSH_MODE
    return FALSE;
#else
    return consumelen;
#endif
}

/***************************************************************************
*
*   FUNCTION NAME - mfw_gst_bsacdec_calc_average_bitrate
*
*   DESCRIPTION
*                   This function calulates the average bitrate by 
                    parsing the input stream.
*   ARGUMENTS
*       bsacdec_info    - pointer to the plugin context
*
*   RETURN VALUE
*       TRUE               - execution succesful
*       FALSE              - error in execution
***************************************************************************/

static gint mfw_gst_bsacdec_calc_average_bitrate(MFW_GST_BSACDEC_INFO_T *
					bsacdec_info)
{
    GstPad *pad = NULL;
    GstPad *peer_pad = NULL;
    GstFormat fmt = GST_FORMAT_BYTES;
    pad = bsacdec_info->sinkpad;
    GstBuffer *pullbuffer = NULL;
    GstFlowReturn ret = GST_FLOW_OK;
    guint pullsize = 0;
    guint64  offset = 0;
    gfloat  avg_bitrate=0;
    guint64 totalduration = 0;
    guint64 bitrate=0;
    guint  frames=0;
    guint8 *inbuffer=NULL;
    AACD_Decoder_info dec_info;
    guint temp = 0;
    gint FileType=0;
    GstClockTime file_duration;
    if (gst_pad_check_pull_range(pad)) 
    {
        if (gst_pad_activate_pull(GST_PAD_PEER(pad), TRUE)) 
        {
            peer_pad = gst_pad_get_peer(bsacdec_info->sinkpad);
            gst_pad_query_duration(peer_pad, &fmt, &totalduration);
            gst_object_unref(GST_OBJECT(peer_pad));
            pullsize = 4;
            ret = gst_pad_pull_range(pad, offset, pullsize,
				       &pullbuffer);
            App_bs_readinit((gchar *) GST_BUFFER_DATA(pullbuffer), 
                pullsize, bsacdec_info);
            FileType = App_bs_look_bits(32, bsacdec_info);
            if (App_FindFileType(FileType, bsacdec_info) != 0) {
                GST_ERROR("InputFile is not AAC\n");
                return -1;
            }
            if(bsacdec_info->app_params.App_adif_header_present==TRUE)
            {
                gst_pad_activate_push(GST_PAD_PEER(pad), TRUE);
                return 0;
            }
            pullsize = ADTS_HEADER_LENGTH;
            while(offset<totalduration)
            {
                ret = gst_pad_pull_range(pad, offset, pullsize,
				       &pullbuffer);

                /* The sampling frequency index is of length 4 bits
                after 18 bits (i.e bit3 to bit6 of the 3rd byte )
                in the ADTS header */

                inbuffer = GST_BUFFER_DATA(pullbuffer);
                temp = *(inbuffer + 2);
                temp = temp & SAMPLING_FREQ_IDX_MASk;
                temp = temp >> 2;
                bsacdec_info->SampFreqIdx = temp;

                /* The Frame Length is of length 13 bits
                after 30 bits (i.e bit7 to bit8 of the 4th byte + 
                5th byte complete + bit1 to bit3 of the 6th byte)
                in the ADTS header */

                temp = ((*(inbuffer + 3)) << 24) | ((*(inbuffer + 4)) << 16) 
                        | ((*(inbuffer + 5)) << 8) | (*(inbuffer + 6));
                temp = temp & BITSPERFRAME_MASK;
                temp = temp >> 13;
                /* If frame length is 0, should quit the while cycle */
                if (temp == 0) {
                    GST_DEBUG(" frame length is 0! ");
                    break;
                }
                bsacdec_info->bitsPerFrame = temp*8;

                ADTSBitrate(&dec_info, bsacdec_info);
                offset += temp;
                frames++;
                bitrate += dec_info.aacd_bit_rate;
           }
        }
        gst_pad_activate_push(GST_PAD_PEER(pad), TRUE);
        avg_bitrate = (gfloat)bitrate/frames;
        bsacdec_info->bit_rate =
                ((avg_bitrate + 500) / 1000) * 1000;
        GST_DEBUG("avg_bitrate=%d \n",bsacdec_info->bit_rate);
        file_duration = gst_util_uint64_scale(totalduration,GST_SECOND * 8,bsacdec_info->bit_rate);
        GST_DEBUG(" file_duration = %" GST_TIME_FORMAT, GST_TIME_ARGS(file_duration));

    }
    return 0;
}

/***************************************************************************
*
*   FUNCTION NAME - mfw_gst_bsacdec_memclean
*
*   DESCRIPTION
*                   This function frees all the memory allocated for the 
*                   plugin;
*   ARGUMENTS
*       bsacdec_info    - pointer to the plugin context
*
*   RETURN VALUE
*       None
*
***************************************************************************/
static void mfw_gst_bsacdec_memclean(MFW_GST_BSACDEC_INFO_T *
					bsacdec_info)
{

    AACD_Decoder_Config *dec_config = NULL;
    AACD_Mem_Alloc_Info_Sub *mem = NULL;
    gint nr = 0;
    gint rec_no = 0;
    GST_DEBUG("in mfw_gst_bsacdec_memclean \n");
    dec_config = bsacdec_info->app_params.dec_config;
    if (dec_config != NULL) {
	nr = dec_config->aacd_mem_info.aacd_num_reqs;
	for (rec_no = 0; rec_no < nr; rec_no++) {
	    mem = &(dec_config->aacd_mem_info.mem_info_sub[rec_no]);

	    if (mem->app_base_ptr) {
		aacd_free(mem->app_base_ptr);
		mem->app_base_ptr = 0;
	    }

	}
	aacd_free(dec_config);
    }
    GST_DEBUG("out of mfw_gst_bsacdec_memclean \n");
}

/*=============================================================================
FUNCTION: mfw_gst_bsacdec_chain

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
mfw_gst_bsacdec_chain(GstPad * pad, GstBuffer * buf)
{


    MFW_GST_BSACDEC_INFO_T *bsacdec_info;
    AACD_RET_TYPE rc = 0;
    GstCaps *src_caps = NULL, *caps = NULL;
    GstBuffer *outbuffer = NULL;
    GstFlowReturn result = GST_FLOW_OK;
    guint8 *inbuffer;
    gint inbuffsize;
    gint FileType;
    gboolean ret;
    guint64 time_duration = 0;
    AACD_Decoder_Config *dec_config = NULL;
    gint i = 0;
    bsacdec_info = MFW_GST_BSACDEC(GST_OBJECT_PARENT(pad));

    if (bsacdec_info->demo_mode == 2)
        return GST_FLOW_ERROR;


    if (bsacdec_global_ptr == NULL)
	bsacdec_global_ptr = bsacdec_info;
    else {
	if (bsacdec_global_ptr != bsacdec_info) {
	    GstFlowReturn res;
	    res = gst_pad_push(bsacdec_info->srcpad, buf);
	    if (res != GST_FLOW_OK) {
		GST_ERROR("aac dec:could not push onto \
                    next element %d\n", res);
	    }
	    return GST_FLOW_OK;
	}
    }

    bsacdec_info->buffer_time = GST_BUFFER_TIMESTAMP(buf);


    if (!bsacdec_info->init_done) {
    	if (GST_CLOCK_TIME_IS_VALID(GST_BUFFER_TIMESTAMP(buf))) {
    		bsacdec_info->time_offset = GST_BUFFER_TIMESTAMP(buf);
    	}

    	dec_config = bsacdec_info->app_params.dec_config;
    	bsacdec_info->inbuffer1 = buf;

        
        GST_BUFFER_OFFSET(bsacdec_info->inbuffer1)=0;
        
    	inbuffer = GST_BUFFER_DATA(bsacdec_info->inbuffer1);
    	inbuffsize = GST_BUFFER_SIZE(bsacdec_info->inbuffer1);


        /* 
         * FixME: 
         *  Get the header information from speicific raw data 
         *  Should be removed in formal version.
         */        
    	App_bs_readinit((gchar *) inbuffer, inbuffsize, bsacdec_info);

        App_get_mp4forbsac_header(&bsacdec_info->app_params.bs_param, bsacdec_info);
        dec_config->params = &bsacdec_info->app_params.bs_param;  

        
#ifndef PUSH_MODE        
    	    GST_BUFFER_OFFSET(bsacdec_info->inbuffer1) +=
    		    (bsacdec_info->app_params.BitsInHeader / 8);
#else
            g_print("buffersize:%d,header size:%d.\n",GST_BUFFER_SIZE(bsacdec_info->inbuffer1),
            bsacdec_info->app_params.BitsInHeader / 8);

            GST_BUFFER_DATA(bsacdec_info->inbuffer1) +=
    		    (bsacdec_info->app_params.BitsInHeader / 8);
            GST_BUFFER_SIZE(bsacdec_info->inbuffer1) -=
    		    (bsacdec_info->app_params.BitsInHeader / 8);

            GST_DEBUG("new buffersize:%d,header size:%d.\n",GST_BUFFER_SIZE(bsacdec_info->inbuffer1),
            bsacdec_info->app_params.BitsInHeader / 8);
            
#endif
 
	    dec_config->params->num_pce = 0;	    
        dec_config->params->iMulti_Channel_Support= 0;	    
        dec_config->params->bsacDecLayer= -1;


#ifdef PUSH_MODE
        if (GST_BUFFER_SIZE(buf)>0){
            gst_adapter_push(bsacdec_info->pAdapter, buf);
            GST_DEBUG("Get apdater size:%d.\n",gst_adapter_available(bsacdec_info->pAdapter));
            push_block_with_timestamp(&bsacdec_info->tsMgr, GST_BUFFER_SIZE(buf), 
                                       GST_BUFFER_TIMESTAMP(buf));
        }else{
            gst_buffer_unref(buf);
        }
#endif

    	bsacdec_info->init_done = TRUE;
        return GST_FLOW_OK;
    }

#ifndef PUSH_MODE    
    bsacdec_info->inbuffer2 = buf;

    GST_BUFFER_OFFSET(bsacdec_info->inbuffer2) = 0;
    inbuffsize = GST_BUFFER_SIZE(bsacdec_info->inbuffer1);
    ret = mfw_gst_bsacdec_data(bsacdec_info, &inbuffsize);
    if (bsacdec_info->inbuffer1) {
        gst_buffer_unref(bsacdec_info->inbuffer1);
        bsacdec_info->inbuffer1 = NULL;
    }
    bsacdec_info->inbuffer1 = bsacdec_info->inbuffer2;
    GST_BUFFER_OFFSET(bsacdec_info->inbuffer1) -= (inbuffsize);
    GST_BUFFER_SIZE(bsacdec_info->inbuffer1) += (inbuffsize);
#else
    gst_adapter_push(bsacdec_info->pAdapter, buf);
    push_block_with_timestamp(&bsacdec_info->tsMgr, GST_BUFFER_SIZE(buf), 
                                       GST_BUFFER_TIMESTAMP(buf));
    while ((inbuffsize = gst_adapter_available(bsacdec_info->pAdapter))>(BS_BUF_SIZE+ADTS_HEADER_LENGTH)){
        gint flushlen;
        flushlen = mfw_gst_bsacdec_data(bsacdec_info, inbuffsize);
        GST_DEBUG("all:%d,consumed:%d.\n",inbuffsize,flushlen);
        if (flushlen == -1) {
            break;
        }
        gst_adapter_flush(bsacdec_info->pAdapter, flushlen);
    }
    
#endif

    if (bsacdec_info->flow_error) {
        GST_ERROR(" flow error !");
        GError *error = NULL;
        GQuark domain;
        domain = g_quark_from_string("mfw_bsacdecoder");
        error = g_error_new(domain, 10, "fatal error");
        gst_element_post_message(GST_ELEMENT(bsacdec_info),
                gst_message_new_error(GST_OBJECT
                (bsacdec_info),
                error,
                "Flow error because the parsing frame length is 0 "
                " AAC decoder plug-in"));
        return GST_FLOW_ERROR;
    }
    GST_DEBUG(" out of mfw_gst_bsacdec_chain routine \n");
    return GST_FLOW_OK;

}

/*=============================================================================
FUNCTION:   mfw_gst_bsacdec_change_state

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
mfw_gst_bsacdec_change_state(GstElement * element,
				GstStateChange transition)
{
    MFW_GST_BSACDEC_INFO_T *bsacdec_info;
    GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
    gint rec_no = 0;
    gint nr=0,retval=0;
    AACD_Decoder_Config *dec_config = NULL;
    AACD_Mem_Alloc_Info_Sub *mem;
    AACD_RET_TYPE rc = 0;
    gboolean res;
    bsacdec_info = MFW_GST_BSACDEC(element);

    GST_DEBUG(" in mfw_gst_bsacdec_change_state routine \n");
    switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
    	bsacdec_info->caps_set = FALSE;
    	bsacdec_info->init_done = FALSE;
    	bsacdec_info->eos = FALSE;
        bsacdec_info->flow_error = FALSE;
    	bsacdec_info->time_offset = 0;
    	bsacdec_info->inbuffer1 = NULL;
    	bsacdec_info->inbuffer2 = NULL;
        bsacdec_info->corrupt_bs = FALSE;

    	memset(&bsacdec_info->app_params, 0,
    	       sizeof(AACD_App_params *));

    	/* allocate memory for config structure */
    	dec_config = (AACD_Decoder_Config *)
    	    aacd_alloc_fast(sizeof(AACD_Decoder_Config));
    	bsacdec_info->app_params.dec_config = dec_config;
    	if (dec_config == NULL) {
    	    GST_ERROR("error in allocation of decoder config structure");
    	    return GST_STATE_CHANGE_FAILURE;
    	}

    	/* call query mem function to know mem requirement of library */
    	if (aacd_query_dec_mem(dec_config) != AACD_ERROR_NO_ERROR) {
    	    GST_ERROR
    		("Failed to get the memory configuration for the decoder\n");
    	    return GST_STATE_CHANGE_FAILURE;
    	}

    	/* Number of memory chunk requests by the decoder */
        nr = dec_config->aacd_mem_info.aacd_num_reqs;

        for(rec_no = 0; rec_no < nr; rec_no++)
        {
            mem = &(dec_config->aacd_mem_info.mem_info_sub[rec_no]);

            if (mem->aacd_type == AACD_FAST_MEMORY)
            {
                mem->app_base_ptr = aacd_alloc_fast (mem->aacd_size);
                if (mem->app_base_ptr == NULL)
                    return GST_STATE_CHANGE_FAILURE;
            }
            else
            {
            mem->app_base_ptr = aacd_alloc_slow (mem->aacd_size);
            if (mem->app_base_ptr == NULL)
                return GST_STATE_CHANGE_FAILURE;
            }
            memset(dec_config->aacd_mem_info.mem_info_sub[rec_no].app_base_ptr,
                    0, dec_config->aacd_mem_info.mem_info_sub[rec_no].aacd_size); 

        }

    	bsacdec_info->app_params.BitsInHeader = 0;
    	bsacdec_info->app_params.App_adif_header_present = FALSE;
    	bsacdec_info->app_params.App_adts_header_present = FALSE;

        /* register the call-back function in the decoder context */
        dec_config->app_swap_buf = app_swap_buffers_aac_dec;

#ifndef OUTPUT_24BITS
        dec_config->num_pcm_bits     = AACD_16_BIT_OUTPUT;
#else
        dec_config->num_pcm_bits     = AACD_24_BIT_OUTPUT;
#endif  /*OUTPUT_24BITS*/				/*OUTPUT_24BITS */

    	rc = aacd_decoder_init(dec_config);
    	if (rc != AACD_ERROR_NO_ERROR) {
    	    GST_ERROR("Error in initializing the decoder");
    	    return GST_STATE_CHANGE_FAILURE;
    	}
	    break;

    case GST_STATE_CHANGE_READY_TO_PAUSED:
	    bsacdec_info->bitsPerFrame = 0;
	    bsacdec_info->bit_rate = 0;
	    bsacdec_info->nFramesReceived = 0;
	    bsacdec_info->bitstream_count = 0;
	    bsacdec_info->bitstream_buf_index = 0;
	    bsacdec_info->in_buf_done = 0;
	    bsacdec_info->nBitsReceived = 0;
	    bsacdec_info->total_time = 0;
	    bsacdec_info->seek_flag = FALSE;
        
#ifdef PUSH_MODE        
        bsacdec_info->pAdapter = gst_adapter_new();
        init_tsmanager(&bsacdec_info->tsMgr);
#endif
#if 0
        retval = mfw_gst_bsacdec_calc_average_bitrate(bsacdec_info);
        
        if(retval != 0)
        {
            GST_ERROR("error in Calculating the average Bitrate\n");
            return GST_STATE_CHANGE_FAILURE;

        }
#endif
        if (gst_pad_check_pull_range(bsacdec_info->sinkpad)) {
    		gst_pad_set_query_function(bsacdec_info->srcpad,
    			                        GST_DEBUG_FUNCPTR
    			                        (mfw_gst_bsacdec_src_query));
    		bsacdec_info->seek_flag = TRUE;
	    }
	    bsacdec_info->total_frames = 0;
        bsacdec_info->time_offset = 0;
	    break;

    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
	    break;
        
    default:
	    break;
    }

    ret = parent_class_aac->change_state(element, transition);

    switch (transition) {
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
	    break;
    
    case GST_STATE_CHANGE_PAUSED_TO_READY:
    	GST_DEBUG("GST_STATE_CHANGE_PAUSED_TO_READY \n");
#ifndef PUSH_MODE        
            /* Following is memory leak fix */   
        if ((bsacdec_info->inbuffer1) && (bsacdec_info->inbuffer1 != bsacdec_info->inbuffer2))
        {

    	    gst_buffer_unref(bsacdec_info->inbuffer1);
    	    bsacdec_info->inbuffer1 = NULL;
    	}  
        if (bsacdec_info->inbuffer2) {

    	    gst_buffer_unref(bsacdec_info->inbuffer2);
    	    bsacdec_info->inbuffer1 = NULL;
    	}
#else
        bsacdec_info->inbuffer1 = NULL;
	    bsacdec_info->inbuffer2 = NULL;
#endif
        mfw_gst_bsacdec_memclean(bsacdec_info);
            
    	bsacdec_info->total_frames = 0;
        
#ifdef PUSH_MODE    
        gst_adapter_clear(bsacdec_info->pAdapter);
        g_object_unref(bsacdec_info->pAdapter);
        deinit_tsmanager(&bsacdec_info->tsMgr);
#endif
	    break;

    case GST_STATE_CHANGE_READY_TO_NULL:
    	GST_DEBUG("GST_STATE_CHANGE_READY_TO_NULL \n");
    	bsacdec_global_ptr = NULL;
#ifdef DUMP_WAV        
        fclose(pfOutput);
#endif
    	break;
        
    default:
	    break;
    }
    GST_DEBUG(" out of mfw_gst_bsacdec_change_state routine \n");

    return ret;

}

/*=============================================================================
FUNCTION: mfw_gst_bsacdec_get_query_types

DESCRIPTION: gets the different types of query supported by the plugin

ARGUMENTS PASSED:
        pad     - pad on which the function is registered 

RETURN VALUE:
        query types ssupported by the plugin

PRE-CONDITIONS:
        None

POST-CONDITIONS:


IMPORTANT NOTES:
        None
=============================================================================*/
static const GstQueryType *mfw_gst_bsacdec_get_query_types(GstPad * pad)
{
    static const GstQueryType src_query_types[] = {
	GST_QUERY_POSITION,
	GST_QUERY_DURATION,
	GST_QUERY_CONVERT,
	0
    };

    return src_query_types;
}

/*==================================================================================================

FUNCTION:   mfw_gst_bsacdec_src_query   

DESCRIPTION:    performs query on src pad.    

ARGUMENTS PASSED:
        pad     -   pointer to GstPad
        query   -   pointer to GstQuery        
            
RETURN VALUE:
        TRUE    -   success
        FALSE   -   failure

PRE-CONDITIONS:
        None

POST-CONDITIONS:
		None

IMPORTANT NOTES:
        None

==================================================================================================*/
static gboolean mfw_gst_bsacdec_src_query(GstPad * pad,
					     GstQuery * query)
{
    gboolean res = TRUE;
    GstPad *peer;

    MFW_GST_BSACDEC_INFO_T *bsacdec_info;
    bsacdec_info = MFW_GST_BSACDEC(GST_OBJECT_PARENT(pad));

    peer = gst_pad_get_peer(bsacdec_info->sinkpad);

    switch (GST_QUERY_TYPE(query)) {

    case GST_QUERY_DURATION:
	{

	    GST_DEBUG("coming in GST_QUERY_DURATION \n");
	    GstFormat format;
	    GstFormat rformat;
	    gint64 total, total_bytes;
	    GstPad *peer;

	    /* save requested format */
	    gst_query_parse_duration(query, &format, NULL);
	    if ((peer =
		 gst_pad_get_peer(bsacdec_info->sinkpad)) == NULL)
		goto error;

	    if (format == GST_FORMAT_TIME && gst_pad_query(peer, query)) {
		gst_query_parse_duration(query, NULL, &total);
		GST_DEBUG_OBJECT(bsacdec_info,
				 "peer returned duration %"
				 GST_TIME_FORMAT, GST_TIME_ARGS(total));

	    }
	    /* query peer for total length in bytes */
	    gst_query_set_duration(query, GST_FORMAT_BYTES, -1);


	    if (!gst_pad_query(peer, query)) {
		goto error;
	    }
	    gst_object_unref(peer);

	    /* get the returned format */
	    gst_query_parse_duration(query, &rformat, &total_bytes);

	    if (rformat == GST_FORMAT_BYTES) {
		GST_DEBUG("peer pad returned total bytes=%d", total_bytes);
	    } else if (rformat == GST_FORMAT_TIME) {
		GST_DEBUG("peer pad returned total time=%",
			  GST_TIME_FORMAT, GST_TIME_ARGS(total_bytes));
	    }

	    /* Check if requested format is returned format */
	    if (format == rformat)
		return TRUE;


	    if (total_bytes != -1) {
		if (format != GST_FORMAT_BYTES) {
		    if (!mfw_gst_bsacdec_convert_sink
			(pad, GST_FORMAT_BYTES, total_bytes, &format,
			 &total))
			goto error;
		} else {
		    total = total_bytes;
		}
	    } else {
		total = -1;
	    }
	    bsacdec_info->total_time = total;
	    gst_query_set_duration(query, format, total);

	    if (format == GST_FORMAT_TIME) {
		GST_DEBUG("duration=%" GST_TIME_FORMAT,
			  GST_TIME_ARGS(total));
	    } else {
		GST_DEBUG("duration=%" G_GINT64_FORMAT ",format=%u", total,
			  format);
	    }
	    break;
	}
    case GST_QUERY_CONVERT:
	{
	    GstFormat src_fmt, dest_fmt;
	    gint64 src_val, dest_val;
	    gst_query_parse_convert(query, &src_fmt, &src_val, &dest_fmt,
				    &dest_val);
	    if (!
		(res =
		 mfw_gst_bsacdec_convert_src(pad, src_fmt, src_val,
						&dest_fmt, &dest_val)))
		goto error;

	    gst_query_set_convert(query, src_fmt, src_val, dest_fmt,
				  dest_val);
	    break;
	}
    default:
	res = FALSE;
	break;
    }
    return res;

  error:
    GST_ERROR("error handling query");
    return FALSE;
}

/*==================================================================================================
FUNCTION:   mfw_gst_bsacdec_convert_src   

DESCRIPTION:    converts the format of value from src format to destination format on src pad .    

ARGUMENTS PASSED:
        pad         -   pointer to GstPad   
        src_format  -   format of source value
        src_value   -   value of soure 
        dest_format -   format of destination value
        dest_value  -   value of destination         

RETURN VALUE:
        TRUE    -   sucess
        FALSE   -   failure  

PRE-CONDITIONS:
        None

POST-CONDITIONS:
		None

IMPORTANT NOTES:
        None

==================================================================================================*/
static gboolean
mfw_gst_bsacdec_convert_src(GstPad * pad, GstFormat src_format,
			       gint64 src_value, GstFormat * dest_format,
			       gint64 * dest_value)
{
    gboolean res = TRUE;
    guint scale = 1;
    gint bytes_per_sample;

    MFW_GST_BSACDEC_INFO_T *bsacdec_info;
    bsacdec_info = MFW_GST_BSACDEC(GST_OBJECT_PARENT(pad));

    bytes_per_sample = bsacdec_info->number_of_channels * 4;

    switch (src_format) {
    case GST_FORMAT_BYTES:
	switch (*dest_format) {
	case GST_FORMAT_DEFAULT:
	    if (bytes_per_sample == 0)
		return FALSE;
	    *dest_value = src_value / bytes_per_sample;
	    break;
	case GST_FORMAT_TIME:
	    {
		gint byterate = bytes_per_sample *
		    bsacdec_info->sampling_freq;
		if (byterate == 0)
		    return FALSE;
		*dest_value = src_value * GST_SECOND / byterate;
		break;
	    }
	default:
	    res = FALSE;
	}
	break;
    case GST_FORMAT_DEFAULT:
	switch (*dest_format) {
	case GST_FORMAT_BYTES:
	    *dest_value = src_value * bytes_per_sample;
	    break;
	case GST_FORMAT_TIME:
	    if (bsacdec_info->sampling_freq == 0)
		return FALSE;
	    *dest_value = src_value * GST_SECOND /
		bsacdec_info->sampling_freq;
	    break;
	default:
	    res = FALSE;
	}
	break;
    case GST_FORMAT_TIME:
	switch (*dest_format) {
	case GST_FORMAT_BYTES:
	    scale = bytes_per_sample;
	    /* fallthrough */
	case GST_FORMAT_DEFAULT:
	    *dest_value = src_value * scale *
		bsacdec_info->sampling_freq / GST_SECOND;
	    break;
	default:
	    res = FALSE;
	}
	break;
    default:
	res = FALSE;
    }

    return res;
}

/*==================================================================================================

FUNCTION:   mfw_gst_bsacdec_convert_sink    

DESCRIPTION:    converts the format of value from src format to destination format on sink pad .  
   

ARGUMENTS PASSED:
        pad         -   pointer to GstPad   
        src_format  -   format of source value
        src_value   -   value of soure 
        dest_format -   format of destination value
        dest_value  -   value of destination 

RETURN VALUE:
        TRUE    -   sucess
        FALSE   -   failure

PRE-CONDITIONS:
        None

POST-CONDITIONS:
		None

IMPORTANT NOTES:
        None

==================================================================================================*/
static gboolean
mfw_gst_bsacdec_convert_sink(GstPad * pad, GstFormat src_format,
				gint64 src_value, GstFormat * dest_format,
				gint64 * dest_value)
{
    gboolean res = TRUE;
    float avg_bitrate = 0;
    MFW_GST_BSACDEC_INFO_T *bsacdec_info;
    GST_DEBUG(" in mfw_gst_bsacdec_convert_sink \n");
    bsacdec_info = MFW_GST_BSACDEC(GST_OBJECT_PARENT(pad));

    if (bsacdec_info->app_params.App_adif_header_present){
        avg_bitrate = bsacdec_info->app_params.BlockParams.BitRate;
    }
    else  
        avg_bitrate = bsacdec_info->bit_rate;

    switch (src_format) {
    case GST_FORMAT_BYTES:
	switch (*dest_format) {
	case GST_FORMAT_TIME:
	    if (avg_bitrate) {

		*dest_value =
		    gst_util_uint64_scale(src_value, 8 * GST_SECOND,
					  avg_bitrate);
	    } else {
		*dest_value = GST_CLOCK_TIME_NONE;
	    }
	    break;
	default:
	    res = FALSE;
	}
	break;
    case GST_FORMAT_TIME:
	switch (*dest_format) {
	case GST_FORMAT_BYTES:
	    if (avg_bitrate) {

		*dest_value = gst_util_uint64_scale(src_value, avg_bitrate,
						    8 * GST_SECOND);

	    } else {
		*dest_value = 0;
	    }

	    break;

	default:
	    res = FALSE;
	}
	break;
    default:
	res = FALSE;
    }

    GST_DEBUG(" out of mfw_gst_bsacdec_convert_sink \n");
    return res;
}

/*==================================================================================================

FUNCTION:   mfw_gst_bsacdec_seek  

DESCRIPTION:    performs seek operation    

ARGUMENTS PASSED:
        bsacdec_info -   pointer to decoder element
        pad         -   pointer to GstPad
        event       -   pointer to GstEvent

RETURN VALUE:
        TRUE    -   sucess
        FALSE   -   failure        

PRE-CONDITIONS:
        None

POST-CONDITIONS:
		None

IMPORTANT NOTES:
        None

==================================================================================================*/
static gboolean
mfw_gst_bsacdec_seek(MFW_GST_BSACDEC_INFO_T * bsacdec_info,
			GstPad * pad, GstEvent * event)
{
    gdouble rate;
    GstFormat format, conv;
    GstSeekFlags flags;
    GstSeekType cur_type, stop_type;
    gint64 cur = 0, stop = 0;
    gint64 time_cur = 0, time_stop = 0;
    gint64 bytes_cur = 0, bytes_stop = 0;
    gboolean flush;
    gboolean res;
    guint bytesavailable;
    gst_event_parse_seek(event, &rate, &format, &flags, &cur_type, &cur,
			 &stop_type, &stop);

    GST_DEBUG("\nseek from  %" GST_TIME_FORMAT "--------------- to %"
	      GST_TIME_FORMAT, GST_TIME_ARGS(cur), GST_TIME_ARGS(stop));

    if (format != GST_FORMAT_TIME) {
	conv = GST_FORMAT_TIME;
	if (!mfw_gst_bsacdec_convert_src
	    (pad, format, cur, &conv, &time_cur))
	    goto convert_error;
	if (!mfw_gst_bsacdec_convert_src
	    (pad, format, stop, &conv, &time_stop))
	    goto convert_error;
    } else {
	time_cur = cur;
	time_stop = stop;
    }
    GST_DEBUG("\nseek from  %" GST_TIME_FORMAT "--------------- to %"
	      GST_TIME_FORMAT, GST_TIME_ARGS(time_cur),
	      GST_TIME_ARGS(time_stop));

    /* shave off the flush flag, we'll need it later */
    flush = ((flags & GST_SEEK_FLAG_FLUSH) != 0);
    res = FALSE;
    conv = GST_FORMAT_BYTES;



    if (!mfw_gst_bsacdec_convert_sink
	(pad, GST_FORMAT_TIME, time_cur, &conv, &bytes_cur))
	goto convert_error;

    if (!mfw_gst_bsacdec_convert_sink
	(pad, GST_FORMAT_TIME, time_stop, &conv, &bytes_stop))
	goto convert_error;

    {
	GstEvent *seek_event;


	seek_event =
	    gst_event_new_seek(rate, GST_FORMAT_BYTES, flags, cur_type,
			       bytes_cur, stop_type, bytes_stop);

	/* do the seek */
	res = gst_pad_push_event(bsacdec_info->sinkpad, seek_event);

    }


    return TRUE;

    /* ERRORS */
  convert_error:
    {
	/* probably unsupported seek format */
	GST_ERROR("failed to convert format %u into GST_FORMAT_TIME",
		  format);
	return FALSE;
    }
}


/*=============================================================================
FUNCTION:   mfw_gst_bsacdec_src_event

DESCRIPTION: This functions handles the events that triggers the
			 source pad of the mpeg4 decoder element.

ARGUMENTS PASSED:
        pad        -    pointer to pad
        event      -    pointer to event
RETURN VALUE:
        TRUE       -	if event is sent to src properly
	    FALSE	   -	if event is not sent to src properly

PRE-CONDITIONS:
        None

POST-CONDITIONS:
        None

IMPORTANT NOTES:
        None
=============================================================================*/
static gboolean
mfw_gst_bsacdec_src_event(GstPad * pad, GstEvent * event)
{

    gboolean res;
    MFW_GST_BSACDEC_INFO_T *bsacdec_info;
    GST_DEBUG(" in mfw_gst_bsacdec_src_event routine \n");
    bsacdec_info = MFW_GST_BSACDEC(GST_OBJECT_PARENT(pad));

    switch (GST_EVENT_TYPE(event)) {
    case GST_EVENT_SEEK:
	gst_event_ref(event);
	if (bsacdec_info->seek_flag == TRUE) {
	    res = mfw_gst_bsacdec_seek(bsacdec_info, pad, event);

	} else {
	    res = gst_pad_push_event(bsacdec_info->sinkpad, event);

	}
	break;

    default:
	res = gst_pad_push_event(pad, event);
	break;


    }
    GST_DEBUG(" out of mfw_gst_bsacdec_src_event routine \n");

    gst_event_unref(event);
    return res;

}

/*=============================================================================
FUNCTION:   mfw_gst_bsacdec_sink_event 

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
mfw_gst_bsacdec_sink_event(GstPad * pad, GstEvent * event)
{
    gboolean result = TRUE;
    MFW_GST_BSACDEC_INFO_T *bsacdec_info;
    AACD_RET_TYPE rc;
    GstCaps *src_caps = NULL, *caps = NULL;
    GstBuffer *outbuffer = NULL;
    GstFlowReturn res = GST_FLOW_OK;
    guint8 *inbuffer;
    gint inbuffsize;
    guint64 time_duration = 0;



    bsacdec_info = MFW_GST_BSACDEC(GST_OBJECT_PARENT(pad));

    GST_DEBUG(" in mfw_gst_bsacdec_sink_event function \n");
    switch (GST_EVENT_TYPE(event)) {
    case GST_EVENT_NEWSEGMENT:
	{
	    GstFormat format;
	    gint64 start, stop, position;
	    gint64 nstart, nstop;
	    GstEvent *nevent;

	    GST_DEBUG(" in GST_EVENT_NEWSEGMENT \n");
	    gst_event_parse_new_segment(event, NULL, NULL, &format, &start,
					&stop, &position);

	    if (format == GST_FORMAT_BYTES) {
		format = GST_FORMAT_TIME;
		if (start != 0)
		    result =
			mfw_gst_bsacdec_convert_sink(pad,
							GST_FORMAT_BYTES,
							start, &format,
							&nstart);
		else
		    nstart = start;
		if (stop != 0)
		    result =
			mfw_gst_bsacdec_convert_sink(pad,
							GST_FORMAT_BYTES,
							stop, &format,
							&nstop);
		else
		    nstop = stop;

		nevent =
		    gst_event_new_new_segment(FALSE, 1.0, GST_FORMAT_TIME,
					      nstart, nstop, nstart);
		gst_event_unref(event);
		bsacdec_info->time_offset = (guint64) nstart;

		result =
		    gst_pad_push_event(bsacdec_info->srcpad, nevent);
		if (TRUE != result) {
		    GST_ERROR
			("\n Error in pushing the event,result	is %d\n",
			 result);

		}
	    } else if (format == GST_FORMAT_TIME) {
		bsacdec_info->time_offset = (guint64) start;

		result =
		    gst_pad_push_event(bsacdec_info->srcpad, event);
		if (TRUE != result) {
		    GST_ERROR
			("\n Error in pushing the event,result	is %d\n",
			 result);
		   
		}
	    }
	    break;
	}
    case GST_EVENT_EOS:
	{

	    GST_DEBUG("\nDecoder: Got an EOS from Demuxer\n");
	    if (bsacdec_global_ptr == NULL)
		bsacdec_global_ptr = bsacdec_info;
	    else {
		if (bsacdec_global_ptr != bsacdec_info) {
		    gboolean res;
		    res =
			gst_pad_push_event(bsacdec_info->srcpad, event);
		    if (res != TRUE) {
			GST_ERROR("aac dec:could not push onto \
                            next element %d\n", res);
		    }
		    return TRUE;
		}
	    }

	    if (bsacdec_info->init_done) {
#ifndef PUSH_MODE
		inbuffsize = GST_BUFFER_SIZE(bsacdec_info->inbuffer1);
#endif
        bsacdec_info->eos = TRUE;

        
            bsacdec_info->inbuffer2=NULL;
#ifndef PUSH_MODE            
		result =
		    mfw_gst_bsacdec_data(bsacdec_info, &inbuffsize);
        
#else
        while ((inbuffsize = gst_adapter_available(bsacdec_info->pAdapter))>0){
        gint flushlen;
        flushlen = mfw_gst_bsacdec_data(bsacdec_info, inbuffsize);
        if (flushlen == -1)
            break;
        if (flushlen>inbuffsize){
            flushlen = inbuffsize;
        }
        gst_adapter_flush(bsacdec_info->pAdapter, flushlen);
        }
#endif
        if (result != TRUE) {
		    GST_ERROR("Error in decoding the frame");
		}


	    }

	    result = gst_pad_push_event(bsacdec_info->srcpad, event);
	    if (TRUE != result) {
		GST_ERROR
		    ("\n Error in pushing the event,result	is %d\n",
		     result);
	
	    }

            return TRUE;
	    break;
	}
    case GST_EVENT_FLUSH_STOP:
	{

	    GST_DEBUG(" GST_EVENT_FLUSH_STOP \n");

	    result = mfw_gst_bsacdec_mem_flush(bsacdec_info);
#ifndef PUSH_MODE
	    if (bsacdec_info->inbuffer2) {
		gst_buffer_unref(bsacdec_info->inbuffer2);
		bsacdec_info->inbuffer2 = NULL;
	    }
#else
        gst_adapter_clear(bsacdec_info->pAdapter);
        clear_tsmanager(&bsacdec_info->tsMgr);
#endif        
	    result = gst_pad_push_event(bsacdec_info->srcpad, event);
	    if (TRUE != result) {
		GST_ERROR("\n Error in pushing the event,result	is %d\n",
			  result);
	
	    }
	    break;
	}

    case GST_EVENT_FLUSH_START:
    default:
	{
	    result = gst_pad_event_default(pad, event);
	    break;
	}
    }

    GST_DEBUG(" out of mfw_gst_bsacdec_sink_event \n");

    return result;
}


/*=============================================================================
FUNCTION:   mfw_gst_bsacdec_set_caps

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
static gboolean mfw_gst_bsacdec_set_caps(GstPad * pad, GstCaps * caps)
{



    MFW_GST_BSACDEC_INFO_T *bsacdec_info;
    const gchar *mime;
    gint mpeg_version;
    GstStructure *structure = gst_caps_get_structure(caps, 0);
    bsacdec_info = MFW_GST_BSACDEC(gst_pad_get_parent(pad));

    GST_DEBUG(" in mfw_gst_bsacdec_set_caps routine \n");
    mime = gst_structure_get_name(structure);

    if (strcmp(mime, "audio/mpeg") != 0) {
	GST_WARNING
	    ("Wrong	mimetype %s	provided, we only support %s",
	     mime, "audio/mpeg");
        gst_object_unref(bsacdec_info);
	return FALSE;
    }
    gst_structure_get_int(structure, "mpegversion", &mpeg_version);
    if (((mpeg_version != 2) && (mpeg_version != 4))) {
	GST_ERROR("Caps negotiation: mpeg version not supprted");
        gst_object_unref(bsacdec_info);
	return FALSE;
    }

    gst_structure_get_int(structure, "bitrate",
			  &bsacdec_info->bit_rate);

    if (!gst_pad_set_caps(bsacdec_info->srcpad, caps)) {
        gst_object_unref(bsacdec_info);
	return FALSE;
    }

    GST_DEBUG(" out of mfw_gst_bsacdec_set_caps routine \n");
    gst_object_unref(bsacdec_info);
    return TRUE;
}

/*=============================================================================
FUNCTION:   mfw_gst_bsacdec_init

DESCRIPTION:This function creates the pads on the elements and register the
			function pointers which operate on these pads.

ARGUMENTS PASSED:
        pointer the aac decoder element handle.

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
mfw_gst_bsacdec_init(MFW_GST_BSACDEC_INFO_T * bsacdec_info)
{

    GstElementClass *klass = GST_ELEMENT_GET_CLASS(bsacdec_info);
    GST_DEBUG(" \n in mfw_gst_bsacdec_init routine \n");
    bsacdec_info->sinkpad =
	gst_pad_new_from_template(gst_element_class_get_pad_template
				  (klass, "sink"), "sink");

    bsacdec_info->srcpad =
	gst_pad_new_from_template(gst_element_class_get_pad_template
				  (klass, "src"), "src");

    gst_element_add_pad(GST_ELEMENT(bsacdec_info),
			bsacdec_info->sinkpad);
    gst_element_add_pad(GST_ELEMENT(bsacdec_info),
			bsacdec_info->srcpad);

    gst_pad_set_setcaps_function(bsacdec_info->sinkpad,
				 mfw_gst_bsacdec_set_caps);
    gst_pad_set_chain_function(bsacdec_info->sinkpad,
                GST_DEBUG_FUNCPTR(
                    mfw_gst_bsacdec_chain));

    gst_pad_set_event_function(bsacdec_info->sinkpad,
			       GST_DEBUG_FUNCPTR
			       (mfw_gst_bsacdec_sink_event));

    gst_pad_set_query_type_function(bsacdec_info->srcpad,
				    GST_DEBUG_FUNCPTR
				    (mfw_gst_bsacdec_get_query_types));
    gst_pad_set_event_function(bsacdec_info->srcpad,
			       GST_DEBUG_FUNCPTR
			       (mfw_gst_bsacdec_src_event));


    GST_DEBUG("\n out of mfw_gst_bsacdec_init \n");

#define MFW_GST_BSAC_PLUGIN VERSION
    PRINT_CORE_VERSION(aacd_decode_versionInfo());
    PRINT_PLUGIN_VERSION(MFW_GST_BSAC_PLUGIN);

    INIT_DEMO_MODE(aacd_decode_versionInfo(), bsacdec_info->demo_mode);

#ifdef DUMP_WAV
    pfOutput = fopen ("output.wav", "wb");    		
    if (pfOutput == NULL)    		
    {               
        printf ("Couldn't open output file %s\n", "output.wav");   
    }
#endif    

}

/*=============================================================================
FUNCTION:   mfw_gst_bsacdec_class_init

DESCRIPTION:Initialise the class only once (specifying what signals,
            arguments and virtual functions the class has and setting up
            global state)
ARGUMENTS PASSED:
       	klass   - pointer to aac decoder's element class

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
mfw_gst_bsacdec_class_init(MFW_GST_BSACDEC_CLASS_T * klass)
{
    GObjectClass *gobject_class = NULL;
    GstElementClass *gstelement_class = NULL;
    gobject_class = (GObjectClass *) klass;
    gstelement_class = (GstElementClass *) klass;
    parent_class_aac =
	(GstElementClass *) g_type_class_ref(GST_TYPE_ELEMENT);
    gobject_class->set_property = mfw_gst_bsacdec_set_property;
    gobject_class->get_property = mfw_gst_bsacdec_get_property;
    gstelement_class->change_state = mfw_gst_bsacdec_change_state;
}

/*=============================================================================
FUNCTION:  mfw_gst_bsacdec_base_init

DESCRIPTION:
            aac decoder element details are registered with the plugin during
            _base_init ,This function will initialise the class and child
            class properties during each new child class creation


ARGUMENTS PASSED:
        Klass   -   pointer to aac decoder plug-in class

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
mfw_gst_bsacdec_base_init(MFW_GST_BSACDEC_CLASS_T * klass)
{
    static GstElementDetails element_details = {
	"Freescale AAC-BSAC Decoder Plugin",
	"Codec/Decoder/Audio",
	"Decodes AAC-BSAC bitstreams",
	FSL_GST_MM_PLUGIN_AUTHOR
    };
    GstElementClass *element_class = GST_ELEMENT_CLASS(klass);
    gst_element_class_add_pad_template(element_class,
				       gst_static_pad_template_get
				       (&src_factory));
    gst_element_class_add_pad_template(element_class,
				       gst_static_pad_template_get
				       (&sink_factory));
    gst_element_class_set_details(element_class, &element_details);
}

/*=============================================================================
FUNCTION: mfw_gst_bsacdec_get_type

DESCRIPTION:    intefaces are initiated in this function.you can register one
                or more interfaces  after having registered the type itself.

ARGUMENTS PASSED:
            None

RETURN VALUE:
                 A numerical value ,which represents the unique identifier of this
            element(bsacdecoder)

PRE-CONDITIONS:
            None

POST-CONDITIONS:
            None

IMPORTANT NOTES:
            None
=============================================================================*/
GType mfw_gst_bsacdec_get_type(void)
{
    static GType bsacdec_type = 0;

    if (!bsacdec_type) {
	static const GTypeInfo bsacdec_info = {
	    sizeof(MFW_GST_BSACDEC_CLASS_T),
	    (GBaseInitFunc) mfw_gst_bsacdec_base_init,
	    NULL,
	    (GClassInitFunc) mfw_gst_bsacdec_class_init,
	    NULL,
	    NULL,
	    sizeof(MFW_GST_BSACDEC_INFO_T),
	    0,
	    (GInstanceInitFunc) mfw_gst_bsacdec_init,
	};
	bsacdec_type = g_type_register_static(GST_TYPE_ELEMENT,
						 "MFW_GST_BSACDEC_INFO_T",
						 &bsacdec_info,
						 (GTypeFlags) 0);
    }
    GST_DEBUG_CATEGORY_INIT(mfw_gst_bsacdec_debug, "mfw_bsacdecoder",
			    0, "FreeScale's AAC Decoder's Log");

    return bsacdec_type;
}


/*****************************************************************************/
/*    This is used to define the entry point and meta data of plugin         */
/*****************************************************************************/
GST_PLUGIN_DEFINE(GST_VERSION_MAJOR,	/* major version of Gstreamer    */
		  GST_VERSION_MINOR,	/* minor version of Gstreamer    */
		  "mfw_bsacdecoder",	/* name of the plugin            */
		  "Decodes AAC-BSAC ",	/* what plugin actually does     */
		  plugin_init,	/* first function to be called   */
		  VERSION,
		  GST_LICENSE_UNKNOWN,
		  FSL_GST_MM_PLUGIN_PACKAGE_NAME, FSL_GST_MM_PLUGIN_PACKAGE_ORIG)
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
    return gst_element_register(plugin, "mfw_bsacdecoder",
				GST_RANK_NONE, MFW_GST_TYPE_BSACDEC);
}

/*=============================================================================
FUNCTION: mfw_gst_bsacdec_mem_flush

DESCRIPTION: this function flushes the current memory and allocate the new memory
                for decoder . 

ARGUMENTS PASSED:
        mp3dec_info -   pointer to mp3decoder element structure      

RETURN VALUE:
        None

PRE-CONDITIONS:
        None

POST-CONDITIONS:


IMPORTANT NOTES:
        None
=============================================================================*/

static gboolean mfw_gst_bsacdec_mem_flush(MFW_GST_BSACDEC_INFO_T *
					     bsacdec_info)
{
    gint loopctr = 0;
    gboolean result = TRUE;
    gint num;
    gint rec_no = 0;
    gint nr;
    AACD_Decoder_Config *dec_config = NULL;
    AACD_Mem_Alloc_Info_Sub *mem;
    AACD_RET_TYPE rc = 0;

    GST_DEBUG("in mfw_gst_bsacdec_mem_flush \n");
    if (!bsacdec_info->init_done)
	return FALSE;


    
    mfw_gst_bsacdec_memclean(bsacdec_info);

    memset(&bsacdec_info->app_params, 0, sizeof(AACD_App_params *));

   /* allocate memory for config structure */
	dec_config = (AACD_Decoder_Config *)
	    aacd_alloc_fast(sizeof(AACD_Decoder_Config));
	bsacdec_info->app_params.dec_config = dec_config;
	if (dec_config == NULL) {
	    GST_ERROR("error in allocation of decoder config structure");
	    return FALSE;
	}



	/* call query mem function to know mem requirement of library */

	if (aacd_query_dec_mem(dec_config) != AACD_ERROR_NO_ERROR) {
	    GST_ERROR
		("Failed to get the memory configuration for the decoder\n");
	    return FALSE;
	}

	/* Number of memory chunk requests by the decoder */
	nr = dec_config->aacd_mem_info.aacd_num_reqs;

	 for(rec_no = 0; rec_no < nr; rec_no++)
     {
         mem = &(dec_config->aacd_mem_info.mem_info_sub[rec_no]);
         
         if (mem->aacd_type == AACD_FAST_MEMORY)
         {
             mem->app_base_ptr = aacd_alloc_fast (mem->aacd_size);
             if (mem->app_base_ptr == NULL)
                 return FALSE;
         }
         else
         {
             mem->app_base_ptr = aacd_alloc_slow (mem->aacd_size);
             if (mem->app_base_ptr == NULL)
                 return FALSE;
         }
         memset(dec_config->aacd_mem_info.mem_info_sub[rec_no].app_base_ptr,
             0, dec_config->aacd_mem_info.mem_info_sub[rec_no].aacd_size); 
         
     }

	bsacdec_info->app_params.BitsInHeader = 0;
	bsacdec_info->app_params.App_adif_header_present = FALSE;

	/* register the call-back function in the decoder context */
	dec_config->app_swap_buf = app_swap_buffers_aac_dec;

#ifndef OUTPUT_24BITS
            dec_config->num_pcm_bits     = AACD_16_BIT_OUTPUT;
#else
            dec_config->num_pcm_bits     = AACD_24_BIT_OUTPUT;
#endif  /*OUTPUT_24BITS*/				/*OUTPUT_24BITS */


	rc = aacd_decoder_init(dec_config);
	if (rc != AACD_ERROR_NO_ERROR) {
	    GST_ERROR("Error in initializing the decoder");
	    return FALSE;
	}

    bsacdec_info->init_done = FALSE;

    GST_DEBUG("out of mfw_gst_bsacdec_mem_flush\n");
    return result;
}
