docker stop ffmpeg_nodejs
docker rm ffmpeg_nodejs

docker run -it --name ffmpeg_nodejs --runtime=nvidia -e NVIDIA_DRIVER_CAPABILITIES=compute,utility,video -v "$(pwd)":/home/node/ffmpeg_nodejs truthbean/ffmpeg-node-test
