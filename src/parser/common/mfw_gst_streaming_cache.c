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
 * Module Name:    mfw_gst_streaming_cache.c
 *
 * Description:    Implementation for streamed based demuxer srcpad cache.
 *
 * Portability:    This code is written for Linux OS.
 */  
 
/*
 * Changelog: 
 *
 */


#include <gst/gst.h>
#include <gst/base/gstadapter.h>

typedef struct {
    GstPad *srcpad;
    GstAdapter * adapter;
    
    guint64 addr; /* address */
    guint64 offset; /* read pointer offset, optimized for read repeatly */
    guint64 size;
    guint64 maxsize; /* threshold for cache max-size */
    gboolean eos;
    
    GMutex * mutex;
    GCond* consume_cond;
    GCond * produce_cond;
    gboolean seeking;
    gboolean close;
}StreamingCache;

#define WAIT_COND_TIMEOUT(cond, mutex, timeout) \
    do{\
        GTimeVal now;\
        g_get_current_time(&now);\
        g_time_val_add(&now, (glong)(timeout));\
        g_cond_timed_wait((cond),(mutex),&now);\
    }while(0)


static GTimeVal timeout = {1,0};

void
mfw_gst_streaming_cache_delete(void * cache)
{
    
    StreamingCache * scache;
    if (cache==NULL){
        return -1;
    }
    
    scache = (StreamingCache *)cache;
    gst_adapter_clear(scache->adapter);
    gst_object_unref(scache->adapter);
    g_cond_free(scache->produce_cond);
    g_cond_free(scache->consume_cond);
    g_mutex_free(scache->mutex);

    g_free(scache);
    
}

void
mfw_gst_streaming_cache_close(void * cache)
{
    StreamingCache * scache;
    if (cache==NULL){
        return -1;
    }
    
    scache = (StreamingCache *)cache;
    scache->close = TRUE;
}


void * 
mfw_gst_streaming_cache_new(GstPad * srcpad, guint64 maxsize)
{
    StreamingCache * scache = g_malloc(sizeof(StreamingCache));
    if (scache==NULL)
        goto error;
    
    memset(scache, 0, sizeof(StreamingCache));
    
    scache->srcpad = srcpad;
    scache->adapter=gst_adapter_new();
    
    scache->mutex = g_mutex_new();
    scache->consume_cond = g_cond_new();
    scache->produce_cond = g_cond_new();
    
    scache->maxsize = maxsize;
    
    return (void *)scache;
    
error:
    if (scache){
        mfw_gst_streaming_cache_delete(scache);
    }
    return NULL;
    
}

gint64 
mfw_gst_streaming_cache_avaliable_bytes(void * cache, guint64 size)
{
    StreamingCache * scache;
    gint64 avail;
    
    if (cache==NULL){
        return -1;
    }
    
    scache = (StreamingCache *)cache;
    
    g_mutex_lock(scache->mutex);
    
    scache = (StreamingCache *)cache;
    avail = scache->size-(scache->offset-scache->addr);
        
    g_mutex_unlock(scache->mutex);
    
    return avail;
}

void 
mfw_gst_streaming_cache_new_segment(void * cache, guint64 addr)
{
    StreamingCache * scache;
    
    if (cache==NULL){
        return -1;
    }
    
    scache = (StreamingCache *)cache;
    
    g_mutex_lock(scache->mutex);

    scache->seeking = FALSE;
    scache->addr = scache->offset = addr;
    gst_adapter_clear(scache->adapter);
    scache->size = 0;
    scache->eos=FALSE;

    g_cond_signal(scache->consume_cond);
    
    g_mutex_unlock(scache->mutex);
}

void 
mfw_gst_streaming_cache_add_buffer(void * cache, GstBuffer * buffer)
{
    StreamingCache * scache;
    guint64 size;
    
    if ( (cache==NULL) || (buffer==NULL)){
        return;
    }
    
    scache = (StreamingCache *)cache;
    size = GST_BUFFER_SIZE(buffer);

    g_mutex_lock(scache->mutex);

    if (scache->seeking==TRUE){
        gst_buffer_unref(buffer);
        g_mutex_unlock(scache->mutex);
        return;
    }
    
    
    if (scache->maxsize){
        if (scache->maxsize<size){
            scache->maxsize = size;
        }
        
        while((scache->size>scache->maxsize)&&(scache->close==FALSE)){
            WAIT_COND_TIMEOUT(scache->consume_cond, scache->mutex, 1000000);
        }
    }

    scache->size+=size;
    gst_adapter_push(scache->adapter, buffer);
    
    g_cond_signal(scache->produce_cond);
    
    g_mutex_unlock(scache->mutex);
}

