#pragma once

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/time.h>

#include <libavutil/avstring.h>
#include <libavutil/time.h>
#include <libavutil/imgutils.h>
#include <libavutil/log.h>
#include <libavutil/frame.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>

#include <jpeglib.h>

enum ImageStreamType
{
    YUV = 0,
    RGB,
    JPEG
};

enum FrameStatus
{
    START = 0,
    END = 1,
    GRAB = 2,
    PENDIING = 3
};

typedef struct FrameData
{
    AVFrame *frame;
    long pts;
    enum ImageStreamType type;
    int quality;

    unsigned long file_size;
    unsigned char *file_data;
    int ret;
    char *error_message;

    bool isThreadly;

    bool abort;
} FrameData;

time_t get_now_microseconds();

void jpeg_write_mem(uint8_t *raw_data, int quality, unsigned int width, unsigned int height,
                    unsigned char **jpeg_data, unsigned long *jpeg_size);

/**
 * 拷贝avframe的数据，并转变成JPEG格式数据
 * @param frame: 图片帧
 * @param quality: jpeg 图片质量，1~100
 * @param codec_context: 视频AVCodecContext
 **/
void copy_frame_data_and_transform_2_jpeg(const AVCodecContext *codec_context, FrameData *result);

/**
 * 拷贝avframe的数据，并转变成YUV或RGB格式
 * @param frame: 图片帧
 * @param codec_context: 视频AVCodecContext
 * @param type: 转换的格式, YUV 或 RGB @see ImageStreamType
 * @return FrameData @see FrameData
 **/
void copy_frame_raw_data(const AVCodecContext *codec_context, FrameData *result);

void open_input_dictionary_set(AVDictionary **dictionary, const bool nobuffer, const int timeout, const bool use_gpu, const bool use_tcp);

void frame_data_deep_copy(FrameData *data, FrameData *dist_data);

void free_frame_data(FrameData *data);
