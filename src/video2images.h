#ifndef FFMPEG_NODEJS_VIDEO2IMAGES_H
#define FFMPEG_NODEJS_VIDEO2IMAGES_H

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libswscale/swscale.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include <libavutil/pixfmt.h>

#include "./common.h"

typedef struct Video2ImageStream {
    AVFormatContext *format_context;
    AVStream *video_stream;
    int video_stream_idx;
    AVCodecContext *video_codec_context;
    AVCodecParserContext *parser_context;
    int ret;
    char *error_message;
    int frame_rate;
} Video2ImageStream;

typedef struct FrameTimeOut {
    time_t count_time;
    time_t grab_time;
    int status;
} FrameTimeOut;

typedef void (Video2ImagesCallback)(FrameData *data);

Video2ImageStream open_inputfile(const char *filename, const bool nobuffer, const int timeout, const bool use_gpu, const bool use_tcp, const char *gpu_id);

void video2images_grab(Video2ImageStream vis, int quality, int chose_frames, enum ImageStreamType type, Video2ImagesCallback callback, FrameData *result);

void release(AVCodecContext *video_codec_context, AVFormatContext *format_context);

#endif // FFMPEG_NODEJS_VIDEO2IMAGES_H