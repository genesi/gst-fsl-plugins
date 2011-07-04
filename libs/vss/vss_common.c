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
 * Module Name:    vss_common.c
 *
 * Description:    Implementation for ipu lib based render service.
 *
 * Portability:    This code is written for Linux OS.
 */  
 
/*
 * Changelog: 
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <stdint.h>

#include <semaphore.h>
#include <fcntl.h>

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <dlfcn.h>


#include "mxc_ipu_hl_lib.h"
#include "video_surface.h"

#include "fsl_debug.h"

#include "vss_common.h"

#include "sconf.h"

typedef struct{
    const char * devname;
    int fb_fd;
    
}FbDesc;

#define MX37_MX51_PAL_MODE      "U:720x576i-50\n"
#define MX37_MX51_NTSC_MODE     "U:720x480i-60\n"
#define MX37_MX51_PAL_MODE      "U:720x576i-50\n"
#define MX37_MX51_720P_MODE      "U:1280x720p-60\n"

static FbDesc gFBDescs[] = {
    {"/dev/fb0", 0},
    {"/dev/fb1", 0},
    {"/dev/fb2", 0}
};

extern  VSLock * 	gVSlock;
extern  VideoSurfacesControl * gVSctl;

typedef void * (* new_hwbuf_func)(int, void **, void **, int flags);
typedef void (* free_hwbuf_func)(void *);
static new_hwbuf_func g_new_hwbuf_handle = NULL;
static free_hwbuf_func g_free_hwbuf_handle = NULL;
static void * g_dlhandle = NULL;


void open_allocator_dll()
{
    char * errstr;
    g_dlhandle = dlopen("libmfwba.so", RTLD_LAZY);
    
    if (!g_dlhandle) {
        printf("Can not open dll, %s.\n", dlerror());
        goto error;
    }
    
    dlerror();
    g_new_hwbuf_handle = dlsym(g_dlhandle, "mfw_new_hw_buffer");
    if ((errstr = dlerror()) != NULL)  {
        printf("Can load symbl for mfw_new_hw_buffer, %s\n", errstr);
        goto error;
    }
    
    dlerror();
    g_free_hwbuf_handle = dlsym(g_dlhandle, "mfw_free_hw_buffer");
    if ((errstr = dlerror()) != NULL)  {
        printf("Can load symbl for mfw_free_hw_buffer, %s\n", errstr);
        goto error;
    }

    return;

error:
    if (g_dlhandle){
        dlclose(g_dlhandle);
        g_dlhandle=NULL;
    }
    g_new_hwbuf_handle = NULL;
    g_free_hwbuf_handle = NULL;
    
}


int fmt2bit(unsigned long fmt)
{
    int bits = 0;
    switch(fmt){
        case IPU_PIX_FMT_ABGR32:
            bits=32;
        break;
        case IPU_PIX_FMT_RGB565:
        case IPU_PIX_FMT_RGB555:
        case IPU_PIX_FMT_YUYV:
        case IPU_PIX_FMT_UYVY:
            bits=16;
        break;
        case IPU_PIX_FMT_RGB32:
            bits=32;
        break;
    }
    return bits;
}

int fmt2cs(unsigned long fmt)
{
    int cs = -1;
    switch(fmt){
        case IPU_PIX_FMT_ABGR32:
        case IPU_PIX_FMT_RGB565:
        case IPU_PIX_FMT_RGB555:
        case IPU_PIX_FMT_RGB32:
            cs=1;
        break;
        case IPU_PIX_FMT_YUYV:
        case IPU_PIX_FMT_UYVY:
            cs=0;
        break;
    }
    return cs;
}

int _getDevicefd(VideoDevice * vd)
{
    int fd;
    if ((fd = gFBDescs[vd->fbidx].fb_fd)==0){
        fd = open(gFBDescs[vd->fbidx].devname, O_RDWR, 0);
        if (fd<=0)
            fd = 0;
        else
            gFBDescs[vd->fbidx].fb_fd = fd;
    }
    return fd;
}


void _dBufferRealloc(DBuffer * dbuf, int size)
{

    if (g_dlhandle==NULL){
        open_allocator_dll();
    }
    
    if (dbuf->handle){
        (*g_free_hwbuf_handle)(dbuf->handle);
    }
    dbuf->handle = (*g_new_hwbuf_handle)(size, (void **)(&dbuf->paddr), (void **)(&dbuf->vaddr), 0);
    dbuf->size = size;
}


void _dBufferFree(DBuffer * dbuf)
{

    
    
    if (dbuf->handle){
        (*g_free_hwbuf_handle)(dbuf->handle);
    }
    dbuf->handle = NULL;
    dbuf->size = 0;
}




VSLock * 
_getAndLockVSLock(int flag)
{
    VS_FLOW("Fun %s in\n", __FUNCTION__);

    VSLock * lock;

    int oflag = 0;
    if (flag & VS_IPC_CREATE)
        oflag |= O_CREAT;

    if (flag & VS_IPC_EXCL)
        oflag |= O_EXCL;

    umask(0);
    
    lock = sem_open(VS_LOCK_NAME, oflag, 0666  , 1);
    
	if(SEM_FAILED == lock){
        VS_ERROR("%s: can not get lock %s!\n", __FUNCTION__, VS_LOCK_NAME);
		goto err;
	}

    VS_LOCK(lock);
    
    return lock;
err:
    return NULL;
}

unsigned int fmt_from_str(char * value)
{
    unsigned int fmt, a, b, c, d;
    a = value[0];
    b = value[1];
    c = value[2];
    d = value[3];

    fmt = (((a)<<0)|((b)<<8)|((c)<<16)|((d)<<24));
    return fmt;
}

void video_mode_to_str(char * value, VideoMode * vm)
{
    if (value && vm)
        sprintf(value, "U:%dx%d%s-%d\n", vm->resx, vm->resy, (vm->interleave?"i":"p"), vm->hz);
}


int
video_mode_from_str(char * value, VideoMode * vm)
{
    char * tmp = strchr(value, ':');
    if (tmp==NULL) goto bail;
    value = tmp+1;
    tmp = strchr(value,'x');
    if (tmp==NULL) goto bail;
    *tmp = '\0';
    vm->resx = 0;
    vm->resx = atoi(value);
    if (vm->resx==0) goto bail;
    value = tmp+1;
    vm->resy = 0;
    vm->resy = atoi(value);
    if (vm->resy==0) goto bail;
    tmp=strchr(value, '-');
    if (tmp==NULL) goto bail;
    if (*(tmp-1)=='p')
        vm->interleave = 0;
    else if (*(tmp-1)=='i')
        vm->interleave = 1;
    else
        goto bail;

    value = tmp+1;
    vm->hz = 0;
    vm->hz = atoi(value);
    if (vm->hz==0) goto bail;
    
    return 0;
bail:
    return -1;
}

void _getVideoDeviceInfo(VideoDevice * vd)
{
    int fd;
    struct fb_var_screeninfo fb_var;

    if ((vd->fbidx!=vd->main_fbidx)||(vd->mode_num==0)){
        fd = open(gFBDescs[vd->main_fbidx].devname, O_RDWR, 0);

done:      
        if (fd>0){

            VS_IOCTL(fd, FBIOGET_VSCREENINFO, error, &fb_var);
            vd->resX = fb_var.xres;
            vd->resY = fb_var.yres;
            VS_MESSAGE("MAX resolution %dx%d\n", vd->resX, vd->resY);
error:
           close(fd);
        }
    }
}


void _initVSControl(VideoSurfacesControl * control)
{
    int i,n;
    ConfigSection * css, * cs;
    
    for (i=0;i<VS_MAX;i++){
        control->surfaces[i].id = INDEX2ID(i);
        control->surfaces[i].status = VS_STATUS_IDLE;
    }
    
    for (i=0;i<VD_MAX;i++){
        control->devices[i].id = INDEX2ID(i);
        control->devices[i].fbidx = -1; /* fb2 */
        control->devices[i].main_fbidx = -1; /* fb2 */
    }
    css = sconf_get_css_from_file("/usr/share/vssconfig");

    i=0;
    cs = css;
    while(cs){
        unsigned int fmt = 0;
        int fb_num = -1, main_fb_num = -1;
        char * value;
        VideoDevice * vd;
        char tmp[10];

        if (value = sconf_cs_get_field(cs,"fb_num", 0))
            fb_num = atoi(value);
        if (value = sconf_cs_get_field(cs,"main_fb_num", 0))
            main_fb_num = atoi(value);
        if (value = sconf_cs_get_field(cs,"format", 0))
            fmt = fmt_from_str(value);

        if ((fmt==0) || (fb_num==-1))
            goto next;

        vd = &control->devices[i];

        if (value = sconf_cs_get_name(cs, 0)){
            strncpy(vd->name, value, NAME_LEN);
            vd->name[NAME_LEN-1] = '\0';
        }else{
            sprintf(vd->name, "DISP%d", i);
        }
        
        vd->fmt = fmt;
        vd->fbidx = fb_num;

        if (main_fb_num!=-1)
            vd->main_fbidx = main_fb_num;
        else
            vd->main_fbidx = vd->fbidx;

        vd->vsmax = 4;
        if (value = sconf_cs_get_field(cs,"vsmax", 0))
            vd->vsmax = fmt_from_str(value);

        n=0;
        vd->mode_num = 0;
        do {
            sprintf(tmp, "mode%d", n);
            if (value = sconf_cs_get_field(cs, tmp, 0)){
                if (video_mode_from_str(value, &vd->modes[vd->mode_num])==0){
                    vd->mode_num++;
                }
            }
            n++;
        }while((vd->mode_num<VM_MAX)&&(value));

        _getVideoDeviceInfo(vd);

        i++;
        if (i>=VD_MAX)
            break;
next:        
        cs = cs->next;
    }

    sconf_free_cs(css);

}

