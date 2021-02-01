#include "./common.h"

time_t get_now_microseconds()
{
    time_t result;
    time_t now_seconds;

#ifdef _WIN32
    now_seconds = time(NULL);
    result = now_seconds;
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    now_seconds = tv.tv_sec;

    result = 1000000 * tv.tv_sec + tv.tv_usec;
#endif

    char buff[20];
#ifdef _WIN32
    struct tm now_time;
    localtime_s(&now_time, &now_seconds);
    strftime(buff, 20, "%Y-%m-%d %H:%M:%S", &now_time);
#else
    strftime(buff, 20, "%Y-%m-%d %H:%M:%S", localtime(&now_seconds));
#endif
    av_log(NULL, AV_LOG_DEBUG, "now: %s\n", buff);

    return result;
}

/**
 * 将rgb 数据 转换成 jpeg数据，存储在内存中
 * @param raw_data: [in] rgb数据
 * @param quality: [in] jpeg 质量，1~100
 * @param width: [in] 图片宽度
 * @param height: [in] 图片高度
 * @param jpeg_data: [out] 存储在内存中的JPEG数据
 * @param jpeg_size: [out] JPEG数据的大小
 **/
void jpeg_write_mem(uint8_t *raw_data, int quality, unsigned int width, unsigned int height,
                    unsigned char **jpeg_data, unsigned long *jpeg_size)
{
    av_log(NULL, AV_LOG_DEBUG, "begin jpeg_write_mem time: %li\n", get_now_microseconds());

    // 分配和一个jpeg压缩对象
    struct jpeg_compress_struct jpeg_struct;
    struct jpeg_error_mgr jpeg_error;

    jpeg_struct.err = jpeg_std_error(&jpeg_error);
    // 初始化
    jpeg_create_compress(&jpeg_struct);

    jpeg_struct.image_width = width;
    jpeg_struct.image_height = height;
    jpeg_struct.input_components = 3;
    jpeg_struct.in_color_space = JCS_RGB;

    // 设置压缩参数
    jpeg_set_defaults(&jpeg_struct);

    // 设置质量参数
    jpeg_set_quality(&jpeg_struct, quality, TRUE);

    // 存放在内存中
    jpeg_mem_dest(&jpeg_struct, jpeg_data, jpeg_size);

    // 开始压缩循环，逐行进行压缩：使用jpeg_start_compress开始一个压缩循环
    jpeg_start_compress(&jpeg_struct, TRUE);

    // 一行数据所占字节数:如果图片为RGB，这个值要*3.灰度图像不用
    int row_stride = width * 3;
    JSAMPROW row_pointer[1];
    while (jpeg_struct.next_scanline < height)
    {
        // 需要压缩成jpeg图片的位图数据缓冲区
        row_pointer[0] = &raw_data[jpeg_struct.next_scanline * row_stride];
        // 1表示写入一行
        (void)jpeg_write_scanlines(&jpeg_struct, row_pointer, 1);
    }

    // 结束压缩循环
    jpeg_finish_compress(&jpeg_struct);
    // 释放jpeg压缩对象
    jpeg_destroy_compress(&jpeg_struct);

    av_log(NULL, AV_LOG_DEBUG, "compress one picture success: %li.\n", get_now_microseconds());
}

/**
 * 拷贝avframe的数据，并转变成YUV或RGB格式
 * @param frame: 图片帧
 * @param codec_context: 视频AVCodecContext
 * @param type: 转换的格式, YUV 或 RGB @see ImageStreamType
 * @return FrameData @see FrameData
 **/
