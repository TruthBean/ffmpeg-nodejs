#include "./video2images.h"

/**
 * 连接视频地址，获取数据流
 * @param filename: 视频地址
 * @param nobuffer: video stream 是否设置缓存
 * @param use_gpu: 是否使用gpu加速
 * 
 * @param vis @see Video2ImageStream
 **/
void open_inputfile(Video2ImageStream *result, const char *filename, bool nobuffer, int64_t timeout, bool use_gpu, bool use_tcp, const char *gpu_id)
{
    int64_t visPointer = (int64_t)result;
    av_log(NULL, AV_LOG_DEBUG, "[%ld] open_inputfile --> start time: %li\n", visPointer, get_now_microseconds());

    int video_stream_idx;

    int ret;

    AVDictionary *dictionary = NULL;

    result->video_stream_idx = -1;
    result->video_codec_context = NULL;
    result->ret = -1;

    av_log(NULL, AV_LOG_DEBUG, "[%ld] open_inputfile --> input url video addr %s \n", visPointer, filename);
    int error = avformat_network_init();
    if (error != 0)
    {
        result->init = false;
        av_log(NULL, AV_LOG_ERROR, "[%ld] network init error\n", visPointer);
        release(result);
        result->ret = -1;
        return;
    }

    result->init = true;

    open_input_dictionary_set(&dictionary, nobuffer, timeout, use_gpu, use_tcp);

    av_log(NULL, AV_LOG_DEBUG, "[%ld] open_inputfile --> allowed_media_types : video \n", visPointer);
    if (av_dict_set(&dictionary, "allowed_media_types", "video", 0) < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "[%ld] set allowed_media_types to video error\n", visPointer);
    }

    if (av_dict_set(&dictionary, "protocol_whitelist", "file,rtp,udp,tcp", 0) < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "[%ld] set file,rtp,udp to protocol_whitelist error\n", visPointer);
    }

    // 初始化 format_context
    result->format_context = NULL;
    av_log(NULL, AV_LOG_DEBUG, "[%ld] open_inputfile --> avformat_alloc_context \n", visPointer);
    result->format_context = avformat_alloc_context();
    av_log(NULL, AV_LOG_DEBUG, "[%ld] input file: %s\n", visPointer, filename);
    if ((ret = avformat_open_input(&(result->format_context), filename, NULL, &dictionary)) != 0)
    {
        result->init = false;
        av_log(NULL, AV_LOG_ERROR, "[%ld] Couldn't open file %s: %d\n", visPointer, filename, ret);
        release(result);
        result->error_message = "avformat_open_input error, Couldn't open this file";
        result->ret = -2;
        return;
    }

    av_dict_free(&dictionary);
    result->format_context->max_delay = 1;

    av_log(NULL, AV_LOG_DEBUG, "[%ld] open_inputfile --> avformat_find_stream_info \n", visPointer);
    if (avformat_find_stream_info(result->format_context, NULL) < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "[%ld] Cannot find stream information\n", visPointer);
        release(result);
        result->error_message = "avformat_find_stream_info error";
        result->ret = -3;
        return;
    }

    av_log(NULL, AV_LOG_DEBUG, "[%ld] open_inputfile --> av_find_best_stream \n", visPointer);
    video_stream_idx = av_find_best_stream(result->format_context, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (video_stream_idx < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "[%ld] Could not find %s stream in input file '%s'\n", visPointer,
               av_get_media_type_string(AVMEDIA_TYPE_VIDEO), filename);
        release(result);
        result->error_message = "av_find_best_stream error";
        result->ret = -4;
        return;
    }

    result->video_stream = result->format_context->streams[video_stream_idx];

    AVCodec *codec = NULL;
    AVDictionary *opts = NULL;
    av_dict_set(&opts, "refcounted_frames", "1", 0);
    enum AVCodecID codec_id = result->video_stream->codecpar->codec_id;
    // 判断摄像头视频格式是h264还是h265
    if (codec_id == AV_CODEC_ID_H264)
    {
        if (use_gpu)
        {
            av_log(NULL, AV_LOG_DEBUG, "[%ld] open_inputfile --> avcodec_find_decoder_by_name h264_cuvid \n", visPointer);
            codec = avcodec_find_decoder_by_name("h264_cuvid");
            // GPU to be used for decoding
            av_dict_set(&opts, "gpu", gpu_id, 0);
        }
        else
        {
            av_log(NULL, AV_LOG_DEBUG, "[%ld] open_inputfile --> avcodec_find_decoder_by_name h264 \n", visPointer);
            codec = avcodec_find_decoder_by_name("h264");
        }
    }
    else if (codec_id == AV_CODEC_ID_HEVC)
    {
        if (use_gpu)
        {
            av_log(NULL, AV_LOG_DEBUG, "[%ld] open_inputfile --> avcodec_find_decoder_by_name hevc_nvenc \n", visPointer);
            codec = avcodec_find_decoder_by_name("hevc_nvenc");
            // GPU to be used for decoding
            av_dict_set(&opts, "gpu", gpu_id, 0);
        }
        else
        {
            av_log(NULL, AV_LOG_DEBUG, "[%ld] open_inputfile --> avcodec_find_decoder_by_name hevc \n", visPointer);
            codec = avcodec_find_decoder_by_name("hevc");
        }
        // force low delay
        av_dict_set(&opts, "flags", "low_delay", 0);
    }
    if (codec == NULL)
    {
        av_log(NULL, AV_LOG_DEBUG, "[%ld] open_inputfile --> avcodec_find_decoder \n", visPointer);
        codec = avcodec_find_decoder(result->video_stream->codecpar->codec_id);
    }
    if (!codec)
    {
        av_log(NULL, AV_LOG_ERROR, "[%ld] Failed to find %s codec\n", visPointer, av_get_media_type_string(AVMEDIA_TYPE_VIDEO));
        release(result);
        result->error_message = "avcodec_find_decoder error";
        result->ret = -5;
        return;
    }

    // 每秒多少帧
    int input_frame_rate = result->video_stream->r_frame_rate.num / result->video_stream->r_frame_rate.den;
    av_log(NULL, AV_LOG_DEBUG, "[%ld] input video frame rate: %d\n", visPointer, input_frame_rate);

    av_log(NULL, AV_LOG_DEBUG, "[%ld] open_inputfile --> av_parser_init \n", visPointer);
    AVCodecParserContext *parser_context = av_parser_init(codec->id);
    if (!parser_context)
    {
        av_log(NULL, AV_LOG_ERROR, "[%ld] parser not found\n", visPointer);
        release(result);
        result->error_message = "parser not found";
        result->ret = -6;
        return;
    }

    av_log(NULL, AV_LOG_DEBUG, "[%ld] open_inputfile --> avcodec_alloc_context3 \n", visPointer);
    result->video_codec_context = avcodec_alloc_context3(codec);
    if (!result->video_codec_context)
    {
        av_log(NULL, AV_LOG_ERROR, "[%ld] Failed to allocate the %s codec context\n", visPointer,
               av_get_media_type_string(AVMEDIA_TYPE_VIDEO));
        release(result);
        result->error_message = "avcodec_alloc_context3 error";
        result->ret = -7;
        return;
    }

    av_log(NULL, AV_LOG_DEBUG, "[%ld] open_inputfile --> avcodec_parameters_to_context \n", visPointer);
    if ((ret = avcodec_parameters_to_context(result->video_codec_context, result->video_stream->codecpar)) < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "[%ld] Failed to copy %s codec parameters to decoder context\n", visPointer,
               av_get_media_type_string(AVMEDIA_TYPE_VIDEO));
        release(result);
        result->error_message = "avcodec_parameters_to_context error";
        result->ret = -8;
        return;
    }

    av_log(NULL, AV_LOG_DEBUG, "[%ld] open_inputfile --> avcodec_open2 \n", visPointer);
    if (avcodec_open2(result->video_codec_context, codec, &opts) < 0)
    {
        av_log(NULL, AV_LOG_WARNING, "[%ld] Failed to open %s codec\n", visPointer, av_get_media_type_string(AVMEDIA_TYPE_VIDEO));

        // 在GPU docker 运行出错后使用CPU
        // =======================================================================================================
        codec = avcodec_find_decoder(result->video_stream->codecpar->codec_id);
        if (!codec)
        {
            av_log(NULL, AV_LOG_ERROR, "[%ld] Failed to find %s codec\n", visPointer, av_get_media_type_string(AVMEDIA_TYPE_VIDEO));
            release(result);
            result->error_message = "avcodec_find_decoder error";
            result->ret = -5;
            return;
        }

        parser_context = av_parser_init(codec->id);
        if (!parser_context)
        {
            av_log(NULL, AV_LOG_ERROR, "[%ld] parser not found\n", visPointer);
            release(result);
            result->error_message = "parser not found";
            result->ret = -6;
            return;
        }

        if (&(result->video_codec_context) && result->video_codec_context)
        {
            avcodec_free_context(&(result->video_codec_context));
        }
        result->video_codec_context = avcodec_alloc_context3(codec);
        if (!result->video_codec_context)
        {
            av_log(NULL, AV_LOG_ERROR, "[%ld] Failed to allocate the %s codec context\n", visPointer,
                   av_get_media_type_string(AVMEDIA_TYPE_VIDEO));
            release(result);
            result->error_message = "avcodec_alloc_context3 error";
            result->ret = -7;
            return;
        }

        if ((ret = avcodec_parameters_to_context(result->video_codec_context, result->video_stream->codecpar)) < 0)
        {
            av_log(NULL, AV_LOG_ERROR, "[%ld] Failed to copy %s codec parameters to decoder context\n", visPointer,
                   av_get_media_type_string(AVMEDIA_TYPE_VIDEO));
            release(result);
            result->error_message = "avcodec_parameters_to_context error";
            result->ret = -8;
            return;
        }

        if (avcodec_open2(result->video_codec_context, codec, &opts) < 0)
        {
            av_log(NULL, AV_LOG_ERROR, "[%ld] Failed to open %s codec\n", visPointer, av_get_media_type_string(AVMEDIA_TYPE_VIDEO));
            release(result);
            result->error_message = "Failed to open codec";
            result->ret = -9;
            return;
        }
        // =======================================================================================================
    }

    if (!result->video_stream)
    {
        av_log(NULL, AV_LOG_ERROR, "[%ld] Could not find audio or video stream in the input, aborting\n", visPointer);
        release(result);
        result->error_message = "Could not find audio or video stream in the input";
        result->ret = -10;
        return;
    }

    result->video_stream_idx = video_stream_idx;
    result->parser_context = parser_context;
    result->ret = 0;
    result->frame_rate = input_frame_rate;
}