VideoSurfacesControl * _getVSControl(int flag)
{
    VS_FLOW("Fun %s in\n", __FUNCTION__);

    VideoSurfacesControl * control;
	int	shmid;
	struct	stat shmStat;


    int oflag = O_RDWR;
    if (flag & VS_IPC_CREATE)
        oflag |= O_CREAT;
    if (flag & VS_IPC_EXCL)
        oflag |= O_EXCL;

    shmid = shm_open(VS_SHMEM_NAME, oflag, 0666);

    if(shmid == -1){
        VS_ERROR("%s: can not get share memory %s!\n", __FUNCTION__, VS_SHMEM_NAME);
		goto err;
	}
    
	ftruncate(shmid,(off_t)(3 * sizeof(VideoSurfacesControl)));
	/* Connect to the shm */
	fstat(shmid, &shmStat);

	control = (VideoSurfacesControl *)mmap(NULL,shmStat.st_size,PROT_READ|PROT_WRITE,MAP_SHARED,shmid,0);
    if ((control==NULL) || (control==MAP_FAILED)){
        VS_ERROR("%s: can not mmap share memory %d!\n", __FUNCTION__, shmid);
        goto err;
    }

    if (control->init==0){
        _initVSControl(control);
        control->init = 1;    
    }

    return control;
err:
    return NULL;
}






