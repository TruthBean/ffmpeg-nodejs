const fs = require('fs');
const FFmpegNode = require('../index');

// let rtsp_addr = "35435435345435345345345";
let rtsp_addr = "rtsp://admin:iec123456@192.168.1.71:554/unicast/c1/s0/live";
// let rtsp_addr = "rtsp://admin:iec123456@192.168.1.72:554/unicast/c1/s0/live";
// let rtsp_addr = "rtsp://admin:123456@192.168.1.61:554/h264/ch1/main/av_stream";
// let rtsp_addr = "rtsp://admin:123456@192.168.1.68:554/h264/ch1/main/av_stream";
let dir = "/opt/ffmpeg_nodejs";
dir = "/mnt/h/oceanai-workspace/ffmpeg-node-cmake";
// dir = "/media/oceanai/DevOps/oceanai-workspace/ffmpeg-node-cmake";

const type = FFmpegNode.TYPE();
const target_type = type.JPEG;

let suffix = ".yuv";
switch (target_type) {
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

async function runWithoutCallback() {
    let ffmpegNode = await FFmpegNode.init(rtsp_addr, false, true);
    let image = null;
    try {
        image = await ffmpegNode.video2ImageBuffer(1, target_type, 80);
    } catch (error) {
        ffmpegNode.emit("error", error);
    }

    let now = new Date();
    let name = dir + "/tmp/images/buffer-" + now.getHours() + "-" + now.getMinutes() + "-" + now.getSeconds() + "-" + i + suffix;
    console.info(name);
    await fs.writeFileSync(name, image);
}

let __begin = new Date().getTime();

function runWithCallback() {
    let ffmpegNode = FFmpegNode.init(rtsp_addr, true, false);

    ffmpegNode.then((obj) => {
        console.info(target_type);
        obj.readImageStream(100, target_type, 1);
        obj.on("data", (buffer) => {
            let begin = new Date();
            console.info("------->>>>>>>>>>>", buffer);
    
            let a = new Promise((resolve, reject) => {
                resolve("555");
            });
        
            a.then(r => {
                console.info(r);
            });
    
            let now = new Date();
            let name = dir + "/tmp/images/buffer-" + now.getHours() + "-" + now.getMinutes() + "-" + now.getSeconds() + "-" + (i++) + suffix;
            console.info(name);
            fs.writeFileSync(name, buffer);
            let end = new Date();
            console.info("-----------", (end - begin));

            if (begin.getTime() - __begin > 60000) {
                obj.destroy();
            }
            
        });

        obj.on("error", (err) => {
            console.info("read image stream error ..... ");
            console.error(err);
        });
    }).catch(err => {
        console.info("init error ....");
        console.error(err);
    });
}

function callbackStream() {
    let ffmpegNode = FFmpegNode.init(rtsp_addr, false, true);
    ffmpegNode.then((obj) => {
        obj.on("error", (error) => {
            console.log("on data error............");
            console.error(error);
        });
    });
}

runWithCallback();

// new Promise((resolve, reject) => {
//     let videoFilePath = dir + "/tmp/videos/buffers.flv";
//     FFmpegNode.recordVideo(rtsp_addr, videoFilePath, 60, true);
//     resolve();
// }).then();


// runWithoutCallback().then().catch(error => {
//     console.error(error);
// });

