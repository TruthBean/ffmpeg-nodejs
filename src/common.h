#pragma once

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/time.h>

#include <libavutil/log.h>
#include <libavutil/frame.h>

typedef struct OriginFrameData {
    AVFrame *frame;
    long pts;
    int ret;
    char *error_message;
} OriginFrameData;

time_t get_time();