/**
 * 释放内存
 */
void release(Video2ImageStream *vis)
{
    if (vis == NULL || !vis || vis->release)
    {
        return;
    }
    av_log(NULL, AV_LOG_DEBUG, "begin release \n");
    vis->release = true;
    int64_t visPointer = (int64_t)vis;
    av_log(NULL, AV_LOG_DEBUG, "[%ld] release ... \n", visPointer);

    AVCodecContext *video_codec_context = vis->video_codec_context;
    AVFormatContext *format_context = vis->format_context;
    bool init = vis->init;

    vis->release = true;
    av_usleep(1);
    av_log(NULL, AV_LOG_DEBUG, "[%ld] ---> 3 ---> free memory\n", visPointer);

    int ret;
    if (video_codec_context != NULL) {
        av_log(NULL, AV_LOG_DEBUG, "[%ld] avcodec_close ... \n", visPointer);
        ret = avcodec_close(video_codec_context);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "[%ld] avcodec_close error \n", visPointer);
        }
    }

    if (video_codec_context != NULL && video_codec_context)
    {
        av_log(NULL, AV_LOG_DEBUG, "[%ld] avcodec_free_context ... \n", visPointer);
        avcodec_free_context(&video_codec_context);
        video_codec_context = NULL;
    }

    if (format_context != NULL && format_context)
    {
        av_log(NULL, AV_LOG_DEBUG, "[%ld] avformat_close_input ... \n", visPointer);
        // avformat_close_input(&format_context);
        // avformat_free_context(format_context);
        format_context = NULL;
    }

    if (init)
    {
        init = false;
        av_log(NULL, AV_LOG_DEBUG, "[%ld] avformat_network_deinit ... \n", visPointer);
        ret = avformat_network_deinit();
        av_log(NULL, AV_LOG_DEBUG, "[%ld] avformat_network_deinit finished. \n", visPointer);
        if (ret < 0)
        {
            av_log(NULL, AV_LOG_ERROR, "[%ld] avformat_network_deinit error \n", visPointer);
        }
    }
    av_log(NULL, AV_LOG_DEBUG, "end release \n");
}

