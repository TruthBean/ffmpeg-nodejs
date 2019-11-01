#include "./video2images.h"

static FrameTimeOut frame_time_out;

/**
 * 连接视频地址，获取数据流
 * @param filename: 视频地址
 * @param nobuffer: video stream 是否设置缓存
 * @param use_gpu: 是否使用gpu加速
 * 
 * @return @see Video2ImageStream
 **/
Video2ImageStream open_inputfile(const char *filename, const bool nobuffer, const int timeout, const bool use_gpu, const bool use_tcp, const char *gpu_id)
{
    FrameTimeOut _frame_time_out = {
        .status = START,
        .grab_time = get_now_microseconds()};

    frame_time_out = _frame_time_out;
    av_log(NULL, AV_LOG_DEBUG, "start time: %li\n", get_now_microseconds());

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
        .ret = -1};

    av_log(NULL, AV_LOG_DEBUG, "input url video addr %s \n", filename);
    int error = avformat_network_init();
    if (error != 0)
    {
        av_log(NULL, AV_LOG_ERROR, "network init error\n");
        release(video_codec_context, format_context);
        result.ret = -1;
        return result;
    }

    open_input_dictionary_set(&dictionary, nobuffer, timeout, use_gpu, use_tcp);

    if (av_dict_set(&dictionary, "allowed_media_types", "video", 0) < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "set allowed_media_types to video error\n");
    }

    // if (av_dict_set(&dictionary, "protocol_whitelist", "file,rtp,udp", 0) < 0)
    // {
    //     av_log(NULL, AV_LOG_ERROR, "set file,rtp,udp to protocol_whitelist error\n");
    // }

    // 初始化 format_context
    format_context = avformat_alloc_context();
    av_log(NULL, AV_LOG_DEBUG, "input file: %s\n", filename);
    if ((ret = avformat_open_input(&format_context, filename, NULL, &dictionary)) != 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Couldn't open file %s: %d\n", filename, ret);
        release(video_codec_context, format_context);
        result.error_message = "avformat_open_input error, Couldn't open this file";
        result.ret = -2;
        return result;
    }

    av_dict_free(&dictionary);
    format_context->max_delay = 1;

    if (avformat_find_stream_info(format_context, NULL) < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Cannot find stream information\n");
        release(video_codec_context, format_context);
        result.error_message = "avformat_find_stream_info error";
        result.ret = -3;
        return result;
    }

    video_stream_idx = av_find_best_stream(format_context, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (video_stream_idx < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Could not find %s stream in input file '%s'\n",
               av_get_media_type_string(AVMEDIA_TYPE_VIDEO),
               filename);
        release(video_codec_context, format_context);
        result.error_message = "av_find_best_stream error";
        result.ret = -4;
        return result;
    }

    video_stream = format_context->streams[video_stream_idx];

    AVCodec *codec = NULL;
    AVDictionary *opts = NULL;
    av_dict_set(&opts, "refcounted_frames", "true", 0);
    enum AVCodecID codec_id = video_stream->codecpar->codec_id;
    // 判断摄像头视频格式是h264还是h265
    if (use_gpu && codec_id == AV_CODEC_ID_H264)
    {
        codec = avcodec_find_decoder_by_name("h264_cuvid");
        av_dict_set(&opts, "gpu", gpu_id, 0);
    }
    else if (codec_id == AV_CODEC_ID_HEVC)
    {
        if (use_gpu)
            codec = avcodec_find_decoder_by_name("hevc_nvenc");
        av_dict_set(&opts, "flags", "low_delay", 0);
        av_dict_set(&opts, "gpu", gpu_id, 0);
    }
    if (codec == NULL)
        codec = avcodec_find_decoder(video_stream->codecpar->codec_id);
    if (!codec)
    {
        av_log(NULL, AV_LOG_ERROR, "Failed to find %s codec\n", av_get_media_type_string(AVMEDIA_TYPE_VIDEO));
        release(video_codec_context, format_context);
        result.error_message = "avcodec_find_decoder error";
        result.ret = -5;
        return result;
    }

    // 每秒多少帧
    int input_frame_rate = video_stream->r_frame_rate.num / video_stream->r_frame_rate.den;
    av_log(NULL, AV_LOG_DEBUG, "input video frame rate: %d\n", input_frame_rate);

    AVCodecParserContext *parser_context = av_parser_init(codec->id);
    if (!parser_context)
    {
        av_log(NULL, AV_LOG_ERROR, "parser not found\n");
        release(video_codec_context, format_context);
        result.error_message = "parser not found";
        result.ret = -6;
        return result;
    }

    video_codec_context = avcodec_alloc_context3(codec);
    if (!video_codec_context)
    {
        av_log(NULL, AV_LOG_ERROR, "Failed to allocate the %s codec context\n",
               av_get_media_type_string(AVMEDIA_TYPE_VIDEO));
        release(video_codec_context, format_context);
        result.error_message = "avcodec_alloc_context3 error";
        result.ret = -7;
        return result;
    }

    if ((ret = avcodec_parameters_to_context(video_codec_context, video_stream->codecpar)) < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Failed to copy %s codec parameters to decoder context\n",
               av_get_media_type_string(AVMEDIA_TYPE_VIDEO));
        release(video_codec_context, format_context);
        result.error_message = "avcodec_parameters_to_context error";
        result.ret = -8;
        return result;
    }

    if (avcodec_open2(video_codec_context, codec, &opts) < 0)
    {
        av_log(NULL, AV_LOG_WARNING, "Failed to open %s codec\n", av_get_media_type_string(AVMEDIA_TYPE_VIDEO));

        // 在GPU docker 运行出错后使用CPU
        // =======================================================================================================
        codec = avcodec_find_decoder(video_stream->codecpar->codec_id);
        if (!codec)
        {
            av_log(NULL, AV_LOG_ERROR, "Failed to find %s codec\n", av_get_media_type_string(AVMEDIA_TYPE_VIDEO));
            release(video_codec_context, format_context);
            result.error_message = "avcodec_find_decoder error";
            result.ret = -5;
            return result;
        }

        parser_context = av_parser_init(codec->id);
        if (!parser_context)
        {
            av_log(NULL, AV_LOG_ERROR, "parser not found\n");
            release(video_codec_context, format_context);
            result.error_message = "parser not found";
            result.ret = -6;
            return result;
        }

        avcodec_free_context(&video_codec_context);
        video_codec_context = avcodec_alloc_context3(codec);
        if (!video_codec_context)
        {
            av_log(NULL, AV_LOG_ERROR, "Failed to allocate the %s codec context\n",
                   av_get_media_type_string(AVMEDIA_TYPE_VIDEO));
            release(video_codec_context, format_context);
            result.error_message = "avcodec_alloc_context3 error";
            result.ret = -7;
            return result;
        }

        if ((ret = avcodec_parameters_to_context(video_codec_context, video_stream->codecpar)) < 0)
        {
            av_log(NULL, AV_LOG_ERROR, "Failed to copy %s codec parameters to decoder context\n",
                   av_get_media_type_string(AVMEDIA_TYPE_VIDEO));
            release(video_codec_context, format_context);
            result.error_message = "avcodec_parameters_to_context error";
            result.ret = -8;
            return result;
        }

        if (avcodec_open2(video_codec_context, codec, &opts) < 0)
        {
            av_log(NULL, AV_LOG_ERROR, "Failed to open %s codec\n", av_get_media_type_string(AVMEDIA_TYPE_VIDEO));
            release(video_codec_context, format_context);
            result.error_message = "Failed to open codec";
            result.ret = -9;
            return result;
        }
        // =======================================================================================================
    }

    if (!video_stream)
    {
        av_log(NULL, AV_LOG_ERROR, "Could not find audio or video stream in the input, aborting\n");
        release(video_codec_context, format_context);
        result.error_message = "Could not find audio or video stream in the input";
        result.ret = -10;
        return result;
    }

    result.format_context = format_context;
    result.video_stream_idx = video_stream_idx;
    result.video_stream = video_stream;
    result.video_codec_context = video_codec_context;
    result.parser_context = parser_context;
    result.ret = 0;
    result.frame_rate = input_frame_rate;

    return result;
}

