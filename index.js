const EventEmitter = require("events").EventEmitter;
const ffmpeg_nodejs = require("./build/Release/ffmpeg_nodejs");

const JPEG = "jpeg", YUV = "yuv", RGB = "rgb";
class FFmpegNode extends EventEmitter {

    /**
     * init
     * @param {String} url 
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
            console.info(nobuffer);
            ffmpeg_nodejs.initReadingVideo(self.url, nobuffer).then((ignored) => {
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
     * read raw data of frame from this url video stream
     * @param {number} frames: chose frames per second, default 1
     * @param {string} type: rgb, yuv or jpeg
     * @param {number} quality: jpeg quality
     * @param {Function} callback callback function
     * @return {Promise<number>} image buffer
     */
    readImageStream(frames, type, quality, callback) {
        if (!this.destroy) {
            if (typeof frames !== "number" || frames <= 0) frames = 1;

            console.info(type);

            if (type !== RGB && type !== YUV && type !== JPEG) type = "rgb";

            if (typeof quality !== "number") quality = 80;
            else if (quality <= 0) quality = 80;
            else if (quality >= 100) quality = 100;

            if (typeof callback !== "function") {
                console.warn("callback is not a function");
                callback = function (buffer) { };
            } else {
                console.info(callback);
            }

            let typeNo = 0;
            if (type === YUV) typeNo = 0;
            else if (type === RGB) typeNo = 1;
            else if (type === JPEG) typeNo = 2;
            return ffmpeg_nodejs.video2ImageStream(frames, typeNo, quality, callback);
        }
    }

    /**
     * 
     * @param {number} quality: jpeg quality
     * @param {string} type: rgb, yuv or jpeg
     * @param {number} frames: chose frames per second, default 1
     * @param {Function} callback callback function
     */
    async onData(quality, type, frames, callback) {
        let result = 0;
        while (result === 0 && !this.destroy) {
            try {
                result = await this.readImageStream(frames, type, quality, callback);
                console.info(this.destroy);
                console.info("----------------> ", result);
                if (this.destroy) break;
            } catch (err) {
                this.emit("error", err);
                break;
            }
        }
        if (result === 1) {
            this.emit("error", "read video error !");
        }
    }

    /**
     * close this url video stream
     */
    close() {
        console.info(this.destroy);
        if (!this.destroy) {
            console.info("destroy...");
            this.destroy = true;
            return ffmpeg_nodejs.destroyStream();
        }
    }

    static TYPE() {
        return {
            JPEG: "jpeg",
            YUV: "yuv",
            RGB: "rgb"
        }
    };
}

module.exports = FFmpegNode;