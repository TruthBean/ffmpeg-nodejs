const EventEmitter = require('events').EventEmitter;
const ffmpeg_nodejs = require('./build/Release/ffmpeg_nodejs');

class FFmpegNode extends EventEmitter {

    /**
     * init
     * @param {stirng} url 
     * @param {boolean} nobuffer
     * @return {Promise<FFmpegNode>}
     */
    static init(url, nobuffer) {
        let self = new FFmpegNode();

        if (typeof url !== "string")
        throw Error("url must be string type");
        self.url = url;
        if (typeof nobuffer !== "boolean")
            throw Error("nobuffer must be boolean type");

        self.destroy = false;
        return new Promise((resolve, reject) => {
            ffmpeg_nodejs.initReadingVideo(self.url, nobuffer).then((value) => {
                resolve(self);
            }).catch((err) => {
                self.destroy = true;
                // self.emit("error", err);
                reject(err);
            });
        });
    }

    /**
     * record video
     * @param {string} url 
     * @param {string} filename 
     * @param {number} recordSeconds 
     */
    static recordVideo(url, filename, recordSeconds) {
        if (typeof url !== "string") {
            throw Error("url must be string type");
        }
        if (typeof filename !== "string")
            throw Error("filename must be string type");
        if (typeof recordSeconds !== "number")
            throw Error("recordSeconds must be number");
        ffmpeg_nodejs.recordVideo(url, filename, recordSeconds);
    }

    /**
     * read jpeg data of frame from this url video stream
     * @param {number} quality default 80
     * @param {number} frames: chose frames per second, default 1
     * @param {Function} callback callback function
     * @return {Promise<Buffer>} jpeg buffer
     */
    readJpegStream(quality, frames, callback) {
        if (!this.destroy) {
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
     * @return {Promise<Buffer>} image buffer
     */
    readRawStream(frames, type, callback) {
        if (!this.destroy) {
            if (typeof frames !== "number" || frames <= 0) frames = 1;

            if (type === "rgb" || type === "yuv") type = "rgb";

            if (typeof callback !== "function") callback = function (buffer) { };

            let typeNo = 0;
            if (type === "yuv") typeNo = 0;
            else if (type === "rgb") typeNo = 1;
            return ffmpeg_nodejs.video2RawImageStream(parseInt(frames), typeNo, callback);
        }
    }

    async onData(quality, type, frames, callback) {
        if (type === "jpeg") {
            try {
                let buffer;
                while ((buffer = await this.readJpegStream(quality, frames, callback)) && buffer !== undefined) {
                    // this.emit("data", buffer);
                }
            } catch (error) {
                this.emit("error", error);
            }
        } else {
            let result = 0;
            while (result === 0) {
                try {
                    result = await this.readRawStream(frames, type, (buffer) => {
                        this.emit("data", buffer);
                    });
                    break;
                } catch (err) {
                    this.emit("error", err);
                    break;
                }
            }
            if (result === 1) {
                this.emit("error", "read video error !");
            }
        }
    }

    /**
     * destroy this url video stream
     */
    destroy() {
        if (!this.destroy) {
            console.info("destroy...");
            this.destroy = true;
            return ffmpeg_nodejs.destroyStream();
        }
    }
}

module.exports = FFmpegNode;