static volatile bool isBreak = false;

void setBreak(bool b)
{
    isBreak = b;
}

/**
 * 释放内存
 */
void release(AVCodecContext *video_codec_context, AVFormatContext *format_context)
{
    isBreak = true;
    // sleep(1);
    frame_time_out.status = END;
    av_log(NULL, AV_LOG_DEBUG, "---> 3 ---> free memory\n");

    int ret;

    if (video_codec_context)
    {
        av_log(NULL, AV_LOG_DEBUG, "avcodec_free_context ... \n");
        avcodec_free_context(&video_codec_context);
        video_codec_context = NULL;
    }

    if (format_context)
    {
        av_log(NULL, AV_LOG_DEBUG, "avformat_close_input ... \n");
        avformat_close_input(&format_context);
        avformat_free_context(format_context);
        format_context = NULL;
    }

    av_log(NULL, AV_LOG_DEBUG, "avformat_network_deinit ... \n");
    ret = avformat_network_deinit();
    if (ret < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "avformat_network_deinit error \n");
    }
}

static void __close(AVFrame *frame, AVPacket *packet)
{
    if (frame != NULL)
    {
        av_frame_unref(frame);
        av_frame_free(&frame);
        frame = NULL;
    }

    if (packet != NULL)
    {
        av_packet_unref(packet);
        av_packet_free(&packet);
        packet = NULL;
    }
}

