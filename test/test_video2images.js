const fs = require('fs');
const FFmpegNode = require('../index');

let rtsp_addr = "rtsp://admin:iec123456@192.168.1.71:554/unicast/c1/s0/live";
// let rtsp_addr = "rtsp://admin:123456@192.168.1.61:554/h264/ch1/main/av_stream";
// let dir = "/mnt/h/oceanai-workspace/ffmpeg-node";
let dir = "/opt/ffmpeg_nodejs";
async function main() {
    let ffmpegNode = new FFmpegNode(rtsp_addr);
    let begin = new Date();
    console.info(begin.getTime());

    ffmpegNode.data(100, 1);

    let i = 0;
    ffmpegNode.on("data", async (buffer) => {
        console.info(buffer);
        // 模拟延迟300ms
        for (var start = Date.now(); Date.now() - start <= 300;) { }
        let t1 = new Date();
        console.info(buffer);
        console.info(t1.getTime() - begin.getTime());
        begin = new Date();
        let name = dir + "/tmp/images/buffer-" + t1.getHours() + "-" + t1.getMinutes() + "-" + t1.getSeconds() + "-" + i + ".jpg";
        console.info(name);
        await fs.writeFileSync(name, buffer, (err) => {
            if (err) console.error(err);
        });
        console.info("====================================");
        if (t1.getSeconds() % 59 === 0) ffmpegNode.destroy();
        console.info("555555555555555555555");
        console.info(i++);
    });

    /* try {
        for (let i = 0; i < 15; i++)
            await ffmpegNode.readJpegStream(100, 1, async (buffer) => {
                for (var start = Date.now(); Date.now() - start <= 300;) { }
                let t1 = new Date();
                console.info(buffer);
                console.info(t1.getTime() - begin.getTime());
                let name = "/mnt/h/oceanai-workspace/ffmpeg-node/tmp/images/buffer-" + t1.getHours() + "-" + t1.getMinutes() + "-" + t1.getSeconds() + ".jpg";
                console.info(name);
                await fs.writeFileSync(name, buffer, (err) => {
                    if (err) console.error(err);
                });
                console.info("====================================");
                if (i == 5) ffmpegNode.destroy();
            });
    } catch (error) {
        console.error(error);
        // ffmpeg_nodejs.destroyStream();
        // try {
        //     await ffmpeg_nodejs.initReadingVideo(rtsp_addr);
        // } catch (err) {
        //     console.error(err);
        // }
    }*/
    // ffmpegNode.destroy();

    console.info("----------------------");
}

main().then();