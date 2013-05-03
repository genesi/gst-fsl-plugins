/*
 * Copyright (C)2010 Freescale Semiconductor, Inc. All rights reserved.
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
 * Module Name:    TimeStamp.c
 *
 * Description:    include TimeStamp stratege for VPU / SW video decoder plugin
 *
 * Portability:    This code is written for Linux OS and Gstreamer
 */

/*
 * Changelog:
  11/2/2010        draft version       Lyon Wang
 *
 */
//#include <gst/gst.h>
#include "TimeStamp.h"
#include <stdio.h>
#include <stdlib.h>


#define xDEBUG

const char *debug_env = "ME_DEBUG";
char *debug= NULL;
int debug_level = 0;

#define TSM_HISTORY_POWER 5
#define TSM_HISTORY_SIZE (1<<TSM_HISTORY_POWER)
#define TSM_ADAPTIVE_INTERVAL(tsm) \
    (tsm->dur_history_total>>TSM_HISTORY_POWER)

#define TSM_SECOND ((TSM_TIMESTAMP)1000000000)
#define TSM_DEFAULT_INTERVAL (TSM_SECOND/30)
#define TSM_DEFAULT_TS_BUFFER_SIZE (128)

#define TSM_TS_IS_VALID(ts)	\
    ((ts) != TSM_TIMESTAMP_NONE)

#define TSM_DISTANCE(tsm)\
    (((tsm->rx)>=(tsm->tx))?((tsm->rx)-(tsm->tx)):(tsm->ts_buf_size-(tsm->tx)+(tsm->rx)))

#define TSM_PLUS_AGE(tsm)\
    (TSM_DISTANCE(tsm)+tsm->invalid_ts_count+2)

#define TSM_ABS(ts0, ts1)\
    (((ts0)>(ts1))?((ts0)-(ts1)):((ts1)-(ts0)))

#define TSM_TIME_FORMAT "u:%02u:%02u.%09u"

#define TSM_TIME_ARGS(t) \
        TSM_TS_IS_VALID (t) ? \
        (unsigned int) (((TSM_TIMESTAMP)(t)) / (TSM_SECOND * 60 * 60)) : 99, \
        TSM_TS_IS_VALID (t) ? \
        (unsigned int) ((((TSM_TIMESTAMP)(t)) / (TSM_SECOND * 60)) % 60) : 99, \
        TSM_TS_IS_VALID (t) ? \
        (unsigned int) ((((TSM_TIMESTAMP)(t)) / TSM_SECOND) % 60) : 99, \
        TSM_TS_IS_VALID (t) ? \
        (unsigned int) (((TSM_TIMESTAMP)(t)) % TSM_SECOND) : 999999999

#define TSM_BUFFER_SET(buf, value, size) \
    do {\
        int i;\
        for (i=0;i<(size);i++){\
            (buf)[i] = (value);\
        }\
    }while(0)


typedef struct {
    TSM_TIMESTAMP ts;
    unsigned long long  age;
}TSMControl;

typedef struct _TSManager
{
    int first;
    int rx;    //timestamps received
    int tx;    //timestamps transfered
    TSM_TIMESTAMP  last_ts_sent;        //last time stamp sent
    unsigned int invalid_ts_count;
    TSMGR_MODE mode;
    int ts_buf_size;
    int dur_history_tx;
    TSM_TIMESTAMP dur_history_total;
    TSM_TIMESTAMP dur_history_buf[TSM_HISTORY_SIZE];
    TSMControl * ts_buf;
    unsigned long long age;
//#ifdef DEBUG
    int tx_cnt;
    int rx_cnt;
//#endif
} TSManager;


/*======================================================================================
FUNCTION:           mfw_gst_receive_ts

DESCRIPTION:        Check timestamp and do frame dropping if enabled

ARGUMENTS PASSED:   pTimeStamp_Object  - TimeStamp Manager to handle related timestamp
                    timestamp - time stamp of the input buffer which has video data.

RETURN VALUE:       None
PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=======================================================================================*/
void TSManagerReceive(void * handle, TSM_TIMESTAMP timestamp)
{
    TSManager * tsm = (TSManager *)handle;

    if (tsm){
        if (tsm->mode==MODE_AI){

            if (TSM_TS_IS_VALID (timestamp) && (TSM_ABS(timestamp, tsm->last_ts_sent)<TSM_SECOND*10))
            {
                tsm->ts_buf[tsm->rx].ts = timestamp;
                tsm->ts_buf[tsm->rx].age = tsm->age+TSM_PLUS_AGE(tsm);
                #ifdef DEBUG
                //printf("age should %lld %lld\n", tsm->age, tsm->ts_buf[tsm->rx].age);
                //printf("++++++ distance = %d  tx=%d, rx=%d, invalid count=%d\n", TSM_DISTANCE(tsm), tsm->tx, tsm->rx,tsm->invalid_ts_count);
                #endif
                tsm->rx = ((tsm->rx + 1) % tsm->ts_buf_size);
            }
            else {
                tsm->invalid_ts_count++;
            }
        }else if (tsm->mode==MODE_FIFO){
            tsm->ts_buf[tsm->rx].ts = timestamp;
            tsm->rx = ((tsm->rx + 1) % tsm->ts_buf_size);
        }
    }


    if(debug_level==2) {
        printf("coming %d  ts = %"TSM_TIME_FORMAT", last_ts=%"TSM_TIME_FORMAT", invalid cnt=%d\n", 
            tsm->rx_cnt++, TSM_TIME_ARGS(timestamp), TSM_TIME_ARGS(tsm->last_ts_sent),  tsm->invalid_ts_count);
    }
    //#endif
}