int _initVideoDevice(VideoDevice * vd, int mode_idx)
{
    struct fb_var_screeninfo fb_var;
    int fd;

    if ((vd->fbidx==vd->main_fbidx)&&(vd->mode_num)){
        
        if ((mode_idx>=0) && (mode_idx<vd->mode_num)){
            vd->current_mode = mode_idx;
            vd->resX = vd->modes[mode_idx].resx;
            vd->resY = vd->modes[mode_idx].resy;
        }else{
            goto error;
        }
    }

    char * palpha;

    if ((palpha = getenv("VSALPHA")) && (vd->main_fbidx!=vd->fbidx))
        vd->setalpha = 1;
    else
        vd->setalpha = 0;

    fd = _getDevicefd(vd);

    VS_IOCTL(fd, FBIOGET_VSCREENINFO, error,&fb_var);

    vd->fbvar = fb_var;

    if ((vd->resX==0) || (vd->resY==0))
        goto error;

    return 0;
error:
    return -1;
}


void
_fillDeviceLocalAlphaBuf(VideoDevice * vd, char * lbuf0, char * lbuf1)
{
    VideoSurface * vs = DEVICE2HEADSURFACE(vd);
    int stride = vd->disp.right-vd->disp.left;
    while(vs){
        int xoff, yoff;
        int width, height;
        int i;
        char * bufp0, * bufp1;
        xoff = vs->adjustdesrect.left-vd->disp.left;
        yoff = vs->adjustdesrect.top-vd->disp.top;
        width = vs->adjustdesrect.right-vs->adjustdesrect.left;
        height= vs->adjustdesrect.bottom-vs->adjustdesrect.top;
        bufp0 = lbuf0+stride*yoff+xoff;
        bufp1 = lbuf1+stride*yoff+xoff;
        for (i=0;i<height;i++){
            memset(bufp0, ALPHA_SOLID, width);
            bufp0+=stride;
            memset(bufp1, ALPHA_SOLID, width);
            bufp1+=stride;
        }
        vs = NEXTSURFACE(vs);
    };
}


