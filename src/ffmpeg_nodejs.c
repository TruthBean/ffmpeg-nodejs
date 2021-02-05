#include "./ffmpeg_nodejs.h"

/**
 * 初始化摄像头
 * 连接摄像头地址，并获取视频流数据
 **/
static napi_value handle_init_read_video(napi_env env, napi_callback_info info)
{
    napi_status status;
    napi_value result = NULL;

    // js 参数 转换成 napi_value
    size_t argc = 7;
    napi_value argv[7];
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
#ifdef _WIN32
    char *input_filename;
#else
    char input_filename[input_filename_len];
#endif // _WIN32

    status = napi_get_value_string_utf8(env, argv[0], input_filename, length + 1, &input_filename_res);

    if (status != napi_ok)
    {
        napi_throw_error(env, NULL, "Invalid value was passed as argument by input_filename");
    }
    av_log(NULL, AV_LOG_DEBUG, "input filename : %s\n", input_filename);

    // timeout
    int64_t timeout;
    status = napi_get_value_int64(env, argv[1], &timeout);
    av_log(NULL, AV_LOG_DEBUG, "timeout : %ld\n", timeout);

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
#ifdef _WIN32
    char *gpu_id;
#else
    char gpu_id[gpu_id_len];
#endif // _WIN32
    status = napi_get_value_string_utf8(env, argv[5], gpu_id, length + 1, &gpu_id_res);

    if (status != napi_ok)
    {
        napi_throw_error(env, NULL, "Invalid value was passed as argument by gpu_id");
    }
    av_log(NULL, AV_LOG_DEBUG, "input gpu_id : %s\n", gpu_id);

    // useTcp
    bool use_tcp;
    status = napi_get_value_bool(env, argv[6], &use_tcp);
    av_log(NULL, AV_LOG_DEBUG, "use_tcp : %d\n", use_tcp);

    // 构建 js promise 对象
    napi_value promise = NULL;
    napi_deferred deferred = NULL;
    status = napi_create_promise(env, &deferred, &promise);
    if (status != napi_ok)
    {
        napi_throw_error(env, NULL, "create promise error");
    }

    Video2ImageStream *vis = (Video2ImageStream *)malloc(sizeof(Video2ImageStream));
    int64_t visPointer = (int64_t)vis;

    // 获取视频流数据，将其存在全局变量中
    open_inputfile(vis, input_filename, nobuffer, timeout, use_gpu, use_tcp, gpu_id);

    if (vis == NULL || !vis)
    {
        av_log(NULL, AV_LOG_ERROR, "[%ld] handle_init_read_video -->  Video2ImageStream is null \n", visPointer);

        av_log(NULL, AV_LOG_DEBUG, "[%ld] handle_init_read_video ---> promise resolve\n", visPointer);
        int64_t zero = 0;
        napi_create_int64(env, zero, &result);
        status = napi_resolve_deferred(env, deferred, result);
        if (status != napi_ok)
        {
            napi_throw_error(env, NULL, "promise resolve error");
        }
    }
    else
    {
        av_log(NULL, AV_LOG_DEBUG, "[%ld] handle_init_read_video -->  vis->ret %d \n", visPointer, vis->ret);
        if (vis->ret < 0)
        {
            // 处理错误信息
            napi_value message = NULL;
            status = napi_create_string_utf8(env, vis->error_message, NAPI_AUTO_LENGTH, &message);
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
            av_log(NULL, AV_LOG_DEBUG, "[%ld] handle_init_read_video ---> promise resolve\n", visPointer);
            int64_t _pointer = (int64_t)vis;
            napi_create_int64(env, _pointer, &result);
            status = napi_resolve_deferred(env, deferred, result);
            if (status != napi_ok)
            {
                napi_throw_error(env, NULL, "promise resolve error");
            }
        }
    }

    return promise;
}

