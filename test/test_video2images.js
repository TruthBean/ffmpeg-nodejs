const fs = require('fs');
const FFmpegNode = require('../index');

const jsbp = require("google-protobuf");
const verifier = require("./verifier/verifier_pb");
const zmq = require('zeromq');

// const router = zmq.socket('router');
const dealer = zmq.socket('dealer');

// dealer.connect('tcp://127.0.0.1:5559');
dealer.connect('tcp://192.168.1.196:5559');
// router.bindSync("tcp://127.0.0.1:5559");

// let rtsp_addr = "35435435345435345345345";
let rtsp_addr = "rtsp://admin:iec123456@192.168.1.71:554/unicast/c1/s0/live";
// let rtsp_addr = "rtsp://admin:123456@192.168.1.61:554/h264/ch1/main/av_stream";
// let rtsp_addr = "rtsp://admin:123456@192.168.1.68:554/h264/ch1/main/av_stream";
let dir = "/mnt/h/oceanai-workspace/ffmpeg-node";
// let dir = "/opt/ffmpeg_nodejs";

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

    let detectReq = new verifier.DetectReq();
    detectReq.setImage(image);

    detectReq.setInterface(14);
    detectReq.setApikey("");
    detectReq.setRows(2048);
    detectReq.setCols(3072);
    detectReq.setFormat(verifier.DetectReq.Format.RGB);
    detectReq.setMinfacesize(80);
    detectReq.setMaxfacesize(300);
    detectReq.setField("normal");

    let req = detectReq.serializeBinary();

    // =================================================================
    console.info(req instanceof Uint8Array);
    let buff = Buffer.alloc(req.length, req);
    console.info(buff);
    dealer.send(buff);
    console.info("====================================>>>>>>>>>>>11111111111111111111");

    dealer.on('message', (msg) => {

        // console.info(msg.toString().length, msg.length);
        // console.info("==================================");
        // let name2 = dir + "/tmp/images/res-" + now.getHours() + "-" + now.getMinutes() + "-" + now.getSeconds() + "-" + i;
        // console.info(name2);
        // fs.writeFileSync(name2, msg);
        // console.info(msg.toJSON())

        let bytes = Buffer.alloc(msg.length + 1);
        msg.copy(bytes);

        try {
            // let bytes = Buffer.alloc(msg.toString().length, msg.toString());
            let res = proto.verifier.DetectRes.deserializeBinary(bytes);

            console.info(res);
            let requestId = res.getRequestid();
            let timeUsed = res.getTimeused();
            let errorMessage = res.getErrormessage();
            console.info(errorMessage);
            let detectNums = res.getDetectnums();
            let trackNums = res.getTracknums();

            let detectArray = res.getDetectList();
            detectArray.forEach((value, index, array) => {
                console.info(value);
                console.info(value.getQuality());
                console.info("----------------------1111111111111111111111111111111");
            });

            let trackArray = res.getTrackList();
            trackArray.forEach((value, index, array) => {
                console.info(value);
                console.info("----------------------222222222222222222222222222222222");
            });
        } catch (error) {
            console.info(error);
        }

        console.info("====================================>>>>>>>>>>>33333333333333333");
        // dealer.close();
    });
    // }
}

function runWithCallback() {
    let ffmpegNode = FFmpegNode.init(rtsp_addr, false);
    ffmpegNode.then((obj) => {
        obj.onData(80, target_type, 1, async (buffer) => {
            // console.info(buffer);
            console.info("====================================>>>>>>>>>>>11111111111111");
            let detectReq = new verifier.DetectReq();
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
            // obj.close();
            // dealer.close();
            // }
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

    // =========================================================================================

    router.on('message', (msg) => {
        console.info(msg instanceof Uint8Array);
        console.info(msg.length);
        let res = verifier.DetectReq.deserializeBinary(msg);
        // console.info(res);
        console.info("====================================>>>>>>>>>>>33333333333333333");
    });
}

// let videoFilePath = dir + "/tmp/videos/buffers.mp4";
// FFmpegNode.recordVideo(rtsp_addr, videoFilePath, 10);

// runWithCallback();

runWithoutCallback().then().catch(error => {
    console.error(error);
});

