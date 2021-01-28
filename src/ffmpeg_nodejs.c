#include "./ffmpeg_nodejs.h"

static Video2ImageStream *vis = NULL;

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

static volatile ReadImageBufferParams params;
static volatile bool thread = false;

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
    char gpu_id[gpu_id_len];
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
    napi_value promise;
    napi_deferred deferred;
    status = napi_create_promise(env, &deferred, &promise);
    if (status != napi_ok)
    {
        napi_throw_error(env, NULL, "create promise error");
    }

    if (vis == NULL)
    {
        vis = (Video2ImageStream *)malloc(sizeof(Video2ImageStream *));
    }
    // 获取视频流数据，将其存在全局变量中
    open_inputfile(vis, input_filename, nobuffer, timeout, use_gpu, use_tcp, gpu_id);

    if (vis == NULL)
    {
        av_log(NULL, AV_LOG_ERROR, "handle_init_read_video -->  Video2ImageStream is null \n");
    }

    av_log(NULL, AV_LOG_DEBUG, "handle_init_read_video -->  vis->ret %d \n", vis->ret);
    if (vis->ret < 0)
    {
        // 处理错误信息
        napi_value message;
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
        av_log(NULL, AV_LOG_DEBUG, "handle_init_read_video ---> promise resolve\n");
        double zero = 0.0;
        napi_create_double(env, zero, &result);
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
        av_log(NULL, AV_LOG_DEBUG, "begin handle_frame_data \n");
        if (data->type == JPEG)
            copy_frame_data_and_transform_2_jpeg(vis->video_codec_context, data);
        else
            copy_frame_raw_data(vis->video_codec_context, data);

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
    if (vis == NULL || vis->ret < 0)
    {
        napi_throw_error(env, NULL, "initReadingVideo method should be invoke first");
    }

    FrameData frameData = {
        .pts = 0,
        .frame = NULL,
        .ret = 0};
    video2images_grab(vis, quality, chose_frames, false, type, handle_frame_data, &frameData);

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
    else
    {
        av_log(NULL, AV_LOG_DEBUG, "frameData.ret : %d\n", frameData.ret);
        av_log(NULL, AV_LOG_DEBUG, "frameData.file_size : %ld\n", frameData.file_size);
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
static void callback_execute(napi_env env, void *data)
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
        .isThreadly = false
        };

    video2images_grab(vis, params.quality, params.chose_frames, true, params.type, handle_frame_data, &frameData);
    av_log(NULL, AV_LOG_DEBUG, "callback_completion-> quality: %d  chose_frames: %d type: %d \n", params.quality, params.chose_frames, params.type);

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

static napi_value async_handle_video_to_image_buffer(napi_env env, napi_callback_info info)
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
    if (vis == NULL || vis->ret < 0)
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
    status = napi_create_async_work(env, async_resource, resource_name, callback_execute, callback_completion, NULL, &(async_work_info.work));
    av_log(NULL, AV_LOG_DEBUG, "napi_create_async_work %d\n", status);

    napi_queue_async_work(env, async_work_info.work);

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
        // FrameData empty_frame_data = {};
        // FrameData *frame_data = &empty_frame_data;
        // FrameData *frame_data = origin_data;
        /*
        while (true)
        {
            vis->video_codec_context;
            av_log(NULL, AV_LOG_DEBUG, "consumer_callback_threadsafe --> begin frame_data_deep_copy \n");
            frame_data_deep_copy(origin_data, frame_data);
            av_log(NULL, AV_LOG_DEBUG, "consumer_callback_threadsafe --> end frame_data_deep_copy \n");
            break;
        }
        */

        napi_value buffer_pointer = NULL;
        if (!frame_data->abort)
        {
            av_log(NULL, AV_LOG_DEBUG, "async_handle_video_to_image_buffer_threadly --> node1 %ld \n", frame_data->pts);

            av_log(NULL, AV_LOG_DEBUG, "async_handle_video_to_image_buffer_threadly --> params.type .......>>>>>>> %d \n", params.type);

            /*
            if (params.type == JPEG)
                copy_frame_data_and_transform_2_jpeg(vis->video_codec_context, frame_data);
            else
                copy_frame_raw_data(vis->video_codec_context, frame_data);
            av_log(NULL, AV_LOG_DEBUG, "async_handle_video_to_image_buffer_threadly --> file_size: %ld\n", frame_data->file_size);
            */

            // char数组 转换成 javascript buffer
            void *buffer_data;
            status = napi_create_buffer_copy(env, frame_data->file_size, (const void *)frame_data->file_data, &buffer_data, &buffer_pointer);
            av_log(NULL, AV_LOG_DEBUG, "async_handle_video_to_image_buffer_threadly --> frameData.file_size : %ld\n", frame_data->file_size);
            av_log(NULL, AV_LOG_DEBUG, "async_handle_video_to_image_buffer_threadly --> napi_create_buffer_copy result %d\n", status);
        }

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
    if (async_work_info.func != NULL)
    {
        if (!frameData->abort)
        {
            av_log(NULL, AV_LOG_DEBUG, "async_handle_video_to_image_buffer_threadly --> node1 %ld \n", frameData->pts);

            av_log(NULL, AV_LOG_DEBUG, "async_handle_video_to_image_buffer_threadly --> params.type .......>>>>>>> %d \n", params.type);

            if (params.type == JPEG)
                copy_frame_data_and_transform_2_jpeg(vis->video_codec_context, frameData);
            else
                copy_frame_raw_data(vis->video_codec_context, frameData);
            av_log(NULL, AV_LOG_DEBUG, "async_handle_video_to_image_buffer_threadly --> file_size: %ld\n", frameData->file_size);
        }

        napi_acquire_threadsafe_function(async_work_info.func);
        napi_status status = napi_call_threadsafe_function(async_work_info.func, frameData, napi_tsfn_blocking);
        av_log(NULL, AV_LOG_DEBUG, "napi_call_threadsafe_function return %d \n", status);
        if (status == napi_closing)
        {
            frameData->abort = true;
        }
    }
    else
    {
        frameData->abort = true;
    }
}