void copy_frame_raw_data(const AVCodecContext *codec_context, FrameData *result)
{

    av_log(NULL, AV_LOG_DEBUG, "begin copy_frame_rgb_data time: %li\n", get_now_microseconds());

    av_log(NULL, AV_LOG_DEBUG, "saving frame %3d\n", codec_context->frame_number);

    // 申请一个新的 frame, 用于转换格式
    AVFrame *target_frame = av_frame_alloc();
    if (!target_frame)
    {
        av_log(NULL, AV_LOG_ERROR, "Could not allocate target images frame\n");
        result->ret = -20;
        result->error_message = "Could not allocate target images frame";
        return;
    }
    target_frame->quality = 1;

    //保存jpeg格式
    enum AVPixelFormat target_pixel_format = AV_PIX_FMT_YUV420P;
    if (result->type == RGB)
        target_pixel_format = AV_PIX_FMT_RGB24;

    int align = 1; // input_pixel_format->linesize[0] % 32;

    // 获取一帧图片的体积
    int picture_size = av_image_get_buffer_size(target_pixel_format, codec_context->width,
                                                codec_context->height, align);

    // 申请一张图片数据的存储空间
    uint8_t *outBuff = (uint8_t *)av_malloc((size_t)picture_size);
    av_image_fill_arrays(target_frame->data, target_frame->linesize, outBuff, target_pixel_format,
                         codec_context->width, codec_context->height, align);

    // 设置图像转换上下文
    int target_width = codec_context->width;
    int target_height = codec_context->height;

    enum AVPixelFormat pixFormat;
    switch (codec_context->pix_fmt)
    {
    case AV_PIX_FMT_YUVJ420P:
        pixFormat = AV_PIX_FMT_YUV420P;
        break;
    case AV_PIX_FMT_YUVJ422P:
        pixFormat = AV_PIX_FMT_YUV422P;
        break;
    case AV_PIX_FMT_YUVJ444P:
        pixFormat = AV_PIX_FMT_YUV444P;
        break;
    case AV_PIX_FMT_YUVJ440P:
        pixFormat = AV_PIX_FMT_YUV440P;
        break;
    default:
        pixFormat = codec_context->pix_fmt;
        break;
    }

    // 转换图像格式
    struct SwsContext *sws_context = sws_getContext(codec_context->width, codec_context->height,
                                                    pixFormat,
                                                    target_width, target_height,
                                                    target_pixel_format,
                                                    SWS_BICUBIC, NULL, NULL, NULL);

    // 图片拉伸，数据转换
    sws_scale(sws_context, (const uint8_t *const *)result->frame->data, result->frame->linesize, 0, result->frame->height,
              target_frame->data, target_frame->linesize);

    target_frame->height = target_height;
    target_frame->width = target_width;
    target_frame->format = target_pixel_format;

    // avframe data to buffer
    int yuv_data_size = av_image_get_buffer_size(target_frame->format, target_frame->width, target_frame->height, 1);
    av_log(NULL, AV_LOG_DEBUG, "buffer size: %d\n", yuv_data_size);
    result->file_data = (unsigned char *)malloc((size_t)(yuv_data_size));
    result->file_size = (unsigned long)av_image_copy_to_buffer(result->file_data, yuv_data_size,
                                                               (const uint8_t **)target_frame->data, target_frame->linesize,
                                                               target_frame->format, target_frame->width, target_frame->height, 1);

    sws_freeContext(sws_context);
    sws_context = NULL;
    av_freep(&outBuff);
    av_free(outBuff);
    if (outBuff)
    {
        free(outBuff);
    }
    outBuff = NULL;
    av_frame_unref(target_frame);
    av_frame_free(&target_frame);
    if (target_frame)
    {
        free(target_frame);
    }
    target_frame = NULL;
    return;
}

/**
 * 拷贝avframe的数据，并转变成JPEG格式数据
 * @param frame: 图片帧
 * @param quality: jpeg 图片质量，1~100
 * @param codec_context: 视频AVCodecContext
 **/
