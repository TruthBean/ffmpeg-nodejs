#ifndef FFMPEG_NODEJS_H
#define FFMPEG_NODEJS_H

#include <memory.h>
#include <stdlib.h>

#include "./node_api.h"
#include "./common.c"
#include "./video2images.c"
#include "./record_video.c"

typedef struct AsyncWorkInfo
{
    napi_ref ref;
    napi_async_work work;
    napi_threadsafe_function func;
} AsyncWorkInfo; //async_work_info = {NULL, NULL};

typedef struct ReadImageBufferParams
{
    // init
    char *filename;
    bool nobuffer;
    int64_t timeout;
    bool use_gpu;
    bool use_tcp;
    char *gpu_id;

    // grab image
    int chose_frames;
    int type;
    int quality;

    // callback
    Video2ImageStream *vis;
    napi_ref ref;
    napi_async_work work;
} ReadImageBufferParams;

#endif // FFMPEG_NODEJS_H