void 
mfw_gst_streaming_cache_seteos(void * cache, gboolean eos)
{
    StreamingCache * scache;
    
    if ( cache==NULL){
        return;
    }
    
    scache = (StreamingCache *)cache;
    
    g_mutex_lock(scache->mutex);

    scache->eos = eos;
    g_cond_signal(scache->produce_cond);
    
    g_mutex_unlock(scache->mutex);
}

gboolean
mfw_gst_streaming_cache_geteos(void * cache)
{
    StreamingCache * scache;
    gboolean eos;
    
    if ( cache==NULL){
        return;
    }
    
    scache = (StreamingCache *)cache;
    
    g_mutex_lock(scache->mutex);

    eos = scache->eos;
    
    g_mutex_unlock(scache->mutex);
    
    return eos;
}

void
mfw_gst_streaming_cache_seek(void * cache, guint64 addr)
{
    StreamingCache * scache;
    gboolean ret;
    guint flushsize;
    
    if (cache==NULL){
        return;
    }

    
    scache = (StreamingCache *)cache;

    g_mutex_lock(scache->mutex);

    
    if ((addr==scache->addr)||((addr>scache->addr) && ((addr<=scache->addr+scache->maxsize)))){
        g_mutex_unlock(scache->mutex);
        return;
    }


    gst_adapter_clear(scache->adapter);
    g_cond_signal(scache->consume_cond);
    scache->size = 0;
    scache->addr = addr;
    scache->offset = addr;

    scache->seeking=TRUE;
    scache->eos=FALSE;
    g_mutex_unlock(scache->mutex);
    ret =  gst_pad_push_event (scache->srcpad, gst_event_new_seek ((gdouble)1, GST_FORMAT_BYTES, GST_SEEK_FLAG_FLUSH, GST_SEEK_TYPE_SET, (gint64)addr,
      GST_SEEK_TYPE_NONE, (gint64)(-1)));
    return 0;
}


gint64 
mfw_gst_streaming_cache_read(void * cache, guint64 addr, guint64 size, char * buffer)
{
    StreamingCache * scache;
    gboolean ret;
    guint flushsize;
    
    if (cache==NULL){
        return -1;
    }

    
    
    scache = (StreamingCache *)cache;

try_read:

    if (scache->close==TRUE){
        return -1;
    }
    
    g_mutex_lock(scache->mutex);

    if (scache->seeking==TRUE)
        goto not_enough_bytes;

    if (addr<scache->addr){
        goto need_reseek;
    }

    if (addr>scache->offset+scache->maxsize){
        goto need_reseek;
    }

    if (flushsize=addr-scache->addr)  {
        gst_adapter_flush(scache->adapter,flushsize);
        g_cond_signal(scache->consume_cond);
        scache->addr+=flushsize;
        scache->size-=flushsize;
    }

    if ((scache->maxsize) && (scache->maxsize<size)){
        scache->maxsize = size;
        /* enlarge maxsize means consumed */
        g_cond_signal(scache->consume_cond);
    }
    
    if (addr+size>scache->addr+scache->size){
        if (scache->eos){/* not enough bytes when eos */
            gst_adapter_copy(scache->adapter, buffer, 0, scache->size);
            return scache->size;
        }
        goto not_enough_bytes;
    }

    gst_adapter_copy(scache->adapter, buffer, 0, size);
    scache->offset=addr+size;

    g_mutex_unlock(scache->mutex);
    return size;

need_reseek:
    gst_adapter_clear(scache->adapter);
    g_cond_signal(scache->consume_cond);
    scache->size = 0;
    scache->addr = addr;
    scache->offset = addr;
    scache->seeking=TRUE;
    scache->eos=FALSE;
    g_mutex_unlock(scache->mutex);
    ret =  gst_pad_push_event (scache->srcpad, gst_event_new_seek ((gdouble)1, GST_FORMAT_BYTES, GST_SEEK_FLAG_FLUSH, GST_SEEK_TYPE_SET, (gint64)addr,
      GST_SEEK_TYPE_NONE, (gint64)(-1)));

    g_mutex_lock(scache->mutex);
not_enough_bytes:
    //g_print("not enough\n");
    WAIT_COND_TIMEOUT(scache->produce_cond, scache->mutex, 1000000);
    g_mutex_unlock(scache->mutex);

    goto try_read;
fail:
    g_print("err coming\n");
    return -1;
}
