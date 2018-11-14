const fs = require('fs');
const FFmpegNode = require('../index');

// let rtsp_addr = "35435435345435345345345";
let rtsp_addr = "rtsp://admin:iec123456@192.168.1.71:554/unicast/c1/s0/live";
// let rtsp_addr = "rtsp://admin:123456@192.168.1.61:554/h264/ch1/main/av_stream";
// let rtsp_addr = "rtsp://admin:123456@192.168.1.68:554/h264/ch1/main/av_stream";
// let dir = "/mnt/h/oceanai-workspace/ffmpeg-node";
let dir = "/opt/ffmpeg_nodejs";

let ffmpegNode = FFmpegNode.init(rtsp_addr, true);

// let i = 0;
ffmpegNode.then((obj) => {
    obj.onData(100, "yuv", 1, async (buffer) => {
        console.error(buffer);

        let begin = new Date();
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
        // console.info("====================================");
        // if (t1.getSeconds() % 5 === 0) ffmpegNode.destroy();
        // console.info(i++);
    });
    obj.on("error", (error) => {
        console.log("on data error............");
        console.error(error);
    });
}).catch(error => {
    console.log("init error.....");
    console.error(error);
});
