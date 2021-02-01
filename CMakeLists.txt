cmake_minimum_required(VERSION 2.8)

project(ffmpeg_nodejs)

add_definitions(-std=c11)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c11")

MESSAGE(STATUS "CMAKE_JS_INC: ${CMAKE_JS_INC}")

include_directories(${CMAKE_JS_INC})

file(GLOB SOURCE_FILES "src/ffmpeg_nodejs.c")

MESSAGE(STATUS "CMAKE_SYSTEM_NAME: ${CMAKE_SYSTEM_NAME}")

if(CMAKE_SYSTEM_NAME MATCHES "Linux" AND UNIX)
    MESSAGE(STATUS "current platform: Linux")
    set(LINUX 1)
elseif(CMAKE_SYSTEM_NAME MATCHES "Windows" OR WIN32 OR CMAKE_HOST_WIN32)
    MESSAGE(STATUS "current platform: Windows")
    set(WINDOWS 1)
elseif(CMAKE_SYSTEM_NAME MATCHES "FreeBSD" AND UNIX)
    MESSAGE(STATUS "current platform: FreeBSD")
    set(FREEBSD 1)
elseif(CMAKE_SYSTEM_NAME MATCHES "MINGW" AND UNIX)
    MESSAGE(STATUS "current platform: MINGW")
    set(MINGW 1)
elseif(APPLE AND NOT UNIX AND CMAKE_HOST_APPLE)
    MESSAGE(STATUS "current platform: MacOS or iOS or watchOS")
elseif(UNIX)
    MESSAGE(STATUS "current platform: Unix")
else()
    MESSAGE(STATUS "other platform: ${CMAKE_SYSTEM_NAME}")
endif()

add_library(${PROJECT_NAME} SHARED ${SOURCE_FILES})

include(${CMAKE_ROOT}/Modules/ExternalProject.cmake)
MESSAGE(STATUS "project source dir: ${PROJECT_SOURCE_DIR}")
SET(CMAKE_MODULE_PATH ${CMAKE_ROOT}/Modules ${PROJECT_SOURCE_DIR}/cmake/module)

set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "" SUFFIX ".node")

target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_JS_INC}")

target_link_libraries(${PROJECT_NAME} ${CMAKE_JS_LIB})

if(WINDOWS)
    # TODO
else()
    # thread
    target_link_libraries(${PROJECT_NAME} "-lpthread")

    # target_link_libraries(${PROJECT_NAME} "-logg")
    # target_link_libraries(${PROJECT_NAME} "-lfribidi")
    # target_link_libraries(${PROJECT_NAME} "-lfontconfig")
    # target_link_libraries(${PROJECT_NAME} "-lharfbuzz")
    # target_link_libraries(${PROJECT_NAME} "-lglib-2.0")
    # target_link_libraries(${PROJECT_NAME} "-lgraphite2")
endif(WINDOWS)

#######################################################################################################

# node
if(WINDOWS)
    include_directories("${PROJECT_SOURCE_DIR}/packages/node-v12.20.1/include")
    target_link_libraries (${PROJECT_NAME} "${PROJECT_SOURCE_DIR}/packages/node-v12.20.1/lib/node.lib")
endif()

# libjpeg
if(WINDOWS)
    include_directories("${PROJECT_SOURCE_DIR}/packages/libjpeg-turbo-2.0.3/include")

    # if(CMAKE_CL_64)
        target_link_libraries (${PROJECT_NAME} "${PROJECT_SOURCE_DIR}/packages/libjpeg-turbo-2.0.3/lib/jpeg.lib")
        target_link_libraries (${PROJECT_NAME} "${PROJECT_SOURCE_DIR}/packages/libjpeg-turbo-2.0.3/lib/turbojpeg.lib")

        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${PROJECT_SOURCE_DIR}/packages/libjpeg-turbo-2.0.3/bin"
                $<TARGET_FILE_DIR:${PROJECT_NAME}>)
    # else(CMAKE_CL_64)
    #    message(STATUS "Not support!")
    # endif()
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
                URL https://github.com/libjpeg-turbo/libjpeg-turbo/archive/2.0.6.tar.gz
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
    include_directories("${PROJECT_SOURCE_DIR}/packages/ffmpeg-4.1.4/include")
    # if(CMAKE_CL_64)
        link_directories("${PROJECT_SOURCE_DIR}/packages/ffmpeg-4.1.4/lib")

        target_link_libraries (${PROJECT_NAME} "${PROJECT_SOURCE_DIR}/packages/ffmpeg-4.1.4/lib/avcodec-58.lib")
        target_link_libraries (${PROJECT_NAME} "${PROJECT_SOURCE_DIR}/packages/ffmpeg-4.1.4/lib/avdevice-58.lib")
        target_link_libraries (${PROJECT_NAME} "${PROJECT_SOURCE_DIR}/packages/ffmpeg-4.1.4/lib/avfilter-7.lib")
        target_link_libraries (${PROJECT_NAME} "${PROJECT_SOURCE_DIR}/packages/ffmpeg-4.1.4/lib/avformat-58.lib")
        target_link_libraries (${PROJECT_NAME} "${PROJECT_SOURCE_DIR}/packages/ffmpeg-4.1.4/lib/avutil-56.lib")
        target_link_libraries (${PROJECT_NAME} "${PROJECT_SOURCE_DIR}/packages/ffmpeg-4.1.4/lib/postproc-55.lib")
        target_link_libraries (${PROJECT_NAME} "${PROJECT_SOURCE_DIR}/packages/ffmpeg-4.1.4/lib/swresample-3.lib")
        target_link_libraries (${PROJECT_NAME} "${PROJECT_SOURCE_DIR}/packages/ffmpeg-4.1.4/lib/swscale-5.lib")

        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${PROJECT_SOURCE_DIR}/packages/ffmpeg-4.1.4/bin"
                $<TARGET_FILE_DIR:${PROJECT_NAME}>)
    # else()
    #    message(STATUS "Not support!")
    # endif()
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
                URL https://ffmpeg.org/releases/ffmpeg-4.1.6.tar.bz2
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