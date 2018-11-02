const fs = require('fs');
const ffmpeg_nodejs = require('../index');

let rtsp_addr = "rtsp://admin:iec123456@192.168.1.71:554/unicast/c1/s0/live";

async function main() {
    try {
        await ffmpeg_nodejs.initReadingVideo(rtsp_addr);
    } catch (err) {
        console.error(err);
    }
    let total_frame = 5 * 60 * 60 * 24;
    let buffer;
    for (let i = 0; i < total_frame; i++) {
        let now = new Date();
        console.info(now.getTime());
        try {
            buffer = await ffmpeg_nodejs.video2JpegStream(100, 25, 25);
            await fs.writeFileSync("/opt/ffmpeg_nodejs/tmp/images/buffer" + now.getSeconds() + i + ".jpg", buffer, (err) => {
                if (err) console.error(err);
            });
        } catch (error) {
            console.error(error);
            // ffmpeg_nodejs.destroyStream();
            // try {
            //     await ffmpeg_nodejs.initReadingVideo(rtsp_addr);
            // } catch (err) {
            //     console.error(err);
            // }
        }
    }
    ffmpeg_nodejs.destroyStream();
}

main().then();