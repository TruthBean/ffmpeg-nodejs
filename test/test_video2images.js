const fs = require('fs');
const FFmpegNode = require('../index');

const verifier = require("./verifier_pb");
const zmq = require('zeromq');

let detectReq = new verifier.DetectReq();
// console.info(verifier);

const dealer = zmq.socket('req');

dealer.connect('tcp://127.0.0.1:30000');


// let rtsp_addr = "35435435345435345345345";
let rtsp_addr = "rtsp://admin:iec123456@192.168.1.71:554/unicast/c1/s0/live";
// let rtsp_addr = "rtsp://admin:123456@192.168.1.61:554/h264/ch1/main/av_stream";
// let rtsp_addr = "rtsp://admin:123456@192.168.1.68:554/h264/ch1/main/av_stream";
// let dir = "/mnt/h/oceanai-workspace/ffmpeg-node";
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
    let image = await ffmpegNode.readImageStream(1, target_type, 80);
}

function runWithCallback() {
    let ffmpegNode = FFmpegNode.init(rtsp_addr, false);
    ffmpegNode.then((obj) => {
        obj.onData(80, target_type, 1, async (buffer) => {
            // console.info(buffer);
            console.info("====================================>>>>>>>>>>>11111111111111");
            detectReq.setImage(buffer);

            detectReq.setInterface(1);
            detectReq.setApikey("apiKey");
            detectReq.setRows(4);
            detectReq.setCols(5);
            detectReq.setFormat(verifier.DetectReq.Format.YUV);
            detectReq.setMinfacesize(7);
            detectReq.setMaxfacesize(8);
            detectReq.setField("9");
            detectReq.setImageid("10");
            detectReq.setCameraid("11");

            let req = detectReq.serializeBinary();
            console.info(req instanceof Uint8Array);
            let buff = Buffer.alloc(req.length, req);
            dealer.send(buff);

            // let res = verifier.DetectReq.deserializeBinary(req);
            // console.info(res);

            console.info();
            console.info("====================================>>>>>>>>>>>22222222222222");

            // ===================================================
            let begin = new Date();
            // // 模拟延迟300ms
            // for (var start = Date.now(); Date.now() - start <= 300;) { }
            let t1 = new Date();
            console.info(t1.getTime() - begin.getTime());
            // begin = new Date();
            let name = dir + "/tmp/images/buffer-" + t1.getHours() + "-" + t1.getMinutes() + "-" + t1.getSeconds() + "-" + i + suffix;
            console.info(name);
            // await fs.writeFileSync(name, buffer);
            console.info("====================================");
            // if (t1.getSeconds() % 2 === 0) {
            obj.close();
            dealer.close();
            // }
            console.info(t1.getSeconds(), i++);
        });
        obj.on("error", (error) => {
            console.log("on data error............");
            console.error(error);
        });
    }).catch(error => {
        console.log("init error.....");
        console.error(error);
    });

    // =========================================================================================

    const router = zmq.socket('rep');

    router.bindSync("tcp://*:30000");

    router.on('message', (msg) => {
        console.info(msg instanceof Uint8Array);
        console.info(msg.length);
        let res = verifier.DetectReq.deserializeBinary(msg);
        console.info(res);
        console.info("====================================>>>>>>>>>>>33333333333333333");
    });
}

// let videoFilePath = dir + "/tmp/videos/buffers.mp4";
// FFmpegNode.recordVideo(rtsp_addr, videoFilePath, 10);

runWithCallback();

