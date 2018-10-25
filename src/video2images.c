#include "./video2images.h"

time_t get_time() {
    time_t result;

    struct timeval tv;
    gettimeofday(&tv, NULL);
    result = tv.tv_sec;
    av_log(NULL, AV_LOG_DEBUG, "now: %ld\n", 1000000 * tv.tv_sec + tv.tv_usec);

    char buff[20];
    strftime(buff, 20, "%Y-%m-%d %H:%M:%S", localtime(&result));
    av_log(NULL, AV_LOG_DEBUG, "now: %s\n", buff);
    av_log(NULL, AV_LOG_DEBUG, "now: %f\n", (double) clock() * 1000.0 / CLOCKS_PER_SEC);
    return result;
}

static void jpeg_write_mem(uint8_t *raw_data, int quality, unsigned int width, unsigned int height,
        unsigned char **jpeg_data, unsigned long *jpeg_size) {

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
        (void) jpeg_write_scanlines(&jpeg_struct, row_pointer, 1);
    }

    // 结束压缩循环
    jpeg_finish_compress(&jpeg_struct);
    // 释放jpeg压缩对象
    jpeg_destroy_compress(&jpeg_struct);

    printf("compress one picture success.\n");
}

static int copy_frame_data(const AVFrame *frame, int quality,
        const AVCodecContext *codec_context, napi_env env, napi_value *result,
        void (*callback_fileinfo(napi_env env, int file_size, char *file_info, napi_value *result))
        ) {
    static uint8_t *images_dst_data[4] = {NULL};
    static int images_dst_linesize[4];

    AVFrame *target_frame = av_frame_alloc();
    if (!target_frame) {
        av_log(NULL, AV_LOG_ERROR, "Could not allocate images frame\n");
        return -1;
    }

    av_log(NULL, AV_LOG_DEBUG, "saving frame %3d\n", codec_context->frame_number);

    /* 图片是解码器分配的内存，不需要释放 */
    target_frame->quality = 1;

    //保存jpeg图片
    // enum AVPixelFormat target_pixel_format = AV_PIX_FMT_YUVJ420P;
    // const char *suffix = "yuv";

    enum AVPixelFormat target_pixel_format = AV_PIX_FMT_RGB24;

    int align = 1; // input_pixel_format->linesize[0] % 32;

    // 获取一帧图片的体积
    int picture_size = av_image_get_buffer_size(target_pixel_format, codec_context->width, codec_context->height, align);

    // 申请一张图片数据的存储空间
    uint8_t *outBuff = (uint8_t *) av_malloc((size_t) picture_size);
    av_image_fill_arrays(target_frame->data, target_frame->linesize, outBuff, target_pixel_format,
                         codec_context->width, codec_context->height, align);

    // 设置图像转换上下文
    int target_width = codec_context->width;
    int target_height = codec_context->height;
    struct SwsContext *sws_context = sws_getContext(codec_context->width, codec_context->height,
                                                    codec_context->pix_fmt,
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


    /* copy decoded frame to destination buffer: this is required since rawvideo expects non aligned data */
    av_image_copy(images_dst_data, images_dst_linesize,
                  (const uint8_t **) (target_frame->data), target_frame->linesize,
                  target_frame->format, target_frame->width, target_frame->height);

    unsigned char *jpeg_data;
    unsigned long jpeg_size = 0;
    jpeg_write_mem(images_dst_data[0], quality, target_width, target_height, &jpeg_data, &jpeg_size);

    callback_fileinfo(env, jpeg_size, jpeg_data, result);

    sws_freeContext(sws_context);
    av_free(outBuff);
    av_frame_free(&target_frame);

    return 0;
}

Video2ImageStream open_inputfile(const char *filename)
{
    av_log(NULL, AV_LOG_DEBUG, "start time: %ld\n", get_time());
    static int          video_stream_idx        = -1;
    static bool         refcount                = false;

    static AVStream     *video_stream           = NULL;

    AVFormatContext     *format_context         = NULL;

    AVCodecContext      *video_codec_context    = NULL;
    AVFrame             *frame                  = NULL;

    int                 ret;

    AVDictionary        *dictionary             = NULL;
    AVPacket            *packet                 = NULL;

    Video2ImageStream   result                  = { 
        .format_context         = NULL,
        .packet                 = NULL,
        .video_stream_idx       = -1,
        .video_codec_context    = NULL,
        .frame                  = NULL,
        .ret                    = -1
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
            release(packet, frame, video_codec_context, format_context, _bool);
            result.ret = -1;
            return result;
        }

        if (av_dict_set(&dictionary, "rtsp_transport", "tcp", 0) < 0) {
            av_log(NULL, AV_LOG_ERROR, "set rtsp_transport to tcp error\n");

            if (av_dict_set(&dictionary, "rtsp_transport", "udp", 0) < 0) {
                av_log(NULL, AV_LOG_ERROR, "set rtsp_transport to udp error\n");
            }
        }

        // -r 1
        av_dict_set(&dictionary, "analyzeduration", "2000", 0);
        av_dict_set(&dictionary, "flush_packets", "1", 0);
        av_dict_set(&dictionary, "fflags", "nobuffer", 0);
        av_dict_set(&dictionary, "buffer_size", "1024000", 0);

        if (av_dict_set(&dictionary, "rtsp_flags", "prefer_tcp", 0) < 0) {
            av_log(NULL, AV_LOG_ERROR, "set rtsp_flags to prefer_tcp error\n");
        }

        if (av_dict_set(&dictionary, "hwaccel_device", "1", 0) < 0) {
            av_log(NULL, AV_LOG_ERROR, "no hwaccel device\n");
        }

        if (av_dict_set(&dictionary, "hwaccel", "opencl", 0) < 0) {
            av_log(NULL, AV_LOG_ERROR, "dxva2 acceleration error\n");
        }

        if (av_dict_set(&dictionary, "hwaccel", "dxva2", 0) < 0) {
            av_log(NULL, AV_LOG_ERROR, "dxva2 acceleration error\n");
        }

        if (av_dict_set(&dictionary, "hwaccel", "cuvid", 0) < 0) {
            av_log(NULL, AV_LOG_ERROR, "cuvid acceleration error\n");
        }

        if (av_dict_set(&dictionary, "allowed_media_types", "video", 0) < 0) {
            av_log(NULL, AV_LOG_ERROR, "set allowed_media_types to video error\n");
        }
    }

    format_context = NULL;

    fprintf(stdout, "input file: %s\n", filename);
    // format_context必须初始化，否则报错
    if ((ret = avformat_open_input(&format_context, filename, NULL, &dictionary)) != 0) {
        av_log(NULL, AV_LOG_ERROR, "Couldn't open file %s: %d", filename, ret);
        release(packet, frame, video_codec_context, format_context, _bool);
        result.ret = -2;
        return result;
    }

    if (avformat_find_stream_info(format_context, NULL) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot find stream information\n");
        release(packet, frame, video_codec_context, format_context, _bool);
        result.ret = -3;
        return result;
    }

    ret = av_find_best_stream(format_context, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Could not find %s stream in input file '%s'\n", av_get_media_type_string(AVMEDIA_TYPE_VIDEO),
                filename);
    }
    video_stream_idx = ret;

    video_stream = format_context->streams[video_stream_idx];

    AVCodec *codec = avcodec_find_decoder(video_stream->codecpar->codec_id);
    if (!codec) {
        av_log(NULL, AV_LOG_ERROR, "Failed to find %s codec\n", av_get_media_type_string(AVMEDIA_TYPE_VIDEO));
        release(packet, frame, video_codec_context, format_context, _bool);
        result.ret = -3;
        return result;
    }

    AVCodecParserContext *parserContext = av_parser_init(codec->id);
    if (!parserContext) {
        av_log(NULL, AV_LOG_ERROR, "parser not found\n");
        release(packet, frame, video_codec_context, format_context, _bool);
        result.ret = -4;
        return result;
    }

    video_codec_context = avcodec_alloc_context3(codec);
    if (!video_codec_context) {
        av_log(NULL, AV_LOG_ERROR, "Failed to allocate the %s codec context\n", av_get_media_type_string(AVMEDIA_TYPE_VIDEO));
        release(packet, frame, video_codec_context, format_context, _bool);
        result.ret = -5;
        return result;
    }

    if ((ret = avcodec_parameters_to_context(video_codec_context, video_stream->codecpar)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Failed to copy %s codec parameters to decoder context\n",
                av_get_media_type_string(AVMEDIA_TYPE_VIDEO));
        release(packet, frame, video_codec_context, format_context, _bool);
        result.ret = -6;
        return result;
    }

    AVDictionary *opts = NULL;
    av_dict_set(&opts, "refcounted_frames", refcount ? "1" : "0", 0);
    if (avcodec_open2(video_codec_context, codec, &opts) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Failed to open %s codec\n", av_get_media_type_string(AVMEDIA_TYPE_VIDEO));
        release(packet, frame, video_codec_context, format_context, _bool);
        result.ret = -7;
        return result;
    }

    /* dump input information to stderr */
    av_dump_format(format_context, 0, filename, 0);

    if (!video_stream) {
        av_log(NULL, AV_LOG_ERROR, "Could not find audio or video stream in the input, aborting\n");
        release(packet, frame, video_codec_context, format_context, _bool);
        result.ret = -8;
        return result;
    }

    frame = av_frame_alloc();
    if (!frame) {
        av_log(NULL, AV_LOG_ERROR, "Could not allocate video frame\n");
        release(packet, frame, video_codec_context, format_context, _bool);
        result.ret = -9;
        return result;
    }

    packet = av_packet_alloc();
    if (!packet) {
        av_log(NULL, AV_LOG_ERROR, "Couldn't alloc packet\n");
        release(packet, frame, video_codec_context, format_context, _bool);
        result.ret = -10;
        return result;
    }

    result.format_context = format_context;
    result.packet = packet;
    result.video_stream_idx = video_stream_idx;
    result.video_codec_context = video_codec_context;
    result.frame = frame;
    result.ret = 0;

    return result;
}

