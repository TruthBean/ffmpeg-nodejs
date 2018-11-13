#include "./video2images.h"

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

static void jpeg_write_mem(uint8_t *raw_data, int quality, unsigned int width, unsigned int height,
                           unsigned char **jpeg_data, unsigned long *jpeg_size) {

#ifdef _WIN32
    av_log(NULL, AV_LOG_DEBUG, "begin jpeg_write_mem time: %lli\n", get_time());
#else
    av_log(NULL, AV_LOG_DEBUG, "begin jpeg_write_mem time: %li\n", get_time());
#endif

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
    while (jpeg_struct.next_scanline < height) {
        // 需要压缩成jpeg图片的位图数据缓冲区
        row_pointer[0] = &raw_data[jpeg_struct.next_scanline * row_stride];
        // 1表示写入一行
        (void)jpeg_write_scanlines(&jpeg_struct, row_pointer, 1);
    }

    // 结束压缩循环
    jpeg_finish_compress(&jpeg_struct);
    // 释放jpeg压缩对象
    jpeg_destroy_compress(&jpeg_struct);
#ifdef _WIN32
    av_log(NULL, AV_LOG_DEBUG, "compress one picture success: %lld.\n", get_time());
#else
    av_log(NULL, AV_LOG_DEBUG, "compress one picture success: %li.\n", get_time());
#endif
}

// static AVBufferRef *hw_device_ctx = NULL;
// static enum AVPixelFormat hw_pix_fmt;
// static bool isGpu = false;

// static int hw_decoder_init(AVCodecContext *ctx, const enum AVHWDeviceType type) {
//     int err = 0;

//     if ((err = av_hwdevice_ctx_create(&hw_device_ctx, type,
//                                       NULL, NULL, 0)) < 0) {
//         fprintf(stderr, "Failed to create specified HW device.\n");
//         return err;
//     }
//     ctx->hw_device_ctx = av_buffer_ref(hw_device_ctx);

//     return err;
// }

// static enum AVPixelFormat get_hw_format(AVCodecContext *ctx,
//                                         const enum AVPixelFormat *pix_fmts) {
//     const enum AVPixelFormat *p;

    

//     for (p = pix_fmts; *p != -1; p++) {
//         fprintf(stdout, "1. get_hw_format: %d\n", hw_pix_fmt);
//         fprintf(stdout, "2. p: %d\n", *p);

//         enum AVPixelFormat pixFormat;
//         switch (*p) {
//             case AV_PIX_FMT_YUVJ420P:
//                 pixFormat = AV_PIX_FMT_YUV420P;
//                 break;
//             case AV_PIX_FMT_YUVJ422P:
//                 pixFormat = AV_PIX_FMT_YUV422P;
//                 break;
//             case AV_PIX_FMT_YUVJ444P:
//                 pixFormat = AV_PIX_FMT_YUV444P;
//                 break;
//             case AV_PIX_FMT_YUVJ440P:
//                 pixFormat = AV_PIX_FMT_YUV440P;
//                 break;
//             default:
//                 pixFormat = *p;
//                 break;
//         }

//         if (pixFormat == hw_pix_fmt)
//             return *p;
//     }

//     fprintf(stderr, "Failed to get HW surface format.\n");
//     return AV_PIX_FMT_NONE;
// }

