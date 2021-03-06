const fs = require('fs');
const FFmpegNode = require('../index');

let video_addr = "http://ivi.bupt.edu.cn/hls/cctv5phd.m3u8";
video_addr = "rtsp://admin:iec123456@192.168.1.71:554/h264/ch5/main/av_stream";
// video_addr = "http://ivi.bupt.edu.cn/hls/cctv1.m3u8";
// video_addr = "rtsp://admin:123456@192.168.1.61:554/h264/ch1/main/av_stream";
// video_addr = "rtsp://admin:123456@192.168.1.68:554/h264/ch1/main/av_stream";
// video_addr = "rtsp://10.130.104.9/16854277_0"
let dir = "/opt/ffmpeg_nodejs/test";
dir = __dirname;

let level = FFmpegNode.LEVEL();
let type = FFmpegNode.TYPE();
let muxingStreamType = FFmpegNode.MuxingStreamType();
let target_type = type.JPEG;

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

async function testSyncReadImageBuffer() {
    let ffmpegNode = await FFmpegNode.init(video_addr, 10, true, true, level.INFO, 0, true);
    let image = null;
    while (true) {
        try {
            image = await ffmpegNode.readImageBuffer(80, target_type, 10);
        } catch (error) {
            // ffmpegNode.emit("error", error);
            console.error(error);
        }
        if (image === null) {
            ffmpegNode.close();
            break;
        }

        let now = new Date();
        let name = dir + "/tmp/images/buffer-" + now.getHours() + "-" + now.getMinutes() + "-" + now.getSeconds() + "-" + (i++) + suffix;
        console.info(name);
        fs.writeFileSync(name, image);
        console.info("====================================");
        // fs.rmSync(name, { recursive: true });
        // global.gc();
        if (now.getSeconds() % 50 === 0) {
            ffmpegNode.close();
            break;
        }
    }
}

function testAsyncReadImageBuffer() {
    let ffmpegNode = FFmpegNode.init(video_addr, 10, true, true, level.DEBUG, 0, true);
    ffmpegNode.then((obj) => {
        obj.readImageStream(100, target_type, 10);
        obj.on("data", (buffer) => {
            let begin = new Date();
            // 模拟延迟300ms
            // for (var start = Date.now(); Date.now() - start <= 300;) { }
            let t1 = new Date();
            // console.info(t1.getTime() - begin.getTime());
            // begin = new Date();
            let name = dir + "/tmp/images/buffer-" + t1.getHours() + "-" + t1.getMinutes() + "-" + t1.getSeconds() + "-" + (i++) + suffix;
            // console.info(name);
            fs.writeFileSync(name, buffer);
            // console.info("====================================");
            // fs.rmdirSync(name, { recursive: true });
            if (t1.getSeconds() % 50 === 0) {
                obj.close();
                return;
            }
            // console.info(t1.getSeconds(), i++);
        });
        obj.on("error", (error) => {
            console.log("on data error............");
            console.error(error);
            obj.close();
        });
    }).catch(error => {
        console.log("init error.....");
        console.error(error);
    });
}

function testAsyncReadImageBufferThreadly() {
    let ffmpegNode = FFmpegNode.init(video_addr, 10, true, true, level.DEBUG, 0, true);
    ffmpegNode.then((obj) => {
        obj.readImageStreamThreadly(80, target_type, 1);
        obj.on("data", (buffer) => {
            let begin = new Date();
            // 模拟延迟300ms
            // for (var start = Date.now(); Date.now() - start <= 300;) { }
            let t1 = new Date();
            console.info(t1.getTime() - begin.getTime());
            begin = new Date();
            let name = dir + "/tmp/images/buffer-" + t1.getHours() + "-" + t1.getMinutes() + "-" + t1.getSeconds() + "-" + (i++) + suffix;
            console.info(name);
            fs.writeFileSync(name, buffer);
            console.info("====================================");
            // fs.rmdirSync(name, { recursive: true });
            if (t1.getSeconds() % 50 === 0) {
                obj.close();
                return;
            }
            // console.info(t1.getSeconds(), i++);
        });
        obj.on("error", (error) => {
            console.log("on data error............");
            console.error(error);
            obj.close();
        });
    }).catch(error => {
        console.log("init error.....");
        console.error(error);
    });
}

