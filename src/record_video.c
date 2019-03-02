#include "./record_video.h"

/**
 * 释放内存
 **/
static int release_record_video_action(AVFormatContext *video_format_context, AVFormatContext *output_format_context, int result) {
    if (output_format_context) {
        avio_close(output_format_context->pb);
        avformat_free_context(output_format_context);
    }

    if (video_format_context)
        avformat_close_input(&video_format_context);

    avformat_network_deinit();
    return result;
}

/**
 * 网络视频录频
 * @param video_url: 视频地址
 * @param output_filename: 录制视频的存储路径
 * @param record_seconds: 录制视频的时长，单位秒
 **/
int record_video(const char *video_url, const char *output_filename, const int record_seconds, const bool use_gpu) {
    av_log(NULL, AV_LOG_DEBUG, "start time: %li\n", get_now_microseconds());

    time_t timenow, timestart;
    int got_key_frame = 0;

    AVFormatContext *video_format_context = NULL;
    AVFormatContext *output_format_context = NULL;

    // ============================== video ========================================

    // 初始化网络
    int error = avformat_network_init();
    if (error != 0) {
        av_log(NULL, AV_LOG_ERROR, "network init error\n");
    }

    AVDictionary *dictionary = NULL;
    if (av_dict_set(&dictionary, "rtsp_transport", "tcp", 0) < 0) {
        av_log(NULL, AV_LOG_ERROR, "set rtsp_transport to tcp error\n");

        if (av_dict_set(&dictionary, "rtsp_transport", "udp", 0) < 0) {
            av_log(NULL, AV_LOG_ERROR, "set rtsp_transport to udp error\n");
        }
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

    // open video
    if (avformat_open_input(&video_format_context, video_url, NULL, &dictionary) != 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot open input file %s\n", video_url);
        return release_record_video_action(video_format_context, output_format_context, -1);
    }

    // find stream info
    if (avformat_find_stream_info(video_format_context, NULL) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot find stream info\n");
        return release_record_video_action(video_format_context, output_format_context, -2);
    }

    // search video stream
    int video_stream_idx = av_find_best_stream(video_format_context, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (video_format_context < 0) {
        av_log(NULL, AV_LOG_DEBUG, "cannot find video stream\n");
        return release_record_video_action(video_format_context, output_format_context, -3);
    }
    AVStream *video_stream = video_format_context->streams[video_stream_idx];
    AVCodecParameters *video_codec_parms = video_stream->codecpar;

    if (video_stream_idx < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot find input video stream\n");
        return release_record_video_action(video_format_context, output_format_context, -3);
    }

    AVCodec *codec = NULL;
    AVDictionary *opts = NULL;
    av_dict_set(&opts, "refcounted_frames", "false", 0);
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
        return release_record_video_action(video_format_context, output_format_context, -4);
    }

    // 每秒多少帧
    int input_frame_rate = video_stream->r_frame_rate.num / video_stream->r_frame_rate.den;
    av_log(NULL, AV_LOG_DEBUG, "input video frame rate: %d\n", input_frame_rate);

    AVCodecParserContext *parserContext = av_parser_init(codec->id);
    if (!parserContext) {
        av_log(NULL, AV_LOG_ERROR, "parser not found\n");
        return release_record_video_action(video_format_context, output_format_context, -5);
    }

    AVCodecContext *video_codec_context = avcodec_alloc_context3(codec);
    if (!video_codec_context) {
        av_log(NULL, AV_LOG_ERROR, "Failed to allocate the %s codec context\n",
               av_get_media_type_string(AVMEDIA_TYPE_VIDEO));
        return release_record_video_action(video_format_context, output_format_context, -6);
    }

    if ((avcodec_parameters_to_context(video_codec_context, video_stream->codecpar)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Failed to copy %s codec parameters to decoder context\n",
               av_get_media_type_string(AVMEDIA_TYPE_VIDEO));
        return release_record_video_action(video_format_context, output_format_context, -7);
    }

    if (avcodec_open2(video_codec_context, codec, &opts) < 0) {
        av_log(NULL, AV_LOG_WARNING, "Failed to open %s codec\n", av_get_media_type_string(AVMEDIA_TYPE_VIDEO));
    }

    av_log(NULL, AV_LOG_DEBUG, "start out time: %li\n", get_now_microseconds());
    
    // ======================================== output ========================================

    // open output file
    // 设置输出文件格式
    AVOutputFormat *output_format = av_guess_format(NULL, output_filename, NULL);

    output_format_context = avformat_alloc_context();
    output_format_context->oformat = output_format;

    if (avio_open2(&output_format_context->pb, output_filename, AVIO_FLAG_WRITE, NULL, NULL) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot open or create input output file\n");
        return release_record_video_action(video_format_context, output_format_context, -4);
    }

    // Create output stream

    AVStream *out_stream = avformat_new_stream(output_format_context, NULL);

    if (avcodec_parameters_copy(out_stream->codecpar, video_codec_parms) < 0) {
        av_log(NULL, AV_LOG_ERROR, "copy codec params error\n");
        return release_record_video_action(video_format_context, output_format_context, -5);
    }

    out_stream->sample_aspect_ratio.num = video_codec_parms->sample_aspect_ratio.num;
    out_stream->sample_aspect_ratio.den = video_codec_parms->sample_aspect_ratio.den;

    // Assume r_frame_rate is accurate
    out_stream->r_frame_rate = video_stream->r_frame_rate;
    out_stream->avg_frame_rate = out_stream->r_frame_rate;
    out_stream->time_base = av_inv_q(out_stream->r_frame_rate);

    out_stream->sample_aspect_ratio = out_stream->time_base;

    avformat_write_header(output_format_context, NULL);

    // start reading packets from stream and write them to file

    timestart = timenow = get_now_microseconds();

    AVPacket pkt;
    av_init_packet(&pkt);

    av_log(NULL, AV_LOG_DEBUG, "end out time: %li\n", get_now_microseconds());

    int ret = 0;
    int i = 0;
    const int count_frame = input_frame_rate * record_seconds;
    while (av_read_frame(video_format_context, &pkt) >= 0 && i <= count_frame) {
        // packet is video
        if (pkt.stream_index == video_stream_idx) {
            // Make sure we start on a key frame
            if (timestart == timenow && !(pkt.flags & AV_PKT_FLAG_KEY) && !got_key_frame) {
                timestart = timenow = get_now_microseconds();
                continue;
            }
            i++;

            got_key_frame = 1;

            // copy packet
            // 转换 PTS/DTS 时序
            pkt.pts = av_rescale_q_rnd(pkt.pts, video_stream->time_base, out_stream->time_base,
                                       AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX);
            pkt.dts = av_rescale_q_rnd(pkt.dts, video_stream->time_base, out_stream->time_base,
                                       AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX);
            pkt.duration = av_rescale_q(pkt.duration, video_stream->time_base, out_stream->time_base);
            pkt.pos = -1;

            // 当网络有问题时，容易出现到达包的先后不一致，pts时序混乱会导致
            ret = av_interleaved_write_frame(output_format_context, &pkt);
            // av_interleaved_write_frame函数报 -22 错误。暂时先丢弃这些迟来的帧吧
            // 若所大部分包都没有pts时序，那就要看情况自己补上时序（比如较前一帧时序+1）再写入。
            // or success
            if (ret < 0 && ret != -22) {
                av_log(NULL, AV_LOG_ERROR, "Error muxing packet.error code %d\n", ret);
                break;
            }
        }
        av_packet_unref(&pkt);
        av_init_packet(&pkt);

        timenow = get_now_microseconds();
    }

    // 写文件尾
    av_write_trailer(output_format_context);

    av_log(NULL, AV_LOG_DEBUG, "end time: %li\n", get_now_microseconds());

    return release_record_video_action(video_format_context, output_format_context, EXIT_SUCCESS);
}
