docker stop ffmpeg_nodejs
docker rm ffmpeg_nodejs
docker run -it --name ffmpeg_nodejs -v ${pwd}:/opt/ffmpeg_nodejs truthbean/ffmpeg-node-docker:4.0.2-node10-ubuntu