int
_setAlpha(VideoDevice * vd)
{

    int fd;
    unsigned long alpha_buf0;
    unsigned long alpha_buf1;
    unsigned long loc_alpha_phy_addr0;
    unsigned long loc_alpha_phy_addr1;
    
    struct mxcfb_loc_alpha l_alpha;

    unsigned long l_alpha_buf_size;


    //return 0;

    fd = _getDevicefd(vd);

    if (0){//(vd->cnt==1){
        struct mxcfb_gbl_alpha g_alpha;
        printf("set global alpha\n");
        g_alpha.alpha = ALPHA_SOLID;
        g_alpha.enable = 1;
        VS_IOCTL(fd, MXCFB_SET_GBL_ALPHA, done, &g_alpha);
    }else{
    
    l_alpha.enable = 1;
    l_alpha.alpha_in_pixel = 0;
    l_alpha.alpha_phy_addr0 = 0;
    l_alpha.alpha_phy_addr1 = 0;

    
    VS_IOCTL(fd, MXCFB_SET_LOC_ALPHA, done, &l_alpha);



    l_alpha_buf_size = (vd->disp.right-vd->disp.left)*(vd->disp.bottom-vd->disp.top);

    loc_alpha_phy_addr0 = (unsigned long)(l_alpha.alpha_phy_addr0);
    loc_alpha_phy_addr1 = (unsigned long)(l_alpha.alpha_phy_addr1);
    
    alpha_buf0 = (char *)mmap(0, l_alpha_buf_size,
                 PROT_READ | PROT_WRITE,
                 MAP_SHARED, fd,
                 loc_alpha_phy_addr0);
    if ((int)alpha_buf0 == -1) {
        VS_ERROR("Error: failed to map alpha buffer 0"
               " to memory.\n");
        goto done;
    }
    alpha_buf1 = (char *)mmap(0, l_alpha_buf_size,
                 PROT_READ | PROT_WRITE,
                 MAP_SHARED, fd,
                 loc_alpha_phy_addr1);
    if ((int)alpha_buf1 == -1) {
        VS_ERROR("Error: failed to map alpha buffer 1"
               " to memory.\n");
        munmap((void *)alpha_buf0, l_alpha_buf_size);
        return -1;
    }

    memset(alpha_buf0, ALPHA_TRANSPARENT, l_alpha_buf_size);
    memset(alpha_buf1, ALPHA_TRANSPARENT, l_alpha_buf_size);

    _fillDeviceLocalAlphaBuf(vd, alpha_buf0, alpha_buf1);
    munmap(alpha_buf0, l_alpha_buf_size);
    munmap(alpha_buf1, l_alpha_buf_size);
    }
done:   
    return 0;
}


