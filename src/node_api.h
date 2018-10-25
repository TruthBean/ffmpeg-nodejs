#ifndef EXTERNAL_NODE_API_H_
#define EXTERNAL_NODE_API_H_

#define EXTERNAL_NAPI
#include "../node_modules/node-addon-api/src/node_api.h"

#define DECLARE_NAPI_METHOD(name, func)     \
  {                                         \
    name, 0, func, 0, 0, 0, napi_default, 0 \
  }

#endif //EXTERNAL_NODE_API_H_