static FrameData copy_frame_data(const AVFrame *frame, const AVCodecContext *codec_context) {

#ifdef _WIN32
    av_log(NULL, AV_LOG_DEBUG, "begin copy_frame_data time: %lld\n", get_time());
#else
    av_log(NULL, AV_LOG_DEBUG, "begin copy_frame_data time: %li\n", get_time());
#endif

    FrameData result = {
        .file_size = 0,
        .file_data = NULL
        };

    av_log(NULL, AV_LOG_DEBUG, "saving frame %3d\n", codec_context->frame_number);

    // =====================================================================================================================
    // uint8_t *images_dst_data[4] = {NULL};
    // int images_dst_linesize[4];
    // int yuv_data_size = av_image_alloc(images_dst_data, images_dst_linesize, frame->width, frame->height, frame->format, 1);

    // /* copy decoded frame to destination buffer:
    //  * this is required since rawvideo expects non aligned data
    //  */
    // ptrdiff_t dst_linesizes[4], src_linesizes[4];
    // int i = 0;
    // for (i = 0; i < 4; i++) {
    //     dst_linesizes[i] = images_dst_linesize[i];
    //     src_linesizes[i] = frame->linesize[i];
    // }

    // av_image_copy_uc_from(images_dst_data, dst_linesizes,(const uint8_t **) (frame->data), src_linesizes,
    //                      frame->format, frame->width, frame->height);
    // =====================================================================================================================

    // yuv_data_size = av_image_get_buffer_size(frame->format, frame->width, frame->height, 1);
    // av_log(NULL, AV_LOG_INFO, "buffer size: %d\n", yuv_data_size);
    // result.file_data = (unsigned char *)malloc((size_t) (yuv_data_size));
    // result.file_size = (unsigned long)av_image_copy_to_buffer(result.file_data, yuv_data_size,
    //                                                           (const uint8_t **) images_dst_data, dst_linesizes,
    //                                                           frame->format, frame->width, frame->height, 1);

    // =====================================================================================================================
    // AVFrame *sw_frame = av_frame_alloc();
    // AVFrame *tmp_frame = NULL;

    // av_log(NULL, AV_LOG_DEBUG, "hw_pix_fmt : %d\n", hw_pix_fmt);
    // if (frame->format == hw_pix_fmt) {
    //     /* retrieve data from GPU to CPU */
    //     av_log(NULL, AV_LOG_INFO, "retrieve data from GPU to CPU\n");
    //     if (av_hwframe_transfer_data(sw_frame, frame, 0) < 0) {
    //         av_log(NULL, AV_LOG_ERROR, "Error transferring the data to system memory\n");
    //     }
    //     tmp_frame = sw_frame;
    // } else {
    //     av_log(NULL, AV_LOG_INFO, "origin frame\n");
    //     tmp_frame = frame;
    // }

    // int yuv_data_size = av_image_get_buffer_size(tmp_frame->format, tmp_frame->width, tmp_frame->height, 1);
    // av_log(NULL, AV_LOG_DEBUG, "buffer size: %d\n", yuv_data_size);
    // result.file_data = (unsigned char *)malloc((size_t) (yuv_data_size));
    // result.file_size = (unsigned long)av_image_copy_to_buffer(result.file_data, yuv_data_size,
    //                                                           (const uint8_t **) tmp_frame->data, tmp_frame->linesize,
    //                                                           tmp_frame->format, tmp_frame->width, tmp_frame->height, 1);

    // av_frame_unref(sw_frame);
    // av_frame_free(&sw_frame);

    // ========================================================================================================================

    return result;
}