void
_clearVideoSurfaceBackground(VideoDevice * vd, VideoSurface * vs)
{
    DBuffer dbuf;
    dbuf.handle = NULL;
    _dBufferRealloc(&dbuf,CLEAR_SOURCE_LENGTH*CLEAR_SOURCE_LENGTH*fmt2bit(vd->fmt)/8);

    memset(dbuf.vaddr, 0, CLEAR_SOURCE_LENGTH*CLEAR_SOURCE_LENGTH*fmt2bit(vd->fmt)/8);

    IPUTaskOne itask;

    itask.mode = 0;
    
    ipu_lib_input_param_t * input = &itask.input;
    ipu_lib_output_param_t * output = &itask.output;

    memset(input, 0, sizeof(ipu_lib_input_param_t));
    *output = vs->itask.output;
    
    input->fmt = IPU_PIX_FMT_RGB565;
    input->width = input->height=CLEAR_SOURCE_LENGTH;
    input->user_def_paddr[0] = dbuf.paddr;
    


    output->user_def_paddr[0] = vd->bufaddr[0];

    KICK_IPUTASKONE(&itask);
    output->user_def_paddr[0] = vd->bufaddr[1];
    KICK_IPUTASKONE(&itask);

    _dBufferFree(&dbuf);
}

void
_clearVideoDeviceBackground(VideoDevice * vd)
{
    DBuffer dbuf;
    dbuf.handle = NULL;
    _dBufferRealloc(&dbuf,CLEAR_SOURCE_LENGTH*CLEAR_SOURCE_LENGTH*fmt2bit(vd->fmt)/8);

    memset(dbuf.vaddr, 0, CLEAR_SOURCE_LENGTH*CLEAR_SOURCE_LENGTH*fmt2bit(vd->fmt)/8);

    IPUTaskOne itask;

    itask.mode = 0;
    
    ipu_lib_input_param_t * input = &itask.input;
    ipu_lib_output_param_t * output = &itask.output;

    memset(input, 0, sizeof(ipu_lib_input_param_t));
    memset(output, 0, sizeof(ipu_lib_output_param_t));
    
    input->fmt = IPU_PIX_FMT_RGB565;
    input->width = input->height = CLEAR_SOURCE_LENGTH;
    input->user_def_paddr[0] = dbuf.paddr;

    output->width = vd->resX;
    output->height = vd->resY;

    output->fmt = vd->fmt;


    output->user_def_paddr[0] = vd->bufaddr[0];

    KICK_IPUTASKONE(&itask);
    output->user_def_paddr[0] = vd->bufaddr[1];
    KICK_IPUTASKONE(&itask);

    _dBufferFree(&dbuf);
}


int
_setDeviceConfig(VideoDevice * vd)
{
    VS_FLOW("Fun %s in\n", __FUNCTION__);
    
    struct fb_var_screeninfo fb_var;
    struct fb_fix_screeninfo fb_fix;
    struct mxcfb_pos pos;
    Rect * rect;
    
    int fd = _getDevicefd(vd);

    /* Workaround for ipu hardware, it need set to 0,0 before change another offset */    
    pos.x = 0;
    pos.y = 0;
    VS_IOCTL(fd, MXCFB_SET_OVERLAY_POS, here1, &pos);
here1:
    VS_IOCTL(fd, FBIOBLANK, done, FB_BLANK_POWERDOWN);

    //VS_MESSAGE("Set device win"WIN_FMT"\n", WIN_ARGS(&vd->disp));

    rect = &vd->disp;
    
    VS_IOCTL(fd, FBIOGET_VSCREENINFO, done,&fb_var);

    fb_var.xres = RECT_WIDTH(rect);
    fb_var.xres_virtual = fb_var.xres;
    fb_var.yres = RECT_HEIGHT(rect);
    fb_var.yres_virtual = fb_var.yres * 2;
    fb_var.activate |= FB_ACTIVATE_FORCE;
    fb_var.nonstd = vd->fmt;
    
    fb_var.bits_per_pixel = fmt2bit(vd->fmt);
    VS_IOCTL(fd, FBIOPUT_VSCREENINFO, done,&fb_var);

    VS_IOCTL(fd, FBIOGET_VSCREENINFO, done, &fb_var);
    VS_IOCTL(fd, FBIOGET_FSCREENINFO, done, &fb_fix);
    
    pos.x = vd->disp.left;
    pos.y = vd->disp.top;
    VS_IOCTL(fd, MXCFB_SET_OVERLAY_POS, here2, &pos);
here2:

    VS_IOCTL(fd, FBIOBLANK, done, FB_BLANK_UNBLANK);
    vd->bufaddr[0] = fb_fix.smem_start;
    vd->bufaddr[1] = fb_fix.smem_start + fb_var.yres * fb_fix.line_length;

    if (fmt2cs(vd->fmt)==0){
        _clearVideoDeviceBackground(vd);
    }


done:   
    return 0;
}


