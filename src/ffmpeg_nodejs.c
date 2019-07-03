#include "./ffmpeg_nodejs.h"

static Video2ImageStream vis;

struct
{
    napi_ref ref;
    napi_async_work work;
    napi_threadsafe_function func;
} async_work_info = {NULL, NULL};

typedef struct ReadImageBufferParams
{
    int chose_frames;
    int type;
    int quality;
} ReadImageBufferParams;

static ReadImageBufferParams params;
static bool thread = false;

/**
 * 初始化摄像头
 * 连接摄像头地址，并获取视频流数据
 **/
static napi_value handle_init_read_video(napi_env env, napi_callback_info info)
{
    napi_status status;
    napi_value result = NULL;

    // js 参数 转换成 napi_value
    size_t argc = 6;
    napi_value argv[6];
    napi_value *thisArg = NULL;
    void *data = NULL;
    status = napi_get_cb_info(env, info, &argc, argv, thisArg, &data);

    if (status != napi_ok)
    {
        napi_throw_error(env, NULL, "Failed to parse arguments");
    }

    // log level
    int log_level;
    status = napi_get_value_int32(env, argv[4], &log_level);
    av_log(NULL, AV_LOG_DEBUG, "log level %d \n", log_level);
    switch (log_level)
    {
    case 1:
        av_log_set_level(AV_LOG_ERROR);
        break;
    case 2:
        av_log_set_level(AV_LOG_DEBUG);
        break;
    case 3:
    default:
        av_log_set_level(AV_LOG_INFO);
        break;
    }

    // 获取视频地址，将 napi_value 转换成 c语言 对应的格式
    // input filename
    size_t length;
    // 获取string长度
    status = napi_get_value_string_utf8(env, argv[0], NULL, 0, &length);
    if (status != napi_ok)
    {
        napi_throw_error(env, NULL, "Invalid number was passed as argument");
    }

    // put param value into char array
    size_t input_filename_res;
    const size_t input_filename_len = length + 1;
    char input_filename[input_filename_len];
    status = napi_get_value_string_utf8(env, argv[0], input_filename, length + 1, &input_filename_res);

    if (status != napi_ok)
    {
        napi_throw_error(env, NULL, "Invalid value was passed as argument by input_filename");
    }
    av_log(NULL, AV_LOG_DEBUG, "input filename : %s\n", input_filename);

    // timeout
    int64_t timeout;
    status = napi_get_value_int64(env, argv[1], &timeout);
    av_log(NULL, AV_LOG_DEBUG, "timeout : %d\n", timeout);

    // nobuffer
    bool nobuffer;
    status = napi_get_value_bool(env, argv[2], &nobuffer);
    av_log(NULL, AV_LOG_DEBUG, "nobuffer : %d\n", nobuffer);

    // useGpu
    bool use_gpu;
    status = napi_get_value_bool(env, argv[3], &use_gpu);
    av_log(NULL, AV_LOG_DEBUG, "useGpu : %d\n", use_gpu);

    // gpu_id
    // 获取string长度
    status = napi_get_value_string_utf8(env, argv[5], NULL, 0, &length);
    if (status != napi_ok)
    {
        napi_throw_error(env, NULL, "Invalid number was passed as argument");
    }

    // put param value into char array
    size_t gpu_id_res;
    const size_t gpu_id_len = length + 1;
    char gpu_id[gpu_id_len];
    status = napi_get_value_string_utf8(env, argv[5], gpu_id, length + 1, &gpu_id_res);

    if (status != napi_ok)
    {
        napi_throw_error(env, NULL, "Invalid value was passed as argument by gpu_id");
    }
    av_log(NULL, AV_LOG_DEBUG, "input gpu_id : %s\n", gpu_id);

    // 构建 js promise 对象
    napi_value promise;
    napi_deferred deferred;
    status = napi_create_promise(env, &deferred, &promise);
    if (status != napi_ok)
    {
        napi_throw_error(env, NULL, "create promise error");
    }

    // 获取视频流数据，将其存在全局变量中
    vis = open_inputfile(input_filename, nobuffer, timeout, use_gpu, true, gpu_id);

    if (vis.ret < 0)
    {
        // 处理错误信息
        napi_value message;
        status = napi_create_string_utf8(env, vis.error_message, NAPI_AUTO_LENGTH, &message);
        if (status != napi_ok)
        {
            napi_throw_error(env, NULL, "error message create error");
        }

        status = napi_reject_deferred(env, deferred, message);
        if (status != napi_ok)
        {
            napi_throw_error(env, NULL, "promise reject error");
        }
    }
    else
    {
        av_log(NULL, AV_LOG_DEBUG, "---> 1 ---> promise resolve\n");
        napi_create_double(env, true, &result);
        status = napi_resolve_deferred(env, deferred, result);
        if (status != napi_ok)
        {
            napi_throw_error(env, NULL, "promise resolve error");
        }
    }

    return promise;
}