static FrameData copy_frame_rgb_data(const AVFrame *frame, const AVCodecContext *codec_context) {
    #ifdef _WIN32
    av_log(NULL, AV_LOG_DEBUG, "begin copy_frame_rgb_data time: %lld\n", get_time());
#else
    av_log(NULL, AV_LOG_DEBUG, "begin copy_frame_rgb_data time: %li\n", get_time());
#endif

    FrameData result = {
        .file_size = 0,
        .file_data = NULL
        };

    av_log(NULL, AV_LOG_DEBUG, "saving frame %3d\n", codec_context->frame_number);

    AVFrame *target_frame = av_frame_alloc();
    if (!target_frame) {
        av_log(NULL, AV_LOG_ERROR, "Could not allocate images frame\n");
        return result;
    }
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
    switch (codec_context->pix_fmt) {
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
    sws_scale(sws_context, frame->data, frame->linesize, 0, frame->height,
              target_frame->data, target_frame->linesize);

    target_frame->height = target_height;
    target_frame->width = target_width;
    target_frame->format = target_pixel_format;

    int yuv_data_size = av_image_get_buffer_size(target_frame->format, target_frame->width, target_frame->height, 1);
    av_log(NULL, AV_LOG_DEBUG, "buffer size: %d\n", yuv_data_size);
    result.file_data = (unsigned char *)malloc((size_t) (yuv_data_size));
    result.file_size = (unsigned long)av_image_copy_to_buffer(result.file_data, yuv_data_size,
                                                              (const uint8_t **) target_frame->data, target_frame->linesize,
                                                              target_frame->format, target_frame->width, target_frame->height, 1);

    av_frame_unref(target_frame);
    av_frame_free(&target_frame);

    return result;
}

static FrameData copy_frame_data_and_transform_2_jpeg(const AVFrame *frame, int quality, 
                                                        const AVCodecContext *codec_context) {
#ifdef _WIN32
    av_log(NULL, AV_LOG_DEBUG, "begin copy_frame_data time: %lld\n", get_time());
#else
    av_log(NULL, AV_LOG_DEBUG, "begin copy_frame_data time: %li\n", get_time());
#endif
    uint8_t *images_dst_data[4] = {NULL};
    int images_dst_linesize[4];

    FrameData result = {
        .file_size = 0,
        .file_data = NULL
        };

    AVFrame *target_frame = av_frame_alloc();
    if (!target_frame) {
        av_log(NULL, AV_LOG_ERROR, "Could not allocate images frame\n");
        return result;
    }

    av_log(NULL, AV_LOG_DEBUG, "saving frame %3d\n", codec_context->frame_number);

    /* 图片是解码器分配的内存，不需要释放 */
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
    switch (codec_context->pix_fmt) {
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
    sws_scale(sws_context, frame->data, frame->linesize, 0, frame->height,
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

    unsigned char *jpeg_data;
    unsigned long jpeg_size = 0;
    jpeg_write_mem(images_dst_data[0], quality, (unsigned int)target_width, 
                    (unsigned int)target_height, &jpeg_data, &jpeg_size);
    result.file_data = (unsigned char *)malloc((size_t)(jpeg_size + 1));
    memcpy(result.file_data, jpeg_data, jpeg_size);
    free(jpeg_data);
    jpeg_data = NULL;

    result.file_size = jpeg_size;

    av_freep(&images_dst_data[0]);
    images_dst_data[0] = NULL;
    *images_dst_data = NULL;

    sws_freeContext(sws_context);
    sws_context = NULL;
    av_free(outBuff);
    outBuff = NULL;
    av_frame_unref(target_frame);
    av_frame_free(&target_frame);
    target_frame = NULL;

    return result;
}

Video2ImageStream open_inputfile(const char *filename, const bool nobuffer) {
#ifdef _WIN32
    av_log(NULL, AV_LOG_DEBUG, "start time: %lld\n", get_time());
#else
    av_log(NULL, AV_LOG_DEBUG, "start time: %li\n", get_time());
#endif
    int video_stream_idx = -1;

    AVStream *video_stream = NULL;

    AVFormatContext *format_context = NULL;

    AVCodecContext *video_codec_context = NULL;

    int ret;

    AVDictionary *dictionary = NULL;

    Video2ImageStream result = {
        .format_context = NULL,
        .video_stream_idx = -1,
        .video_codec_context = NULL,
        .ret = -1
        };

    // handle rtsp addr
    const char *PREFIX = "rtsp://";
    size_t lenpre = strlen(PREFIX),
           lenstr = strlen(filename);
    bool _bool = lenstr < lenpre ? false : strncmp(filename, PREFIX, lenpre) == 0;
    result.isRtsp = _bool;

    if (_bool) {
        av_log(NULL, AV_LOG_DEBUG, "input url rtsp addr %s \n", filename);
        int error = avformat_network_init();
        if (error != 0) {
            av_log(NULL, AV_LOG_ERROR, "network init error\n");
            release(video_codec_context, format_context, _bool);
            result.ret = -1;
            return result;
        }

        if (av_dict_set(&dictionary, "rtsp_transport", "tcp", 0) < 0) {
            av_log(NULL, AV_LOG_ERROR, "set rtsp_transport to tcp error\n");

            if (av_dict_set(&dictionary, "rtsp_transport", "udp", 0) < 0) {
                av_log(NULL, AV_LOG_ERROR, "set rtsp_transport to udp error\n");
                av_dict_set(&dictionary, "flush_packets", "1", 0);
            }
        }

        if (nobuffer) {
            av_dict_set(&dictionary, "fflags", "nobuffer", 0);
        } else {
            av_dict_set(&dictionary, "buffer_size", "4096", 0);
            av_dict_set(&dictionary, "flush_packets", "1", 0);
            av_dict_set(&dictionary, "max_delay", "0", 0);
            av_dict_set(&dictionary, "rtbufsize", "4096", 0);
        }

        if (av_dict_set(&dictionary, "hwaccel_device", "1", 0) < 0) {
            av_log(NULL, AV_LOG_ERROR, "no hwaccel device\n");
        }

        if (av_dict_set(&dictionary, "hwaccel", "cuda", 0) < 0) {
            av_log(NULL, AV_LOG_ERROR, "cuda acceleration error\n");
        }

        if (av_dict_set(&dictionary, "hwaccel", "cuvid", 0) < 0) {
            av_log(NULL, AV_LOG_ERROR, "cuvid acceleration error\n");
        }

        if (av_dict_set(&dictionary, "hwaccel", "opencl", 0) < 0) {
            av_log(NULL, AV_LOG_ERROR, "opencl acceleration error\n");
        }

        if (av_dict_set(&dictionary, "allowed_media_types", "video", 0) < 0) {
            av_log(NULL, AV_LOG_ERROR, "set allowed_media_types to video error\n");
        }
    }

    // enum AVHWDeviceType type = av_hwdevice_find_type_by_name("cuda");
    // fprintf(stdout, "AVHWDeviceType %d\n", type);
    // if (type == AV_HWDEVICE_TYPE_NONE) {
    //     av_log(NULL, AV_LOG_ERROR, "Device type cuda is not supported.\n");
    //     av_log(NULL, AV_LOG_DEBUG, "Available device types:");
    //     while((type = av_hwdevice_iterate_types(type)) != AV_HWDEVICE_TYPE_NONE)
    //         av_log(NULL, AV_LOG_DEBUG, " %s", av_hwdevice_get_type_name(type));
    //     av_log(NULL, AV_LOG_DEBUG, "\n");
    // }

    format_context = NULL;
    av_log(NULL, AV_LOG_DEBUG, "input file: %s\n", filename);
    // format_context必须初始化，否则报错
    if ((ret = avformat_open_input(&format_context, filename, NULL, &dictionary)) != 0) {
        av_log(NULL, AV_LOG_ERROR, "Couldn't open file %s: %d", filename, ret);
        release(video_codec_context, format_context, _bool);
        result.error_message = "avformat_open_input error";
        result.ret = -2;
        return result;
    }

    if (avformat_find_stream_info(format_context, &dictionary) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot find stream information\n");
        release(video_codec_context, format_context, _bool);
        result.error_message = "avformat_find_stream_info error";
        result.ret = -3;
        return result;
    }

    video_stream_idx = ret;
    video_stream = format_context->streams[video_stream_idx];

    AVCodec *codec = NULL;
    AVDictionary *opts = NULL;
    av_dict_set(&opts, "refcounted_frames", "false", 0);
    enum AVCodecID codec_id = video_stream->codecpar->codec_id;
    if (codec_id == AV_CODEC_ID_H264) {
        codec = avcodec_find_decoder_by_name("h264_cuvid");
    } else if (codec_id == AV_CODEC_ID_HEVC) {
        codec = avcodec_find_decoder_by_name("hevc_nvenc");
        av_dict_set(&opts, "flags", "low_delay", 0);
    }
    if (codec == NULL)
        codec = avcodec_find_decoder(video_stream->codecpar->codec_id);
    if (!codec) {
        av_log(NULL, AV_LOG_ERROR, "Failed to find %s codec\n", av_get_media_type_string(AVMEDIA_TYPE_VIDEO));
        release(video_codec_context, format_context, _bool);
        result.error_message = "avcodec_find_decoder error";
        result.ret = -5;
        return result;
    }

    ret = av_find_best_stream(format_context, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Could not find %s stream in input file '%s'\n",
               av_get_media_type_string(AVMEDIA_TYPE_VIDEO),
               filename);
        release(video_codec_context, format_context, _bool);
        result.error_message = "av_find_best_stream error";
        result.ret = -4;
        return result;
    }

    // 每秒多少帧
    int input_frame_rate = video_stream->r_frame_rate.num / video_stream->r_frame_rate.den;
    av_log(NULL, AV_LOG_INFO, "input video frame rate: %d\n", input_frame_rate);

    AVCodecParserContext *parserContext = av_parser_init(codec->id);
    if (!parserContext) {
        av_log(NULL, AV_LOG_ERROR, "parser not found\n");
        release(video_codec_context, format_context, _bool);
        result.error_message = "parser not found";
        result.ret = -6;
        return result;
    }

    // enum AVPixelFormat hw_pix_fmt = AV_PIX_FMT_NONE;
    // for (int i = 0;; i++) {
    //     const AVCodecHWConfig *config = avcodec_get_hw_config(codec, i);
    //     if (!config) {
    //         av_log(NULL, AV_LOG_ERROR, "Decoder %s does not support device type %s.\n",
    //                 codec->name, av_hwdevice_get_type_name(type));
    //         break;
    //     }
    //     if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX &&
    //         config->device_type == type) {
    //         hw_pix_fmt = config->pix_fmt;
    //         isGpu = true;
    //         break;
    //     }
    // }

    video_codec_context = avcodec_alloc_context3(codec);
    if (!video_codec_context) {
        av_log(NULL, AV_LOG_ERROR, "Failed to allocate the %s codec context\n",
               av_get_media_type_string(AVMEDIA_TYPE_VIDEO));
        release(video_codec_context, format_context, _bool);
        result.error_message = "avcodec_alloc_context3 error";
        result.ret = -7;
        return result;
    }

    if ((ret = avcodec_parameters_to_context(video_codec_context, video_stream->codecpar)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Failed to copy %s codec parameters to decoder context\n",
               av_get_media_type_string(AVMEDIA_TYPE_VIDEO));
        release(video_codec_context, format_context, _bool);
        result.error_message = "avcodec_parameters_to_context error";
        result.ret = -8;
        return result;
    }

    // if (hw_pix_fmt != AV_PIX_FMT_NONE) {
    //     video_codec_context->get_format  = get_hw_format;
    //     av_opt_set_int(video_codec_context, "refcounted_frames", 1, 0);

    //     if (hw_decoder_init(video_codec_context, type) < 0) {
    //         av_log(NULL, AV_LOG_ERROR, "decoder init error\n");
    //     }
    // }

    if (avcodec_open2(video_codec_context, codec, &opts) < 0) {
        av_log(NULL, AV_LOG_WARNING, "Failed to open %s codec\n", av_get_media_type_string(AVMEDIA_TYPE_VIDEO));

        // 在GPU docker 运行出错后使用CPU
        // =======================================================================================================
        codec = avcodec_find_decoder(video_stream->codecpar->codec_id);
        if (!codec) {
            av_log(NULL, AV_LOG_ERROR, "Failed to find %s codec\n", av_get_media_type_string(AVMEDIA_TYPE_VIDEO));
            release(video_codec_context, format_context, _bool);
            result.error_message = "avcodec_find_decoder error";
            result.ret = -5;
            return result;
        }

        parserContext = av_parser_init(codec->id);
        if (!parserContext) {
            av_log(NULL, AV_LOG_ERROR, "parser not found\n");
            release(video_codec_context, format_context, _bool);
            result.error_message = "parser not found";
            result.ret = -6;
            return result;
        }

        avcodec_free_context(&video_codec_context);
        video_codec_context = avcodec_alloc_context3(codec);
        if (!video_codec_context) {
            av_log(NULL, AV_LOG_ERROR, "Failed to allocate the %s codec context\n",
                   av_get_media_type_string(AVMEDIA_TYPE_VIDEO));
            release(video_codec_context, format_context, _bool);
            result.error_message = "avcodec_alloc_context3 error";
            result.ret = -7;
            return result;
        }

        if ((ret = avcodec_parameters_to_context(video_codec_context, video_stream->codecpar)) < 0) {
            av_log(NULL, AV_LOG_ERROR, "Failed to copy %s codec parameters to decoder context\n",
                   av_get_media_type_string(AVMEDIA_TYPE_VIDEO));
            release(video_codec_context, format_context, _bool);
            result.error_message = "avcodec_parameters_to_context error";
            result.ret = -8;
            return result;
        }

        if (avcodec_open2(video_codec_context, codec, &opts) < 0) {
            av_log(NULL, AV_LOG_ERROR, "Failed to open %s codec\n", av_get_media_type_string(AVMEDIA_TYPE_VIDEO));
            release(video_codec_context, format_context, _bool);
            result.error_message = "Failed to open codec";
            result.ret = -9;
            return result;
        }
        // =======================================================================================================
    }

    /* dump input information to stderr */
    av_dump_format(format_context, 0, filename, 0);

    if (!video_stream) {
        av_log(NULL, AV_LOG_ERROR, "Could not find audio or video stream in the input, aborting\n");
        release(video_codec_context, format_context, _bool);
        result.error_message = "Could not find audio or video stream in the input";
        result.ret = -10;
        return result;
    }

    result.format_context = format_context;
    result.video_stream_idx = video_stream_idx;
    result.video_stream = video_stream;
    result.video_codec_context = video_codec_context;
    result.ret = 0;
    result.frame_rate = input_frame_rate;

    return result;
}

int init_filters(const char *filters_descr, AVCodecContext *dec_ctx, AVRational time_base,
                    AVFilterContext *buffersink_ctx, AVFilterContext *buffersrc_ctx) {
    char args[512];
    int ret = 0;
    const AVFilter *buffersrc = avfilter_get_by_name("buffer");
    const AVFilter *buffersink = avfilter_get_by_name("buffersink");
    AVFilterInOut *outputs = avfilter_inout_alloc();
    AVFilterInOut *inputs = avfilter_inout_alloc();
    // AVRational time_base = fmt_ctx->streams[video_stream_index]->time_base;
    enum AVPixelFormat pix_fmts[] = {AV_PIX_FMT_YUV420P, AV_PIX_FMT_YUVJ420P, AV_PIX_FMT_NONE};

    AVFilterGraph *filter_graph = avfilter_graph_alloc();
    if (!outputs || !inputs || !filter_graph) {
        ret = AVERROR(ENOMEM);
        goto end;
    }

    /* buffer video source: the decoded frames from the decoder will be inserted here. */
    snprintf(args, sizeof(args),
             "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
             dec_ctx->width, dec_ctx->height, dec_ctx->pix_fmt,
             time_base.num, 25,
             dec_ctx->sample_aspect_ratio.num, dec_ctx->sample_aspect_ratio.den);

    fprintf(stdout, args);


    ret = avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in",
                                       args, NULL, filter_graph);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot create buffer source\n");
        goto end;
    }

    /* buffer video sink: to terminate the filter chain. */
    ret = avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out",
                                       NULL, NULL, filter_graph);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot create buffer sink\n");
        goto end;
    }

    ret = av_opt_set_int_list(buffersink_ctx, "pix_fmts", pix_fmts,
                              AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot set output pixel format\n");
        goto end;
    }

    /*
     * Set the endpoints for the filter graph. The filter_graph will
     * be linked to the graph described by filters_descr.
     */

    /*
     * The buffer source output must be connected to the input pad of
     * the first filter described by filters_descr; since the first
     * filter input label is not specified, it is set to "in" by
     * default.
     */
    outputs->name = av_strdup("in");
    outputs->filter_ctx = buffersrc_ctx;
    outputs->pad_idx = 0;
    outputs->next = NULL;

    /*
     * The buffer sink input must be connected to the output pad of
     * the last filter described by filters_descr; since the last
     * filter output label is not specified, it is set to "out" by
     * default.
     */
    inputs->name = av_strdup("out");
    inputs->filter_ctx = buffersink_ctx;
    inputs->pad_idx = 0;
    inputs->next = NULL;

    if ((ret = avfilter_graph_parse_ptr(filter_graph, filters_descr,
                                        &inputs, &outputs, NULL)) < 0)
        goto end;

    if ((ret = avfilter_graph_config(filter_graph, NULL)) < 0)
        goto end;

    end:
    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);

    return ret;
}