static int pts = 0;
static int hadHasFrames = false;
static int retry = 0;

/**
 * 视频流获取 图片帧
 * @param vis: 存储在全局变量中的视频流数据变量
 * @param quality: jpeg 图片质量，1~100
 * @param chose_frames: 一秒取多少帧, 超过视频帧率则为视频帧率
 * @param chose_now: 是否马上取帧，为true时只取一帧或者每帧都取
 * @param type: 图片类型 @see ImageStreamType
 * @return @see FrameData
 **/
void video2images_grab(Video2ImageStream vis, int quality, int chose_frames, bool chose_now, enum ImageStreamType type,
                       Video2ImagesCallback callback, FrameData *result)
{

    int ret;
    AVFrame *frame = av_frame_alloc();
    if (!frame)
    {
        av_log(NULL, AV_LOG_ERROR, "Could not allocate image frame\n");
        result->ret = -11;
        result->error_message = "Could not allocate image frame";
        return;
    }

    AVPacket *orig_pkt = av_packet_alloc();
    if (!orig_pkt)
    {
        av_log(NULL, AV_LOG_ERROR, "Couldn't alloc packet\n");
        result->ret = -12;
        result->error_message = "Couldn't alloc packet";
        __close(frame, orig_pkt);
        return;
    }

    /* read frames from the file */
    av_log(NULL, AV_LOG_DEBUG, "begin av_read_frame time: %li\n", get_now_microseconds());

    int times = 0;

    frame_time_out.status = PENDIING;
    frame_time_out.grab_time = get_now_microseconds();
    if (isBreak)
    {
        __close(frame, orig_pkt);
        return;
    }
    
    while (av_read_frame(vis.format_context, orig_pkt) >= 0)
    {
        if (isBreak)
        {
            __close(frame, orig_pkt);
            return;
        }

        frame_time_out.status = GRAB;
        frame_time_out.grab_time = get_now_microseconds();
        av_log(NULL, AV_LOG_DEBUG, "end av_read_frame time: %li\n", get_now_microseconds());
        if (orig_pkt->stream_index == vis.video_stream_idx)
        {
            if (orig_pkt->flags & AV_PKT_FLAG_KEY)
            {
                av_log(NULL, AV_LOG_DEBUG, "key frame\n");
                if (orig_pkt->pts < 0)
                {
                    pts = 0;
                }
            }
            if (vis.video_stream == NULL)
            {
                av_log(NULL, AV_LOG_ERROR, "error: video stream is null\n");
                result->ret = -13;
                result->error_message = "video stream is null";
                __close(frame, orig_pkt);
                frame_time_out.status = END;
                frame_time_out.grab_time = get_now_microseconds();
                return;
            }

            long pts_time = 0;
            // 获取帧数
            if (orig_pkt->pts >= 0)
                pts_time = (long)(orig_pkt->pts * av_q2d(vis.video_stream->time_base) * vis.frame_rate);
            else
                pts_time = pts++;

            ret = avcodec_send_packet(vis.video_codec_context, orig_pkt);
            av_log(NULL, AV_LOG_DEBUG, "end avcodec_send_packet time: %li\n", get_now_microseconds());
            if (isBreak)
            {
                __close(frame, orig_pkt);
                return;
            }

            if (ret < 0)
            {
                av_log(NULL, AV_LOG_DEBUG, "retry: %d, hadHasFrames: %d, times: %d \n", retry, hadHasFrames, times);
                if (!hadHasFrames)
                {
                    if (retry < 5)
                    {
                        retry += 1;
                        av_packet_unref(orig_pkt);
                        av_frame_unref(frame);
                        continue;
                    }
                }
                else if (times < vis.frame_rate)
                {
                    times += 1;
                    av_packet_unref(orig_pkt);
                    av_frame_unref(frame);
                    continue;
                }

                av_log(NULL, AV_LOG_ERROR, "Error while sending a packet to the decoder\n");
                __close(frame, orig_pkt);
                result->ret = -14;
                result->error_message = "Error while sending a packet to the decoder";
                frame_time_out.status = END;
                frame_time_out.grab_time = get_now_microseconds();
                return;
            }

            chose_frames = chose_frames > vis.frame_rate ? vis.frame_rate : chose_frames;
            int c = vis.frame_rate / chose_frames;
            av_log(NULL, AV_LOG_DEBUG, "frame_rate %d chose_frames %d c %d\n", vis.frame_rate, chose_frames, c);
            long check = pts_time % c;
            av_log(NULL, AV_LOG_DEBUG, "check %ld\n", check);

            ret = avcodec_receive_frame(vis.video_codec_context, frame);
            av_log(NULL, AV_LOG_DEBUG, "end avcodec_receive_frame time: %li\n", get_now_microseconds());
            // 解码一帧数据
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            {
                av_log(NULL, AV_LOG_DEBUG, "Decode finished\n");
                frame_time_out.status = PENDIING;
                frame_time_out.grab_time = get_now_microseconds();

                av_packet_unref(orig_pkt);
                av_frame_unref(frame);

                continue;
            }
            if (ret < 0)
            {
                av_log(NULL, AV_LOG_ERROR, "Decode error\n");
                __close(frame, orig_pkt);
                result->ret = -15;
                result->error_message = "Error while receive frame from a packet";
                frame_time_out.status = END;
                frame_time_out.grab_time = get_now_microseconds();
                return;
            }

            hadHasFrames = true;
            times = 0;

            av_log(NULL, AV_LOG_DEBUG, "pts_time: %ld chose_frames: %d frame_rate: %d\n", pts_time, chose_frames, vis.frame_rate);
            // 判断帧数，是否取
            if (chose_now || check == c - 1)
            {
                result->frame = frame;
                result->pts = pts_time;
                result->type = type;
                result->quality = quality;
                result->ret = 0;

                av_log(NULL, AV_LOG_DEBUG, "result.pts %ld \n", result->pts);

                if (callback != NULL)
                {
                    callback(result);
                    av_log(NULL, AV_LOG_DEBUG, "result->isThreadly %d \n", result->isThreadly);
                    if (!result->isThreadly || result->abort)
                    {
                        frame_time_out.status = END;
                        frame_time_out.grab_time = get_now_microseconds();
                        break;
                    }
                }
                // av_frame_unref(result->frame);
                av_log(NULL, AV_LOG_DEBUG, "frame ....................\n");
            }
            av_usleep(0);
            frame_time_out.status = PENDIING;
            frame_time_out.grab_time = get_now_microseconds();
        }

        av_packet_unref(orig_pkt);
        av_frame_unref(frame);
    }

    __close(frame, orig_pkt);
}