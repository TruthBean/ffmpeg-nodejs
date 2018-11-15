#include "./ffmpeg_nodejs.h"

static Video2ImageStream vis;

napi_value handle_init_read_video(napi_env env, napi_callback_info info) {
    napi_status status;
    napi_value result = NULL;

    size_t argc = 2;
    napi_value argv[2];
    napi_value *thisArg = NULL;
    void *data = NULL;
    status = napi_get_cb_info(env, info, &argc, argv, thisArg, &data);

    if (status != napi_ok) {
        napi_throw_error(env, NULL, "Failed to parse arguments");
    }

    size_t length;

    // input filename
    status = napi_get_value_string_utf8(env, argv[0], NULL, 0, &length);
    if (status != napi_ok) {
        napi_throw_error(env, NULL, "Invalid number was passed as argument");
    }

    // put param value into char array
    size_t input_filename_res;
    const size_t input_filename_len = length + 1;
    char input_filename[input_filename_len];
    status = napi_get_value_string_utf8(env, argv[0], input_filename, length + 1, &input_filename_res);

    if (status != napi_ok) {
        napi_throw_error(env, NULL, "Invalid value was passed as argument by input_filename");
    }
    av_log(NULL, AV_LOG_INFO, "input filename : %s\n", input_filename);

    // nobuffer
    bool nobuffer;
    status = napi_get_value_bool(env, argv[1], &nobuffer);
    av_log(NULL, AV_LOG_INFO, "nobuffer : %d\n", nobuffer);

    napi_value promise;
    napi_deferred deferred;
    status = napi_create_promise(env, &deferred, &promise);
    if (status != napi_ok) {
        napi_throw_error(env, NULL, "create promise error");
    }

    vis = open_inputfile(input_filename, nobuffer);

    av_log(NULL, AV_LOG_DEBUG, "---> 1 ---> ret: %d\n", vis.ret);
    if (vis.ret < 0) {
        av_log(NULL, AV_LOG_DEBUG, "---> 1 ---> promise reject\n");

        napi_value message;
        status = napi_create_string_utf8(env, vis.error_message, NAPI_AUTO_LENGTH, &message);
        if (status != napi_ok) {
            napi_throw_error(env, NULL, "error message create error");
        }

        status = napi_reject_deferred(env, deferred, message);
        if (status != napi_ok) {
            napi_throw_error(env, NULL, "promise reject error");
        }
    } else {
        av_log(NULL, AV_LOG_DEBUG, "---> 1 ---> promise resolve\n");
        napi_create_double(env, true, &result);
        status = napi_resolve_deferred(env, deferred, result);
        if (status != napi_ok) {
            napi_throw_error(env, NULL, "promise resolve error");
        }
    }

    return promise;
}

napi_value handle_video_to_image_stream(napi_env env, napi_callback_info info) {
    av_log(NULL, AV_LOG_INFO, "begin handle_video_to_raw_image_stream time: %ld\n", get_time());
    napi_status status;

    size_t argc = 4;
    napi_value argv[4];
    napi_value *thisArg = NULL;
    void *data = NULL;
    status = napi_get_cb_info(env, info, &argc, argv, thisArg, &data);

    if (status != napi_ok) {
        napi_throw_error(env, NULL, "Failed to parse arguments");
    }

    // int chose_frames
    int chose_frames = 1;
    status = napi_get_value_int32(env, argv[0], &chose_frames);
    if (status != napi_ok) {
        napi_throw_error(env, NULL, "chose_frames is invalid number");
    }
    av_log(NULL, AV_LOG_DEBUG, "input chose_frames: %d\n", chose_frames);

    // int type
    int type;
    status = napi_get_value_int32(env, argv[1], &type);
    if (status != napi_ok) {
        napi_throw_error(env, NULL, "type is invalid number");
    }
    av_log(NULL, AV_LOG_INFO, "input type: %d\n", type);

    // jpeg quality
    int quality = 80;
    status = napi_get_value_int32(env, argv[2], &quality);
    if (status != napi_ok) {
        napi_throw_error(env, NULL, "jpeg quality is invalid number");
    }
    av_log(NULL, AV_LOG_DEBUG, "input jpeg quality: %d\n", quality);

    // callback
    napi_value callback = argv[3];
    napi_valuetype argv2_type;
    status = napi_typeof(env, callback, &argv2_type);
    if (status != napi_ok) {
        napi_throw_error(env, NULL, "wrong javascript type");
    }
    if (argv2_type != napi_function) {
        napi_throw_error(env, NULL, "param is not a function");
    }

    // 处理图片
    if (vis.ret < 0) {
        napi_throw_error(env, NULL, "initReadingVideo method should be invoke first");
    }

    FrameData frameData = video2images_stream(vis, quality, chose_frames, type);

    if (status != napi_ok) {
        napi_throw_error(env, NULL, "Create buffer error");
    }

    napi_value buffer_pointer = NULL;
    int isNil = 1;
    if (frameData.ret == 0 && frameData.file_size > 0) {
        isNil = 0;

        // char数组 转换成 javascript buffer
        void *buffer_data;
        status = napi_create_buffer_copy(env, frameData.file_size, (const void*)frameData.file_data, &buffer_data, &buffer_pointer);
        av_log(NULL, AV_LOG_DEBUG, "frameData.file_size : %ld\n", frameData.file_size);
        av_log(NULL, AV_LOG_DEBUG, "napi_create_buffer_copy result %d\n", status);
    }

    napi_value result;
    status = napi_call_function(env, &thisArg, callback, 1, &buffer_pointer, &result);
    av_log(NULL, AV_LOG_INFO, "napi_call_function status %d\n", status);

    av_freep(&frameData.file_data);
    free(frameData.file_data);
    frameData.file_data = NULL;

    av_log(NULL, AV_LOG_INFO, "end handle_video_to_raw_image_stream time: %ld\n", get_time());

    napi_value promise;
    napi_deferred deferred;
    status = napi_create_promise(env, &deferred, &promise);
    if (status != napi_ok) {
        napi_throw_error(env, NULL, "create promise error");
    }

    av_log(NULL, AV_LOG_DEBUG, "isNil %d\n", isNil);

    if (isNil == 1) {

        char * _err_msg = frameData.error_message;
        if (frameData.ret == 0) {
            _err_msg = "image data is null, network may connect wrong";
        }

        napi_value message;
        status = napi_create_string_utf8(env, _err_msg, NAPI_AUTO_LENGTH, &message);
        if (status != napi_ok) {
            napi_throw_error(env, NULL, "error message create error");
        }

        status = napi_reject_deferred(env, deferred, message);
        if (status != napi_ok) {
            napi_throw_error(env, NULL, "promise reject error");
        }
    } else {
        napi_value return_value;
        napi_create_int32(env, isNil, &return_value);

        status = napi_resolve_deferred(env, deferred, return_value);
        if (status != napi_ok) {
            napi_throw_error(env, NULL, "promise resolve error");
        }
    }

    return promise;
}