/**
 * 释放内存
 */
void release(AVCodecContext *video_codec_context, AVFormatContext *format_context, bool isRtsp) {
    av_log(NULL, AV_LOG_DEBUG, "---> 3 ---> free memory\n");

    if (video_codec_context != NULL)
        avcodec_close(video_codec_context);

    if (video_codec_context)
        avcodec_free_context(&video_codec_context);

    if (format_context) {
        avformat_close_input(&format_context);
        avformat_free_context(format_context);
    }

    if (isRtsp)
        avformat_network_deinit();
}

static void close(AVFrame *frame, AVPacket *packet) {
    if (frame != NULL) {
        av_frame_unref(frame);
        av_frame_free(&frame);
    }

    if (packet != NULL) {
        av_packet_unref(packet);
        av_packet_free(&packet);
    }
}

static int pts = 0;

FrameData video2images_stream(Video2ImageStream vis, int quality, int chose_frames, enum ImageStreamType type) {

    FrameData result = {
        .file_size = 0,
        .file_data = NULL
        };

    int ret;
    AVFrame *frame = av_frame_alloc();
    if (!frame) {
        av_log(NULL, AV_LOG_ERROR, "Could not allocate video frame\n");
        return result;
    }

    AVPacket *orig_pkt = av_packet_alloc();
    if (!orig_pkt) {
        av_log(NULL, AV_LOG_ERROR, "Couldn't alloc packet\n");
        close(frame, orig_pkt);
        return result;
    }

    /* read frames from the file */
#ifdef _WIN32
    av_log(NULL, AV_LOG_DEBUG, "begin av_read_frame time: %lld\n", get_time());
#else
    av_log(NULL, AV_LOG_DEBUG, "begin av_read_frame time: %li\n", get_time());
#endif
    // 大概300微秒
    while (av_read_frame(vis.format_context, orig_pkt) >= 0) {
#ifdef _WIN32
        av_log(NULL, AV_LOG_DEBUG, "end av_read_frame time: %lld\n", get_time());
#else
        av_log(NULL, AV_LOG_DEBUG, "end av_read_frame time: %li\n", get_time());
#endif
        if (orig_pkt->stream_index == vis.video_stream_idx) {
            if (orig_pkt->flags & AV_PKT_FLAG_KEY) {
                av_log(NULL, AV_LOG_DEBUG, "key frame\n");
                if (orig_pkt->pts < 0) {
                    pts = 0;
                }
            }
            if (vis.video_stream == NULL) {
                av_log(NULL, AV_LOG_ERROR, "error\n");
            }

            long pts_time = 0;
            if (orig_pkt->pts >= 0)
                pts_time = (long)(orig_pkt->pts * av_q2d(vis.video_stream->time_base) * vis.frame_rate);
            else
                pts_time = pts++;

            // 大概50微秒
            ret = avcodec_send_packet(vis.video_codec_context, orig_pkt);
#ifdef _WIN32
            av_log(NULL, AV_LOG_DEBUG, "end avcodec_send_packet time: %lld\n", get_time());
#else
            av_log(NULL, AV_LOG_DEBUG, "end avcodec_send_packet time: %li\n", get_time());
#endif
            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR, "Error while sending a packet to the decoder\n");
                close(frame, orig_pkt);
                return result;
            }

            int c = vis.frame_rate / chose_frames;
            long check = pts_time % c;

            // 大概30微秒
            ret = avcodec_receive_frame(vis.video_codec_context, frame);
#ifdef _WIN32
            av_log(NULL, AV_LOG_DEBUG, "end avcodec_receive_frame time: %lli\n", get_time());
#else
            av_log(NULL, AV_LOG_DEBUG, "end avcodec_receive_frame time: %li\n", get_time());
#endif
            //解码一帧数据
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                av_log(NULL, AV_LOG_DEBUG, "Decode finished\n");
                continue;
            }
            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR, "Decode error\n");
                close(frame, orig_pkt);
                return result;
            }

            av_log(NULL, AV_LOG_DEBUG, "pts_time: %ld chose_frames: %d frame_rate: %d\n", pts_time,
                    chose_frames, vis.frame_rate);
            if (check == c - 1) {
                if (type == JPEG)
                    result = copy_frame_data_and_transform_2_jpeg(frame, quality, vis.video_codec_context);
                if (type == RGB)
                    result = copy_frame_rgb_data(frame, vis.video_codec_context);
                else
                    result = copy_frame_data(frame, vis.video_codec_context);
                av_log(NULL, AV_LOG_DEBUG, "file_size: %ld\n", result.file_size);
                if (result.file_data == NULL) {
                    av_log(NULL, AV_LOG_DEBUG, "file_data NULL\n");
                    continue;
                } else {
                    // close(frame, orig_pkt);
                    av_packet_unref(orig_pkt);
                    av_packet_free(&orig_pkt);
                    return result;
                }
            } else {
                // continue;
            }
            av_packet_unref(orig_pkt);
        }
    }

    close(frame, orig_pkt);
    av_log(NULL, AV_LOG_DEBUG, "Demuxing succeeded.\n");
    return result;
}