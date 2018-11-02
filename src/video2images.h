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
// #include <turbojpeg.h>

typedef struct Video2ImageStream {
    AVFormatContext *format_context;
    AVStream *video_stream;
    int video_stream_idx;
    AVCodecContext *video_codec_context;
    int ret;
    char *error_message;
    bool isRtsp;
} Video2ImageStream;

typedef struct FrameData {
    unsigned long file_size;
    unsigned char *file_data;
} FrameData;

Video2ImageStream open_inputfile(const char *filename);

FrameData video2images_stream(Video2ImageStream vis, int quality, int frame_persecond, int chose_frames);

void release(AVCodecContext *video_codec_context,
             AVFormatContext *format_context, bool isRtsp);

#endif