int
_closeDevice(VideoDevice * vd)
{
    int fd = _getDevicefd(vd);
    if (vd->mode_num){
        VS_IOCTL(fd, FBIOPUT_VSCREENINFO, done, &vd->fbvar);
    }
    VS_IOCTL(fd, FBIOBLANK, done, FB_BLANK_POWERDOWN);
    close(fd);
    gFBDescs[vd->fbidx].fb_fd = 0;
    
done:    
    return 0;
}

int
_openDevice(VideoDevice * vd)
{
    int pfb1_mode;
    char buf[100];
    if (vd->mode_num){
        sprintf(buf, "/sys/class/graphics/fb%d/mode", vd->fbidx);
        pfb1_mode = fopen(buf, "w");
        if (pfb1_mode < 0) {
            VS_ERROR("No /sys/class/graphics/fb1/mode device to open\n");
            goto error;
        }

        video_mode_to_str(buf, &vd->modes[vd->current_mode]);

        fwrite(buf, 1, strlen(buf), pfb1_mode);
        fflush(pfb1_mode);
        fclose(pfb1_mode);
    }

    return 0;
    
error:
    return -1;
}


int
_needRender(VideoSurface * curSurf, Updated * updated, int renderidx)
{
    VS_FLOW("Fun %s in\n", __FUNCTION__);

    if (curSurf->paddr==NULL)
        return 0;
    if (curSurf->status==VS_STATUS_INVISIBLE)
        return 0;
    if ((curSurf->rendmask & (1<<renderidx))==0)
        return 1;
    if ((updated->updated) && (OVERLAPED_RECT((&(curSurf->adjustdesrect)), (&updated->rect))))
        return 1;

    return 0;
}


void _clearRect(char * buf, int width, int height, int stride, int pixbytes)
{
    int y;
    for (y=0;y<height;y++){
        memset(buf, 0, width*pixbytes);
        buf+=stride*pixbytes;
    }
}


void _clearImageARGB(unsigned long * buf, int x0, int y0,int stride)
{
    int x,y;
    for (y=0;y<y0;y++){
    for (x=0;x<x0;x++){
            buf[x] = 0;
        }
        buf+=stride;
    }
}



#define rgb2y(r, g, b)\
    (unsigned char)(((int)30*(r) +(int)59*(g) +(int)11*(b))/100)
#define rgb2u(r, g, b)\
    (unsigned char)(((int)-17*(r) -(int)33*(g) +(int)50*(b)+12800)/100)
#define rgb2v(r, g, b)\
    (unsigned char)(((int)50*(r) -(int)42*(g) -(int)8*(b)+12800)/100)
#define XSRC(xx, x1, x0) ((xx)*(x0)/(x1))


void _copyImage(unsigned long * srcbuf, unsigned long * desbuf, int x1, int y1, int stride)
{
    int x,y;
    unsigned long tmp;
    for (y=0;y<y1;y++){
        for (x=0;x<x1;x++){
            if ((tmp=srcbuf[x])&0xff000000){
                desbuf[x] = tmp;

            }
        }
        desbuf+=stride;

        srcbuf+=x1;
    }
}

void _resizeImage(unsigned long * srcbuf, int x0, int y0, unsigned long * desbuf, int x1, int y1, int stride)
{
    int x,y;
    unsigned long tmp;
    
    for (y=0;y<y1;y++){
        for (x=0;x<x1;x++){
            if ((tmp=(srcbuf[(XSRC(x, x1, x0)+XSRC(y, y1, y0)*x0)]))&0xff000000){
                desbuf[x] = tmp;
            }else
            desbuf[x] = 0;
        }
        desbuf+=stride;
    }
}




