MESSAGE(STATUS "Using bundled FindTurboJpeg.cmake...")

FIND_PATH(LIB_TURBO_JPEG_INCLUDE_DIR
        jpeglib.h turbojpeg.h
        "/usr/include/"
        "/usr/local/include/")

FIND_LIBRARY(LIB_TURBO_JPEG_LIBRARIES
        NAMES turboJpeg
        PATHS "/usr/lib/" "/usr/local/lib/")