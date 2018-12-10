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

#include <jpeglib.h>

#include "./common.h"

typedef struct Video2ImageStream {
    AVFormatContext *format_context;
    AVStream *video_stream;
    int video_stream_idx;
    AVCodecContext *video_codec_context;
    int ret;
    char *error_message;
    bool isRtsp;
    int frame_rate;
} Video2ImageStream;

enum ImageStreamType {
    YUV = 0,
    RGB,
    JPEG
};

typedef struct FrameData {
    unsigned long file_size;
    unsigned char *file_data;
    int ret;
    char *error_message;
} FrameData;

Video2ImageStream open_inputfile(const char *filename, const bool nobuffer, const bool use_gpu);

FrameData video2images_stream(Video2ImageStream vis, int quality, int chose_frames, enum ImageStreamType type);

OriginFrameData video_to_frame(Video2ImageStream vis, int chose_frames, napi_threadsafe_function func);

void release(AVCodecContext *video_codec_context,
             AVFormatContext *format_context, bool isRtsp);

#endif // FFMPEG_NODEJS_VIDEO2IMAGES_H