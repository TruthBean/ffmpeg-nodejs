const fs = require('fs');
const FFmpegNode = require('../../index');
const verifier = require("./verifier_pb");
const zmq = require('zeromq');

const dealer = zmq.socket('dealer');
dealer.connect('tcp://192.168.1.196:5559');

// const zmqReq = zmq.socket('req');
// zmqReq.connect('tcp://127.0.0.1:23332');

let dir = "/opt/ffmpeg_nodejs";
let rtsp_addr = "rtsp://admin:iec123456@192.168.1.71:554/unicast/c1/s0/live";
dir = "/media/oceanai/DevOps/oceanai-workspace/ffmpeg-node-cmake";
dir = "/mnt/h/oceanai-workspace/ffmpeg-node-cmake";

const type = FFmpegNode.TYPE();
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

function zmqAction(image) {
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

    let buff = Buffer.alloc(req.length, req);
    dealer.send("pb", zmq.ZMQ_SNDMORE);
    dealer.send(buff);

    // zmqReq.send(buff);
    console.info(buff);

    let a = new Promise((resolve, reject) => {
        resolve("6666");
    });
    a.then((r) => {
        console.info(r);
    });
    console.info("0000000");

    dealer.on('message', (msg) => {
        console.info("<<<<<<<<<<<<<<<<<<<<", msg);
        let bytes = Buffer.alloc(msg.length + 1);
        msg.copy(bytes);
        // let bytes = msg;

        try {
            let res = proto.verifier.DetectRes.deserializeBinary(bytes);

            let requestId = res.getRequestid();
            let timeUsed = res.getTimeused();
            let errorMessage = res.getErrormessage();
            console.info(errorMessage);

            let detectNums = res.getDetectnums();
            let trackNums = res.getTracknums();

            let detectArray = res.getDetectList();
            detectArray.forEach((value, index, array) => {
                console.info(value.getQuality());
            });

            let trackArray = res.getTrackList();
            trackArray.forEach((value, index, array) => {
                console.info(value.getScore());
            });
        } catch (error) {
            console.info(error);
        }
    });
}

async function noCb() {
    let i = 0;
    let ffmpegNode = await FFmpegNode.init(rtsp_addr, false, false);
    let image = null;

    try {
        image = await ffmpegNode.video2ImageBuffer(80, type.RGB, 1);
    } catch (error) {
        console.error(error);
        ffmpegNode.emit("error", error);
    }

    console.info(image);

    let now = new Date()
    let name = dir + "/tmp/images/buffer-" + now.getHours() + "-" + now.getMinutes() + "-" + now.getSeconds() + "-" + i + suffix;
    console.info(name);
    await fs.writeFileSync(name, image);

    zmqAction(image);
}

noCb().then();

function cb() {
    FFmpegNode.init(rtsp_addr, true, false).then((obj) => {
        let i = 0;
        obj.readImageStream(80, target_type, 1);
        obj.on("data", (image) => {
            i++;
            let now = new Date()
            let name = dir + "/tmp/images/buffer-" + now.getHours() + "-" + now.getMinutes() + "-" + now.getSeconds() + "-" + i + suffix;
            console.info(name);
            fs.writeFileSync(name, image);
            console.info("------------------------------------");

            process.nextTick(zmqAction, image);

            if (i > 1) {
                obj.close();
            }
        });

        obj.on("error", (error) => {
            console.error(error);
        })

    }).catch(error => {
        console.error(error);
    });
}

// cb();

// const rep = zmq.socket('rep');
// rep.bindSync("tcp://*:23332");

// rep.on('message', (msg) => {
//     console.info(msg instanceof Uint8Array);
//     console.info(msg.length);
//     let res = verifier.DetectReq.deserializeBinary(msg);
//     // console.info(res);
//     console.info("====================================>>>>>>>>>>>33333333333333333");
// });