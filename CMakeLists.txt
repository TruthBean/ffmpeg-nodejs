cmake_minimum_required(VERSION 2.8)

project(ffmpeg_nodejs)

add_definitions(-std=c11)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c11")

MESSAGE(STATUS "CMAKE_JS_INC: ${CMAKE_JS_INC}")

include_directories(${CMAKE_JS_INC})

file(GLOB SOURCE_FILES "src/ffmpeg_nodejs.c")

add_library(${PROJECT_NAME} SHARED ${SOURCE_FILES})

include(${CMAKE_ROOT}/Modules/ExternalProject.cmake)
SET(CMAKE_MODULE_PATH ${CMAKE_ROOT}/Modules ${PROJECT_SOURCE_DIR}/cmake/module)

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

#######################################################################################################

# libjpeg
if(WINDOWS)
    # TODO
else()
    find_package(TurboJpeg REQUIRED)
    mark_as_advanced(LIB_TURBO_JPEG_INCLUDE_DIR LIB_TURBO_JPEG_LIBRARIES)
    message(STATUS "LIB_TURBO_JPEG_INCLUDE_DIR: " "${LIB_TURBO_JPEG_INCLUDE_DIR}")
    message(STATUS "LIB_TURBO_JPEG_LIBRARIES: " "${LIB_TURBO_JPEG_LIBRARIES}")
    if (LIB_TURBO_JPEG_INCLUDE_DIR)
        include_directories(${LIB_TURBO_JPEG_INCLUDE_DIR})
        if(LIB_TURBO_JPEG_LIBRARIES)
            target_link_libraries(${PROJECT_NAME} ${LIB_TURBO_JPEG_LIBRARIES})
        else(LIB_TURBO_JPEG_LIBRARIES)
            target_link_libraries(${PROJECT_NAME} "-ljpeg")
            target_link_libraries(${PROJECT_NAME} "-lturbojpeg")
        endif(LIB_TURBO_JPEG_LIBRARIES)
    else()
        message(STATUS "ExternalProject_Add libjpeg...................")

        ExternalProject_Add(
                libjpeg
                URL https://github.com/libjpeg-turbo/libjpeg-turbo/archive/2.0.1.tar.gz
                PREFIX ${PROJECT_SOURCE_DIR}/deps
                BUILD_IN_SOURCE false
                CONFIGURE_COMMAND cmake -D CMAKE_BUILD_TYPE=Release -D CMAKE_INSTALL_PREFIX=${PROJECT_SOURCE_DIR}/deps/build/libjpeg ../libjpeg
                BUILD_COMMAND make -j8

        )
        add_dependencies(${PROJECT_NAME} libjpeg)

        include_directories("${PROJECT_SOURCE_DIR}/deps/build/libjpeg/include")
        link_directories("${PROJECT_SOURCE_DIR}/deps/build/libjpeg/lib")
        target_link_libraries (${PROJECT_NAME} "${PROJECT_SOURCE_DIR}/deps/build/libjpeg/lib/libjpeg.so")

        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${PROJECT_SOURCE_DIR}/deps/build/libjpeg/lib"
                $<TARGET_FILE_DIR:${PROJECT_NAME}>)
    endif()
endif()

#######################################################################################################

# ffmpeg
if(WINDOWS)
    # TODO
else()
    find_package(FFmpeg REQUIRED)
    if(FFMPEG_FOUND)
        message(STATUS "Found FFMPEG/LibAV libraries")
        message(STATUS "FFMPEG_LIBRARY: " "${FFMPEG_LIBRARY}")
        message(STATUS "FFMPEG_INCLUDE_DIR: " "${FFMPEG_INCLUDE_DIR}")
        message(STATUS "FFMPEG_LIBRARIES: " "${FFMPEG_LIBRARIES}")
        include_directories(${FFMPEG_INCLUDE_DIR})
        target_link_libraries(${PROJECT_NAME} ${FFMPEG_LIBRARIES})
    else()
        message(STATUS "ExternalProject_Add ffmpeg...................")

        ExternalProject_Add(
                ffmpeg
                URL https://ffmpeg.org/releases/ffmpeg-4.1.tar.bz2
                PREFIX ${PROJECT_SOURCE_DIR}/deps
                BUILD_IN_SOURCE false
                CONFIGURE_COMMAND bash ../ffmpeg/configure --prefix=${PROJECT_SOURCE_DIR}/deps/build/ffmpeg --enable-gpl --enable-version3 --enable-nonfree --disable-static --enable-shared --enable-network --disable-avresample --enable-protocols --enable-encoders --enable-decoders --enable-muxers --enable-demuxers --enable-parsers --enable-bsfs --enable-indevs --enable-outdevs --enable-filters
                BUILD_COMMAND make -j8

        )
        add_dependencies(${PROJECT_NAME} ffmpeg)

        include_directories("${PROJECT_SOURCE_DIR}/deps/build/ffmpeg/include")
        link_directories("${PROJECT_SOURCE_DIR}/deps/build/ffmpeg/lib")
        target_link_libraries (${PROJECT_NAME} "${PROJECT_SOURCE_DIR}/deps/build/ffmpeg/lib/libavcodec.so")
        target_link_libraries (${PROJECT_NAME} "${PROJECT_SOURCE_DIR}/deps/build/ffmpeg/lib/libavdevice.so")
        target_link_libraries (${PROJECT_NAME} "${PROJECT_SOURCE_DIR}/deps/build/ffmpeg/lib/libavcodec.so")
        target_link_libraries (${PROJECT_NAME} "${PROJECT_SOURCE_DIR}/deps/build/ffmpeg/lib/libavfilter.so")
        target_link_libraries (${PROJECT_NAME} "${PROJECT_SOURCE_DIR}/deps/build/ffmpeg/lib/libavformat.so")
        target_link_libraries (${PROJECT_NAME} "${PROJECT_SOURCE_DIR}/deps/build/ffmpeg/lib/libavutil.so")
        target_link_libraries (${PROJECT_NAME} "${PROJECT_SOURCE_DIR}/deps/build/ffmpeg/lib/libswresample.so")
        target_link_libraries (${PROJECT_NAME} "${PROJECT_SOURCE_DIR}/deps/build/ffmpeg/lib/libswscale.so")
        target_link_libraries (${PROJECT_NAME} "${PROJECT_SOURCE_DIR}/deps/build/ffmpeg/lib/libpostproc.so")

        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${PROJECT_SOURCE_DIR}/deps/build/ffmpeg/lib"
                $<TARGET_FILE_DIR:${PROJECT_NAME}>)
    endif()

endif()

#######################################################################################################