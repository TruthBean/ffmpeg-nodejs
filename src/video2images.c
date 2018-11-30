#include "./video2images.h"

/**
 * 将rgb 数据 转换成 jpeg数据，存储在内存中
 * @param raw_data: [in] rgb数据
 * @param quality: [in] jpeg 质量，1~100
 * @param width: [in] 图片宽度
 * @param height: [in] 图片高度
 * @param jpeg_data: [out] 存储在内存中的JPEG数据
 * @param jpeg_size: [out] JPEG数据的大小
 **/
static void jpeg_write_mem(uint8_t *raw_data, int quality, unsigned int width, unsigned int height,
                           unsigned char **jpeg_data, unsigned long *jpeg_size) {
    av_log(NULL, AV_LOG_DEBUG, "begin jpeg_write_mem time: %li\n", get_time());

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

    av_log(NULL, AV_LOG_DEBUG, "compress one picture success: %li.\n", get_time());

}

/**
 * 拷贝avframe的数据，并转变成YUV或RGB格式
 * @param frame: 图片帧
 * @param codec_context: 视频AVCodecContext
 * @param type: 转换的格式, YUV 或 RGB @see ImageStreamType
 * @return FrameData @see FrameData
 **/
static FrameData copy_frame_raw_data(const AVFrame *frame, const AVCodecContext *codec_context, enum ImageStreamType type) {

    av_log(NULL, AV_LOG_DEBUG, "begin copy_frame_rgb_data time: %li\n", get_time());

    FrameData result = {
        .file_size = 0,
        .file_data = NULL,
        .ret = 0
        };

    av_log(NULL, AV_LOG_DEBUG, "saving frame %3d\n", codec_context->frame_number);

    // 申请一个新的 frame, 用于转换格式
    AVFrame *target_frame = av_frame_alloc();
    if (!target_frame) {
        av_log(NULL, AV_LOG_ERROR, "Could not allocate target images frame\n");
        result.ret = -20;
        result.error_message = "Could not allocate target images frame";
        return result;
    }
    target_frame->quality = 1;

    //保存jpeg格式
    enum AVPixelFormat target_pixel_format = AV_PIX_FMT_YUV420P;
    if (type == RGB) target_pixel_format = AV_PIX_FMT_RGB24;
    

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

    // 转换图像格式
    struct SwsContext *sws_context = sws_getContext(codec_context->width, codec_context->height,
                                                    pixFormat,
                                                    target_width, target_height,
                                                    target_pixel_format,
                                                    SWS_BICUBIC, NULL, NULL, NULL);

    // 图片拉伸，数据转换
    sws_scale(sws_context, frame->data, frame->linesize, 0, frame->height,
              target_frame->data, target_frame->linesize);

    target_frame->height = target_height;
    target_frame->width = target_width;
    target_frame->format = target_pixel_format;

    // avframe data to buffer
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

/**
 * 拷贝avframe的数据，并转变成JPEG格式数据
 * @param frame: 图片帧
 * @param quality: jpeg 图片质量，1~100
 * @param codec_context: 视频AVCodecContext
 **/
static FrameData copy_frame_data_and_transform_2_jpeg(const AVFrame *frame, int quality, 
                                                        const AVCodecContext *codec_context) {
    av_log(NULL, AV_LOG_DEBUG, "begin copy_frame_data time: %li\n", get_time());

    uint8_t *images_dst_data[4] = {NULL};
    int images_dst_linesize[4];

    FrameData result = {
        .file_size = 0,
        .file_data = NULL,
        .ret = 0
        };

    AVFrame *target_frame = av_frame_alloc();
    if (!target_frame) {
        av_log(NULL, AV_LOG_ERROR, "Could not allocate target images frame\n");
        result.ret = -10;
        result.error_message = "Could not allocate target images frame";
        return result;
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

    // 图片压缩成Jpeg格式
    unsigned char *jpeg_data;
    unsigned long jpeg_size = 0;
    jpeg_write_mem(images_dst_data[0], quality, (unsigned int)target_width, 
                    (unsigned int)target_height, &jpeg_data, &jpeg_size);
    result.file_data = (unsigned char *)malloc((size_t)(jpeg_size + 1));
    memcpy(result.file_data, jpeg_data, jpeg_size);
    free(jpeg_data);
    jpeg_data = NULL;

    result.file_size = jpeg_size;

    // 释放内存
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

static int time_out = 0;
static bool _init = true;
//核心是超时返回1，正常等待返回0
static int interrupt_cb(void *ctx) {
    time_out += 1;
    av_log(NULL, AV_LOG_DEBUG, "interrupt_cb ......................... timeout %d \n", time_out);
    if (!_init || time_out > 21000) {
        time_out=0;
        //这个就是超时的返回
        return 1;
    }
    // _init = false;
    return 0;
}

/**
 * 连接视频地址，获取数据流
 * @param filename: 视频地址
 * @param nobuffer: rtsp 是否设置缓存
 * @param use_gpu: 是否使用gpu加速
 * 
 * @return @see Video2ImageStream
 **/
Video2ImageStream open_inputfile(const char *filename, const bool nobuffer, const bool use_gpu) {

    av_log(NULL, AV_LOG_DEBUG, "start time: %li\n", get_time());

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
            } else {
                av_dict_set(&dictionary, "flush_packets", "1", 0);
            }
        }

        if (nobuffer) {
            av_dict_set(&dictionary, "fflags", "nobuffer", 0);
        } else {
            // 设置缓存大小
            av_dict_set(&dictionary, "buffer_size", "4096", 0);
            av_dict_set(&dictionary, "flush_packets", "1", 0);
            av_dict_set(&dictionary, "max_delay", "0", 0);
            av_dict_set(&dictionary, "rtbufsize", "4096", 0);
        }

        if (use_gpu) {
            if (av_dict_set(&dictionary, "hwaccel_device", "1", 0) < 0) {
                av_log(NULL, AV_LOG_ERROR, "no hwaccel device\n");
            }

            // 使用cuda
            if (av_dict_set(&dictionary, "hwaccel", "cuda", 0) < 0) {
                av_log(NULL, AV_LOG_ERROR, "cuda acceleration error\n");
            }

            // 使用 cuvid
            if (av_dict_set(&dictionary, "hwaccel", "cuvid", 0) < 0) {
                av_log(NULL, AV_LOG_ERROR, "cuvid acceleration error\n");
            }

            // 使用 opencl
            if (av_dict_set(&dictionary, "hwaccel", "opencl", 0) < 0) {
                av_log(NULL, AV_LOG_ERROR, "opencl acceleration error\n");
            }
        }

        if (av_dict_set(&dictionary, "allowed_media_types", "video", 0) < 0) {
            av_log(NULL, AV_LOG_ERROR, "set allowed_media_types to video error\n");
        }
    }

    format_context = avformat_alloc_context();
    format_context->interrupt_callback.callback = interrupt_cb;
    format_context->interrupt_callback.opaque = format_context;
    av_log(NULL, AV_LOG_DEBUG, "input file: %s\n", filename);
    // format_context必须初始化，否则报错
    if ((ret = avformat_open_input(&format_context, filename, NULL, &dictionary)) != 0) {
        av_log(NULL, AV_LOG_ERROR, "Couldn't open file %s: %d\n", filename, ret);
        release(video_codec_context, format_context, _bool);
        result.error_message = "avformat_open_input error, Couldn't open this file";
        result.ret = -2;
        return result;
    }

    if (avformat_find_stream_info(format_context, NULL) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot find stream information\n");
        release(video_codec_context, format_context, _bool);
        result.error_message = "avformat_find_stream_info error";
        result.ret = -3;
        return result;
    }

    video_stream_idx = av_find_best_stream(format_context, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (video_stream_idx < 0) {
        av_log(NULL, AV_LOG_ERROR, "Could not find %s stream in input file '%s'\n",
               av_get_media_type_string(AVMEDIA_TYPE_VIDEO),
               filename);
        release(video_codec_context, format_context, _bool);
        result.error_message = "av_find_best_stream error";
        result.ret = -4;
        return result;
    }

    video_stream = format_context->streams[video_stream_idx];

    AVCodec *codec = NULL;
    AVDictionary *opts = NULL;
    av_dict_set(&opts, "refcounted_frames", "false", 0);
    av_dict_set(&opts, "timeout", "1", 0);
    av_dict_set(&opts, "listen_timeout", "1", 0);
    enum AVCodecID codec_id = video_stream->codecpar->codec_id;
    // 判断摄像头视频格式是h264还是h265
    if (use_gpu && codec_id == AV_CODEC_ID_H264) {
        codec = avcodec_find_decoder_by_name("h264_cuvid");
    } else if (codec_id == AV_CODEC_ID_HEVC) {
        if (use_gpu)
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

/**
 * 视频流获取 图片帧
 * @param vis: 存储在全局变量中的视频流数据变量
 * @param quality: jpeg 图片质量，1~100
 * @param chose_frames: 一秒取多少帧, 超过视频帧率则为视频帧率
 * @param type: 图片类型 @see ImageStreamType
 * @return @see FrameData
 **/
FrameData video2images_stream(Video2ImageStream vis, int quality, int chose_frames, enum ImageStreamType type) {

    FrameData result = {
        .file_size = 0,
        .file_data = NULL,
        .ret = 0
        };

    int ret;
    AVFrame *frame = av_frame_alloc();
    if (!frame) {
        av_log(NULL, AV_LOG_ERROR, "Could not allocate image frame\n");
        result.ret = -1;
        result.error_message = "Could not allocate image frame";
        return result;
    }

    AVPacket *orig_pkt = av_packet_alloc();
    if (!orig_pkt) {
        av_log(NULL, AV_LOG_ERROR, "Couldn't alloc packet\n");
        result.ret = -2;
        result.error_message = "Couldn't alloc packet";
        close(frame, orig_pkt);
        return result;
    }

    /* read frames from the file */
    av_log(NULL, AV_LOG_DEBUG, "begin av_read_frame time: %li\n", get_time());

    int _ret = 0;
    while (true) {
        av_log(NULL, AV_LOG_INFO, "-------------2432434232432 %d \n", _ret);
        _ret = av_read_frame(vis.format_context, orig_pkt);
        av_log(NULL, AV_LOG_INFO, "--------------------------------- %d \n", _ret);
        if (_ret < 0) break;
        av_log(NULL, AV_LOG_DEBUG, "end av_read_frame time: %li\n", get_time());
        if (orig_pkt->stream_index == vis.video_stream_idx) {
            if (orig_pkt->flags & AV_PKT_FLAG_KEY) {
                av_log(NULL, AV_LOG_DEBUG, "key frame\n");
                if (orig_pkt->pts < 0) {
                    pts = 0;
                }
            }
            if (vis.video_stream == NULL) {
                av_log(NULL, AV_LOG_ERROR, "error: video stream is null\n");
                result.ret = -3;
                result.error_message = "video stream is null";
                close(frame, orig_pkt);
                return result;
            }

            long pts_time = 0;
            // 获取帧数
            if (orig_pkt->pts >= 0)
                pts_time = (long)(orig_pkt->pts * av_q2d(vis.video_stream->time_base) * vis.frame_rate);
            else
                pts_time = pts++;

            ret = avcodec_send_packet(vis.video_codec_context, orig_pkt);
            av_log(NULL, AV_LOG_DEBUG, "end avcodec_send_packet time: %li\n", get_time());
            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR, "Error while sending a packet to the decoder\n");
                close(frame, orig_pkt);
                result.ret = -4;
                result.error_message = "Error while sending a packet to the decoder";
                return result;
            }

            chose_frames = chose_frames > vis.frame_rate ? vis.frame_rate : chose_frames;
            int c = vis.frame_rate / chose_frames;
            av_log(NULL, AV_LOG_DEBUG, "frame_rate %d chose_frames %d c %d\n", vis.frame_rate, chose_frames ,c);
            long check = pts_time % c;
            av_log(NULL, AV_LOG_DEBUG, "check %ld\n", check);


            ret = avcodec_receive_frame(vis.video_codec_context, frame);
            av_log(NULL, AV_LOG_DEBUG, "end avcodec_receive_frame time: %li\n", get_time());
            // 解码一帧数据
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                av_log(NULL, AV_LOG_DEBUG, "Decode finished\n");
                continue;
            }
            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR, "Decode error\n");
                close(frame, orig_pkt);
                result.ret = -4;
                result.error_message = "Error while receive frame from a packet";
                return result;
            }

            av_log(NULL, AV_LOG_DEBUG, "pts_time: %ld chose_frames: %d frame_rate: %d\n", pts_time,
                    chose_frames, vis.frame_rate);
            // 判断帧数，是否取
            if (check == c - 1) {
                if (type == JPEG)
                    result = copy_frame_data_and_transform_2_jpeg(frame, quality, vis.video_codec_context);
                else
                    result = copy_frame_raw_data(frame, vis.video_codec_context, type);
                av_log(NULL, AV_LOG_DEBUG, "file_size: %ld\n", result.file_size);
                if (result.file_size == 0 || result.file_data == NULL) {
                    av_log(NULL, AV_LOG_DEBUG, "file_data NULL\n");
                    close(frame, orig_pkt);
                    continue;
                } else {
                    if (result.ret < 0 ) {
                        result.ret = -5;
                        result.error_message = "image data is null";
                    }
                    av_packet_unref(orig_pkt);
                    av_packet_free(&orig_pkt);
                    return result;
                }
            }
            av_packet_unref(orig_pkt);
        }
    }

    close(frame, orig_pkt);

    return result;
}