function testAsyncGrabImageMultithreadedly() {
    let ffmpegNode = FFmpegNode.asyncGrabImageMultithreadedly({url: video_addr, timeout: 10, nobuffer: true, useGpu: true, level: level.DEBUG, gpuId: 0, useTcp: true, quality: 80, type: target_type, frames: 1});

    ffmpegNode.on("data", (buffer) => {
        console.info(buffer);
        let begin = new Date();
        // 模拟延迟300ms
        // for (var start = Date.now(); Date.now() - start <= 300;) { }
        let t1 = new Date();
        console.info(t1.getTime() - begin.getTime());
        begin = new Date();
        let name = dir + "/tmp/images/buffer-" + t1.getHours() + "-" + t1.getMinutes() + "-" + t1.getSeconds() + "-" + (i++) + suffix;
        console.info(name);
        fs.writeFileSync(name, buffer);
        console.info("====================================");
        // fs.rmdirSync(name, { recursive: true });
        if (t1.getSeconds() % 50 === 0) {
            ffmpegNode.close();
            return;
        }
        // console.info(t1.getSeconds(), i++);
    });
    ffmpegNode.on("error", (error) => {
        console.log("on data error............");
        console.error(error);
        ffmpegNode.close();
    });
}

async function testReadImageStreamThreadly() {
    try {
        let ffmpegNode = await FFmpegNode.init(video_addr, 10, false, true, level.INFO, 0, true);
        ffmpegNode.readImageStreamThreadly(100, target_type, 5);
        ffmpegNode.on("data", (buffer) => {
            let begin = new Date();
            // 模拟延迟300ms
            // for (var start = Date.now(); Date.now() - start <= 300;) { }
            let t1 = new Date();
            console.info(t1.getTime() - begin.getTime());
            begin = new Date();
            let name = dir + "/tmp/images/buffer-" + t1.getHours() + "-" + t1.getMinutes() + "-" + t1.getSeconds() + "-" + (i++) + suffix;
            console.info(name);
            console.log(buffer);
            fs.writeFileSync(name, buffer);
            console.info("====================================");
            // fs.rmdirSync(name, { recursive: true });
            // global.gc();
            // if (t1.getSeconds() % 30 === 0) {
            //   ffmpegNode.close();
            //   return;
            // }
            // console.info(t1.getSeconds(), i++);
        });
        ffmpegNode.on("error", (error) => {
            console.log("on data error............");
            console.error(error);
            ffmpegNode.close();
        });
    } catch (e) {
        console.log("init error.....");
        console.error(e);
    }
}

async function runWithoutCallback() {
    try {
        let ffmpegNode = await FFmpegNode.init(video_addr, 10, false, false, FFmpegNode.LEVEL().INFO, 0, false);
        let image = null;
        while (true) {
            try {
                image = await ffmpegNode.readImageBuffer(80, target_type, 1);

                let now = new Date();
                let name = dir + "/tmp/images/buffer-" + now.getHours() + "-" + now.getMinutes() + "-" + now.getSeconds() + "-" + i + suffix;
                console.info(name);
                fs.writeFileSync(name, image);
            } catch (error) {
                console.error(error);
            }
            // if (image === null) break;
        }
        ffmpegNode.close();
    } catch (e) {
        console.error(e);
    }

}

function runWithCallback() {
    let ffmpegNode = FFmpegNode.init(video_addr, 10, false, false, FFmpegNode.LEVEL().INFO, 0, true);
    ffmpegNode.then((obj) => {
        obj.asyncReadImageBuffer(100, target_type, 10);
        obj.on("data", (buffer) => {
            let begin = new Date();
            // // 模拟延迟300ms
            // for (var start = Date.now(); Date.now() - start <= 300;) { }
            let t1 = new Date();
            console.info(t1.getTime() - begin.getTime());
            // begin = new Date();
            let name = dir + "/tmp/images/buffer-" + t1.getHours() + "-" + t1.getMinutes() + "-" + t1.getSeconds() + "-" + i + suffix;
            console.info(name);
            fs.writeFileSync(name, buffer);
            console.info("====================================");
            if (t1.getSeconds() % 2 === 0) {
                obj.close();
            }
            console.info(t1.getSeconds(), i++);
        });
        obj.on("error", (error) => {
            console.log("on data error............");
            console.error(error);
            obj.close();
        });
    }).catch(error => {
        console.log("init error.....");
        console.error(error);
    });
}

let videoFilePath = dir + "/tmp/videos/buffers.flv";
// FFmpegNode.recordVideo(rtsp_addr, videoFilePath, 5, true);

// testSyncReadImageBuffer();
// testAsyncReadImageBuffer();
// testAsyncReadImageBufferThreadly();
testAsyncGrabImageMultithreadedly();
// testAsyncGrabImageMultithreadedly();
// testAsyncGrabImageMultithreadedly();
// testAsyncGrabImageMultithreadedly();
// runWithCallback();
// testReadImageStreamThreadly();

setInterval(() => {
    console.info("fffffffffffffff ....");
}, 1000);

// FFmpegNode.sayHello(520, (output) => {
//     console.log(output);
//     console.log("......");
// });

// runWithoutCallback().then().catch(error => {
//     console.error(error);
// });