void copy_frame_data_and_transform_2_jpeg(const AVCodecContext *codec_context, FrameData *result)
{
    av_log(NULL, AV_LOG_DEBUG, "begin copy_frame_data time: %li\n", get_now_microseconds());

    uint8_t *images_dst_data[4] = {NULL};
    int images_dst_linesize[4];

    result->type = JPEG;
    result->file_data = NULL;
    result->file_size = 0;

    AVFrame *target_frame = av_frame_alloc();
    if (!target_frame)
    {
        av_log(NULL, AV_LOG_ERROR, "Could not allocate target images frame\n");
        result->ret = -30;
        result->error_message = "Could not allocate target images frame";
        return;
    }

    av_log(NULL, AV_LOG_DEBUG, "saving frame %3d\n", codec_context->frame_number);

    target_frame->quality = 1;

    //保存jpeg格式
    enum AVPixelFormat target_pixel_format = AV_PIX_FMT_RGB24;

    int align = 1; // input_pixel_format->linesize[0] % 32;

    // 获取一帧图片的体积
    int picture_size = av_image_get_buffer_size(target_pixel_format, codec_context->width,
                                                codec_context->height, align);

    // 申请一张图片数据的存储空间
    uint8_t *outBuff = (uint8_t *)av_malloc((size_t)picture_size);
    av_image_fill_arrays(target_frame->data, target_frame->linesize, outBuff, target_pixel_format,
                         codec_context->width, codec_context->height, align);

    // 设置图像转换上下文
    int target_width = codec_context->width;
    int target_height = codec_context->height;

    enum AVPixelFormat pixFormat;
    switch (codec_context->pix_fmt)
    {
    case AV_PIX_FMT_YUVJ420P:
        pixFormat = AV_PIX_FMT_YUV420P;
        break;
    case AV_PIX_FMT_YUVJ422P:
        pixFormat = AV_PIX_FMT_YUV422P;
        break;
    case AV_PIX_FMT_YUVJ444P:
        pixFormat = AV_PIX_FMT_YUV444P;
        break;
    case AV_PIX_FMT_YUVJ440P:
        pixFormat = AV_PIX_FMT_YUV440P;
        break;
    default:
        pixFormat = codec_context->pix_fmt;
        break;
    }

    struct SwsContext *sws_context = sws_getContext(codec_context->width, codec_context->height,
                                                    pixFormat,
                                                    target_width, target_height,
                                                    target_pixel_format,
                                                    SWS_BICUBIC, NULL, NULL, NULL);

    // 转换图像格式
    sws_scale(sws_context, (const uint8_t *const *)result->frame->data, result->frame->linesize, 0, result->frame->height,
              target_frame->data, target_frame->linesize);

    target_frame->height = target_height;
    target_frame->width = target_width;
    target_frame->format = target_pixel_format;

    av_image_alloc(images_dst_data, images_dst_linesize,
                   target_frame->width, target_frame->height, target_frame->format, 16);

    /* copy decoded frame to destination buffer: 
     * this is required since rawvideo expects non aligned data
     */
    av_image_copy(images_dst_data, images_dst_linesize,
                  (const uint8_t **)(target_frame->data), target_frame->linesize,
                  target_frame->format, target_frame->width, target_frame->height);

    // 图片压缩成Jpeg格式
    unsigned char *jpeg_data;
    unsigned long jpeg_size = 0;
    jpeg_write_mem(images_dst_data[0], result->quality, (unsigned int)target_width,
                   (unsigned int)target_height, &jpeg_data, &jpeg_size);
    result->file_data = (unsigned char *)malloc((size_t)(jpeg_size + 1));
    memcpy(result->file_data, jpeg_data, jpeg_size);
    free(jpeg_data);
    jpeg_data = NULL;

    result->file_size = jpeg_size;

    // 释放内存
    av_freep(&images_dst_data[0]);
    av_free(images_dst_data[0]);
    if (images_dst_data)
    {
        free(*images_dst_data);
    }
    images_dst_data[0] = NULL;
    *images_dst_data = NULL;

    sws_freeContext(sws_context);
    sws_context = NULL;
    av_freep(&outBuff);
    av_free(outBuff);
    outBuff = NULL;
    av_frame_unref(target_frame);
    av_frame_free(&target_frame);
    target_frame = NULL;

    return;
}

