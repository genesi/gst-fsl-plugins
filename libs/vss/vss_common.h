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
 * Module Name:    vss_common.h
 *
 * Description:    Head file for ipu lib based render service.
 *
 * Portability:    This code is written for Linux OS.
 */  
 
/*
 * Changelog: 
 *
 */

#ifndef __VSS_COMMON_H__
#define __VSS_COMMON_H__

#define VS_ERROR(format,...)    DEBUG_ERROR(format, ##__VA_ARGS__)
#define VS_FLOW(format,...)     DEBUG_FLOW(format, ##__VA_ARGS__)
#define VS_MESSAGE(format,...)  DEBUG_MESSAGE(format, ##__VA_ARGS__)

#define WIN_FMT "(%d,%d-%d,%d:%dx%d)"
#define WIN_ARGS(rect) \
    (rect)->left,(rect)->top,(rect)->right,(rect)->bottom,(rect)->right-(rect)->left,(rect)->bottom-(rect)->top

#define FOURCC_FMT "%c%c%c%c"
#define FOURCC_ARGS(fourcc) (char)(fourcc),(char)((fourcc)>>8),(char)((fourcc)>>16),(char)((fourcc)>>24)

#define VS_MAX 8
#define VS_SUBFRAME_MAX 1

#define ALPHA_TRANSPARENT 0
#define ALPHA_SOLID 255
#define RGB565_BLACK 0

#define MAIN_DEVICE_NAME "/dev/fb0"
#define VS_LOCK_NAME "vss_lock"
#define VS_SHMEM_NAME "vss_shmem"

#define MIN_RENDER_INTERVAL_IN_MICROSECOND (33000/2)

#define VS_LEFT_OUT     0x1
#define VS_RIGHT_OUT    0x2
#define VS_TOP_OUT      0x4
#define VS_BOTTOM_OUT   0x8



#define VS_IPC_CREATE 0x1
#define VS_IPC_EXCL 0x2

#define DEVICE_LEFT_EDGE 0
#define DEVICE_TOP_EDGE 0

#define SUBFRAME_DEFAULT_FMT IPU_PIX_FMT_ABGR32

#define SUBFRAME_FMT IPU_PIX_FMT_YUYV
#define FB1_FMT IPU_PIX_FMT_UYVY
#define FB2_FMT IPU_PIX_FMT_RGB565

#define CLEAR_SOURCE_LENGTH 64

