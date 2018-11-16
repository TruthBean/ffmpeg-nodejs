const FFmpegNode = require('../../index');
const verifier = require("./verifier/verifier_pb");
const zmq = require('zeromq');

const dealer = zmq.socket('dealer');
dealer.connect('tcp://192.168.1.196:5559');

let rtsp_addr = "rtsp://admin:iec123456@192.168.1.71:554/unicast/c1/s0/live";

const type = FFmpegNode.TYPE();

async function main() {
    let ffmpegNode = await FFmpegNode.init(rtsp_addr, false);
    let image = null;

    try {
        image = await ffmpegNode.readImageStream(1, type.RGB, 80);
    } catch (error) {
        ffmpegNode.emit("error", error);
    }

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

    let buff = Buffer.alloc(req.length, req);
    dealer.send(buff);

    dealer.on('message', (msg) => {
        let bytes = Buffer.alloc(msg.length + 1);
        msg.copy(bytes);

        try {
            let res = proto.verifier.DetectRes.deserializeBinary(bytes);

            let requestId = res.getRequestid();
            let timeUsed = res.getTimeused();
            let errorMessage = res.getErrormessage();

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

main().then();