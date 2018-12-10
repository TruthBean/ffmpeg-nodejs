cmake_minimum_required(VERSION 2.8)

project(ffmpeg_node)

add_definitions(-std=c11)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c11")

# include_directories("/usr/local/include")

MESSAGE(STATUS "CMAKE_JS_INC: ${CMAKE_JS_INC}")

include_directories(${CMAKE_JS_INC})

file(GLOB SOURCE_FILES "src/ffmpeg_nodejs.c")

add_library(${PROJECT_NAME} SHARED ${SOURCE_FILES})
set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "" SUFFIX ".node")

target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_JS_INC}")

# include_directories("/usr/include/node/uv/")
# include_directories("/usr/include/node/")

#target_include_directories(${PROJECT_NAME} PRIVATE
#   ${CMAKE_SOURCE_DIR}/node_modules/node-addon-api
#   ${CMAKE_SOURCE_DIR}/node_modules/node-addon-api/src
#   ${CMAKE_JS_INC})

target_link_libraries(${PROJECT_NAME} ${CMAKE_JS_LIB})

# thread
target_link_libraries(${PROJECT_NAME} "-lpthread")

# ffmpeg
target_link_libraries(${PROJECT_NAME} "-lm")
target_link_libraries(${PROJECT_NAME} "-lswscale")
target_link_libraries(${PROJECT_NAME} "-lswresample")
target_link_libraries(${PROJECT_NAME} "-lavfilter")
target_link_libraries(${PROJECT_NAME} "-lavcodec")
target_link_libraries(${PROJECT_NAME} "-lavformat")
target_link_libraries(${PROJECT_NAME} "-lavutil")
target_link_libraries(${PROJECT_NAME} "-lavdevice")

# libjpeg
target_link_libraries(${PROJECT_NAME} "-ljpeg")
# target_link_libraries(${PROJECT_NAME} "-lturbojpeg")