{
    "targets": [
        {
            "target_name": "ffmpeg_nodejs",
            "sources": [ "src/ffmpeg_nodejs.c" ],
            "libraries": [
                "-lm",
                "-lswscale",
                "-lswresample",
                "-lavfilter",
                "-lavcodec",
                "-lavformat",
                "-lavutil",
                "-lavdevice",
                "-ljpeg",
                # "-lturbojpeg"
                ]
        }
    ]
}