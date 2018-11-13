const EventEmitter = require('events').EventEmitter;
const ffmpeg_nodejs = require('./build/Release/ffmpeg_nodejs');

class FFmpegNode extends EventEmitter {

    /**
     * 
     * @param {string} url
     * @param {boolean} nobuffer
     */
    constructor(url, nobuffer) {
        super();
        if (typeof url !== "string")
            throw Error("url must be string type");
        this.url = url;
        if (typeof nobuffer !== "boolean")
            throw Error("nobuffer must be boolean type");
        this.__destroy = false;
        ffmpeg_nodejs.initReadingVideo(this.url, nobuffer).then();
    }

    /**
     * read jpeg data of frame from this url video stream
     * @param {number} quality default 80
     * @param {number} frames: chose frames per second, default 1
     * @param {Function} callback callback function
     * @return {Promise<Buffer>} jpeg buffer
     */
    readJpegStream(quality, frames, callback) {
        if (!this.__destroy) {
            if (typeof quality !== "number") quality = 80;
            else if (quality <= 0) quality = 80;
            else if (quality >= 100) quality = 100;

            if (typeof frames !== "number" || frames <= 0) frames = 1;

            if (typeof callback !== "function") callback = function (buffer) { };

            return ffmpeg_nodejs.video2JpegStream(parseInt(quality), parseInt(frames), callback);
        }
    }

    /**
     * read raw data of frame from this url video stream
     * @param {number} frames: chose frames per second, default 1
     * @param {string} type: rgb or yuv
     * @param {Function} callback callback function
     * @return {Promise<Buffer>} jpeg buffer
     */
    readRawStream(frames, type, callback) {
        if (!this.__destroy) {
            if (typeof frames !== "number" || frames <= 0) frames = 1;

            if (type === "rgb" || type === "yuv") type = "rgb";

            if (typeof callback !== "function") callback = function (buffer) { };

            let typeNo = 0;
            if (type === "yuv") typeNo = 0;
            else if (type === "rgb") typeNo = 1;
            return ffmpeg_nodejs.video2RawImageStream(parseInt(frames), typeNo, callback);
        }
    }

    async data(quality, type, frames, callback) {
        let buffer;
        try {
            if (type === "jpeg") {
                while ((buffer = await this.readJpegStream(quality, frames, callback)) && buffer !== undefined) {
                    this.emit("data", buffer);
                }
            } else {
                while (true) {
                    await this.readRawStream(frames, type, (buffer) => {
                        console.info(buffer);
                        if (buffer === undefined) {
                            console.info("..............");
                        }
                        this.emit("data", buffer);
                    });
                }
            }
        } catch (error) {
            console.error(error);
        }
    }

    /**
     * destroy this url video stream
     */
    destroy() {
        if (!this.__destroy) {
            console.info("destroy...");
            this.__destroy = true;
            return ffmpeg_nodejs.destroyStream();
        }
    }
}

module.exports = FFmpegNode;