LinkedQueueNodeData video_to_frame(Video2ImageStream vis, int chose_frames, LinkedQueue *queue, sem_t *semaphore) {
    av_log(NULL, AV_LOG_INFO, "start video_to_frame\n");
    LinkedQueueNodeData result = {
        .pts = 0,
        .frame = NULL,
        .ret = 0
        };

    int ret;
    AVFrame *frame = av_frame_alloc();
    if (!frame) {
        av_log(NULL, AV_LOG_ERROR, "Could not allocate image frame\n");
        result.ret = -1;
        result.error_message = "Could not allocate image frame";
        return result;
    }

    AVPacket *orig_pkt = av_packet_alloc();
    if (!orig_pkt) {
        av_log(NULL, AV_LOG_ERROR, "Couldn't alloc packet\n");
        result.ret = -2;
        result.error_message = "Couldn't alloc packet";
        close(frame, orig_pkt);
        return result;
    }

    /* read frames from the file */
    av_log(NULL, AV_LOG_DEBUG, "begin av_read_frame time: %li\n", get_time());

    while (av_read_frame(vis.format_context, orig_pkt) >= 0) {
        av_log(NULL, AV_LOG_DEBUG, "end av_read_frame time: %li\n", get_time());
        if (orig_pkt->stream_index == vis.video_stream_idx) {
            if (orig_pkt->flags & AV_PKT_FLAG_KEY) {
                av_log(NULL, AV_LOG_DEBUG, "key frame\n");
                if (orig_pkt->pts < 0) {
                    pts = 0;
                }
            }
            if (vis.video_stream == NULL) {
                av_log(NULL, AV_LOG_ERROR, "error: video stream is null\n");
                result.ret = -3;
                result.error_message = "video stream is null";
                close(frame, orig_pkt);
                return result;
            }

            long pts_time = 0;
            // 获取帧数
            if (orig_pkt->pts >= 0)
                pts_time = (long)(orig_pkt->pts * av_q2d(vis.video_stream->time_base) * vis.frame_rate);
            else
                pts_time = pts++;

            ret = avcodec_send_packet(vis.video_codec_context, orig_pkt);
            av_log(NULL, AV_LOG_DEBUG, "end avcodec_send_packet time: %li\n", get_time());
            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR, "Error while sending a packet to the decoder\n");
                close(frame, orig_pkt);
                result.ret = -4;
                result.error_message = "Error while sending a packet to the decoder";
                return result;
            }

            ret = avcodec_receive_frame(vis.video_codec_context, frame);
            av_log(NULL, AV_LOG_DEBUG, "end avcodec_receive_frame time: %li\n", get_time());
            // 解码一帧数据
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                av_log(NULL, AV_LOG_DEBUG, "Decode finished\n");
                continue;
            }
            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR, "Decode error\n");
                close(frame, orig_pkt);
                result.ret = -4;
                result.error_message = "Error while receive frame from a packet";
                return result;
            }

            chose_frames = chose_frames > vis.frame_rate ? vis.frame_rate : chose_frames;
            int c = vis.frame_rate / chose_frames;
            av_log(NULL, AV_LOG_DEBUG, "frame_rate %d chose_frames %d c %d\n", vis.frame_rate, chose_frames ,c);
            long check = pts_time % c;
            av_log(NULL, AV_LOG_DEBUG, "check %ld\n", check);

            av_log(NULL, AV_LOG_DEBUG, "pts_time: %ld chose_frames: %d frame_rate: %d\n", pts_time,
                    chose_frames, vis.frame_rate);
            // 判断帧数，是否取
            // if (check == c - 1) {
                result.frame = frame;
                result.pts = pts_time;
                // av_log(NULL, AV_LOG_INFO, "result.pts %d \n", result.pts);
                if (queue == NULL) {
                    av_log(NULL, AV_LOG_INFO, "queue is null\n");
                }
                // fprintf(stdout, "queue real_size %d\n", queue->real_size);
                push_linkedQueue(queue, result);
                if (semaphore != NULL) sem_post(semaphore);
                // av_log(NULL, AV_LOG_INFO, "frame ....................\n");
            // }

            av_packet_unref(orig_pkt);
        }
    }

    close(frame, orig_pkt);

    return result;
}

