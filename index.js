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

    config() {
        this.fflags = "nobuffer";
        // 500ms
        this.maxDelay = "500000";
        // max 327680
        this.bufferSize = "4096";
        this.rtbufsize = "4096";
    }

    /**
     * read jpeg data of frame from this url video stream
     * @param {number} jpeg_quality default 80
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
     * read raw yuv data of frame from this url video stream
     * @param {number} frames: chose frames per second, default 1
     * @param {Function} callback callback function
     * @return {Promise<Buffer>} jpeg buffer
     */
    readYuvStream(frames, callback) {
        if (!this.__destroy) {
            if (typeof frames !== "number" || frames <= 0) frames = 1;

            if (typeof callback !== "function") callback = function (buffer) { };

            return ffmpeg_nodejs.video2YuvImageStream(parseInt(frames), callback);
        }
    }

    /**
     * read raw rgb data of frame from this url video stream
     * @param {number} frames: chose frames per second, default 1
     * @param {Function} callback callback function
     * @return {Promise<Buffer>} jpeg buffer
     */
    readRgbStream(frames, callback) {
        if (!this.__destroy) {
            if (typeof frames !== "number" || frames <= 0) frames = 1;

            if (typeof callback !== "function") callback = function (buffer) { };

            return ffmpeg_nodejs.video2RgbImageStream(parseInt(frames), callback);
        }
    }

    async data(quality, frames, callback) {
        let buffer;
        try {
            while ((buffer = await this.readYuvStream(quality, frames, callback)) !== undefined) {
                this.emit("data", buffer);
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