include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(FFMPEG
        FOUND_VAR FFMPEG_FOUND
        REQUIRED_VARS
        FFMPEG_LIBRARY
        FFMPEG_INCLUDE_DIR
        VERSION_VAR FFMPEG_VERSION
        )

if(FFMPEG_LIBRARIES AND FFMPEG_INCLUDE_DIR)
    # in cache already
    set(FFMPEG_FOUND TRUE)
else()
    # use pkg-config to get the directories and then use these values
    # in the FIND_PATH() and FIND_LIBRARY() calls
    find_package(PkgConfig)
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(_FFMPEG_AVCODEC libavcodec)
        pkg_check_modules(_FFMPEG_AVFORMAT libavformat)
        pkg_check_modules(_FFMPEG_AVUTIL libavutil)
        pkg_check_modules(_FFMPEG_SWSCALE libswscale)
        pkg_check_modules(_FFMPEG_AVDEVICE libavdevice)
        pkg_check_modules(_FFMPEG_AVFILTER libavfilter)
    endif()

    find_path(FFMPEG_AVCODEC_INCLUDE_DIR
            NAMES libavcodec/avcodec.h
            PATHS ${_FFMPEG_AVCODEC_INCLUDE_DIRS}
            /usr/include
            /usr/local/include
            /opt/local/include
            /sw/include
            PATH_SUFFIXES ffmpeg libav)

    find_library(FFMPEG_LIBAVCODEC
            NAMES avcodec
            PATHS ${_FFMPEG_AVCODEC_LIBRARY_DIRS}
            /usr/lib
            /usr/local/lib
            /opt/local/lib
            /sw/lib)

    find_library(FFMPEG_LIBAVFORMAT
            NAMES avformat
            PATHS ${_FFMPEG_AVFORMAT_LIBRARY_DIRS}
            /usr/lib
            /usr/local/lib
            /opt/local/lib
            /sw/lib)

    find_library(FFMPEG_LIBAVUTIL
            NAMES avutil
            PATHS ${_FFMPEG_AVUTIL_LIBRARY_DIRS}
            /usr/lib
            /usr/local/lib
            /opt/local/lib
            /sw/lib)

    find_path(FFMPEG_AVDEVICE_INCLUDE_DIR
            NAMES libavdevice/avdevice.h
            PATHS ${_FFMPEG_AVDEVICE_INCLUDE_DIRS}
            /usr/lib
            /usr/local/lib
            /opt/local/lib
            /sw/lib)

    find_library(FFMPEG_LIBAVDEVICE
            NAMES avdevice
            PATHS ${_FFMPEG_AVDEVICE_INCLUDE_DIRs}
            /usr/lib
            /usr/local/lib
            /opt/local/lib
            /sw/lib)

    find_path(FFMPEG_AVFILTER_INCLUDE_DIR
            NAMES libavfilter/avfilter.h
            PATHS ${_FFMPEG_AVFILTER_INCLUDE_DIRS}
            /usr/lib
            /usr/local/lib
            /opt/local/lib
            /sw/lib)

    find_library(FFMPEG_LIBAVFILTER
            NAMES avfilter
            PATHS ${_FFMPEG_AVFILTER_INCLUDE_DIRS}
            /usr/lib
            /usr/local/lib
            /opt/local/lib
            /sw/lib)

    find_library(FFMPEG_LIBSWSCALE
            NAMES swscale
            PATHS ${_FFMPEG_SWSCALE_INCLUDE_DIRS}
            /usr/lib
            /usr/local/lib
            /opt/local/lib
            /sw/lib)

    if(FFMPEG_LIBAVCODEC AND FFMPEG_LIBAVFORMAT)
        set(FFMPEG_FOUND TRUE)
    endif()

    if(FFMPEG_FOUND)
        set(FFMPEG_INCLUDE_DIR ${FFMPEG_AVCODEC_INCLUDE_DIR})
        set(FFMPEG_LIBRARIES
                ${FFMPEG_LIBAVCODEC}
                ${FFMPEG_LIBAVFORMAT}
                ${FFMPEG_LIBAVUTIL}
                ${FFMPEG_LIBAVDEVICE}
                ${FFMPEG_LIBAVFILTER}
                ${FFMPEG_LIBSWSCALE})
    endif()

    if(FFMPEG_FOUND)
        if(NOT FFMPEG_FIND_QUIETLY)
            message(STATUS
                    "Found FFMPEG or Libav: ${FFMPEG_LIBRARIES}, ${FFMPEG_INCLUDE_DIR}")
        endif()
    else()
        if(FFMPEG_FIND_REQUIRED)
            message(FATAL_ERROR
                    "Could not find libavcodec or libavformat or libavutil")
        endif()
    endif()
endif()