docker stop ffmpeg_nodejs
docker rm ffmpeg_nodejs
docker run -it --name ffmpeg_nodejs -v ${pwd}:/opt/ffmpeg_nodejs truthbean/ffmpeg-node-docker:4.0.2-10-cuda9.2-ubuntu16 /bin/bash
# docker run -it --name ffmpeg_nodejs --runtime=nvidia -e NVIDIA_DRIVER_CAPABILITIES=compute,utility,video -v ${pwd}:/opt/ffmpeg_nodejs truthbean/ffmpeg-node-docker:4.0.2-10-cuda9.2 /bin/bash