static void handle_frame_data(FrameData *data)
{
    if (data->ret == 0 && data->frame != NULL)
    {
        Video2ImageStream *vis = data->vis;
        int64_t visPointer = (int64_t)vis;
        av_log(NULL, AV_LOG_INFO, "[%ld] handle_frame_data --> vis pointer: %ld \n", visPointer, (int64_t)vis);
        av_log(NULL, AV_LOG_DEBUG, "[%ld] begin handle_frame_data \n", visPointer);
        if (data->type == JPEG)
            copy_frame_data_and_transform_2_jpeg(vis->video_codec_context, data);
        else
            copy_frame_raw_data(vis->video_codec_context, data);

        av_log(NULL, AV_LOG_DEBUG, "[%ld] file_size: %ld\n", visPointer, data->file_size);
        if (data->file_size == 0 || data->file_data == NULL)
        {
            av_log(NULL, AV_LOG_DEBUG, "[%ld] file_data NULL\n", visPointer);
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
    else
    {
        av_log(NULL, AV_LOG_DEBUG, "data->frame is null \n");
    }
}

/**
 * 视频流取图片数据
 **/
static napi_value sync_handle_video_to_image_buffer(napi_env env, napi_callback_info info)
{
    napi_status status;

    size_t argc = 4;
    napi_value argv[4];
    napi_value *thisArg = NULL;
    void *data = NULL;
    status = napi_get_cb_info(env, info, &argc, argv, thisArg, &data);

    if (status != napi_ok)
    {
        napi_throw_error(env, NULL, "Failed to parse arguments");
    }

    // vis pointer
    int64_t visPointer;
    status = napi_get_value_int64(env, argv[0], &visPointer);
    if (status != napi_ok)
    {
        av_log(NULL, AV_LOG_INFO, "napi_get_value_int64 error: %ud\n", status);
        napi_throw_error(env, NULL, "vis pointer is invalid number");
        return NULL;
    }
    av_log(NULL, AV_LOG_INFO, "vis pointer: %ld\n", visPointer);
    Video2ImageStream *vis = (Video2ImageStream *)visPointer;

    // int chose_frames
    int chose_frames;
    status = napi_get_value_int32(env, argv[1], &chose_frames);
    if (status != napi_ok)
    {
        napi_throw_error(env, NULL, "chose_frames is invalid number");
    }
    av_log(NULL, AV_LOG_DEBUG, "[%ld] input chose_frames: %d\n", visPointer, chose_frames);

    // int type
    int type;
    status = napi_get_value_int32(env, argv[2], &type);
    if (status != napi_ok)
    {
        napi_throw_error(env, NULL, "type is invalid number");
    }
    av_log(NULL, AV_LOG_DEBUG, "[%ld] input type: %d\n", visPointer, type);

    // jpeg quality
    int quality = 80;
    status = napi_get_value_int32(env, argv[3], &quality);
    if (status != napi_ok)
    {
        napi_throw_error(env, NULL, "jpeg quality is invalid number");
    }
    av_log(NULL, AV_LOG_DEBUG, "[%ld] input jpeg quality: %d\n", visPointer, quality);

    // 处理图片
    if (vis == NULL || vis->ret < 0)
    {
        napi_throw_error(env, NULL, "initReadingVideo method should be invoke first");
    }

    FrameData frameData = {
        .vis = vis,
        .pts = 0,
        .frame = NULL,
        .ret = 0};
    video2images_grab(vis, quality, chose_frames, false, type, handle_frame_data, &frameData);

    if (status != napi_ok)
    {
        napi_throw_error(env, NULL, "Create buffer error");
    }

    av_log(NULL, AV_LOG_DEBUG, "[%ld] begin napi_create_buffer_copy \n", visPointer);
    napi_value buffer_pointer = NULL;
    int isNil = 1;
    // 判断有没有图片数据，如网络中断或其他原因，导致无法获取到图片数据
    if (frameData.ret == 0 && frameData.file_size > 0)
    {
        isNil = 0;

        // char数组 转换成 javascript buffer
        void *buffer_data = NULL;
        status = napi_create_buffer_copy(env, frameData.file_size, (const void *)frameData.file_data, &buffer_data, &buffer_pointer);
        av_log(NULL, AV_LOG_DEBUG, "[%ld] frameData.file_size : %ld\n", visPointer, frameData.file_size);
        av_log(NULL, AV_LOG_DEBUG, "[%ld] napi_create_buffer_copy result %d\n", visPointer, status);
    }
    else
    {
        av_log(NULL, AV_LOG_DEBUG, "[%ld] frameData.ret : %d\n", visPointer, frameData.ret);
        av_log(NULL, AV_LOG_DEBUG, "[%ld] frameData.file_size : %ld\n", visPointer, frameData.file_size);
    }

    // 释放 图片数据 的 内存
    av_freep(&frameData.file_data);
    free(frameData.file_data);
    frameData.file_data = NULL;

    // 返回Promise
    napi_value promise = NULL;
    napi_deferred deferred = NULL;
    status = napi_create_promise(env, &deferred, &promise);
    if (status != napi_ok)
    {
        napi_throw_error(env, NULL, "create promise error");
    }

    av_log(NULL, AV_LOG_DEBUG, "[%ld] isNil %d\n", visPointer, isNil);

    if (isNil == 1)
    {
        // 返回错误原因
        char *_err_msg = frameData.error_message;
        if (frameData.ret == 0 && frameData.file_size == 0)
        {
            _err_msg = "image data is null, network may connect wrong";
        }

        napi_value message = NULL;
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
static void callback_execute(napi_env env, void *data)
{
    // DO NOTHING
}

static void callback_completion(napi_env env, napi_status status, void *data)
{
    time_t begin = get_now_microseconds();
    av_log(NULL, AV_LOG_DEBUG, "callback_completion\n");

    FrameData *frameData = (FrameData*)malloc(sizeof(FrameData));
    frameData->pts = 0;
    frameData->frame = NULL;
    frameData->ret = 0;
    frameData->isThreadly = false;

    ReadImageBufferParams *params = (ReadImageBufferParams *)data;
    Video2ImageStream *vis = params->vis;
    int64_t visPointer = (int64_t)vis;
    video2images_grab(vis, params->quality, params->chose_frames, true, params->type, handle_frame_data, frameData);
    av_log(NULL, AV_LOG_DEBUG, "[%ld] callback_completion-> quality: %d  chose_frames: %d type: %d \n", visPointer, params->quality, params->chose_frames, params->type);

    av_log(NULL, AV_LOG_DEBUG, "[%ld] reading ...........\n", visPointer);

    napi_value cb = NULL, js_status = NULL;
    napi_get_reference_value(env, params->ref, &cb);
    napi_create_uint32(env, (uint32_t)status, &js_status);

    av_log(NULL, AV_LOG_DEBUG, "[%ld] frameData.file_size %ld \n", visPointer, frameData->file_size);
    av_log(NULL, AV_LOG_DEBUG, "[%ld] frameData.file_data %p \n", visPointer, frameData->file_data);

    napi_value buffer_pointer;
    // char数组 转换成 javascript buffer
    void *buffer_data = NULL;
    status = napi_create_buffer_copy(env, frameData->file_size, (const void *)frameData->file_data, &buffer_data, &buffer_pointer);
    av_log(NULL, AV_LOG_DEBUG, "[%ld] frameData.file_size : %ld\n", visPointer, frameData->file_size);
    av_log(NULL, AV_LOG_DEBUG, "[%ld] napi_create_buffer_copy result %d\n", visPointer, status);

    // 释放 图片数据 的 内存
    av_freep(&frameData->file_data);
    free(frameData->file_data);
    frameData->file_data = NULL;

    // 返回错误原因
    napi_value message = NULL;
    status = napi_get_undefined(env, &message);

    if (frameData->file_size == 0)
    {
        char *_err_msg;
        // 判断有没有图片数据，如网络中断或其他原因，导致无法获取到图片数据
        if (frameData->ret == 0)
        {
            _err_msg = "image data is null, network may connect wrong";
        }
        else
        {
            _err_msg = frameData->error_message;
        }
        av_log(NULL, AV_LOG_DEBUG, "%s \n", _err_msg);

        status = napi_create_string_utf8(env, _err_msg, NAPI_AUTO_LENGTH, &message);
        if (status != napi_ok)
        {
            napi_throw_error(env, NULL, "error message create error");
        }
    }

    napi_value obj = NULL;
    status = napi_create_object(env, &obj);
    if (status != napi_ok)
    {
        napi_throw_error(env, NULL, "callback object create error");
    }

    napi_value error_name = NULL;
    napi_create_string_utf8(env, "error", NAPI_AUTO_LENGTH, &error_name);
    napi_value data_name = NULL;
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

static napi_value async_handle_video_to_image_buffer(napi_env env, napi_callback_info info)
{
    napi_status status;

    size_t argc = 5;
    napi_value argv[5];
    napi_value thisArg;
    void *data = NULL;
    status = napi_get_cb_info(env, info, &argc, argv, &thisArg, &data);

    if (status != napi_ok)
    {
        napi_throw_error(env, NULL, "Failed to parse arguments");
    }

    // vis pointer
    int64_t visPointer;
    status = napi_get_value_int64(env, argv[0], &visPointer);
    if (status != napi_ok)
    {
        av_log(NULL, AV_LOG_INFO, "napi_get_value_int64 error: %ud\n", status);
        napi_throw_error(env, NULL, "vis pointer is invalid number");
        return NULL;
    }
    av_log(NULL, AV_LOG_INFO, "vis pointer: %ld\n", visPointer);
    Video2ImageStream *vis = (Video2ImageStream *)visPointer;

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

    // int chose_frames
    int chose_frames;
    status = napi_get_value_int32(env, argv[3], &chose_frames);
    if (status != napi_ok)
    {
        napi_throw_error(env, NULL, "chose_frames is invalid number");
    }
    av_log(NULL, AV_LOG_DEBUG, "[%ld] input chose_frames: %d\n", visPointer, chose_frames);

    // callback
    napi_value callback = argv[4];
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
    if (vis == NULL || vis->ret < 0)
    {
        napi_throw_error(env, NULL, "initReadingVideo method should be invoke first");
    }

    napi_ref ref = NULL;
    status = napi_create_reference(env, callback, 1, &ref);
    av_log(NULL, AV_LOG_DEBUG, "[%ld] napi_create_reference %d \n", visPointer, status);
    vis->ref = ref;

    napi_value resource_name = NULL;
    status = napi_create_string_utf8(env, "callback", NAPI_AUTO_LENGTH, &resource_name);
    av_log(NULL, AV_LOG_DEBUG, "[%ld] napi_create_string_utf8 %d\n", visPointer, status);

    ReadImageBufferParams *_params = (ReadImageBufferParams *)malloc(sizeof(ReadImageBufferParams));

    _params->chose_frames = chose_frames;
    _params->quality = quality;
    _params->type = type;
    _params->vis = vis;
    _params->ref = ref;

    napi_value async_resource = NULL;
    napi_async_work work = NULL;
    status = napi_create_async_work(env, async_resource, resource_name, callback_execute, callback_completion, (void *)_params, &work);
    av_log(NULL, AV_LOG_DEBUG, "[%ld] napi_create_async_work %d\n", visPointer, status);
    vis->work = work;

    napi_queue_async_work(env, work);

    return NULL;
}

static void consumer_callback_threadsafe(napi_env env, napi_value js_callback, void *context, void *data)
{
    napi_status status;
    av_log(NULL, AV_LOG_DEBUG, "async_handle_video_to_image_buffer_threadly --> consumer_callback_threadsafe\n");
    if (js_callback == NULL)
    {
        av_log(NULL, AV_LOG_DEBUG, "consumer_callback_threadsafe --> js_callback is null \n");
        return;
    }

    if (data != NULL && data)
    {
        av_log(NULL, AV_LOG_DEBUG, "consumer_callback_threadsafe --> cast data to FrameData \n");
        FrameData *frame_data = (FrameData *)data;
        if (frame_data == NULL || !frame_data)
        {
            av_log(NULL, AV_LOG_WARNING, "consumer_callback_threadsafe --> data is not instance FrameData \n");
            return;
        }

        napi_value buffer_pointer = NULL;
        if (!frame_data->abort)
        {
            av_log(NULL, AV_LOG_DEBUG, "async_handle_video_to_image_buffer_threadly --> node1 %ld \n", frame_data->pts);

            // char数组 转换成 javascript buffer
            void *buffer_data = NULL;
            status = napi_create_buffer_copy(env, frame_data->file_size, (const void *)frame_data->file_data, &buffer_data, &buffer_pointer);
            av_log(NULL, AV_LOG_DEBUG, "async_handle_video_to_image_buffer_threadly --> frameData.file_size : %ld\n", frame_data->file_size);
            av_log(NULL, AV_LOG_DEBUG, "async_handle_video_to_image_buffer_threadly --> napi_create_buffer_copy result %d\n", status);
        }

        // 返回错误原因
        napi_value message = NULL;
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
            av_log(NULL, AV_LOG_WARNING, "%s \n", _err_msg);

            status = napi_create_string_utf8(env, _err_msg, NAPI_AUTO_LENGTH, &message);
            if (status != napi_ok)
            {
                napi_throw_error(env, NULL, "error message create error");
            }
        }

        napi_value obj = NULL;
        status = napi_create_object(env, &obj);
        if (status != napi_ok)
        {
            napi_throw_error(env, NULL, "callback object create error");
        }

        napi_value error_name;
        napi_create_string_utf8(env, "error", NAPI_AUTO_LENGTH, &error_name);
        napi_value data_name;
        napi_create_string_utf8(env, "data", NAPI_AUTO_LENGTH, &data_name);

        if (frame_data->error_message != NULL)
        {
            status = napi_create_string_utf8(env, frame_data->error_message, NAPI_AUTO_LENGTH, &message);
        }

        napi_set_property(env, obj, error_name, message);
        napi_set_property(env, obj, data_name, buffer_pointer);

        if (frame_data->vis->multiThread)
        {
            napi_value libraryId_name = NULL;
            napi_create_string_utf8(env, "libraryId", NAPI_AUTO_LENGTH, &libraryId_name);

            napi_value libraryId = NULL;
            napi_create_int64(env, (int64_t)frame_data->vis->multiThread, &libraryId);
            napi_set_property(env, obj, libraryId_name, libraryId);
        }

        // 调用 callback
        napi_value result;
        status = napi_call_function(env, js_callback, js_callback, 1, &obj, &result);
        av_log(NULL, AV_LOG_DEBUG, "async_handle_video_to_image_buffer_threadly --> napi_call_function status %d\n", status);

        if (status != napi_ok)
        {
            napi_throw_error(env, NULL, "call function error");
        }

        if (frame_data->frame != NULL && frame_data->frame)
        {
            av_frame_unref(frame_data->frame);
            // av_frame_free(&(frame_data->frame));
        }
        if (frame_data->file_data != NULL && frame_data->file_data)
        {
            free(frame_data->file_data);
            frame_data->file_data = NULL;
            frame_data->file_size = 0;
        }
    }
    else
    {
        av_log(NULL, AV_LOG_DEBUG, "consumer_callback_threadsafe --> data is null \n");
        napi_value obj = NULL;
        status = napi_create_object(env, &obj);
        if (status != napi_ok)
        {
            napi_throw_error(env, NULL, "callback object create error");
        }

        napi_value error_name = NULL;
        napi_create_string_utf8(env, "error", NAPI_AUTO_LENGTH, &error_name);
        napi_value data_name = NULL;
        napi_create_string_utf8(env, "data", NAPI_AUTO_LENGTH, &data_name);

        napi_value message = NULL;
        status = napi_create_string_utf8(env, "call threadsafe function error", NAPI_AUTO_LENGTH, &message);
        napi_set_property(env, obj, error_name, message);

        napi_value buffer_pointer;
        status = napi_get_undefined(env, &buffer_pointer);
        napi_set_property(env, obj, data_name, buffer_pointer);

        // 调用 callback
        napi_value result;
        status = napi_call_function(env, js_callback, js_callback, 1, &obj, &result);
        av_log(NULL, AV_LOG_DEBUG, "napi_call_function status %d\n", status);

        if (status != napi_ok)
        {
            napi_throw_error(env, NULL, "call function error");
        }
    }
}

static void handle_frame_data_threadly(FrameData *frameData)
{
    av_log(NULL, AV_LOG_DEBUG, "handle_frame_data_threadly \n");
    if (frameData != NULL && frameData->vis != NULL)
    {
        if (!frameData->abort && frameData->ret >= 0)
        {
            av_log(NULL, AV_LOG_DEBUG, "async_handle_video_to_image_buffer_threadly --> node1 %ld \n", frameData->pts);

            Video2ImageStream *vis = frameData->vis;
            av_log(NULL, AV_LOG_INFO, "handle_frame_data_threadly vis pointer: %ld \n", (int64_t)vis);

            if (frameData->type == JPEG)
                copy_frame_data_and_transform_2_jpeg(vis->video_codec_context, frameData);
            else
                copy_frame_raw_data(vis->video_codec_context, frameData);
            av_log(NULL, AV_LOG_DEBUG, "async_handle_video_to_image_buffer_threadly --> file_size: %ld\n", frameData->file_size);
        }

        napi_ref_threadsafe_function(frameData->env, (napi_threadsafe_function)frameData->vis->func);
        napi_acquire_threadsafe_function((napi_threadsafe_function)(frameData->vis->func));
        napi_status status = napi_call_threadsafe_function((napi_threadsafe_function)frameData->vis->func, (void*)frameData, napi_tsfn_blocking);
        av_log(NULL, AV_LOG_DEBUG, "napi_call_threadsafe_function return %d \n", status);
        if (status == napi_closing)
        {
            frameData->abort = true;
        }
    }
    else if (frameData != NULL)
    {
        frameData->abort = true;
    }
}

static void callback_thread(napi_env env, void *data)
{
    av_log(NULL, AV_LOG_DEBUG, "async_handle_video_to_image_buffer_threadly --> callback_thread \n");

    if (data == NULL || !data)
    {
        av_log(NULL, AV_LOG_ERROR, "callback_thread data is null! \n");
    }
    ReadImageBufferParams *_params = (ReadImageBufferParams *)data;
    Video2ImageStream *vis = _params->vis;
    int64_t visPointer = (int64_t)vis;
    av_log(NULL, AV_LOG_DEBUG, "[%ld] callback_thread data pointer: %ld \n", visPointer, (long)_params);
    av_log(NULL, AV_LOG_INFO, "callback_thread vis pointer: %ld \n", (int64_t)vis);

    if (vis->multiThread)
    {
        av_log(NULL, AV_LOG_DEBUG, "[%ld] callback_thread data filename: %s \n", visPointer, _params->filename);
        // 获取视频流数据，将其存在全局变量中
        open_inputfile(vis, _params->filename, _params->nobuffer, _params->timeout, _params->use_gpu, _params->use_tcp, _params->gpu_id);
    }

    FrameData *frameData = (FrameData *)malloc(sizeof(FrameData));
    frameData->vis = vis;
    frameData->pts = 0;
    frameData->frame = NULL;
    frameData->ret = 0;
    frameData->isThreadly = true;
    frameData->abort = false;
    frameData->chose_frames = _params->chose_frames;
    av_log(NULL, AV_LOG_DEBUG, "[%ld] callback_thread data chose_frames: %d \n", visPointer, _params->chose_frames);
    frameData->type = _params->type;
    av_log(NULL, AV_LOG_DEBUG, "[%ld] callback_thread data type: %d \n", visPointer, _params->type);
    frameData->quality = _params->quality;
    av_log(NULL, AV_LOG_DEBUG, "[%ld] callback_thread data quality: %d \n", visPointer, _params->quality);
    if (vis->multiThread)
    {
        napi_ref_threadsafe_function(env, vis->func);
        frameData->env = env;
    }
    video2images_grab(vis, _params->quality, _params->chose_frames, false, _params->type, handle_frame_data_threadly, frameData);
    av_log(NULL, AV_LOG_INFO, "[%ld] async_handle_video_to_image_buffer_threadly --> callback_thread exit \n", visPointer);
    if (frameData->frame != NULL)
    {
        av_log(NULL, AV_LOG_DEBUG, "[%ld] async_handle_video_to_image_buffer_threadly --> frameData.frame not null \n", visPointer);
        av_log(NULL, AV_LOG_DEBUG, "[%ld] async_handle_video_to_image_buffer_threadly --> frameData.frame freed \n", visPointer);
    }
    if (frameData->file_data != NULL && frameData->file_data)
    {
        av_log(NULL, AV_LOG_DEBUG, "[%ld] async_handle_video_to_image_buffer_threadly --> frameData.file_data not null \n", visPointer);
        av_freep(&frameData->file_data);
        av_free(frameData->file_data);
        if (frameData->file_data)
        {
            free(frameData->file_data);
        }
        frameData->file_data = NULL;
        av_log(NULL, AV_LOG_DEBUG, "[%ld] async_handle_video_to_image_buffer_threadly --> frameData.file_data freed \n", visPointer);
    }
    if (vis == NULL || !vis)
    {
        return;
    }
    release(vis);

    vis->format_context = NULL;
    vis->video_stream_idx = -1;
    vis->video_codec_context = NULL;
    vis->ret = -1;

    free(frameData);
}

static void callback_thread_completion(napi_env env, napi_status status, void *data)
{
    av_log(NULL, AV_LOG_DEBUG, "callback_thread_completion ........ \n");
    ReadImageBufferParams *_params = (ReadImageBufferParams *)data;

    if (_params->ref != NULL && _params->ref)
    {
        napi_status _status = napi_delete_reference(env, (napi_ref)_params->ref);
        av_log(NULL, AV_LOG_DEBUG, "napi_delete_reference status : %d \n", status);
        _params->ref = NULL;
    }

    if (_params->work != NULL && _params->work)
    {
        napi_status _status = napi_delete_async_work(env, (napi_async_work)_params->work);
        av_log(NULL, AV_LOG_DEBUG, "napi_delete_async_work status : %d \n", status);
        _params->work = NULL;
    }

    if (_params)
    {
        av_log(NULL, AV_LOG_DEBUG, "ReadImageBufferParams free \n");
        if (_params->vis->multiThread)
        {
            free(_params->filename);
            _params->filename = NULL;
        }
        free(_params);
        av_log(NULL, AV_LOG_DEBUG, "ReadImageBufferParams end \n");
        _params = NULL;
        data = NULL;
    }
}

void finalize(napi_env env, void *data, void *hint)
{
    av_log(NULL, AV_LOG_DEBUG, "finalize consumer ........ \n");
}

napi_value async_handle_video_to_image_buffer_threadly(napi_env env, napi_callback_info info)
{
    napi_status status;

    size_t argc = 5;
    napi_value argv[5];
    napi_value thisArg;
    void *data = NULL;
    status = napi_get_cb_info(env, info, &argc, argv, &thisArg, &data);

    if (status != napi_ok)
    {
        napi_throw_error(env, NULL, "Failed to parse arguments");
    }

    // vis pointer
    int64_t visPointer;
    status = napi_get_value_int64(env, argv[0], &visPointer);
    if (status != napi_ok)
    {
        av_log(NULL, AV_LOG_ERROR, "napi_get_value_int64 error: %ud\n", status);
        napi_throw_error(env, NULL, "vis pointer is invalid number");
        return NULL;
    }
    av_log(NULL, AV_LOG_INFO, "vis pointer: %ld\n", visPointer);
    Video2ImageStream *vis = (Video2ImageStream *)visPointer;

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

    // int chose_frames
    int chose_frames;
    status = napi_get_value_int32(env, argv[3], &chose_frames);
    if (status != napi_ok)
    {
        napi_throw_error(env, NULL, "chose_frames is invalid number");
    }
    av_log(NULL, AV_LOG_DEBUG, "input chose_frames: %d\n", chose_frames);

    // callback
    napi_value callback = argv[4];
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
    if (vis == NULL || vis->ret < 0)
    {
        napi_throw_error(env, NULL, "initReadingVideo method should be invoke first");
    }

    napi_value resource_name = NULL;
    status = napi_create_string_utf8(env, "callback", NAPI_AUTO_LENGTH, &resource_name);
    av_log(NULL, AV_LOG_DEBUG, "[%ld] async_handle_video_to_image_buffer_threadly --> napi_create_string_utf8 %d\n", visPointer, status);

    vis->multiThread = false;

    // ReadImageBufferParams params = {};
    ReadImageBufferParams *_params =  (ReadImageBufferParams *)malloc(sizeof(ReadImageBufferParams));

    _params->chose_frames = chose_frames;
    _params->quality = quality;
    _params->type = type;
    _params->vis = vis;
    _params->work = NULL;

    napi_threadsafe_function result = NULL;
    napi_create_threadsafe_function(env, callback, NULL, resource_name, 1, 1, NULL, &finalize, NULL, &consumer_callback_threadsafe, &result);
    napi_ref_threadsafe_function(env, result);
    _params->ref = NULL;
    vis->func = result;

    av_log(NULL, AV_LOG_DEBUG, "async_handle_video_to_image_buffer_threadly data chose_frames: %d \n", _params->chose_frames);
    av_log(NULL, AV_LOG_DEBUG, "async_handle_video_to_image_buffer_threadly data type: %d \n", _params->type);
    av_log(NULL, AV_LOG_DEBUG, "async_handle_video_to_image_buffer_threadly data quality: %d \n", _params->quality);
    av_log(NULL, AV_LOG_DEBUG, "async_handle_video_to_image_buffer_threadly data pointer: %ld \n", (long)_params);

    napi_value async_resource = NULL;
    status = napi_create_async_work(env, async_resource, resource_name, &callback_thread, &callback_thread_completion, (void *)_params, &(_params->work));
    av_log(NULL, AV_LOG_DEBUG, "async_handle_video_to_image_buffer_threadly --> napi_create_async_work %d\n", status);
    vis->work = _params->work;
    napi_queue_async_work(env, _params->work);

    return NULL;
}

napi_value async_grab_image_from_video_multithreadedly(napi_env env, napi_callback_info info)
{
    napi_status status;

    // js 参数 转换成 napi_value
    size_t argc = 11;
    napi_value argv[11];
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
#ifdef _WIN32
    char *input_filename;
#else
    char input_filename[input_filename_len];
#endif // _WIN32

    status = napi_get_value_string_utf8(env, argv[0], input_filename, length + 1, &input_filename_res);

    if (status != napi_ok)
    {
        napi_throw_error(env, NULL, "Invalid value was passed as argument by input_filename");
    }
    av_log(NULL, AV_LOG_DEBUG, "async_grab_image_from_video_multithreadedly --> input filename : %s\n", input_filename);

    // timeout
    int64_t timeout;
    status = napi_get_value_int64(env, argv[1], &timeout);
    av_log(NULL, AV_LOG_DEBUG, "async_grab_image_from_video_multithreadedly --> timeout : %ld\n", timeout);

    // nobuffer
    bool nobuffer;
    status = napi_get_value_bool(env, argv[2], &nobuffer);
    av_log(NULL, AV_LOG_DEBUG, "async_grab_image_from_video_multithreadedly --> nobuffer : %d\n", nobuffer);

    // useGpu
    bool use_gpu;
    status = napi_get_value_bool(env, argv[3], &use_gpu);
    av_log(NULL, AV_LOG_DEBUG, "async_grab_image_from_video_multithreadedly --> useGpu : %d\n", use_gpu);

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
#ifdef _WIN32
    char *gpu_id;
#else
    char gpu_id[gpu_id_len];
#endif // _WIN32
    status = napi_get_value_string_utf8(env, argv[5], gpu_id, length + 1, &gpu_id_res);

    if (status != napi_ok)
    {
        napi_throw_error(env, NULL, "Invalid value was passed as argument by gpu_id");
    }
    av_log(NULL, AV_LOG_DEBUG, "async_grab_image_from_video_multithreadedly --> input gpu_id : %s\n", gpu_id);

    // useTcp
    bool use_tcp;
    status = napi_get_value_bool(env, argv[6], &use_tcp);
    av_log(NULL, AV_LOG_DEBUG, "async_grab_image_from_video_multithreadedly --> use_tcp : %d\n", use_tcp);

    // int type
    int type;
    status = napi_get_value_int32(env, argv[7], &type);
    if (status != napi_ok)
    {
        napi_throw_error(env, NULL, "type is invalid number");
    }
    av_log(NULL, AV_LOG_DEBUG, "async_grab_image_from_video_multithreadedly --> input type: %d\n", type);

    // jpeg quality
    int quality = 80;
    status = napi_get_value_int32(env, argv[8], &quality);
    if (status != napi_ok)
    {
        napi_throw_error(env, NULL, "jpeg quality is invalid number");
    }
    av_log(NULL, AV_LOG_DEBUG, "async_grab_image_from_video_multithreadedly --> input jpeg quality: %d\n", quality);

    // int chose_frames
    int chose_frames;
    status = napi_get_value_int32(env, argv[9], &chose_frames);
    if (status != napi_ok)
    {
        napi_throw_error(env, NULL, "chose_frames is invalid number");
    }
    av_log(NULL, AV_LOG_DEBUG, "async_grab_image_from_video_multithreadedly --> input chose_frames: %d\n", chose_frames);

    // callback
    napi_value callback = argv[10];
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

    napi_value resource_name = NULL;
    status = napi_create_string_utf8(env, "callback", NAPI_AUTO_LENGTH, &resource_name);

    Video2ImageStream *vis = (Video2ImageStream *)malloc(sizeof(Video2ImageStream));
    int64_t visPointer = (int64_t)vis;

    vis->multiThread = true;

    ReadImageBufferParams *_params = (ReadImageBufferParams *)malloc(sizeof(ReadImageBufferParams));

    _params->filename = (char*)malloc(sizeof(char) * input_filename_len);
    strcpy(_params->filename, input_filename);
    _params->nobuffer = nobuffer;
    _params->timeout = timeout;
    _params->use_gpu = use_gpu;
    _params->use_tcp = use_tcp;
    _params->gpu_id = gpu_id;

    _params->chose_frames = chose_frames;
    _params->quality = quality;
    _params->type = type;
    _params->vis = vis;

    napi_threadsafe_function result = NULL;
    napi_create_threadsafe_function(env, callback, NULL, resource_name, 1, 1, NULL, &finalize, NULL, &consumer_callback_threadsafe, &result);
    napi_ref_threadsafe_function(env, result);
    _params->ref = NULL;
    vis->func = result;

    av_log(NULL, AV_LOG_DEBUG, "async_grab_image_from_video_multithreadedly -->  data chose_frames: %d \n", _params->chose_frames);
    av_log(NULL, AV_LOG_DEBUG, "async_grab_image_from_video_multithreadedly -->  data type: %d \n", _params->type);
    av_log(NULL, AV_LOG_DEBUG, "async_grab_image_from_video_multithreadedly -->  data quality: %d \n", _params->quality);
    av_log(NULL, AV_LOG_DEBUG, "async_grab_image_from_video_multithreadedly -->  data pointer: %ld \n", (long)_params);

    napi_value async_resource = NULL;
    _params->work = NULL;
    status = napi_create_async_work(env, async_resource, resource_name, &callback_thread, &callback_thread_completion, (void *)_params, &(_params->work));
    av_log(NULL, AV_LOG_DEBUG, "async_handle_video_to_image_buffer_threadly --> napi_create_async_work %d\n", status);
    vis->work = _params->work;
    napi_queue_async_work(env, _params->work);

    /* while (vis != NULL && vis)
    {
        av_log(NULL, AV_LOG_DEBUG, "[%ld] async_grab_image_from_video_multithreadedly --> vis freed \n", visPointer);
        free(vis);
        vis = NULL;
        break;
    } */
    return NULL;

}

/**
 * 释放视频连接
 **/
napi_value handle_destroy_stream(napi_env env, napi_callback_info info)
{
    napi_status status;
    size_t argc = 1;
    napi_value argv[1];
    napi_value *thisArg = NULL;
    void *data = NULL;
    status = napi_get_cb_info(env, info, &argc, argv, thisArg, &data);

    if (status != napi_ok)
    {
        napi_throw_error(env, NULL, "Failed to parse arguments");
    }

    // vis pointer
    int64_t visPointer;
    status = napi_get_value_int64(env, argv[0], &visPointer);
    if (status != napi_ok)
    {
        napi_throw_error(env, NULL, "vis pointer is invalid number");
        return NULL;
    }
    av_log(NULL, AV_LOG_INFO, "vis pointer: %ld\n", visPointer);
    Video2ImageStream *vis = (Video2ImageStream *)visPointer;

    av_log(NULL, AV_LOG_INFO, "FFmpeg-Nodejs: destroy stream handle ... \n");
    vis->release = true;

    napi_threadsafe_function func = (napi_threadsafe_function)vis->func;
    if (func != NULL)
    {
        while (true)
        {
            vis->release = true;
            av_usleep(1);
            // status = napi_acquire_threadsafe_function(func);
            // av_log(NULL, AV_LOG_DEBUG, "napi_acquire_threadsafe_function status : %d \n", status);
            // if (status == napi_closing)
            // {
            //    break;
            // }

            status = napi_release_threadsafe_function(func, napi_tsfn_abort);
            av_log(NULL, AV_LOG_DEBUG, "napi_release_threadsafe_function status : %d \n", status);
            break;
        }

        napi_status _status = napi_unref_threadsafe_function(env, func);
        av_log(NULL, AV_LOG_DEBUG, "napi_unref_threadsafe_function status : %d \n", _status);
    }
    else
    {
        napi_ref ref = (napi_ref)vis->ref;
        if (ref != NULL)
        {
            napi_status _status = napi_delete_reference(env, ref);
            av_log(NULL, AV_LOG_DEBUG, "napi_delete_reference status : %d \n", _status);
            ref = NULL;
        }

        napi_async_work work = (napi_async_work)vis->work;
        if (work != NULL)
        {
            napi_status _status = napi_delete_async_work(env, work);
            av_log(NULL, AV_LOG_DEBUG, "napi_delete_async_work status : %d \n", _status);
            work = NULL;
        }

        if (vis == NULL || !vis)
        {
            return NULL;
        }

        while (true)
        {
            release(vis);
            break;
        }

        vis->format_context = NULL;
        vis->video_stream_idx = -1;
        vis->video_codec_context = NULL;
        vis->ret = -1;

        while (vis != NULL && vis)
        {
            av_log(NULL, AV_LOG_DEBUG, "[%ld] handle_destroy_stream --> vis freed \n", visPointer);
            free(vis);
            vis = NULL;
            break;
        }

    }

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
#ifdef _WIN32
    char *input_filename;
#else
    char input_filename[input_filename_len];
#endif // _WIN32
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
#ifdef _WIN32
    char *output_filename;
#else
    char output_filename[output_filename_len];
#endif // _WIN32
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

/**
 * napi 构建js的方法
 **/
static napi_value init(napi_env env, napi_value exports)
{
    napi_status status;

    napi_property_descriptor methods[] = {
        DECLARE_NAPI_METHOD("initReadingVideo", handle_init_read_video),
        DECLARE_NAPI_METHOD("syncReadImageBuffer", sync_handle_video_to_image_buffer),
        DECLARE_NAPI_METHOD("asyncReadImageBuffer", async_handle_video_to_image_buffer),
        DECLARE_NAPI_METHOD("destroyStream", handle_destroy_stream),
        DECLARE_NAPI_METHOD("asyncReadImageBufferThreadly", async_handle_video_to_image_buffer_threadly),
        DECLARE_NAPI_METHOD("asyncGrabImageMultithreadedly", async_grab_image_from_video_multithreadedly),
        DECLARE_NAPI_METHOD("recordVideo", handle_record_video)};

    status = napi_define_properties(env, exports, sizeof(methods) / sizeof(methods[0]), methods);
    if (status != napi_ok)
        return NULL;

    return exports;
}

// 初始化 js 模块
NAPI_MODULE(NODE_GYP_MODULE_NAME, init);
