const fs = require('fs');
const ffmpeg_nodejs = require('../index');

let rtsp_addr = "rtsp://admin:iec123456@192.168.1.71:554/unicast/c1/s0/live";

ffmpeg_nodejs.initReadingVideo(rtsp_addr);

let buffer = ffmpeg_nodejs.video2JpegStream(100);
fs.writeFile("/opt/ffmpeg_nodejs/tmp/images/buffer.jpg", buffer, (err)=> {
    if (err) console.error(err);
});

buffer = ffmpeg_nodejs.video2JpegStream(90);
fs.writeFile("/opt/ffmpeg_nodejs/tmp/images/buffer2.jpg", buffer, (err)=> {
    if (err) console.error(err);
});

buffer = ffmpeg_nodejs.video2JpegStream(80);
fs.writeFile("/opt/ffmpeg_nodejs/tmp/images/buffer3.jpg", buffer, (err)=> {
    if (err) console.error(err);
});

buffer = ffmpeg_nodejs.video2JpegStream(70);
fs.writeFile("/opt/ffmpeg_nodejs/tmp/images/buffer4.jpg", buffer, (err)=> {
    if (err) console.error(err);
});

ffmpeg_nodejs.destroyStream();