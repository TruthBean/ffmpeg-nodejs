#ifndef FFMPEG_NODEJS_RECORD_VIDEO_H
#define FFMPEG_NODEJS_RECORD_VIDEO_H

#include <stdio.h>
#include <stdlib.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>

#include "./common.h"

int record_video(const char *video_url, const char *output_filename, const int record_seconds, const bool use_gpu);

#endif //FFMPEG_NODEJS_RECORD_VIDEO_H