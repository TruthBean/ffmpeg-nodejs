const fs = require('fs');
const FFmpegNode = require('../index')

// let rtsp_addr = "35435435345435345345345";
let rtsp_addr = "rtsp://admin:iec123456@192.168.1.71:554/unicast/c1/s0/live";
// let rtsp_addr = "rtsp://admin:123456@192.168.1.61:554/h264/ch1/main/av_stream";
// let rtsp_addr = "rtsp://admin:123456@192.168.1.68:554/h264/ch1/main/av_stream";
let dir = "/opt/ffmpeg_nodejs";

let type = FFmpegNode.TYPE();
let target_type = type.RGB;

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
}

let i = 0;

async function runWithoutCallback() {
    let ffmpegNode = await FFmpegNode.init(rtsp_addr, false);
    let image = null;
    // while (true) {
    try {
        image = await ffmpegNode.readImageStream(1, target_type, 80);
    } catch (error) {
        ffmpegNode.emit("error", error);
    }
    // if (image === null) break;

    let now = new Date()
    let name = dir + "/tmp/images/buffer-" + now.getHours() + "-" + now.getMinutes() + "-" + now.getSeconds() + "-" + i + suffix;
    console.info(name);
    await fs.writeFileSync(name, image);
}

function runWithCallback() {
    let ffmpegNode = FFmpegNode.init(rtsp_addr, false);
    ffmpegNode.then((obj) => {
        obj.onData(80, target_type, 1, async (buffer) => {
            let begin = new Date();
            // // 模拟延迟300ms
            // for (var start = Date.now(); Date.now() - start <= 300;) { }
            let t1 = new Date();
            console.info(t1.getTime() - begin.getTime());
            // begin = new Date();
            let name = dir + "/tmp/images/buffer-" + t1.getHours() + "-" + t1.getMinutes() + "-" + t1.getSeconds() + "-" + i + suffix;
            console.info(name);
            await fs.writeFileSync(name, buffer);
            console.info("====================================");
            if (t1.getSeconds() % 2 === 0) {
                obj.close();
            }
            console.info(t1.getSeconds(), i++);
        }).then();
        obj.on("error", (error) => {
            console.log("on data error............");
            console.error(error);
        });
    }).catch(error => {
        console.log("init error.....");
        console.error(error);
    });
}

let videoFilePath = dir + "/tmp/videos/buffers.mp4";
FFmpegNode.recordVideo(rtsp_addr, videoFilePath, 10);

// runWithCallback();

runWithoutCallback().then().catch(error => {
    console.error(error);
});