/**
 * 释放内存
 */
void release(AVPacket *packet, AVFrame *frame, AVCodecContext *video_codec_context, 
    AVFormatContext *format_context, bool isRtsp)
{
    fprintf(stdout, "free memory\n");
    if (packet != NULL)
        av_packet_free(&packet);

    if (frame != NULL)
        av_frame_unref(frame);

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

int video2images_stream(Video2ImageStream vis, int quality, napi_env env, napi_value *result,
    void (callback_fileinfo(napi_env env, int file_size, char *file_info, napi_value *result))
    ) 
{
    int ret;
    AVPacket *orig_pkt = vis.packet;
    /* read frames from the file */
    while (av_read_frame(vis.format_context, orig_pkt) >= 0) {
        if (orig_pkt->stream_index == vis.video_stream_idx) {
            if (orig_pkt->flags & AV_PKT_FLAG_KEY) {
                fprintf(stdout, "key frame\n");
            }
            ret = avcodec_send_packet(vis.video_codec_context, orig_pkt);
            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR, "Error while sending a packet to the decoder\n");
                return -1;
            }
            do {
                ret = avcodec_receive_frame(vis.video_codec_context, vis.frame);
                //解码一帧数据
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    av_log(NULL, AV_LOG_DEBUG, "Decode finished\n");
                    break;
                }
                if (ret < 0) {
                    av_log(NULL, AV_LOG_ERROR, "Decode error\n");
                    break;
                }

                ret = copy_frame_data(vis.frame, quality, vis.video_codec_context, env, result, callback_fileinfo);
                if (ret < 0)
                    break;
                if (ret == 0)
                    return 0;
                orig_pkt->data += ret;
                orig_pkt->size -= ret;
            } while (orig_pkt->size > 0);
            av_packet_unref(orig_pkt);
        }
    }

    /* flush cached frames */
    orig_pkt->data = NULL;
    orig_pkt->size = 0;
    // decode_frame(frame, target_frame, packet, outfilename, video_codec_context, sws_context);

    printf("Demuxing succeeded.\n");

    return 0;
}