static void callback_thread(napi_env env, void *data)
{
    av_log(NULL, AV_LOG_DEBUG, "async_handle_video_to_image_buffer_threadly --> callback_thread \n");
    // napi_acquire_threadsafe_function(async_work_info.func);
    FrameData frameData = {
        .pts = 0,
        .frame = NULL,
        .ret = 0,
        .isThreadly = true,
        .abort = false};
    video2images_grab(vis, params.quality, params.chose_frames, false, params.type, handle_frame_data_threadly, &frameData);
    av_log(NULL, AV_LOG_INFO, "async_handle_video_to_image_buffer_threadly --> callback_thread exit \n");
    if (frameData.frame != NULL)
    {
        av_log(NULL, AV_LOG_DEBUG, "async_handle_video_to_image_buffer_threadly --> frameData.frame not null \n");
        frameData.frame = NULL;
    }
    if (frameData.file_data != NULL && frameData.file_data)
    {
        av_log(NULL, AV_LOG_DEBUG, "async_handle_video_to_image_buffer_threadly --> frameData.file_data not null \n");
        av_freep(&frameData.file_data);
        av_free(frameData.file_data);
        if (frameData.file_data)
        {
            free(frameData.file_data);
        }
        frameData.file_data = NULL;
    }
    if (vis == NULL)
    {
        return;
    }
    release(vis->video_codec_context, vis->format_context, vis->init);

    vis->format_context = NULL;
    vis->video_stream_idx = -1;
    vis->video_codec_context = NULL;
    vis->ret = -1;

    while (vis != NULL && vis)
    {
        free(vis);
        break;
    }

    vis = NULL;
}