// av_dict_set 参数在 libavcodec/options_table.h libavformat/options_table.h
void open_input_dictionary_set(AVDictionary **dictionary, const bool nobuffer, const int64_t timeout, const bool use_gpu, const bool use_tcp)
{
    const int64_t stimeout = timeout * 1000;
    // av_log(NULL, AV_LOG_DEBUG, "open_input_dictionary_set --> timeout : %ld \n", stimeout);
    // set maximum timeout (in seconds) to wait for incoming connections (-1 is infinite, imply flag listen)
    // av_dict_set_int(dictionary, "timeout", stimeout, 0);

    // set timeout (in microseconds) of socket TCP I/O operations
    // // 单位 微秒
    // char str[12];
    // sprintf(str, "%d", timeout * 1000000);
    av_log(NULL, AV_LOG_DEBUG, "open_input_dictionary_set --> stimeout : %ld \n", stimeout);
    // 当av_read_frame由于视频流断开而阻塞的时候，超时 timeout 秒后会断开连接;
    // av_dict_set(dictionary, "stimeout", str, 0);
    av_dict_set_int(dictionary, "stimeout", stimeout, 0);


    // av_dict_set_int(dictionary, "start-on-prepared", (int64_t)1, 0);

    // av_dict_set_int(dictionary, "max_cached_duration", (int64_t)10, 0);

    // specify how many microseconds are analyzed to probe the input
    av_log(NULL, AV_LOG_DEBUG, "open_input_dictionary_set --> analyzeduration : 155808000 \n");
    // 1920 * 1080 * 25 * 3
    // 1920x1080 25fps YUV420P
    av_dict_set_int(dictionary, "analyzeduration", (int64_t)155808000, 0);

    av_log(NULL, AV_LOG_DEBUG, "open_input_dictionary_set --> probsize : 2073600 \n");
    av_dict_set_int(dictionary, "probsize", (int64_t)2073600, 0);

    // fps default value is 25
    // av_dict_set_int(dictionary, "fps", (int64_t)25, 0);

    // fflags的值(默认值是autobsf)：
    //  flush_packets: reduce the latency by flushing out packets immediately
    //  ignidx: ignore index
    //  genpts: generate pts
    //  nofillin: do not fill in missing values that can be exactly calculated
    //  noparse: disable AVParsers, this needs nofillin too
    //  igndts: ignore dts
    //  discardcorrupt: discard corrupted frames
    //  sortdts: try to interleave outputted packets by dts
    //  fastseek: fast but inaccurate seeks
    //  nobuffer: reduce the latency introduced by optional buffering
    //  bitexact: do not write random/volatile data
    //  shortest: stop muxing with the shortest stream
    //  autobsf: add needed bsfs automatically
    //
    if (nobuffer)
    {
        av_log(NULL, AV_LOG_DEBUG, "open_input_dictionary_set --> fflags : nobuffer \n");
        av_dict_set(dictionary, "fflags", "nobuffer", 0);
    }
    else
    {
        av_dict_set(dictionary, "fflags", "autobsf", 0);
        // 设置缓存大小
        av_log(NULL, AV_LOG_DEBUG, "open_input_dictionary_set --> buffer_size : 4096 \n");
        av_dict_set(dictionary, "buffer_size", "40960", 0);

        // enable flushing of the I/O context after each packet，value is -1 ~ 1
        av_log(NULL, AV_LOG_DEBUG, "open_input_dictionary_set --> flush_packets : 1 \n");
        av_dict_set(dictionary, "flush_packets", "1", 0);
        // maximum muxing or demuxing delay in microseconds
        av_log(NULL, AV_LOG_DEBUG, "open_input_dictionary_set --> max_delay : 0 \n");
        // av_dict_set(dictionary, "max_delay", "0", 0);
        av_dict_set_int(dictionary, "max_delay", (int64_t)0, 0);
        // max memory used for buffering real-time frames
        av_log(NULL, AV_LOG_DEBUG, "open_input_dictionary_set --> rtbufsize : 4096 \n");
        av_dict_set_int(dictionary, "rtbufsize", (int64_t)2073600, 0);
    }

    if (use_tcp)
    {
        av_log(NULL, AV_LOG_DEBUG, "open_input_dictionary_set --> rtsp_transport : tcp \n");
        if (av_dict_set(dictionary, "rtsp_transport", "tcp", 0) < 0)
        {
            av_log(NULL, AV_LOG_ERROR, "set rtsp_transport to tcp error\n");

            // fall to udp
            if (av_dict_set(dictionary, "rtsp_transport", "udp", 0) < 0)
            {
                av_log(NULL, AV_LOG_ERROR, "set rtsp_transport to udp error\n");
            }
            else
            {
                av_dict_set(dictionary, "flush_packets", "1", 0);
            }
        }
    }
    else
    {
        av_log(NULL, AV_LOG_DEBUG, "open_input_dictionary_set --> rtsp_transport : udp \n");
        if (av_dict_set(dictionary, "rtsp_transport", "udp", 0) < 0)
        {
            av_log(NULL, AV_LOG_ERROR, "set rtsp_transport to udp error\n");
        }
        else
        {
            av_dict_set(dictionary, "flush_packets", "1", 0);
        }
    }

    if (use_gpu)
    {
        av_log(NULL, AV_LOG_DEBUG, "open_input_dictionary_set --> hwaccel_device : 0 \n");
        if (av_dict_set(dictionary, "hwaccel_device", "0", 0) < 0)
        {
            av_log(NULL, AV_LOG_ERROR, "no hwaccel device\n");
        }

        // 使用cuda
        /* av_log(NULL, AV_LOG_DEBUG, "open_input_dictionary_set --> hwaccel : cuda \n");
        if (av_dict_set(dictionary, "hwaccel", "cuda", 1) < 0)
        {
            av_log(NULL, AV_LOG_ERROR, "cuda acceleration error\n");
        } */

        // 使用 cuvid
        av_log(NULL, AV_LOG_DEBUG, "open_input_dictionary_set --> hwaccel : cuvid \n");
        if (av_dict_set(dictionary, "hwaccel", "cuvid", 1) < 0)
        {
            av_log(NULL, AV_LOG_ERROR, "cuvid acceleration error\n");
        }

        // 使用 opencl
        /*av_log(NULL, AV_LOG_DEBUG, "open_input_dictionary_set --> hwaccel : opencl \n");
        if (av_dict_set(dictionary, "hwaccel", "opencl", 1) < 0)
        {
            av_log(NULL, AV_LOG_ERROR, "opencl acceleration error\n");
        }*/
    }
}

