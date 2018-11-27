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
            ffmpeg_nodejs.initReadingVideo(self.url, nobuffer, useGpu).then((ignored) => {
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
     * @param {number} frames: chose frames per second, default 1
     * @param {string} type: rgb, yuv or jpeg
     * @param {number} quality: jpeg quality
     * @param {Function} callback callback function
     * @return {Promise<Buffer>} image buffer
     */
    readImageBuffer(frames, type, quality) {
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
    async readImageStream(quality, type, frames) {
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
                let begin = new Date().getTime();
                ffmpeg_nodejs.video2ImageStream(typeNo, quality, frames, (obj) => {
                    if (obj !== undefined && obj !== null) {
                        if (obj.error !== undefined && obj.error !== null) {
                            that.emit("error", obj.error);
                        }
                        if (obj.data !== undefined && obj.data !== null) {
                            that.emit("data", obj.data);
                        }
                    }
                    let end = new Date().getTime();
                    console.info("video2ImageStream spend time: " + (end - begin));
                });
            }, (1000 / frames));
        }
    }

    /**
     * close this url video stream
     */
    close() {
        if (!this.destroy) {
            this.destroy = true;
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
}

module.exports = FFmpegNode;