void _copyImage2(unsigned long * srcbuf, unsigned short * desbuf, char * alphabuf, int x1, int y1, int stride)
{
    int x,y;
    unsigned long tmp;
    unsigned short yuv;
    for (y=0;y<y1;y++){
        for (x=0;x<x1;x++){
            if ((tmp=srcbuf[x])&0xff000000){
                int b = (tmp & 0xff0000)>>16;
                int g = (tmp & 0xff00)>>8;
                int r = (tmp & 0xff)>>8;
                if (((unsigned long)(desbuf+x))&0x2){
                    desbuf[x] =  (((unsigned short)rgb2y(r, g, b)))|(((unsigned short)rgb2u(r, g, b)<<8));
                }else{
                    desbuf[x] = (((unsigned short)rgb2y(r, g, b)))|(((unsigned short)rgb2v(r, g, b))<<8);
                }
                alphabuf[x] = tmp>>24;

            }
        }
        desbuf+=stride;
        alphabuf+=stride;
        srcbuf+=x1;
    }
}

void _resizeImage2(unsigned long * srcbuf, int x0, int y0, unsigned short * desbuf1, unsigned char * alphabuf, int x1, int y1, int stride)
{
    int x,y;
    unsigned long tmp;
     unsigned short * desbuf = desbuf1;
    for (y=0;y<y1;y++){
        for (x=0;x<x1;x++){
            if ((tmp=(srcbuf[(XSRC(x, x1, x0)+XSRC(y, y1, y0)*x0)]))&0xff000000){
                int b = (tmp & 0xff0000)>>16;
                int g = (tmp & 0xff00)>>8;
                int r = (tmp & 0xff)>>8;
                if (((unsigned long)(desbuf+x))&0x2){
                    desbuf[x] =  (((unsigned short)rgb2y(r, g, b)))|(((unsigned short)rgb2u(r, g, b)<<8));
                }else{
                    desbuf[x] = (((unsigned short)rgb2y(r, g, b)))|(((unsigned short)rgb2v(r, g, b))<<8);
                }
                alphabuf[x] = tmp>>24;
            }
        }
        desbuf+=stride;
        alphabuf+=stride;
    }
}



int 
_renderSuface(VideoSurface * surf, VideoDevice * vd, Updated * updated)
{
    VS_FLOW("Fun %s in\n", __FUNCTION__);

    Rect * surfrect = &(surf->adjustdesrect);
    
    if (updated->updated==0){
        updated->updated = 1;
        updated->rect = *surfrect;
    }else{
        Rect * updatedfileld = &updated->rect;
        if (surfrect->left<updatedfileld->left)
            updatedfileld->left = surfrect->left;
        if (surfrect->right>updatedfileld->right)
            updatedfileld->right = surfrect->right;
        if (surfrect->top<updatedfileld->top)
            updatedfileld->top = surfrect->top;
        if (surfrect->bottom>updatedfileld->bottom)
            updatedfileld->bottom = surfrect->bottom;
    }

    surf->itask.input.user_def_paddr[0] = surf->paddr;
    surf->itask.output.user_def_paddr[0] = vd->bufaddr[vd->renderidx];



    KICK_IPUTASKONE(&surf->itask);
    
    surf->rendmask|=(1<<vd->renderidx);
    return 0;
}

int 
_FlipOnDevice(VideoDevice * vd)
{
    VS_FLOW("Fun %s in\n", __FUNCTION__);

	struct fb_var_screeninfo fb_var;
    int ret;
    int fd = _getDevicefd(vd);

    VS_IOCTL(fd, FBIOGET_VSCREENINFO, done, &fb_var);

	if (vd->renderidx == 0)
		fb_var.yoffset = 0;
	else
		fb_var.yoffset = fb_var.yres;

    VS_FLOW("render  %d %d\n", vd->renderidx, fb_var.yoffset);

    VS_IOCTL(fd, FBIOPAN_DISPLAY, done, &fb_var);
    
    vd->renderidx= NEXT_RENDER_ID(vd->renderidx);
    
    VS_FLOW("render  %dfinish\n", vd->renderidx);
done:
    return 0;
}




