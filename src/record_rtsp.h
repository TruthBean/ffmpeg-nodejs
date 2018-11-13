#ifndef LEARNC_FFMPEG_RECORD_RTSP_H
#define LEARNC_FFMPEG_RECORD_RTSP_H

#include <stdio.h>
#include <stdlib.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>

int record_rtsp(const char *rtsp_url, const char *output_filename, int record_seconds);

#endif //LEARNC_FFMPEG_RECORD_RTSP_H

#ifdef _WIN32
#include <time.h>
#else
#include <sys/time.h>
#endif