static void handle_frame_data(FrameData *data)
{
    if (data->ret == 0 && data->frame != NULL)
    {
        if (data->type == JPEG)
            copy_frame_data_and_transform_2_jpeg(vis.video_codec_context, data);
        else
            copy_frame_raw_data(vis.video_codec_context, data);

        av_log(NULL, AV_LOG_DEBUG, "file_size: %ld\n", data->file_size);
        if (data->file_size == 0 || data->file_data == NULL)
        {
            av_log(NULL, AV_LOG_DEBUG, "file_data NULL\n");
        }
        else
        {
            if (data->ret < 0)
            {
                data->ret = -40;
                data->error_message = "image data is null";
            }
        }
    }
}

/**
 * 视频流取图片数据
 **/
static napi_value handle_video_to_image_buffer(napi_env env, napi_callback_info info)
{
    napi_status status;

    size_t argc = 3;
    napi_value argv[3];
    napi_value *thisArg = NULL;
    void *data = NULL;
    status = napi_get_cb_info(env, info, &argc, argv, thisArg, &data);

    if (status != napi_ok)
    {
        napi_throw_error(env, NULL, "Failed to parse arguments");
    }

    // int chose_frames
    int chose_frames;
    status = napi_get_value_int32(env, argv[0], &chose_frames);
    if (status != napi_ok)
    {
        napi_throw_error(env, NULL, "chose_frames is invalid number");
    }
    av_log(NULL, AV_LOG_DEBUG, "input chose_frames: %d\n", chose_frames);

    // int type
    int type;
    status = napi_get_value_int32(env, argv[1], &type);
    if (status != napi_ok)
    {
        napi_throw_error(env, NULL, "type is invalid number");
    }
    av_log(NULL, AV_LOG_DEBUG, "input type: %d\n", type);

    // jpeg quality
    int quality = 80;
    status = napi_get_value_int32(env, argv[2], &quality);
    if (status != napi_ok)
    {
        napi_throw_error(env, NULL, "jpeg quality is invalid number");
    }
    av_log(NULL, AV_LOG_DEBUG, "input jpeg quality: %d\n", quality);

    // 处理图片
    if (vis.ret < 0)
    {
        napi_throw_error(env, NULL, "initReadingVideo method should be invoke first");
    }

    FrameData frameData = {
        .pts = 0,
        .frame = NULL,
        .ret = 0};
    video2images_grab(vis, quality, chose_frames, type, handle_frame_data, &frameData);

    if (status != napi_ok)
    {
        napi_throw_error(env, NULL, "Create buffer error");
    }

    av_log(NULL, AV_LOG_DEBUG, "begin napi_create_buffer_copy \n");
    napi_value buffer_pointer = NULL;
    int isNil = 1;
    // 判断有没有图片数据，如网络中断或其他原因，导致无法获取到图片数据
    if (frameData.ret == 0 && frameData.file_size > 0)
    {
        isNil = 0;

        // char数组 转换成 javascript buffer
        void *buffer_data;
        status = napi_create_buffer_copy(env, frameData.file_size, (const void *)frameData.file_data, &buffer_data, &buffer_pointer);
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
    if (status != napi_ok)
    {
        napi_throw_error(env, NULL, "create promise error");
    }

    av_log(NULL, AV_LOG_DEBUG, "isNil %d\n", isNil);

    if (isNil == 1)
    {
        // 返回错误原因
        char *_err_msg = frameData.error_message;
        if (frameData.ret == 0 && frameData.file_size == 0)
        {
            _err_msg = "image data is null, network may connect wrong";
        }

        napi_value message;
        status = napi_create_string_utf8(env, _err_msg, NAPI_AUTO_LENGTH, &message);
        if (status != napi_ok)
        {
            napi_throw_error(env, NULL, "error message create error");
        }

        status = napi_reject_deferred(env, deferred, message);
        if (status != napi_ok)
        {
            napi_throw_error(env, NULL, "promise reject error");
        }
    }
    else
    {
        // return image buffer
        status = napi_resolve_deferred(env, deferred, buffer_pointer);
        if (status != napi_ok)
        {
            napi_throw_error(env, NULL, "promise resolve error");
        }
    }

    return promise;
}

// =====================================================================================================================
static void callback_nothing(napi_env env, void *data)
{
    // DO NOTHING
}

static void callback_completion(napi_env env, napi_status status, void *data)
{
    time_t begin = get_now_microseconds();
    av_log(NULL, AV_LOG_DEBUG, "callback_completion\n");

    FrameData frameData = {
        .pts = 0,
        .frame = NULL,
        .ret = 0,
        .isThreadly = false};
    video2images_grab(vis, params.quality, params.chose_frames, params.type, handle_frame_data, &frameData);
    av_log(NULL, AV_LOG_DEBUG, "quality: %d  chose_frames: %d type: %d \n", params.quality, params.chose_frames, params.type);

    av_log(NULL, AV_LOG_DEBUG, "reading ...........\n");

    napi_value cb, js_status;
    napi_get_reference_value(env, async_work_info.ref, &cb);
    napi_create_uint32(env, (uint32_t)status, &js_status);

    av_log(NULL, AV_LOG_DEBUG, "frameData.file_size %ld \n", frameData.file_size);
    av_log(NULL, AV_LOG_DEBUG, "frameData.file_data %p \n", frameData.file_data);

    napi_value buffer_pointer;
    // char数组 转换成 javascript buffer
    void *buffer_data;
    status = napi_create_buffer_copy(env, frameData.file_size, (const void *)frameData.file_data, &buffer_data, &buffer_pointer);
    av_log(NULL, AV_LOG_DEBUG, "frameData.file_size : %ld\n", frameData.file_size);
    av_log(NULL, AV_LOG_DEBUG, "napi_create_buffer_copy result %d\n", status);

    // 释放 图片数据 的 内存
    av_freep(&frameData.file_data);
    free(frameData.file_data);
    frameData.file_data = NULL;

    // 返回错误原因
    napi_value message;
    status = napi_get_undefined(env, &message);

    if (frameData.file_size == 0)
    {
        char *_err_msg;
        // 判断有没有图片数据，如网络中断或其他原因，导致无法获取到图片数据
        if (frameData.ret == 0)
        {
            _err_msg = "image data is null, network may connect wrong";
        }
        else
        {
            _err_msg = frameData.error_message;
        }
        av_log(NULL, AV_LOG_DEBUG, "%s \n", _err_msg);

        status = napi_create_string_utf8(env, _err_msg, NAPI_AUTO_LENGTH, &message);
        if (status != napi_ok)
        {
            napi_throw_error(env, NULL, "error message create error");
        }
    }

    napi_value obj;
    status = napi_create_object(env, &obj);
    if (status != napi_ok)
    {
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
    time_t begin1 = get_now_microseconds();
    av_log(NULL, AV_LOG_DEBUG, "get buffer time: %ld \n", (begin1 - begin));
    status = napi_call_function(env, cb, cb, 1, &obj, &result);
    av_log(NULL, AV_LOG_DEBUG, "napi_call_function status %d\n", status);

    if (status != napi_ok)
    {
        napi_throw_error(env, NULL, "Create buffer error");
    }
}

static napi_value handle_video_to_image_stream(napi_env env, napi_callback_info info)
{
    napi_status status;

    size_t argc = 4;
    napi_value argv[4];
    napi_value thisArg;
    void *data = NULL;
    status = napi_get_cb_info(env, info, &argc, argv, &thisArg, &data);

    if (status != napi_ok)
    {
        napi_throw_error(env, NULL, "Failed to parse arguments");
    }

    // int type
    int type;
    status = napi_get_value_int32(env, argv[0], &type);
    if (status != napi_ok)
    {
        napi_throw_error(env, NULL, "type is invalid number");
    }
    av_log(NULL, AV_LOG_DEBUG, "input type: %d\n", type);

    // jpeg quality
    int quality = 80;
    status = napi_get_value_int32(env, argv[1], &quality);
    if (status != napi_ok)
    {
        napi_throw_error(env, NULL, "jpeg quality is invalid number");
    }
    av_log(NULL, AV_LOG_DEBUG, "input jpeg quality: %d\n", quality);

    // int chose_frames
    int chose_frames;
    status = napi_get_value_int32(env, argv[2], &chose_frames);
    if (status != napi_ok)
    {
        napi_throw_error(env, NULL, "chose_frames is invalid number");
    }
    av_log(NULL, AV_LOG_DEBUG, "input chose_frames: %d\n", chose_frames);

    // callback
    napi_value callback = argv[3];
    napi_valuetype argv2_type;
    status = napi_typeof(env, callback, &argv2_type);
    if (status != napi_ok)
    {
        napi_throw_error(env, NULL, "wrong javascript type");
    }
    if (argv2_type != napi_function)
    {
        napi_throw_error(env, NULL, "param is not a function");
    }

    // 处理图片
    if (vis.ret < 0)
    {
        napi_throw_error(env, NULL, "initReadingVideo method should be invoke first");
    }

    status = napi_create_reference(env, callback, 1, &(async_work_info.ref));
    av_log(NULL, AV_LOG_DEBUG, "napi_create_reference %d \n", status);
    napi_value resource_name;
    status = napi_create_string_utf8(env, "callback", NAPI_AUTO_LENGTH, &resource_name);
    av_log(NULL, AV_LOG_DEBUG, "napi_create_string_utf8 %d\n", status);

    ReadImageBufferParams _params = {
        .chose_frames = chose_frames,
        .quality = quality,
        .type = type};

    params = _params;

    napi_value async_resource = NULL;
    status = napi_create_async_work(env, async_resource, resource_name, callback_nothing, callback_completion,
                                    &params, &(async_work_info.work));
    av_log(NULL, AV_LOG_DEBUG, "napi_create_async_work %d\n", status);

    napi_queue_async_work(env, async_work_info.work);
}

static void *consumer_callback_threadsafe(napi_env env, napi_value js_callback, void *context, void *data)
{
    napi_status status;

    av_log(NULL, AV_LOG_DEBUG, "callback_completion\n");
    napi_value cb, js_status;
    status = napi_get_reference_value(env, async_work_info.ref, &cb);
    av_log(NULL, AV_LOG_DEBUG, "napi_get_reference_value status %d \n", status);

    status = napi_create_uint32(env, (uint32_t)status, &js_status);
    av_log(NULL, AV_LOG_DEBUG, "napi_create_uint32 status %d \n", status);

    if (data != NULL)
    {
        FrameData *frame_data = (FrameData *)data;
        av_log(NULL, AV_LOG_DEBUG, "node1 %ld \n", frame_data->pts);

        av_log(NULL, AV_LOG_DEBUG, "params.type .......>>>>>>> %d \n", params.type);

        if (params.type == JPEG)
            copy_frame_data_and_transform_2_jpeg(vis.video_codec_context, frame_data);
        else
            copy_frame_raw_data(vis.video_codec_context, params.type);
        av_log(NULL, AV_LOG_DEBUG, "file_size: %ld\n", frame_data->file_size);

        napi_value buffer_pointer;
        // char数组 转换成 javascript buffer
        void *buffer_data;
        status = napi_create_buffer_copy(env, frame_data->file_size, (const void *)frame_data->file_data, &buffer_data, &buffer_pointer);
        av_log(NULL, AV_LOG_DEBUG, "frameData.file_size : %ld\n", frame_data->file_size);
        av_log(NULL, AV_LOG_DEBUG, "napi_create_buffer_copy result %d\n", status);

        // 返回错误原因
        napi_value message;
        status = napi_get_undefined(env, &message);

        if (frame_data->file_size == 0)
        {
            char *_err_msg;
            // 判断有没有图片数据，如网络中断或其他原因，导致无法获取到图片数据
            if (frame_data->ret == 0)
            {
                _err_msg = "image data is null, network may connect wrong";
            }
            else
            {
                _err_msg = frame_data->error_message;
            }
            av_log(NULL, AV_LOG_DEBUG, "%s \n", _err_msg);

            status = napi_create_string_utf8(env, _err_msg, NAPI_AUTO_LENGTH, &message);
            if (status != napi_ok)
            {
                napi_throw_error(env, NULL, "error message create error");
            }
        }

        napi_value obj;
        status = napi_create_object(env, &obj);
        if (status != napi_ok)
        {
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
        status = napi_call_function(env, cb, cb, 1, &obj, &result);
        av_log(NULL, AV_LOG_DEBUG, "napi_call_function status %d\n", status);

        if (status != napi_ok)
        {
            napi_throw_error(env, NULL, "call function error");
        }

        // 释放 图片数据 的 内存
        av_freep(&frame_data->file_data);
        free(frame_data->file_data);
        frame_data->file_data = NULL;
    }
    else
    {
        napi_value obj;
        status = napi_create_object(env, &obj);
        if (status != napi_ok)
        {
            napi_throw_error(env, NULL, "callback object create error");
        }

        napi_value error_name;
        napi_create_string_utf8(env, "error", NAPI_AUTO_LENGTH, &error_name);
        napi_value data_name;
        napi_create_string_utf8(env, "data", NAPI_AUTO_LENGTH, &data_name);

        napi_value message;
        status = napi_create_string_utf8(env, "call threadsafe function error", NAPI_AUTO_LENGTH, &message);
        napi_set_property(env, obj, error_name, message);

        napi_value buffer_pointer;
        status = napi_get_undefined(env, &buffer_pointer);
        napi_set_property(env, obj, data_name, buffer_pointer);

        // 调用 callback
        napi_value result;
        status = napi_call_function(env, cb, cb, 1, &obj, &result);
        av_log(NULL, AV_LOG_DEBUG, "napi_call_function status %d\n", status);

        if (status != napi_ok)
        {
            napi_throw_error(env, NULL, "call function error");
        }
    }
}

static void handle_frame_data_threadly(FrameData *frameData)
{
    napi_status status = napi_call_threadsafe_function(async_work_info.func, frameData, napi_tsfn_nonblocking);
    av_log(NULL, AV_LOG_DEBUG, "napi_call_threadsafe_function return %d \n", status);
}

static void *callback_thread(napi_env env, void *data)
{
    napi_acquire_threadsafe_function(async_work_info.func);
    FrameData frameData = {
        .pts = 0,
        .frame = NULL,
        .ret = 0,
        .isThreadly = true};
    video2images_grab(vis, params.quality, params.chose_frames, params.type, handle_frame_data_threadly, &frameData);
    av_log(NULL, AV_LOG_ERROR, "callback_thread return error \n");
    if (frameData.frame != NULL)
    {
        frameData.frame = NULL;
    }
    else if (frameData.file_data != NULL)
    {
        av_freep(&frameData.file_data);
        free(frameData.file_data);
        frameData.file_data = NULL;
    }

    napi_status status = napi_call_threadsafe_function(async_work_info.func, NULL, napi_tsfn_nonblocking);
    av_log(NULL, AV_LOG_DEBUG, "napi_call_threadsafe_function status %d \n", status);
}

void *finalize(napi_env env, void *data, void *hint)
{
    av_log(NULL, AV_LOG_DEBUG, "finalize consumer ........ \n");

    if (async_work_info.work != NULL && async_work_info.ref != NULL)
    {
        napi_status status = napi_unref_threadsafe_function(env, async_work_info.func);
        async_work_info.func = NULL;
        av_log(NULL, AV_LOG_DEBUG, "napi_unref_threadsafe_function status : %d \n", status);
        status = napi_delete_async_work(env, async_work_info.work);
        av_log(NULL, AV_LOG_DEBUG, "napi_delete_async_work status : %d \n", status);

        status = napi_delete_reference(env, async_work_info.ref);
        av_log(NULL, AV_LOG_DEBUG, "napi_delete_reference status : %d \n", status);
        async_work_info.work = NULL;
        async_work_info.ref = NULL;
    }

    return NULL;
}

napi_value handle_video_to_image_stream_threadly(napi_env env, napi_callback_info info)
{
    napi_status status;

    size_t argc = 4;
    napi_value argv[4];
    napi_value thisArg;
    void *data = NULL;
    status = napi_get_cb_info(env, info, &argc, argv, &thisArg, &data);

    if (status != napi_ok)
    {
        napi_throw_error(env, NULL, "Failed to parse arguments");
    }

    // int type
    int type;
    status = napi_get_value_int32(env, argv[0], &type);
    if (status != napi_ok)
    {
        napi_throw_error(env, NULL, "type is invalid number");
    }
    av_log(NULL, AV_LOG_DEBUG, "input type: %d\n", type);

    // jpeg quality
    int quality = 80;
    status = napi_get_value_int32(env, argv[1], &quality);
    if (status != napi_ok)
    {
        napi_throw_error(env, NULL, "jpeg quality is invalid number");
    }
    av_log(NULL, AV_LOG_DEBUG, "input jpeg quality: %d\n", quality);

    // int chose_frames
    int chose_frames;
    status = napi_get_value_int32(env, argv[2], &chose_frames);
    if (status != napi_ok)
    {
        napi_throw_error(env, NULL, "chose_frames is invalid number");
    }
    av_log(NULL, AV_LOG_DEBUG, "input chose_frames: %d\n", chose_frames);

    // callback
    napi_value callback = argv[3];
    napi_valuetype argv2_type;
    status = napi_typeof(env, callback, &argv2_type);
    if (status != napi_ok)
    {
        napi_throw_error(env, NULL, "wrong javascript type");
    }
    if (argv2_type != napi_function)
    {
        napi_throw_error(env, NULL, "param is not a function");
    }

    // 处理图片
    if (vis.ret < 0)
    {
        napi_throw_error(env, NULL, "initReadingVideo method should be invoke first");
    }

    status = napi_create_reference(env, callback, 1, &(async_work_info.ref));
    av_log(NULL, AV_LOG_DEBUG, "napi_create_reference %d \n", status);
    napi_value resource_name;
    status = napi_create_string_utf8(env, "callback", NAPI_AUTO_LENGTH, &resource_name);
    av_log(NULL, AV_LOG_DEBUG, "napi_create_string_utf8 %d\n", status);

    ReadImageBufferParams _params = {
        .chose_frames = chose_frames,
        .quality = quality,
        .type = type};

    params = _params;

    napi_threadsafe_function result;
    napi_create_threadsafe_function(env, callback, NULL, resource_name, 10, 1, NULL, finalize, &thisArg, consumer_callback_threadsafe, &result);
    napi_ref_threadsafe_function(env, result);

    async_work_info.func = result;
    thread = true;

    napi_value async_resource = NULL;
    status = napi_create_async_work(env, async_resource, resource_name, callback_thread, callback_nothing,
                                    &params, &(async_work_info.work));
    av_log(NULL, AV_LOG_DEBUG, "napi_create_async_work %d\n", status);

    napi_queue_async_work(env, async_work_info.work);
}

/**
 * 释放视频连接
 **/
napi_value handle_destroy_stream(napi_env env, napi_callback_info info)
{
    av_log(NULL, AV_LOG_DEBUG, "release ..... \n");
    av_log(NULL, AV_LOG_DEBUG, "%p \n", async_work_info.func);
    napi_status status;

    for (int i = 0; i < 10; i++)
    {
        if (async_work_info.func != NULL)
        {
            av_log(NULL, AV_LOG_DEBUG, "release thread \n");
            status = napi_release_threadsafe_function(async_work_info.func, napi_tsfn_abort);
            av_log(NULL, AV_LOG_DEBUG, "napi_release_threadsafe_function status : %d \n", status);
            if (status != napi_ok)
                break;
        }
    }

    release(vis.video_codec_context, vis.format_context);
    Video2ImageStream _vis = {
        .format_context = NULL,
        .video_stream_idx = -1,
        .video_codec_context = NULL,
        .ret = -1};

    vis = _vis;

    return NULL;
}

/**
 * video视频录频
 **/
napi_value handle_record_video(napi_env env, napi_callback_info info)
{
    size_t argc = 6;
    napi_value argv[6];
    napi_value *thisArg = NULL;
    void *data = NULL;

    napi_status status = napi_get_cb_info(env, info, &argc, argv, thisArg, &data);
    if (status != napi_ok)
    {
        napi_throw_error(env, NULL, "Failed to parse arguments");
    }

    // input filename
    size_t length;
    status = napi_get_value_string_utf8(env, argv[0], NULL, 0, &length);
    if (status != napi_ok)
    {
        napi_throw_error(env, NULL, "Invalid number was passed as argument");
    }

    // log level
    int log_level;
    status = napi_get_value_int32(env, argv[4], &log_level);
    switch (log_level)
    {
    case 1:
        av_log_set_level(AV_LOG_ERROR);
        av_log(NULL, AV_LOG_INFO, "log set level with AV_LOG_ERROR \n");
        break;
    case 2:
        av_log_set_level(AV_LOG_DEBUG);
        av_log(NULL, AV_LOG_INFO, "log set level with AV_LOG_DEBUG \n");
        break;
    case 3:
    default:
        av_log_set_level(AV_LOG_INFO);
        av_log(NULL, AV_LOG_INFO, "log set level with AV_LOG_INFO \n");
        break;
    }

    // put param value into char array
    size_t input_filename_res;
    const size_t input_filename_len = length + 1;
    char input_filename[input_filename_len];
    status = napi_get_value_string_utf8(env, argv[0], input_filename, length + 1, &input_filename_res);

    if (status != napi_ok)
    {
        napi_throw_error(env, NULL, "Invalid value was passed as argument by input_filename");
    }
    av_log(NULL, AV_LOG_DEBUG, "input filename : %s\n", input_filename);

    // output filename
    length = 0;
    status = napi_get_value_string_utf8(env, argv[1], NULL, 0, &length);
    if (status != napi_ok)
    {
        napi_throw_error(env, NULL, "Invalid number was passed as argument");
    }

    // put param value into char array
    size_t output_filename_res;
    const size_t output_filename_len = length + 1;
    char output_filename[output_filename_len];
    status = napi_get_value_string_utf8(env, argv[1], output_filename, length + 1, &output_filename_res);

    if (status != napi_ok)
    {
        napi_throw_error(env, NULL, "Invalid value was passed as argument by output_filename");
    }
    av_log(NULL, AV_LOG_DEBUG, "output filename : %s\n", output_filename);

    // record seconds
    int record_seconds;
    status = napi_get_value_int32(env, argv[2], &record_seconds);
    if (status != napi_ok)
    {
        napi_throw_error(env, NULL, "Invalid number was passed as argument");
    }
    av_log(NULL, AV_LOG_DEBUG, "record_seconds : %d\n", record_seconds);

    // use gpu
    bool use_gpu;
    status = napi_get_value_bool(env, argv[3], &use_gpu);
    if (status != napi_ok)
    {
        napi_throw_error(env, NULL, "Invalid bool was passed as argument");
    }
    av_log(NULL, AV_LOG_DEBUG, "use_gpu : %d\n", use_gpu);

    // type
    int muxing_stream_type;
    status = napi_get_value_int32(env, argv[5], &muxing_stream_type);
    av_log(NULL, AV_LOG_DEBUG, "muxing_stream_type %d \n", muxing_stream_type);

    int result = record_video(input_filename, output_filename, muxing_stream_type, record_seconds, use_gpu);
    av_log(NULL, AV_LOG_DEBUG, "record video result : %d\n", result);

    return NULL;
}

static napi_value ffmpeg_nodejs_class_constructor(napi_env env, napi_callback_info info)
{
    napi_value result;
    return result;
}

/**
 * napi 构建js的方法
 **/
static napi_value init(napi_env env, napi_value exports)
{
    napi_status status;

    napi_property_descriptor methods[] = {
        DECLARE_NAPI_METHOD("initReadingVideo", handle_init_read_video),
        DECLARE_NAPI_METHOD("video2ImageBuffer", handle_video_to_image_buffer),
        DECLARE_NAPI_METHOD("video2ImageStream", handle_video_to_image_stream),
        DECLARE_NAPI_METHOD("video2ImageStreamThreadly", handle_video_to_image_stream_threadly),
        DECLARE_NAPI_METHOD("destroyStream", handle_destroy_stream),
        DECLARE_NAPI_METHOD("recordVideo", handle_record_video)};

    status = napi_define_properties(env, exports, sizeof(methods) / sizeof(methods[0]), methods);
    if (status != napi_ok)
        return NULL;

    return exports;
}

// 初始化 js 模块
NAPI_MODULE(NODE_GYP_MODULE_NAME, init);
