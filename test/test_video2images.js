const fs = require('fs');
const FFmpegNode = require('../index');

let rtsp_addr = "rtsp://admin:iec123456@192.168.1.71:554/unicast/c1/s0/live";
// let rtsp_addr = "rtsp://admin:123456@192.168.1.61:554/h264/ch1/main/av_stream";
// let rtsp_addr = "rtsp://admin:123456@192.168.1.68:554/h264/ch1/main/av_stream";
// let dir = "/mnt/h/oceanai-workspace/ffmpeg-node";
let dir = "/opt/ffmpeg_nodejs";

let ffmpegNode = new FFmpegNode(rtsp_addr, true);
let begin = new Date();
console.info(begin.getTime());

ffmpegNode.data(100, "rgb", 1);

let i = 0;
ffmpegNode.on("data", async (buffer) => {
    console.info(buffer);
    // 模拟延迟300ms
    for (var start = Date.now(); Date.now() - start <= 300;) { }
    let t1 = new Date();
    console.info(buffer);
    console.info(t1.getTime() - begin.getTime());
    begin = new Date();
    let name = dir + "/tmp/images/buffer-" + t1.getHours() + "-" + t1.getMinutes() + "-" + t1.getSeconds() + "-" + i + ".yuv";
    console.info(name);
    await fs.writeFileSync(name, buffer, (err) => {
        if (err) console.error(err);
    });
    console.info("====================================");
    if (t1.getSeconds() % 5 === 0) ffmpegNode.destroy();
    console.info("555555555555555555555");
    console.info(i++);
});

console.info("----------------------");