/*======================================================================================
FUNCTION:           TSManagerSend

DESCRIPTION:        Check timestamp and do frame dropping if enabled

ARGUMENTS PASSED:   pTimeStamp_Object  - TimeStamp Manager to handle related timestamp
                    ptimestamp - returned timestamp to use at render

RETURN VALUE:       None
PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=======================================================================================*/
TSM_TIMESTAMP TSManagerSend (void * handle)
{
    TSManager * tsm = (TSManager *)handle;
    int i = tsm->tx;
    int index = -1;
    TSM_TIMESTAMP ts0, tstmp = TSM_TIMESTAMP_NONE;
    int age;
    TSM_TIMESTAMP half_interval = TSM_ADAPTIVE_INTERVAL(tsm)>>1;

    if (tsm==NULL)
        goto fail;



    if (tsm->mode==MODE_AI){

        if (tsm->first==0){
            tstmp = tsm->last_ts_sent + TSM_ADAPTIVE_INTERVAL(tsm);
        }else{
            tstmp = tsm->last_ts_sent;
        }

        while (i != tsm->rx) {
            if (index>=0) {
                if (tsm->ts_buf[i].ts < ts0) {
                    ts0 = tsm->ts_buf[i].ts;
                    age = tsm->ts_buf[i].age;
                    index = i;
                }
            }
            else {
                ts0 = tsm->ts_buf[i].ts;
                age = tsm->ts_buf[i].age;
                index = i;
            }
            i = ((i + 1) % tsm->ts_buf_size);
        }
        if (index>=0) {
            if ((tsm->invalid_ts_count) && (ts0 >=((tstmp) + half_interval)) && (age>tsm->age)){
                /* use calculated ts0 */
                tsm->invalid_ts_count--;
            }else{
                if (index != tsm->tx) {
                    tsm->ts_buf[index] =
                        tsm->ts_buf[tsm->tx];
                }
                tsm->tx = ((tsm->tx + 1) % tsm->ts_buf_size);

               #if 0 
                if (ts0 >=((tstmp) + half_interval))
                    tstmp = tstmp;
                else
                    tstmp = ts0;
               #else
                tstmp = ts0;
               #endif
            }

        }else{
            tsm->invalid_ts_count--;
        }

        if (tsm->first==0){
        

            if (tstmp>tsm->last_ts_sent){
                ts0 = (tstmp-tsm->last_ts_sent);
            }else{
                ts0 = 0;
                tstmp=tsm->last_ts_sent;
            }

           if(debug_level==3) 
            {
                if (ts0>TSM_ADAPTIVE_INTERVAL(tsm)*3/2)
                    printf("Warning: !!!!!!!!!!!!dur this time %"TSM_TIME_FORMAT" > frame_interval:%"TSM_TIME_FORMAT"* 3/2\n",
                        TSM_TIME_ARGS(ts0), TSM_TIME_ARGS(TSM_ADAPTIVE_INTERVAL(tsm)));
            }

            tsm->dur_history_total-=tsm->dur_history_buf[tsm->dur_history_tx];
            tsm->dur_history_buf[tsm->dur_history_tx] = ts0;
            tsm->dur_history_tx = ((tsm->dur_history_tx + 1) % TSM_HISTORY_SIZE);
            tsm->dur_history_total+=ts0;
        }

        tsm->last_ts_sent = tstmp;
        tsm->age++;
        tsm->first = 0;

        
    }else if (tsm->mode==MODE_FIFO){
        if (tsm->tx!=tsm->rx){
            tstmp = tsm->ts_buf[tsm->tx].ts;
            tsm->tx = ((tsm->tx + 1) % tsm->ts_buf_size);
            tsm->last_ts_sent = tstmp;
        }else{
            tstmp = tsm->last_ts_sent;
            printf("\FATAL Error[no timestamp received], Maybe a framework bug!\n");
        }
    }

    if(debug_level==1){
        printf("frame_interval = %"TSM_TIME_FORMAT"\n", TSM_TIME_ARGS(TSM_ADAPTIVE_INTERVAL(tsm)));
    }
    
    if(debug_level==2) {
        printf("outing +++++++ %d  ts out:%"TSM_TIME_FORMAT" frame_interval:%"TSM_TIME_FORMAT"\n", 
        tsm->tx_cnt++, TSM_TIME_ARGS(tstmp), TSM_TIME_ARGS(ts0));
    }
    //#endif
fail:    
    
    return tstmp;
}