static void callback_thread_completion(napi_env env, napi_status status, void *data)
{
    av_log(NULL, AV_LOG_DEBUG, "callback_thread_completion ........ \n");

    if (async_work_info.ref != NULL)
    {
        napi_status status = napi_delete_reference(env, async_work_info.ref);
        av_log(NULL, AV_LOG_DEBUG, "napi_delete_reference status : %d \n", status);
        async_work_info.ref = NULL;
    }

    if (async_work_info.work != NULL)
    {
        napi_status status = napi_delete_async_work(env, async_work_info.work);
        av_log(NULL, AV_LOG_DEBUG, "napi_delete_async_work status : %d \n", status);
        async_work_info.work = NULL;
    }
}

void finalize(napi_env env, void *data, void *hint)
{
    av_log(NULL, AV_LOG_DEBUG, "finalize consumer ........ \n");
}

napi_value async_handle_video_to_image_buffer_threadly(napi_env env, napi_callback_info info)
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
    if (vis == NULL || vis->ret < 0)
    {
        napi_throw_error(env, NULL, "initReadingVideo method should be invoke first");
    }

    napi_value resource_name;
    status = napi_create_string_utf8(env, "callback", NAPI_AUTO_LENGTH, &resource_name);
    av_log(NULL, AV_LOG_DEBUG, "async_handle_video_to_image_buffer_threadly --> napi_create_string_utf8 %d\n", status);

    ReadImageBufferParams _params = {
        .chose_frames = chose_frames,
        .quality = quality,
        .type = type};

    params = _params;

    napi_threadsafe_function result;
    napi_create_threadsafe_function(env, callback, NULL, resource_name, 1, 1, NULL, &finalize, NULL, &consumer_callback_threadsafe, &result);
    napi_ref_threadsafe_function(env, result);

    async_work_info.func = result;
    thread = true;

    napi_value async_resource = NULL;
    status = napi_create_async_work(env, async_resource, resource_name, &callback_thread, &callback_thread_completion, NULL, &(async_work_info.work));
    av_log(NULL, AV_LOG_DEBUG, "async_handle_video_to_image_buffer_threadly --> napi_create_async_work %d\n", status);

    napi_queue_async_work(env, async_work_info.work);

    return NULL;
}

/**
 * 释放视频连接
 **/