static void __close(AVFrame *frame, AVPacket *packet)
{
    if (frame != NULL && frame)
    {
        av_frame_unref(frame);
        av_frame_free(&frame);
        frame = NULL;
    }

    if (packet != NULL && packet)
    {
        av_packet_unref(packet);
        av_packet_free(&packet);
        packet = NULL;
    }
}

/**
 * 视频流获取 图片帧
 * @param vis: 存储在全局变量中的视频流数据变量
 * @param quality: jpeg 图片质量，1~100
 * @param chose_frames: 一秒取多少帧, 超过视频帧率则为视频帧率
 * @param chose_now: 是否马上取帧，为true时只取一帧或者每帧都取
 * @param type: 图片类型 @see ImageStreamType
 * @return @see FrameData
 **/
void video2images_grab(Video2ImageStream *vis, int quality, int chose_frames, bool chose_now, enum ImageStreamType type,
                       Video2ImagesCallback callback, FrameData *result)
{
    if (vis == NULL || vis->ret < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Video is closed or not open\n");
        result->ret = -2;
        result->error_message = "Video is closed or not open";
        result->abort = true;
        if (callback != NULL)
        {
            callback(result);
        }
        return;
    }
    int64_t visPointer = (int64_t)vis;

    int pts = 0;
    int hadHasFrames = false;
    int retry = 0;

    int ret;
    AVFrame *frame = av_frame_alloc();
    if (!frame)
    {
        av_log(NULL, AV_LOG_ERROR, "[%ld] Could not allocate image frame\n", visPointer);
        result->ret = -11;
        result->error_message = "Could not allocate image frame";
        result->abort = true;
        if (callback != NULL)
        {
            callback(result);
        }
        return;
    }

    AVPacket *orig_pkt = av_packet_alloc();
    if (!orig_pkt)
    {
        av_log(NULL, AV_LOG_ERROR, "[%ld] Couldn't alloc packet\n", visPointer);
        result->ret = -12;
        result->error_message = "Couldn't alloc packet";
        result->abort = true;
        if (callback != NULL)
        {
            callback(result);
        }
        __close(frame, orig_pkt);
        return;
    }

    int times = 0;

    if (vis->release)
    {
        __close(frame, orig_pkt);
        return;
    }

    int err_time = 0;
    bool hasKeyFrame = false;
    while (true)
    {
        if (vis->release)
        {
            result->abort = true;
            __close(frame, orig_pkt);
            av_log(NULL, AV_LOG_DEBUG, "[%ld] video2images_grab --> isBreak \n", visPointer);
            if (callback != NULL)
            {
                callback(result);
            }
            return;
        }

        av_usleep(0);
        if (err_time > 10)
        {
            av_log(NULL, AV_LOG_DEBUG, "[%ld] video2images_grab --> err_time > 10 \n", visPointer);
            break;
        }

        if (vis == NULL || !vis || vis->format_context == NULL || !vis->format_context || orig_pkt == NULL || !orig_pkt)
        {
            result->abort = true;
            av_log(NULL, AV_LOG_DEBUG, "[%ld] video2images_grab --> vis is null, or vis->format_context is null \n", visPointer);
            if (callback != NULL)
            {
                callback(result);
            }
            __close(frame, orig_pkt);
            return;
        }
        else
        {
            /* read frames from the file */
            av_log(NULL, AV_LOG_DEBUG, "[%ld] begin av_read_frame time: %li\n", visPointer, get_now_microseconds());
            ret = av_read_frame(vis->format_context, orig_pkt);
            if (ret < 0)
            {
                av_log(NULL, AV_LOG_DEBUG, "[%ld] av_read_frame error. \n", visPointer);
                err_time++;
                continue;
            }
        }
        if (vis->release)
        {
            av_log(NULL, AV_LOG_DEBUG, "[%ld] video2images_grab --> isBreak... \n", visPointer);
            result->abort = true;
            __close(frame, orig_pkt);
            return;
        }
        av_log(NULL, AV_LOG_DEBUG, "[%ld] end av_read_frame time: %li\n", visPointer, get_now_microseconds());
        if (orig_pkt != NULL && orig_pkt && vis != NULL && vis && orig_pkt->stream_index == vis->video_stream_idx)
        {
            av_log(NULL, AV_LOG_DEBUG, "[%ld] video2images_grab --> has video stream \n", visPointer);
            if (orig_pkt->flags & AV_PKT_FLAG_KEY)
            {
                av_log(NULL, AV_LOG_DEBUG, "[%ld] key frame\n", visPointer);
                if (orig_pkt->pts < 0)
                {
                    pts = 0;
                }
            }
            if (vis->video_stream == NULL)
            {
                av_log(NULL, AV_LOG_ERROR, "[%ld] error: video stream is null\n", visPointer);
                result->ret = -13;
                result->error_message = "video stream is null";
                result->abort = true;
                if (callback != NULL)
                {
                    callback(result);
                }
                __close(frame, orig_pkt);
                return;
            }

            long pts_time = 0;
            // 获取帧数
            if (orig_pkt->pts >= 0)
                pts_time = (long)(orig_pkt->pts * av_q2d(vis->video_stream->time_base) * vis->frame_rate);
            else
                pts_time = pts++;

            av_log(NULL, AV_LOG_DEBUG, "[%ld] begin avcodec_send_packet time: %li\n", visPointer, get_now_microseconds());
            ret = avcodec_send_packet(vis->video_codec_context, orig_pkt);
            av_log(NULL, AV_LOG_DEBUG, "[%ld] end avcodec_send_packet time: %li\n", visPointer, get_now_microseconds());
            if (vis->release)
            {
                result->abort = true;
                if (callback != NULL)
                {
                    callback(result);
                }
                __close(frame, orig_pkt);
                return;
            }

            if (ret < 0)
            {
                av_log(NULL, AV_LOG_DEBUG, "[%ld] retry: %d, hadHasFrames: %d, times: %d \n", visPointer, retry, hadHasFrames, times);
                if (!hadHasFrames)
                {
                    av_packet_unref(orig_pkt);
                    av_frame_unref(frame);
                    continue;
                }
                else if (times < vis->frame_rate)
                {
                    times += 1;
                    av_packet_unref(orig_pkt);
                    av_frame_unref(frame);
                    continue;
                }

                av_log(NULL, AV_LOG_ERROR, "[%ld] Error while sending a packet to the decoder\n", visPointer);
                __close(frame, orig_pkt);
                result->ret = -14;
                result->error_message = "Error while sending a packet to the decoder";
                result->abort = true;
                if (callback != NULL)
                {
                    callback(result);
                }
                return;
            }

            chose_frames = chose_frames > vis->frame_rate ? vis->frame_rate : chose_frames;
            int c = vis->frame_rate / chose_frames;
            av_log(NULL, AV_LOG_DEBUG, "[%ld] frame_rate %d chose_frames %d c %d\n", visPointer, vis->frame_rate, chose_frames, c);
            long check = pts_time % c;
            av_log(NULL, AV_LOG_DEBUG, "[%ld] check %ld\n", visPointer, check);

            av_frame_unref(frame);
            ret = avcodec_receive_frame(vis->video_codec_context, frame);
            av_log(NULL, AV_LOG_DEBUG, "[%ld] end avcodec_receive_frame time: %li\n", visPointer, get_now_microseconds());
            // 解码一帧数据
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            {
                av_log(NULL, AV_LOG_DEBUG, "[%ld] Decode finished\n", visPointer);

                av_packet_unref(orig_pkt);
                av_frame_unref(frame);

                continue;
            }
            if (ret < 0)
            {
                av_log(NULL, AV_LOG_ERROR, "[%ld] Decode error\n", visPointer);
                __close(frame, orig_pkt);
                result->ret = -15;
                result->error_message = "Error while receive frame from a packet";
                result->abort = true;
                if (callback != NULL)
                {
                    callback(result);
                }
                return;
            }

            hadHasFrames = true;
            times = 0;

            av_log(NULL, AV_LOG_DEBUG, "[%ld] pts_time: %ld chose_frames: %d frame_rate: %d\n", visPointer, pts_time, chose_frames, vis->frame_rate);
            // 判断帧数，是否取
            if (chose_now || check == c - 1)
            {
                result->frame = frame;
                result->pts = pts_time;
                result->type = type;
                result->quality = quality;
                result->ret = 0;
                result->vis = vis;

                av_log(NULL, AV_LOG_DEBUG, "[%ld] result.pts %ld \n", visPointer, result->pts);

                if (callback != NULL)
                {
                    callback(result);
                    av_log(NULL, AV_LOG_DEBUG, "[%ld] result->isThreadly %d \n", visPointer, result->isThreadly);
                    if (!result->isThreadly || result->abort)
                    {
                        break;
                    }
                }
                av_log(NULL, AV_LOG_DEBUG, "[%ld] frame ....................\n", visPointer);
                pts_time = 0;
                pts = 0;
            }
            av_usleep(0);
        }

        if (orig_pkt != NULL)
        {
            av_packet_unref(orig_pkt);
        }
        if (frame != NULL)
        {
            av_frame_unref(frame);
        }
    }

    __close(frame, orig_pkt);

    result->abort = true;
    if (callback != NULL)
    {
        callback(result);
    }
}
