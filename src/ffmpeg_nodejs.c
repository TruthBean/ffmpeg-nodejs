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
    av_log(NULL, AV_LOG_DEBUG, "input filename : %s\n", input_filename);

    // return promise
    napi_value promise;
    napi_deferred deferred = NULL;
    status = napi_create_promise(env, &deferred, &promise);
    if (status != napi_ok) {
        napi_throw_error(env, NULL, "create promise error");
    }

    vis = open_inputfile(input_filename);
    av_log(NULL, AV_LOG_DEBUG, "---> 1 ---> ret: %d\n", vis.ret);
    if (vis.ret < 0) {
        av_log(NULL, AV_LOG_DEBUG, "---> 1 ---> promise reject\n");
        napi_value code;
        napi_create_int64(env, vis.ret + 10000, &code);
        napi_value message;
        napi_create_string_utf8(env, vis.error_message, NAPI_AUTO_LENGTH, &message);
        napi_value error;
        napi_create_error(env, code, message, &error);

        status = napi_reject_deferred(env, deferred, error);
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

typedef struct CallbackFunctionParams{
    napi_env env;
    napi_value recv;
    napi_value func;
    size_t argc;
    const napi_value* argv;
    napi_value* result;
} CallbackFunctionParams;

static void *call_func(void *data) {
    CallbackFunctionParams *params = (CallbackFunctionParams *) data;
    napi_call_function(params->env, params->recv, params->func, params->argc, params->argv, params->result);
}

struct {
  napi_ref ref;
  napi_async_work work;
} async_work_info = { NULL, NULL };

static void callback_jpeg_buffer(napi_env env, void *data) {
    
}

static void callback_completion(napi_env env, napi_status status, void* data) {
    napi_value cb, js_status;
    napi_get_reference_value(env, &async_work_info.ref, &cb);
    napi_delete_async_work(env, async_work_info.work);
    napi_delete_reference(env, async_work_info.ref);
    async_work_info.work = NULL;
    async_work_info.ref = NULL;
    napi_create_uint32(env, (uint32_t)status, &js_status);
    napi_call_function(env, cb, cb, 1, &js_status, NULL);
}

napi_value handle_video2image_stream(napi_env env, napi_callback_info info) {
    av_log(NULL, AV_LOG_DEBUG, "begin handle_video2image_stream time: %ld\n", get_time());
    napi_status status;

    size_t argc = 3;
    napi_value argv[3];
    napi_value *thisArg = NULL;
    void *data = NULL;
    status = napi_get_cb_info(env, info, &argc, argv, thisArg, &data);

    if (status != napi_ok) {
        napi_throw_error(env, NULL, "Failed to parse arguments");
    }

    // quality
    int quality = 80;
    status = napi_get_value_int32(env, argv[0], &quality);
    if (status != napi_ok) {
        napi_throw_error(env, NULL, "quality is invalid number");
    }
    av_log(NULL, AV_LOG_DEBUG, "input quality: %d\n", quality);

    // int chose_frames
    int chose_frames = 1;
    status = napi_get_value_int32(env, argv[1], &chose_frames);
    if (status != napi_ok) {
        napi_throw_error(env, NULL, "chose_frames is invalid number");
    }
    av_log(NULL, AV_LOG_DEBUG, "input chose_frames: %d\n", chose_frames);

    // callback
    napi_value callback = argv[2];
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

    // create promiese
    napi_value promise;
    napi_deferred deferred = NULL;
    status = napi_create_promise(env, &deferred, &promise);
    if (status != napi_ok) {
        napi_throw_error(env, NULL, "create promise error");
    }

    FrameData frameData = video2images_stream(vis, quality, chose_frames);

    if (status != napi_ok) {
        napi_throw_error(env, NULL, "Create buffer error");
    }

    // 文件内容转换成javascript buffer
    napi_value buffer_pointer;
    void *buffer_data;
    status = napi_create_buffer_copy(env, frameData.file_size, frameData.file_data, &buffer_data, &buffer_pointer);
    av_log(NULL, AV_LOG_DEBUG, "frameData.file_size : %ld\n", frameData.file_size);
    av_log(NULL, AV_LOG_DEBUG, "napi_create_buffer_copy result %d\n", status);
    free(frameData.file_data);

    napi_value result;
    // CallbackFunctionParams callback_func_params = {
    //     .env = env,
    //     .recv = &thisArg,
    //     .func = callback,
    //     .argc = 1,
    //     .argv = &buffer_pointer,
    //     .result = &result
    // };
    // pthread_t pid;
    // pthread_create(&pid, NULL, call_func, (void *) &callback_func_params);

    // napi_call_function(env, &thisArg, callback, 1, &buffer_pointer, &result);

    napi_value resource_name;
    napi_create_string_utf8(env, "callback", NAPI_AUTO_LENGTH, &resource_name);
    // ==========================================================================================
    // napi_value cb;
    // napi_create_reference(env, cb, 1, &&async_work_info.ref);

    // napi_value async_resource = NULL;
    // napi_create_async_work(env, async_resource, resource_name, callback_jpeg_buffer, callback_completion, &repeated_work_info, &repeated_work_info.work)

    // ==========================================================================================
    napi_async_context context;
    napi_async_init(env, callback, resource_name, &context);

    napi_make_callback(env, context, &thisArg, callback, 1, &buffer_pointer, &result);

    napi_async_destroy(env, context);
    // ==========================================================================================

    if (frameData.file_size == 0 || status != napi_ok) {
        av_log(NULL, AV_LOG_ERROR, "---> 2 ---> promise reject\n");
        napi_value code;
        napi_create_int64(env, vis.ret + 10000, &code);
        napi_value message;
        napi_create_string_utf8(env, "Create buffer error", NAPI_AUTO_LENGTH, &message);
        napi_value error;
        napi_create_error(env, code, message, &error);

        status = napi_reject_deferred(env, deferred, error);
        av_log(NULL, AV_LOG_ERROR, "promise reject error status: %d\n", status);
        if (status != napi_ok) {
            napi_throw_error(env, NULL, "promise reject error");
        }
    } else {
        av_log(NULL, AV_LOG_DEBUG, "---> 2 ---> promise resolve\n");
        status = napi_resolve_deferred(env, deferred, buffer_pointer);
        if (status != napi_ok) {
            napi_throw_error(env, NULL, "promise resolve error");
        }
    }

    av_log(NULL, AV_LOG_DEBUG, "end handle_video2image_stream time: %ld\n", get_time());

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

napi_value init(napi_env env, napi_value exports) {
    napi_status status;
    av_log_set_level(AV_LOG_INFO);

    napi_property_descriptor methods[] = {
        DECLARE_NAPI_METHOD("initReadingVideo", handle_init_read_video),
        DECLARE_NAPI_METHOD("video2JpegStream", handle_video2image_stream),
        DECLARE_NAPI_METHOD("destroyStream", handle_destroy_stream)};

    status = napi_define_properties(env, exports, sizeof(methods) / sizeof(methods[0]), methods);
    if (status != napi_ok)
        return NULL;

    return exports;
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, init);