napi_value handle_destroy_stream(napi_env env, napi_callback_info info)
{
    napi_status status;
    av_log(NULL, AV_LOG_INFO, "FFmpeg-Nodejs: destroy stream handle ... \n");
    setBreak(true);

    if (async_work_info.func != NULL)
    {
        while (true)
        {
            setBreak(true);
            av_usleep(1);
            // status = napi_acquire_threadsafe_function(async_work_info.func);
            // av_log(NULL, AV_LOG_DEBUG, "napi_acquire_threadsafe_function status : %d \n", status);
            // if (status == napi_closing)
            // {
            //    break;
            // }

            status = napi_release_threadsafe_function(async_work_info.func, napi_tsfn_abort);
            av_log(NULL, AV_LOG_DEBUG, "napi_release_threadsafe_function status : %d \n", status);
            break;
        }

        napi_status status = napi_unref_threadsafe_function(env, async_work_info.func);
        av_log(NULL, AV_LOG_DEBUG, "napi_unref_threadsafe_function status : %d \n", status);
    }
    else
    {
        if (async_work_info.ref != NULL)
        {
            napi_status status = napi_delete_reference(env, async_work_info.ref);
            av_log(NULL, AV_LOG_DEBUG, "napi_delete_reference status : %d \n", status);
            async_work_info.ref = NULL;
        }

        if (async_work_info.work != NULL)
        {
            napi_status status = napi_delete_async_work(env, async_work_info.work);
            av_log(NULL, AV_LOG_DEBUG, "napi_delete_async_work status : %d \n", status);
            async_work_info.work = NULL;
        }

        release(vis->video_codec_context, vis->format_context, vis->init);

        vis->format_context = NULL;
        vis->video_stream_idx = -1;
        vis->video_codec_context = NULL;
        vis->ret = -1;

        while (vis != NULL && vis)
        {
            free(vis);
            break;
        }

        vis = NULL;
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

uv_async_t async;
struct async_send_info
{
    napi_env env;
    int out;
    napi_ref callback;
    napi_async_context context;
};

struct async_info
{
    uv_work_t req;
    napi_env env;
    int input;
    int output;
    napi_ref callback;
    napi_async_context context;
};

void async_work_callback(uv_work_t *r)
{
    printf("async_work_callback \n");
    struct async_info *req = r->data;
    // Simulate CPU intensive process...
    av_usleep(1000);

}

void after_async_work_callback(uv_work_t* req, int s) {
    printf("after_async_work_callback status: %d \n", s);

    struct async_info* data = req->data;
    napi_env env = data->env;
    napi_handle_scope scope;
    napi_status status = napi_open_handle_scope(env, &scope);
    printf("napi_open_handle_scope \n");
    if (status != napi_ok) {
        napi_throw_error(env, NULL, "napi_open_handle_scope error");
        return;
    }

    napi_async_context context = data->context;

    napi_value recv;
    status = napi_create_int64(env, data->output, &recv);
    printf("napi_create_int64 %d \n", data->output);
    if (status != napi_ok) {
        napi_throw_error(env, NULL, "napi_create_int64 error");
        return;
    }

    napi_value callback;
    printf("napi_get_reference_value \n");
    status = napi_get_reference_value(env, data->callback, &callback);
    if (status != napi_ok) {
        napi_throw_error(env, NULL, "napi_get_reference_value error");
        return;
    }

    // napi_value global;
    // status = napi_get_global(env, &global);
    // printf("napi_get_global \n");
    // if (status != napi_ok) {
    //     napi_throw_error(env, NULL, "napi_get_global error");
    //     return;
    // }

    napi_value result;
    printf("before napi_make_callback \n");
    status = napi_make_callback(env, context, callback, callback, 1, &recv, &result);
    printf("napi_make_callback \n");
    if (status != napi_ok) {
        napi_throw_error(env, NULL, "napi_make_callback error");
        return;
    }

    // printf("before napi_call_function \n");
    // status = napi_call_function(env, global, callback, 1, &recv, &result);
    // printf("napi_call_function \n");
    // if (status != napi_ok) {
    //     napi_throw_error(env, NULL, "napi_call_function error");
    //     return NULL;
    // }

    // status = napi_close_handle_scope(env, &scope);
    // printf("napi_close_handle_scope \n");
    // if (status != napi_ok) {
    //     napi_throw_error(env, NULL, "napi_close_handle_scope error");
    //     return NULL;
    // }

    status = napi_async_destroy(env, data->context);
    printf("napi_async_destroy \n");
    if (status != napi_ok) {
        napi_throw_error(env, NULL, "napi_make_callback error");
        return;
    }

   return;
}

napi_value *say_hello_callback(napi_env env, napi_callback_info info)
{
    printf("Hello Callback!\n");
    return NULL;
}

void async_work_job(uv_async_t* handle)
{
    /*
    struct async_info* data = handle->data;
    napi_env env = data->env;
    napi_handle_scope scope;
    napi_status status = napi_open_handle_scope(env, &scope);
    printf("napi_open_handle_scope \n");
    if (status != napi_ok) {
        napi_throw_error(env, NULL, "napi_open_handle_scope error");
    }

    napi_async_context context = data->context;

    napi_value recv;
    status = napi_create_int64(env, data->output, &recv);
    printf("napi_create_int64 %d \n", data->output);
    if (status != napi_ok) {
        napi_throw_error(env, NULL, "napi_create_int64 error");
    }

    napi_value callback;
    printf("napi_get_reference_value \n");
    status = napi_get_reference_value(env, data->callback, &callback);
    if (status != napi_ok) {
        napi_throw_error(env, NULL, "napi_get_reference_value error");
    }

    napi_value result;
    printf("before napi_make_callback \n");
    status = napi_make_callback(env, context, callback, callback, 1, &recv, &result);
    printf("napi_make_callback \n");
    if (status != napi_ok) {
        napi_throw_error(env, NULL, "napi_make_callback error");
    }

    status = napi_async_destroy(env, data->context);
    printf("napi_async_destroy \n");
    if (status != napi_ok) {
        napi_throw_error(env, NULL, "napi_make_callback error");
    }
    */
}

napi_value say_hello(napi_env env, napi_callback_info info)
{
    printf("Begin Hello\n");

    size_t argc = 2;
    napi_value argv[2];
    napi_value thisArg;
    void *data = NULL;
    napi_status status = napi_get_cb_info(env, info, &argc, argv, &thisArg, &data);

    if (status != napi_ok)
    {
        napi_throw_error(env, NULL, "Failed to parse arguments");
    }

    // int input
    int input;
    status = napi_get_value_int32(env, argv[0], &input);
    if (status != napi_ok)
    {
        napi_throw_error(env, NULL, "input is invalid number");
    }
    av_log(NULL, AV_LOG_DEBUG, "input input: %d\n", input);

    // callback
    napi_value callback = argv[1];
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
    napi_ref fn_ref;
    status = napi_create_reference(env, callback, 1, &fn_ref);

    // uv event loop
    struct uv_loop_s loop;
    struct uv_loop_s *loop_ptr = &loop;

    napi_get_uv_event_loop(env, &loop_ptr);

    // context
    napi_async_context context;
    napi_value resource_name;
    status = napi_create_string_utf8(env, "callback", NAPI_AUTO_LENGTH, &resource_name);
    napi_async_init(env, NULL, resource_name, &context);

    struct async_info req_data = {
        .env = env,
        .input = input,
        .output = 0,
        .context = context,
        .callback = fn_ref
    };
    uv_work_t req;
    req.data = (void*)&req_data;

    uv_queue_work(loop_ptr, &req, &async_work_callback, &after_async_work_callback);
    uv_async_init(loop_ptr, &async, &async_work_job);

    int tmp = 0;
    struct async_send_info _data = {
        .env = env,
        .callback = fn_ref,
        .context = context
    };
    for (size_t i = 0; i < 1; i++)
    {
        tmp = req_data.input + 2 * i;
        req_data.output = tmp;
        _data.out = tmp;
    }
    // async.data = (void*) &(_data);
    // uv_async_send(&async);
    // uv_run(loop_ptr, UV_RUN_NOWAIT);
    // uv_stop(loop_ptr);

    printf("End Hello\n");
    return NULL;
}

/**
 * napi 构建js的方法
 **/
static napi_value init(napi_env env, napi_value exports)
{
    napi_status status;

    napi_value fn;
    const char *methodName = "sayHello";
    status = napi_create_function(env, methodName, NAPI_AUTO_LENGTH, say_hello, NULL, &fn);
    if (status != napi_ok)
        return NULL;

    status = napi_set_named_property(env, exports, methodName, fn);
    if (status != napi_ok)
        return NULL;

    napi_property_descriptor methods[] = {
        DECLARE_NAPI_METHOD("initReadingVideo", handle_init_read_video),
        DECLARE_NAPI_METHOD("syncReadImageBuffer", sync_handle_video_to_image_buffer),
        DECLARE_NAPI_METHOD("asyncReadImageBuffer", async_handle_video_to_image_buffer),
        DECLARE_NAPI_METHOD("destroyStream", handle_destroy_stream),
        DECLARE_NAPI_METHOD("asyncReadImageBufferThreadly", async_handle_video_to_image_buffer_threadly),
        DECLARE_NAPI_METHOD("recordVideo", handle_record_video)};

    status = napi_define_properties(env, exports, sizeof(methods) / sizeof(methods[0]), methods);
    if (status != napi_ok)
        return NULL;

    return exports;
}

// 初始化 js 模块
NAPI_MODULE(NODE_GYP_MODULE_NAME, init);
