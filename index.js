const EventEmitter = require("events").EventEmitter;
const ffmpeg_nodejs = require("./build/Release/ffmpeg_nodejs");

const JPEG = "jpeg", YUV = "yuv", RGB = "rgb";
const INFO = 3, DEBUG = 2, ERROR = 1;
class FFmpegNode extends EventEmitter {

    /**
     * init
     * @param {String} url 
     * @param {boolean} nobuffer
     * @param {boolean} useGpu
     * @param {string} level
     * @param {number} gpuId
     * @return {Promise<FFmpegNode>}
     */
    static init(url, timeout, nobuffer, useGpu, level, gpuId) {
        let self = new FFmpegNode();

        if (typeof url !== "string")
            throw new Error("url must be string type");
        self.url = url;

        if (typeof timeout !== "number")
            throw new Error("timeout must be number");

        if (typeof nobuffer !== "boolean")
            throw new Error("nobuffer must be boolean type");

        if (typeof useGpu !== "boolean")
            throw new Error("useGpu muse be boolean type");

        if (typeof level !== "number")
            level = INFO;
            
        if (typeof gpuId !== "number")
            gpuId = 0;

        self.destroy = false;
        return new Promise((resolve, reject) => {
            ffmpeg_nodejs.initReadingVideo(self.url, timeout, nobuffer, useGpu, level, gpuId.toString()).then((ignored) => {
                resolve(self);
            }).catch((err) => {
                self.destroy = true;
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
     * @param {number} quality: jpeg quality
     * @param {string} type: rgb, yuv or jpeg
     * @param {number} frames: chose frames per second, default 1
     * @param {Function} callback callback function
     * @return {Promise<Buffer>} image buffer
     */
    readImageBuffer(quality, type, frames) {
        if (!this.destroy) {
            if (typeof frames !== "number" || frames <= 0) frames = 1;
            if (type !== RGB && type !== YUV && type !== JPEG) type = "rgb";

            if (typeof quality !== "number") quality = 80;
            else if (quality <= 0) quality = 80;
            else if (quality >= 100) quality = 100;

            let typeNo = 0;
            if (type === YUV) typeNo = 0;
            else if (type === RGB) typeNo = 1;
            else if (type === JPEG) typeNo = 2;
            return ffmpeg_nodejs.video2ImageBuffer(frames, typeNo, quality);
        }
    }

    /**
     * @private
     * @param {number} quality: jpeg quality
     * @param {string} type: rgb, yuv or jpeg
     * @param {number} frames: chose frames per second, default 1
     */
    readImageStream(quality, type, frames) {
        if (!this.destroy) {
            if (typeof frames !== "number" || frames <= 0) frames = 1;
            if (type !== RGB && type !== YUV && type !== JPEG) type = "rgb";

            if (typeof quality !== "number") quality = 80;
            else if (quality <= 0) quality = 80;
            else if (quality >= 100) quality = 100;

            let typeNo = 0;
            if (type === YUV) typeNo = 0;
            else if (type === RGB) typeNo = 1;
            else if (type === JPEG) typeNo = 2;
            let that = this;
            this.read = setInterval(() => {
                ffmpeg_nodejs.video2ImageStream(typeNo, quality, frames, (obj) => {
                    if (obj !== undefined && obj !== null) {
                        if (obj.error !== undefined && obj.error !== null) {
                            that.emit("error", obj.error);
                        }
                        if (obj.data !== undefined && obj.data !== null) {
                            that.emit("data", obj.data);
                        }
                    }
                });
            }, (1000 / frames));
        }
    }

    readImageStreamThreadly(quality, type, frames) {
        if (!this.destroy) {
            if (typeof frames !== "number" || frames <= 0) frames = 1;
            if (type !== RGB && type !== YUV && type !== JPEG) type = "rgb";

            if (typeof quality !== "number") quality = 80;
            else if (quality <= 0) quality = 80;
            else if (quality >= 100) quality = 100;

            let typeNo = 0;
            if (type === YUV) typeNo = 0;
            else if (type === RGB) typeNo = 1;
            else if (type === JPEG) typeNo = 2;
            let that = this;
            ffmpeg_nodejs.video2ImageStreamThreadly(typeNo, quality, frames, (obj) => {
                if (obj !== undefined && obj !== null) {
                    if (obj.error !== undefined && obj.error !== null) {
                        that.emit("error", obj.error);
                    }
                    if (obj.data !== undefined && obj.data !== null) {
                        that.emit("data", obj.data);
                    }
                }
            });
        }
    }

    /**
     * close this url video stream
     */
    close() {
        if (!this.destroy) {
            this.destroy = true;
            if (this.read)
                clearInterval(this.read);
            return ffmpeg_nodejs.destroyStream();
        }
    }

    static TYPE() {
        return {
            JPEG: JPEG,
            YUV: YUV,
            RGB: RGB
        }
    };

    static LEVEL() {
        return {
            INFO: INFO,
            DEBUG: DEBUG,
            ERROR: ERROR
        }
    }
}

module.exports = FFmpegNode;