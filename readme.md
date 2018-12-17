# ffmpeg-nodejs

    The project call the ffmpeg API by c language to achieve work which is video frame-to-picture and video recording, and call nodejs's napi to provide nodejs calls.

## preparation
```bash
# install nodejs
# https://github.com/nodesource/distributions/blob/master/README.md

# install compilers
npm i cmake-js -g

x install clang cmake g++
```

## develop
    1. learn c

    2. learn ffmpeg
    https://ffmpeg.org/ffmpeg.html

    3. learn napi
    https://nodejs.org/dist/latest/docs/api/n-api.html

## usage
```JavaScript
const FFmpegNode = require('ffmpeg-nodejs');

const rtsp_addr = ... ;
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
    let ffmpegNode = FFmpegNode.init(rtsp_addr, 2, false, false, logLevel, 1);

    ffmpegNode.then((obj) => {
        console.info(targetType);
        obj.readImageStream(100, targetType, 1);
        obj.on("data", (buffer) => {
            let now = new Date();
            let name = dir + "/tmp/images/buffer-" + now.getHours() + "-" + now.getMinutes() + "-" + now.getSeconds() + "-" + (i++) + suffix;
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