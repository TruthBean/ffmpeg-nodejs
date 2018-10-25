#include "./ffmpeg_nodejs.h"

static Video2ImageStream vis;

static void callback_fileinfo(napi_env env, int file_size, char *file_info, napi_value *result)
{
    // 文件内容转换成javascript buffer
    const unsigned int kBufferSize = file_size;
    char *buffer_data;
    napi_status status = napi_create_buffer(env, kBufferSize, (void **)(&buffer_data), result);
    memcpy(buffer_data, file_info, kBufferSize);
    free(file_info);
    // fprintf(stdout, "拷贝内容: %s\n", buffer_data);
    if (status != napi_ok)
    {
        napi_throw_error(env, NULL, "Create buffer error");
    }
}

napi_value handle_init_read_video(napi_env env, napi_callback_info info)
{
    napi_status status;
    napi_value result;

    size_t argc = 2;
    napi_value argv[2];
    napi_value *thisArg = NULL;
    void *data = NULL;
    status = napi_get_cb_info(env, info, &argc, argv, thisArg, &data);

    if (status != napi_ok)
    {
        napi_throw_error(env, NULL, "Failed to parse arguments");
    }

    size_t length;

    // input filename ==================================================================
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
    fprintf(stdout, "input filename : %s\n", input_filename);

    vis = open_inputfile(input_filename);

    return result;
}

napi_value handle_video2image_stream(napi_env env, napi_callback_info info)
{
    napi_status status;
    napi_value result;

    size_t argc = 1;
    napi_value argv[1];
    napi_value *thisArg = NULL;
    void *data = NULL;
    status = napi_get_cb_info(env, info, &argc, argv, thisArg, &data);

    if (status != napi_ok)
    {
        napi_throw_error(env, NULL, "Failed to parse arguments");
    }

    size_t length;

    // quality ==========================================================================
    int quality = 80;
    status = napi_get_value_int32(env, argv[0], &quality);
    if (status != napi_ok)
    {
        napi_throw_error(env, NULL, "quality is invalid number");
    }
    fprintf(stdout, "input quality: %d\n", quality);

    // 处理图片
    if (&vis == NULL)
    {
        napi_throw_error(env, NULL, "initReadingVideo method should be invoke first");
    }
    int ret = video2images_stream(vis, quality, env, &result, callback_fileinfo);
    fprintf(stdout, "video2images result: %d\n", ret);
    return result;
}

napi_value handle_destroy_stream(napi_env env, napi_callback_info info)
{
    napi_status status;
    napi_value result;

    release(vis.packet, vis.frame, vis.video_codec_context, vis.format_context, vis.isRtsp);
    Video2ImageStream _vis = { 
        .format_context         = NULL,
        .packet                 = NULL,
        .video_stream_idx       = -1,
        .video_codec_context    = NULL,
        .frame                  = NULL,
        .ret                    = -1
        };

    vis = _vis;

    return result;
}

napi_value init(napi_env env, napi_value exports)
{
    napi_status status;
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