void frame_data_deep_copy(FrameData *data, FrameData *dist_data)
{
    dist_data->pts = data->pts;
    dist_data->type = data->type;
    dist_data->quality = data->quality;

    dist_data->file_size = data->file_size;
    dist_data->ret = data->ret;
    dist_data->isThreadly = data->isThreadly;
    dist_data->abort = data->abort;

    if (data->frame != NULL && data->frame)
    {
        av_log(NULL, AV_LOG_DEBUG, "frame_data_deep_copy --> clone data->frame \n");
        dist_data->frame = av_frame_clone(data->frame);
        // av_log(NULL, AV_LOG_DEBUG, "frame_data_deep_copy --> copy data->frame \n");
        // av_frame_copy(dist_data->frame, data->frame);
        // av_log(NULL, AV_LOG_DEBUG, "frame_data_deep_copy --> copy data->frame finish \n");
        // break;
    }
    if (data->file_size > 0 && data->file_data != NULL && data->file_data)
    {
        av_log(NULL, AV_LOG_DEBUG, "frame_data_deep_copy --> data->file_data is not null \n");
        dist_data->file_data = malloc(data->file_size);
        memcpy(dist_data->file_data, data->file_data, data->file_size);
    }
    if (data->error_message != NULL)
    {
        dist_data->error_message = data->error_message;
    }
}

void free_frame_data(FrameData *data)
{
    if (data == NULL || !data)
    {
        return;
    }
    if (data->frame != NULL && data->frame)
    {
        av_frame_unref(data->frame);
        av_frame_free(&data->frame);
        if (data->frame)
        {
            free(data->frame);
        }
        data->frame = NULL;
    }
    if (data->file_data != NULL && data->file_data)
    {
        free(data->file_data);
        data->file_data = NULL;
    }
    data->pts = 0;
    data->type = -1;
    data->quality = 0;
    data->file_size = 0;
    data->ret = -1;
    data->isThreadly = false;
    data->abort = false;
}