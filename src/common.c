#include "./common.h"

time_t get_time() {
    time_t result;
    time_t now_seconds;

    struct timeval tv;
    gettimeofday(&tv, NULL);
    now_seconds = tv.tv_sec;

    result = 1000000 * tv.tv_sec + tv.tv_usec;

    char buff[20];
    strftime(buff, 20, "%Y-%m-%d %H:%M:%S", localtime(&now_seconds));
    av_log(NULL, AV_LOG_DEBUG, "now: %s\n", buff);

    return result;
}