napi_value handle_destroy_stream(napi_env env, napi_callback_info info) {
    release(vis.video_codec_context, vis.format_context, vis.isRtsp);
    Video2ImageStream _vis = {
        .format_context = NULL,
        .video_stream_idx = -1,
        .video_codec_context = NULL,
        .ret = -1
        };

    vis = _vis;

    return NULL;
}

void handle_record_video(napi_env env, napi_callback_info info) {
    size_t argc = 3;
    napi_value argv[3];
    napi_value *thisArg = NULL;
    void *data = NULL;
    
    napi_status status = napi_get_cb_info(env, info, &argc, argv, thisArg, &data);
    if (status != napi_ok) {
        napi_throw_error(env, NULL, "Failed to parse arguments");
    }

    // input filename
    size_t length;
    status = napi_get_value_string_utf8(env, argv[0], NULL, 0, &length);
    if (status != napi_ok) {
        napi_throw_error(env, NULL, "Invalid number was passed as argument");
    }

    // put param value into char array
    size_t input_filename_res;
    const size_t input_filename_len = length + 1;
    char input_filename[input_filename_len];
    status = napi_get_value_string_utf8(env, argv[0], input_filename, length + 1, &input_filename_res);

    if (status != napi_ok) {
        napi_throw_error(env, NULL, "Invalid value was passed as argument by input_filename");
    }
    av_log(NULL, AV_LOG_DEBUG, "input filename : %s\n", input_filename);

    // output filename
    length = 0;
    status = napi_get_value_string_utf8(env, argv[1], NULL, 0, &length);
    if (status != napi_ok) {
        napi_throw_error(env, NULL, "Invalid number was passed as argument");
    }

    // put param value into char array
    size_t output_filename_res;
    const size_t output_filename_len = length + 1;
    char output_filename[output_filename_len];
    status = napi_get_value_string_utf8(env, argv[1], output_filename, length + 1, &output_filename_res);

    if (status != napi_ok) {
        napi_throw_error(env, NULL, "Invalid value was passed as argument by output_filename");
    }
    av_log(NULL, AV_LOG_DEBUG, "output filename : %s\n", output_filename);

    // record seconds
    int record_seconds = 0;
    status = napi_get_value_int32(env, argv[2], &record_seconds);
    if (status != napi_ok) {
        napi_throw_error(env, NULL, "Invalid number was passed as argument");
    }
    av_log(NULL, AV_LOG_DEBUG, "record_seconds : %d\n", record_seconds);

    int result = record_rtsp(input_filename, output_filename, record_seconds);
    av_log(NULL, AV_LOG_DEBUG, "record rtsp result : %d\n", result);
}

napi_value init(napi_env env, napi_value exports) {
    napi_status status;
    av_log_set_level(AV_LOG_INFO);

    napi_property_descriptor methods[] = {
        DECLARE_NAPI_METHOD("initReadingVideo", handle_init_read_video),
        DECLARE_NAPI_METHOD("video2ImageStream", handle_video_to_image_stream),
        DECLARE_NAPI_METHOD("destroyStream", handle_destroy_stream),
        DECLARE_NAPI_METHOD("recordVideo", handle_record_video)
        };

    status = napi_define_properties(env, exports, sizeof(methods) / sizeof(methods[0]), methods);
    if (status != napi_ok)
        return NULL;

    return exports;
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, init);