/* function macros */
#define VS_IOCTL(device, request, errorroute, ...)\
    do{\
        int ret;\
        if ((ret = ioctl((device), (request), ##__VA_ARGS__))<0){\
            VS_ERROR("%s:%d ioctl error, return %d\n", __FILE__, __LINE__, ret);\
            goto errorroute;\
        }\
    }while(0)

#define VS_LOCK(lock) \
    do {\
                sem_wait((lock));\
    }while(0)
#define VS_TRY_LOCK(lock) \
    do {\
        sem_trywait((lock));\
    }while(0)
#define VS_UNLOCK(lock) \
    do {\
        sem_post((lock));\
    }while(0)
    





#define DEVICE2HEADSURFACE(device)\
    (((device)->headid==0)?NULL:(&(gVSctl->surfaces[(device)->headid-1])))

#define SET_DEVICEHEADSURFACE(device, surface)\
    do{\
        if ((surface)){\
            (device)->headid = (surface)->id;\
        }else{\
            (device)->headid = 0;\
        }\
    }while(0)

#define SET_NEXTSURFACE(surfacebefore,surface)\
    do{\
        if ((surface)){\
            (surfacebefore)->nextid = (surface)->id;\
        }else{\
            (surfacebefore)->nextid = 0;\
        }\
    }while(0)

#define NEXTSURFACE(surface)\
    (((surface)->nextid==0)?NULL:(&(gVSctl->surfaces[(surface)->nextid-1])))

#define SURFACE2DEVICE(surface)\
    (&(gVSctl->devices[(surface)->vd_id-1]))

#define NEXT_RENDER_ID(idx)\
    (((idx)==0)?1:0)

#define ID2INDEX(id) ((id)-1)
#define INDEX2ID(index) ((index)+1)


#define OVERLAPED_RECT(rect1, rect2)\
    (((rect1)->top<(rect2)->bottom)||((rect2)->top<(rect1)->bottom)\
    ||((rect1)->left<(rect2)->right)||((rect2)->left<(rect1)->right))

#define KICK_IPUTASKONE(itask)\
    do{\
        if ((itask)->mode==0){\
            mxc_ipu_lib_task_init(&(itask)->input, NULL, &(itask)->output, TASK_PP_MODE|OP_NORMAL_MODE, &(itask)->handle);\
            mxc_ipu_lib_task_buf_update(&(itask)->handle, NULL, NULL, NULL, NULL,NULL);\
            mxc_ipu_lib_task_uninit(&(itask)->handle);\
        }else{\
            mxc_ipu_lib_task_init(&(itask)->input, &(itask)->overlay, &(itask)->output, TASK_VF_MODE|OP_NORMAL_MODE, &(itask)->handle);\
            mxc_ipu_lib_task_buf_update(&(itask)->handle, NULL, NULL, NULL, NULL,NULL);\
            mxc_ipu_lib_task_uninit(&(itask)->handle);\
        }\
    }while(0)

#define ALIGNLEFT8(value)\
        do{\
            (value) = (((value)>>3)<<3);\
        }while(0)
    
#define ALIGNRIGHT8(value)\
        do{\
            (value) = (((value+7)>>3)<<3);\
        }while(0)

/*=============================================================================
                LOCAL TYPEDEFS (STRUCTURES, UNIONS, ENUMS)
=============================================================================*/



typedef enum{
    VS_STATUS_IDLE = 0,
    VS_STATUS_VISIBLE,
    VS_STATUS_INVISIBLE,
}VS_STATUS;

typedef sem_t VSLock;

typedef struct {
    int updated;
    Rect rect;
}Updated;

typedef struct {
    ipu_lib_input_param_t input;
    ipu_lib_output_param_t output;
    ipu_lib_overlay_param_t overlay;
    ipu_lib_handle_t handle;
    int mode; /* 0 no overlay; 1: with overlay */
}IPUTaskOne;

typedef struct {
    int size;
    void * handle;
    char * paddr;
    char * vaddr;
}DBuffer;

typedef struct {
    DBuffer imgbuf;
    DBuffer alphabuf;
    Rect display;
}SubFrameBuffer;

typedef struct _VideoSurface{
    int id;
    int nextid;
    int vd_id;
    
    volatile void * paddr;

    int mainframeupdate;
    SubFrame subframes[VS_SUBFRAME_MAX]; 
    SubFrameBuffer subframesbuffer; /* subtitle buffer, dmable */
    
    volatile unsigned int rendmask; /* render mask for pingpang buffer */
    VS_STATUS status;
    SourceFmt srcfmt;
    DestinationFmt desfmt;
    Rect adjustdesrect;
    IPUTaskOne itask;
    int outside; /* out of screen and need reconfig input */
    struct _VideoSurface * next;
}VideoSurface;

typedef struct _VideoDevice{
    int headid;
    int fbidx;
    int main_fbidx;
    int renderidx;
    void * bufaddr[2];
    int fmt;
    
    Rect disp;
    int resX;
    int resY;
    
    int id;
    int init;
    int setalpha;
    
    struct fb_var_screeninfo fbvar;
    int cnt;

#ifdef METHOD2    
    IPUTaskOne copytask;
#endif
    struct timeval timestamp;
    int vsmax;

    int current_mode;
    int mode_num;
    VideoMode modes[VM_MAX];

    char name[NAME_LEN];
    
}VideoDevice;

typedef struct {
    VideoSurface surfaces[VS_MAX];
    VideoDevice devices[VD_MAX];/* start from fb1 to fb2 */
    int init;
}VideoSurfacesControl;

typedef VSFlowReturn (* ConfigHandle)(void *, void *);

typedef struct{
    VSConfigID cid;
    int parameterlen;
    ConfigHandle handle;
}ConfigHandleEntry;

#endif
