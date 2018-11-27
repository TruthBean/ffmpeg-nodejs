#include "./ffmpeg_nodejs.h"

static Video2ImageStream vis;
static LinkedQueue *queue;
static sem_t *semaphore;

struct {
    napi_ref ref;
    napi_async_work work;
} async_work_info = {NULL, NULL};

typedef struct ReadImageBufferParams {
    int chose_frames;
    int type;
    int quality;
    napi_env env;
    napi_value thisArg;
    napi_value callback;
} ReadImageBufferParams;

int lock = 0;

ReadImageBufferParams params;

/**
 * 初始化摄像头
 * 连接摄像头地址，并获取视频流数据
 **/
napi_value handle_init_read_video_and_queue(napi_env env, napi_callback_info info) {
    napi_status status;
    napi_value result = NULL;

    // js 参数 转换成 napi_value
    size_t argc = 3;
    napi_value argv[3];
    napi_value *thisArg = NULL;
    void *data = NULL;
    status = napi_get_cb_info(env, info, &argc, argv, thisArg, &data);

    if (status != napi_ok) {
        napi_throw_error(env, NULL, "Failed to parse arguments");
    }

    // 获取视频地址，将 napi_value 转换成 c语言 对应的格式
    // input filename
    size_t length;
    // 获取string长度
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

    // nobuffer
    bool nobuffer;
    status = napi_get_value_bool(env, argv[1], &nobuffer);
    av_log(NULL, AV_LOG_DEBUG, "nobuffer : %d\n", nobuffer);

    // useGpu
    bool use_gpu;
    status = napi_get_value_bool(env, argv[2], &use_gpu);
    av_log(NULL, AV_LOG_DEBUG, "useGpu : %d\n", use_gpu);

    // 构建 js promise 对象
    napi_value promise;
    napi_deferred deferred;
    status = napi_create_promise(env, &deferred, &promise);
    if (status != napi_ok) {
        napi_throw_error(env, NULL, "create promise error");
    }

    // 获取视频流数据，将其存在全局变量中
    vis = open_inputfile(input_filename, nobuffer, use_gpu);
    // 创建队列
    const size_t size = 4;
    queue = create_linkedQueue(queue, size);
    av_log(NULL, AV_LOG_DEBUG, "....... queue size: %d\n", queue->size);

    // 创建线程通知条件
    static sem_t _semaphore;
    sem_init(&_semaphore, 0, 0);
    semaphore = &_semaphore;

    if (vis.ret < 0) {
        // 处理错误信息
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

/**
 * 视频流取图片数据
 **/
napi_value handle_video_to_image_buffer(napi_env env, napi_callback_info info) {
    napi_status status;

    size_t argc = 3;
    napi_value argv[3];
    napi_value *thisArg = NULL;
    void *data = NULL;
    status = napi_get_cb_info(env, info, &argc, argv, thisArg, &data);

    if (status != napi_ok) {
        napi_throw_error(env, NULL, "Failed to parse arguments");
    }

    // int chose_frames
    int chose_frames;
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
    av_log(NULL, AV_LOG_DEBUG, "input type: %d\n", type);

    // jpeg quality
    int quality = 80;
    status = napi_get_value_int32(env, argv[2], &quality);
    if (status != napi_ok) {
        napi_throw_error(env, NULL, "jpeg quality is invalid number");
    }
    av_log(NULL, AV_LOG_DEBUG, "input jpeg quality: %d\n", quality);

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
    // 判断有没有图片数据，如网络中断或其他原因，导致无法获取到图片数据
    if (frameData.ret == 0 && frameData.file_size > 0) {
        isNil = 0;

        // char数组 转换成 javascript buffer
        void *buffer_data;
        status = napi_create_buffer_copy(env, frameData.file_size, (const void*)frameData.file_data, &buffer_data, &buffer_pointer);
        av_log(NULL, AV_LOG_DEBUG, "frameData.file_size : %ld\n", frameData.file_size);
        av_log(NULL, AV_LOG_DEBUG, "napi_create_buffer_copy result %d\n", status);
    }

    // 释放 图片数据 的 内存
    av_freep(&frameData.file_data);
    free(frameData.file_data);
    frameData.file_data = NULL;

    // 返回Promise
    napi_value promise;
    napi_deferred deferred;
    status = napi_create_promise(env, &deferred, &promise);
    if (status != napi_ok) {
        napi_throw_error(env, NULL, "create promise error");
    }

    av_log(NULL, AV_LOG_DEBUG, "isNil %d\n", isNil);

    if (isNil == 1) {
        // 返回错误原因
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
        // return image buffer
        status = napi_resolve_deferred(env, deferred, buffer_pointer);
        if (status != napi_ok) {
            napi_throw_error(env, NULL, "promise resolve error");
        }
    }

    return promise;
}

static void callback_nothing(napi_env env, void *data) {
    // DO NOTHING
}

static void callback_completion(napi_env env, napi_status status, void *data) {
    av_log(NULL, AV_LOG_DEBUG, "callback_completion\n");

    FrameData frame_data = video2images_stream(vis, params.quality, params.chose_frames, params.type);
    av_log(NULL, AV_LOG_DEBUG, "quality: %d  chose_frames: %d type: %d \n", params.quality, params.chose_frames, params.type);
    // LinkedQueueNode *node = pop_linkedQueue(queue);
    // av_log(NULL, AV_LOG_DEBUG, "node ptr : %p\n", node);
    // 判断有没有图片数据，如网络中断或其他原因，导致无法获取到图片数据
    // while (frame_data.ret == 0 && frame_data.file_size > 0) {
        av_log(NULL, AV_LOG_DEBUG, "reading ...........\n");

        napi_value cb, js_status;
        napi_get_reference_value(env, async_work_info.ref, &cb);
        napi_create_uint32(env, (uint32_t) status, &js_status);

        av_log(NULL, AV_LOG_DEBUG, "frame_data.file_size %d \n", frame_data.file_size);
        av_log(NULL, AV_LOG_DEBUG, "frame_data.file_data %p \n", frame_data.file_data);

        if (params.env == NULL) {
            av_log(NULL, AV_LOG_ERROR, "params.env == NULL \n");
        }

        napi_value buffer_pointer;
        // char数组 转换成 javascript buffer
        void *buffer_data;
        status = napi_create_buffer_copy(params.env, frame_data.file_size, (const void*)frame_data.file_data, &buffer_data, &buffer_pointer);
        av_log(NULL, AV_LOG_DEBUG, "frameData.file_size : %ld\n", frame_data.file_size);
        av_log(NULL, AV_LOG_DEBUG, "napi_create_buffer_copy result %d\n", status);

        if (params.thisArg == NULL) {
            av_log(NULL, AV_LOG_ERROR, "params.thisArg == NULL \n");
        }

        if (params.callback == NULL) {
            av_log(NULL, AV_LOG_ERROR, "params.callback == NULL \n");
        }

        // 返回错误原因
        char * _err_msg = frame_data.error_message;
        if (frame_data.ret == 0) {
            _err_msg = "image data is null, network may connect wrong";
        }

        napi_value message;
        status = napi_create_string_utf8(env, _err_msg, NAPI_AUTO_LENGTH, &message);
        if (status != napi_ok) {
            napi_throw_error(env, NULL, "error message create error");
        }

        napi_value obj;
        status = napi_create_object(env, &obj);
        if (status != napi_ok) {
            napi_throw_error(env, NULL, "callback object create error");
        }

        napi_value error_name;
        napi_create_string_utf8(env, "error", NAPI_AUTO_LENGTH, &error_name);
        napi_value data_name;
        napi_create_string_utf8(env, "data", NAPI_AUTO_LENGTH, &data_name);

        napi_set_property(env, obj, error_name, message);
        napi_set_property(env, obj, data_name, buffer_pointer);


        // 调用 callback
        napi_value result;
        status = napi_call_function(params.env, cb, cb, 1, &obj, &result);
        av_log(NULL, AV_LOG_DEBUG, "napi_call_function status %d\n", status);

        if (status != napi_ok) {
            napi_throw_error(params.env, NULL, "Create buffer error");
        }

        // 释放 图片数据 的 内存
        av_freep(&frame_data.file_data);
        free(frame_data.file_data);
        frame_data.file_data = NULL;

    //    frame_data = video2images_stream(vis, params.quality, params.chose_frames, params.type);
    // }
}

napi_value handle_video_2_image_stream(napi_env env, napi_callback_info info) {
    napi_status status;

    size_t argc = 4;
    napi_value argv[4];
    napi_value thisArg;
    void *data = NULL;
    status = napi_get_cb_info(env, info, &argc, argv, &thisArg, &data);

    if (status != napi_ok) {
        napi_throw_error(env, NULL, "Failed to parse arguments");
    }

    // int type
    int type;
    status = napi_get_value_int32(env, argv[0], &type);
    if (status != napi_ok) {
        napi_throw_error(env, NULL, "type is invalid number");
    }
    av_log(NULL, AV_LOG_DEBUG, "input type: %d\n", type);

    // jpeg quality
    int quality = 80;
    status = napi_get_value_int32(env, argv[1], &quality);
    if (status != napi_ok) {
        napi_throw_error(env, NULL, "jpeg quality is invalid number");
    }
    av_log(NULL, AV_LOG_DEBUG, "input jpeg quality: %d\n", quality);

    // int chose_frames
    int chose_frames;
    status = napi_get_value_int32(env, argv[2], &chose_frames);
    if (status != napi_ok) {
        napi_throw_error(env, NULL, "chose_frames is invalid number");
    }
    av_log(NULL, AV_LOG_DEBUG, "input chose_frames: %d\n", chose_frames);

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

    av_log(NULL, AV_LOG_DEBUG, "queue size : %d\n", queue->size);

    status = napi_create_reference(env, callback, 1, &(async_work_info.ref));
    av_log(NULL, AV_LOG_DEBUG, "napi_create_reference %d \n", status);
    napi_value resource_name;
    status = napi_create_string_utf8(env, "callback", NAPI_AUTO_LENGTH, &resource_name);
    av_log(NULL, AV_LOG_DEBUG, "napi_create_string_utf8 %d\n", status);

    ReadImageBufferParams _params = {
        .chose_frames = chose_frames,
        .callback = callback,
        .env = env,
        .quality = quality,
        .thisArg = &thisArg,
        .type = type
    };

    params = _params;

    napi_value async_resource = NULL;
    status = napi_create_async_work(env, async_resource, resource_name, callback_nothing, callback_completion,
                           &params, &(async_work_info.work));
    av_log(NULL, AV_LOG_DEBUG, "napi_create_async_work %d\n", status);

    napi_queue_async_work(env, async_work_info.work);
}

/**
 * 释放视频连接
 **/
napi_value handle_destroy_stream_and_queue(napi_env env, napi_callback_info info) {
    release(vis.video_codec_context, vis.format_context, vis.isRtsp);
    Video2ImageStream _vis = {
        .format_context = NULL,
        .video_stream_idx = -1,
        .video_codec_context = NULL,
        .ret = -1
        };

    vis = _vis;

    if (queue != NULL)
        destroy_linkedQueue(queue);

    return NULL;
}

/**
 * rtsp视频录频
 **/
void handle_record_video(napi_env env, napi_callback_info info) {
    size_t argc = 4;
    napi_value argv[4];
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
    int record_seconds;
    status = napi_get_value_int32(env, argv[2], &record_seconds);
    if (status != napi_ok) {
        napi_throw_error(env, NULL, "Invalid number was passed as argument");
    }
    av_log(NULL, AV_LOG_DEBUG, "record_seconds : %d\n", record_seconds);

    // use gpu
    bool use_gpu;
    status = napi_get_value_bool(env, argv[3], &use_gpu);
    if (status != napi_ok) {
        napi_throw_error(env, NULL, "Invalid bool was passed as argument");
    }
    av_log(NULL, AV_LOG_DEBUG, "use_gpu : %d\n", use_gpu);

    int result = record_rtsp(input_filename, output_filename, record_seconds, use_gpu);
    av_log(NULL, AV_LOG_DEBUG, "record rtsp result : %d\n", result);
}

napi_value ffmpeg_nodejs_class_constructor(napi_env env, napi_callback_info info) {
    napi_value result;
    return result;
}

/**
 * napi 构建js的方法
 **/
napi_value init(napi_env env, napi_value exports) {
    napi_value result;
    napi_status status;
    av_log_set_level(AV_LOG_INFO);

    napi_property_descriptor methods[] = {
        DECLARE_NAPI_METHOD("initReadingVideoAndQueue", handle_init_read_video_and_queue),
        DECLARE_NAPI_METHOD("destroyStreamAndQueue", handle_destroy_stream_and_queue),

        DECLARE_NAPI_METHOD("video2ImageStream", handle_video_2_image_stream),

        DECLARE_NAPI_METHOD("video2ImageBuffer", handle_video_to_image_buffer),
        DECLARE_NAPI_METHOD("recordVideo", handle_record_video),
        };

    size_t property_count = sizeof(methods) / sizeof(methods[0]);
    // status = napi_define_class(env, "FFmpegNodejs", NAPI_AUTO_LENGTH, ffmpeg_nodejs_class_constructor,
    //                             NULL, property_count, methods, &result);
    status = napi_define_properties(env, exports, property_count, methods);
    result = exports;
    if (status != napi_ok)
        return NULL;

    return result;
}

// 初始化 js 模块
NAPI_MODULE(NODE_GYP_MODULE_NAME, init);
