FFmpeg-Nodejs
============

The project call the ffmpeg API by c language to achieve work which is video frame-to-picture and video recording, and call nodejs's napi to provide nodejs calls.

Preparation
-----------
```bash
# install nodejs
# https://github.com/nodesource/distributions/blob/master/README.md

# install compilers
npm i cmake-js -g

x install clang cmake g++
NOTE: x means your system package manager command, like apt, yum, dnf, or something else.
```

Develop
-------
1. learn javascript  
https://developer.mozilla.org/en-US/docs/Web/JavaScript

2. learn nodejs  
https://nodejs.org/dist/latest/docs/api/

3. learn c  
I suggest that you read the book "c primer plus"

4. learn cmake  
https://cmake.org/cmake/help/latest/

5. learn napi  
https://nodejs.org/dist/latest/docs/api/n-api.html

6. learn cmake-js  
https://www.npmjs.com/package/cmake-js

7. learn ffmpeg  
https://ffmpeg.org/ffmpeg.html


Compile
-------
If you had not installed ffmpeg-dev whose version is 4.x, and libjpeg, you should install nasm and pkg-config first

Then compiling command is as follows:
```bash
npm install (or npm run compile or yarn build)
```
If you want more command, please see package.json scripts, and do not use cmake or make directly, because it not a pure c project, it is a NODEJS project.

Or using cmake:
````shell
cmake "/mnt/z/U/DevOps/javascript/ffmpeg-nodejs" --no-warn-unused-cli -G"Unix Makefiles" -DCMAKE_JS_VERSION="6.1.0" \
-DCMAKE_BUILD_TYPE="Release" -DCMAKE_LIBRARY_OUTPUT_DIRECTORY="/mnt/z/U/DevOps/javascript/ffmpeg-nodejs/build/Release" \
-DCMAKE_JS_INC="/home/truthbean/.cmake-js/node-x64/v12.20.1/include/node" -DCMAKE_JS_SRC="" -DNODE_RUNTIME="node" \
-DNODE_RUNTIMEVERSION="12.20.1" -DNODE_ARCH="x64" \
-DCMAKE_CXX_COMPILER:FILEPATH=/usr/bin/clang -DCMAKE_C_COMPILER:FILEPATH=/usr/bin/clang -DCMAKE_BUILD_TYPE:STRING=RELEASE
````
### note
windows not support c11 yet, so don't use it on windows!

### test in docker
```shell
docker run -it --name ffmpeg_nodejs --runtime=nvidia -e NVIDIA_DRIVER_CAPABILITIES=compute,utility,video -v "$(pwd)":/opt/ffmpeg_nodejs \
  truthbean/cuda-ubuntu-node-ffmpeg:10.1-16-12-4.1 /bin/bash

# docker run -it --name ffmpeg_nodejs -v "$(pwd)":/opt/ffmpeg_nodejs truthbean/ffmpeg-node-docker:4.1.3-10-ubuntu /bin/bash
```

```powershell
docker run -it --name ffmpeg_nodejs -v ${pwd}:/opt/ffmpeg_nodejs truthbean/ffmpeg-node-docker:4.0.2-10-cuda9.2-ubuntu16 /bin/bash
# docker run -it --name ffmpeg_nodejs --runtime=nvidia -e NVIDIA_DRIVER_CAPABILITIES=compute,utility,video -v ${pwd}:/opt/ffmpeg_nodejs truthbean/ffmpeg-node-docker:4.0.2-10-cuda9.2 /bin/bash

```

Examples
--------
```JavaScript
const FFmpegNode = require('ffmpeg-nodejs');

const video_addr = ... ;
const dir = ... ;

const type = FFmpegNode.TYPE();
const targetType = type.JPEG;

const logLevel = FFmpegNode.LEVEL().INFO;

let suffix = ".yuv";
switch (targetType) {
    case type.YUV:
        suffix = ".yuv";
        break;
    case type.RGB:
        suffix = ".rgb";
        break;
    case type.JPEG:
        suffix = ".jpg";
        break;
}

let i = 0;

function runWithCallback() {
    let ffmpegNode = FFmpegNode.init(video_addr, 2, false, false, logLevel, 1, true);

    ffmpegNode.then((obj) => {
        console.info(targetType);
        obj.readImageStreamThreadly(100, targetType, 1);
        obj.on("data", (buffer) => {
            let now = new Date();
            let name = dir + "/tmp/images/buffer-" + now.getHours() + "-" + 
                        now.getMinutes() + "-" + now.getSeconds() + "-" + 
                        (i++) + suffix;
            fs.writeFileSync(name, buffer);
        });

        obj.on("error", (err) => {
            console.error(err);
            obj.close();
        });
    }).catch(err => {
        console.error(err);
    });
}

runWithCallback();
```