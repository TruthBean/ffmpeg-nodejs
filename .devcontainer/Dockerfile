FROM truthbean/ffmpeg-node-docker:4.1.3-12-ubuntu

RUN npm i npm cmake-js eslint -g --registry=https://registry.npm.taobao.org

# Switch back to dialog for any ad-hoc use of apt-get
ENV DEBIAN_FRONTEND=dialog