#ifndef FFMPEG_NODEJS_VIDEO2IMAGES_H
#define FFMPEG_NODEJS_VIDEO2IMAGES_H

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include <libavutil/pixfmt.h>

#include <sys/time.h>

#include <jpeglib.h>

#include "./node_api.h"

typedef struct Video2ImageStream {
    AVFormatContext *format_context;
    AVPacket *packet;
    int video_stream_idx;
    AVCodecContext *video_codec_context;
    AVFrame *frame;
    int ret;
    bool isRtsp;
} Video2ImageStream;

Video2ImageStream open_inputfile(const char *filename);

int video2images_stream(Video2ImageStream vis, int quality, napi_env env, napi_value *result,
    void (callback_fileinfo(napi_env env, int file_size, char *file_info, napi_value *result))
    );

void release(AVPacket *packet, AVFrame *frame, AVCodecContext *video_codec_context, 
    AVFormatContext *format_context, bool isRtsp);

#endif