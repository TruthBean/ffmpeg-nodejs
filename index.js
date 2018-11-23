const EventEmitter = require("events").EventEmitter;
const ffmpeg_nodejs = require("./build/Release/ffmpeg_nodejs");

const JPEG = "jpeg", YUV = "yuv", RGB = "rgb";
class FFmpegNode extends EventEmitter {

    /**
     * init
     * @param {String} url 
     * @param {boolean} nobuffer
     * @param {boolean} useGpu
     * @return {Promise<FFmpegNode>}
     */
    static init(url, nobuffer, useGpu) {
        let self = new FFmpegNode();

        if (typeof url !== "string")
            throw new Error("url must be string type");
        self.url = url;
        if (typeof nobuffer !== "boolean")
            throw new Error("nobuffer must be boolean type");

        if (typeof useGpu !== "boolean")
            throw new Error("useGpu muse be boolean type");

        self.destroy = false;
        return new Promise((resolve, reject) => {
            console.info(nobuffer);
            ffmpeg_nodejs.initReadingVideo(self.url, nobuffer, useGpu).then((ignored) => {
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
     * @param {boolean} useGpu
     */
    static recordVideo(url, filename, recordSeconds, useGpu) {
        if (typeof url !== "string") {
            throw new Error("url must be string type");
        }
        if (typeof filename !== "string")
            throw new Error("filename must be string type");
        if (typeof recordSeconds !== "number")
            throw new Error("recordSeconds must be number");
        if (typeof useGpu !== "boolean")
            throw new Error("useGpu must be boolean");
        ffmpeg_nodejs.recordVideo(url, filename, recordSeconds, useGpu);
    }

    /**
     * read raw data of frame from this url video stream
     * @param {number} frames: chose frames per second, default 1
     * @param {string} type: rgb, yuv or jpeg
     * @param {number} quality: jpeg quality
     * @param {Function} callback callback function
     * @return {Promise<Buffer>} image buffer
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
        let result = null;
        while (!this.destroy) {
            console.info("let's go....");
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
        if (result === null) {
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