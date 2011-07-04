#ifndef __MFW_ISINK_FRAME_H__
#define __MFW_ISINK_FRAME_H__


#define PLANE_NUM 3
#define ICB_VERSION 1

typedef enum {
    ICB_RESULT_INIT,
    ICB_RESULT_SUCCESSFUL,
    ICB_RESULT_FAILED,
}ISinkCallbackResult;

typedef struct  {
    ISinkCallbackResult result;
    int version;
    void * data;
}ISinkCallBack;

typedef struct {
    void * context;
    void * context1;
    int width;
    int height;
    int left;
    int right;
    int top;
    int bottom;
    unsigned int paddr[PLANE_NUM];
    unsigned int vaddr[PLANE_NUM];
}ISinkFrame;


typedef struct {
    int frame_num;
    unsigned int fmt;
    ISinkFrame * frames[0];
}ISinkFrameAllocInfo;


#endif