void resyncTSManager(void * handle, TSM_TIMESTAMP synctime, TSMGR_MODE mode)
{
    TSManager * tsm = (TSManager *)handle;
    if (tsm){
        tsm->first = 1;
        if (TSM_TS_IS_VALID(synctime))
            tsm->last_ts_sent = synctime;

        tsm->tx = tsm->rx = 0;
        tsm->invalid_ts_count = 0;
        tsm->mode = mode;
        tsm->age = 0;
        //#ifdef DEBUG
        tsm->rx_cnt = tsm->tx_cnt = 0;
        //#endif
        //TSM_BUFFER_SET(tsm->ts_buf, TSM_TIMESTAMP_NONE, tsm->ts_buf_size);
    }
}


/*======================================================================================
FUNCTION:           mfw_gst_init_ts

DESCRIPTION:        malloc and initialize timestamp strcture

ARGUMENTS PASSED:   ppTimeStamp_Object  - pointer of TimeStamp Manager to handle related timestamp

RETURN VALUE:       TimeStamp structure pointer
PRE-CONDITIONS:     None
POST-CONDITIONS:    None
IMPORTANT NOTES:    None
=======================================================================================*/
void * createTSManager(int ts_buf_size)
{
    TSManager * tsm = (TSManager *)malloc(sizeof(TSManager));
    debug = getenv(debug_env);
    if (debug){
        debug_level = atoi(debug);
    }
    
   // printf("debug = %s \n ++++++++++++++++++++++++++++",debug);
    if (tsm){
        tsm->tx = tsm->rx = 0;
        if (ts_buf_size<=0){
            ts_buf_size = TSM_DEFAULT_TS_BUFFER_SIZE;
        }
        tsm->ts_buf_size = ts_buf_size;
        tsm->ts_buf = malloc(sizeof(TSMControl)*ts_buf_size);

        if(tsm->ts_buf==NULL) {
            goto fail;
        }

        resyncTSManager(tsm, (TSM_TIMESTAMP)0, MODE_AI);
        
        tsm->dur_history_tx = 0;
        TSM_BUFFER_SET(tsm->dur_history_buf, TSM_DEFAULT_INTERVAL, TSM_HISTORY_SIZE);
        tsm->dur_history_total = TSM_DEFAULT_INTERVAL<<TSM_HISTORY_POWER;
    }
    return tsm;
fail:    
    if (tsm){
        if (tsm->ts_buf){
            free(tsm->ts_buf);
        }
        free(tsm);
        tsm=NULL;
    }
    return tsm;
}


void destroyTSManager(void * handle)
{
    TSManager * tsm = (TSManager *)handle;
    if (tsm){
        if (tsm->ts_buf){
            free(tsm->ts_buf);
        }
        free(tsm);
        tsm=NULL;
    }
}


void setTSManagerFrameRate(void * handle, int fps_n, int fps_d)
//void setTSManagerFrameRate(void * handle, float framerate)
{
    TSManager * tsm = (TSManager *)handle;
    TSM_TIMESTAMP ts;
    if (fps_n != 0)
        ts = TSM_SECOND*fps_d/fps_n;
    else
        ts = TSM_DEFAULT_INTERVAL;
   // TSM_TIMESTAMP ts = TSM_SECOND / framerate;
   
    if (tsm){
        TSM_BUFFER_SET(tsm->dur_history_buf, ts, TSM_HISTORY_SIZE);
        tsm->dur_history_total = (ts<<TSM_HISTORY_POWER);
    if(debug)
        printf("+++Frame intrval:%"TSM_TIME_FORMAT"+++\n",TSM_TIME_ARGS(ts));
    }
}

TSM_TIMESTAMP getTSManagerFrameInterval(void * handle)
{
    TSManager * tsm = (TSManager *)handle;
    TSM_TIMESTAMP ts = 0;
    if (tsm){
        ts = TSM_ADAPTIVE_INTERVAL(tsm);
    }
    return ts;
}

TSM_TIMESTAMP getTSManagerPosition(void * handle)
{
    TSManager * tsm = (TSManager *)handle;
    TSM_TIMESTAMP ts = 0;
    if (tsm){
       ts = tsm->last_ts_sent;
    }
    return ts;
}
