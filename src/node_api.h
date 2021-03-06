#ifndef _ADDON_H_
#define _ADDON_H_


/*
        1           2           3           4           5           6
v6.x                v6.14.2*
v8.x    v8.0.0*     v8.10.0*    v8.11.2     v8.16.0
v9.x    v9.0.0*     v9.3.0*     v9.11.0*
v10.x   v10.0.0     v10.0.0     v10.0.0     v10.16.0    v10.17.0    v10.20.0
v11.x   v11.0.0     v11.0.0     v11.0.0     v11.8.0
v12.x   v12.0.0     v12.0.0     v12.0.0     v12.0.0     v12.11.0    v12.17.0
v13.x   v13.0.0     v13.0.0     v13.0.0     v13.0.0     v13.0.0
v14.x   v14.0.0     v14.0.0     v14.0.0     v14.0.0     v14.0.0     v14.0.0
*/
#define NAPI_VERSION 6
#include <js_native_api.h>
#include <node_api.h>

#ifndef __USE_UNIX98
#define __USE_UNIX98
#endif

#ifdef _WIN32
#include <node/uv.h>
#else
#include <uv.h>
#endif // _WIN32


// Empty value so that macros here are able to return NULL or void
#define NAPI_RETVAL_NOTHING // Intentionally blank #define

#define GET_AND_THROW_LAST_ERROR(env)                                                                                          \
    do                                                                                                                         \
    {                                                                                                                          \
        const napi_extended_error_info *error_info = NULL;                                                                     \
        napi_get_last_error_info((env), &error_info);                                                                          \
        bool is_pending;                                                                                                       \
        napi_is_exception_pending((env), &is_pending);                                                                         \
        /* If an exception is already pending, don't rethrow it */                                                             \
        if (!is_pending)                                                                                                       \
        {                                                                                                                      \
            const char *error_message = error_info->error_message != NULL ? error_info->error_message : "empty error message"; \
            napi_throw_error((env), NULL, error_message);                                                                      \
            return NULL                                                                                                        \
        }                                                                                                                      \
    } while (0)

#define NAPI_ASSERT_BASE(env, assertion, message, ret_val)      \
    do                                                          \
    {                                                           \
        if (!(assertion))                                       \
        {                                                       \
            napi_throw_error(                                   \
                (env),                                          \
                NULL,                                           \
                "assertion (" #assertion ") failed: " message); \
            return ret_val;                                     \
        }                                                       \
    } while (0)

// Returns NULL on failed assertion.
// This is meant to be used inside napi_callback methods.
#define NAPI_ASSERT(env, assertion, message) \
    NAPI_ASSERT_BASE(env, assertion, message, NULL)

// Returns empty on failed assertion.
// This is meant to be used inside functions with void return type.
#define NAPI_ASSERT_RETURN_VOID(env, assertion, message) \
    NAPI_ASSERT_BASE(env, assertion, message, NAPI_RETVAL_NOTHING)

#define NAPI_CALL_BASE(env, the_call, ret_val) \
    do                                         \
    {                                          \
        if ((the_call) != napi_ok)             \
        {                                      \
            GET_AND_THROW_LAST_ERROR((env));   \
            return ret_val;                    \
        }                                      \
    } while (0)

// Returns NULL if the_call doesn't return napi_ok.
#define NAPI_CALL(env, the_call) \
    NAPI_CALL_BASE(env, the_call, NULL)

// Returns empty if the_call doesn't return napi_ok.
#define NAPI_CALL_RETURN_VOID(env, the_call) \
    NAPI_CALL_BASE(env, the_call, NAPI_RETVAL_NOTHING)

#define DECLARE_NAPI_PROPERTY(name, func)                          \
    {                                                              \
        (name), NULL, (func), NULL, NULL, NULL, napi_default, NULL \
    }

#define DECLARE_NAPI_GETTER(name, func)                            \
    {                                                              \
        (name), NULL, NULL, (func), NULL, NULL, napi_default, NULL \
    }

#define DECLARE_NAPI_METHOD(name, func)         \
    {                                           \
        name, 0, func, 0, 0, 0, napi_default, 0 \
    }

napi_value create_addon(napi_env env);

#endif //_ADDON_H_