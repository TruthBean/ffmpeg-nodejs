const EventEmitter = require("events").EventEmitter;
const ffmpeg_nodejs = require("./build/Release/ffmpeg_nodejs");

const JPEG = "jpeg", YUV = "yuv", RGB = "rgb";
const INFO = 3, DEBUG = 2, ERROR = 1;
const HLS = 0, MPEGTS = 1, FLV = 2, MP4 = 3, RAW = 4;
class FFmpegNode extends EventEmitter {

    libraryId = 0;

    /**
     * init
     * @param {String} url 
     * @param {boolean} nobuffer
     * @param {boolean} useGpu
     * @param {number} timeout
     * @param {number} level
     * @param {number} gpuId
     * @param {boolean} useTcp
     * @return {Promise<FFmpegNode>}
     */
    static init(url, timeout, nobuffer, useGpu, level, gpuId, useTcp) {
        let self = new FFmpegNode();

        if (typeof url !== "string")
            throw new Error("url must be string type");
        self.url = url;

        if (typeof timeout !== "number")
            throw new Error("timeout must be number");

        if (typeof nobuffer !== "boolean") {
            console.error("nobuffer must be boolean type, it turn to default false");
            nobuffer = false;
        }

        if (typeof useGpu !== "boolean") {
            console.error("useGpu must be boolean type, it turn to default false");
            useGpu = false;
        }

        if (typeof level !== "number") {
            console.error("level must be number type, it turn to default INFO");
            level = INFO;
        }

        if (typeof gpuId !== "number") {
            console.error("gpuId must be number type, it turn to default 0");
            gpuId = 0;
        }

        if (typeof useTcp !== "boolean") {
            console.error("useTcp must be boolean type, it turn to default false");
            useTcp = false;
        }

        self.destroy = false;
        return new Promise((resolve, reject) => {
            ffmpeg_nodejs.initReadingVideo(self.url, timeout, nobuffer, useGpu, level, gpuId.toString(), useGpu, useTcp).then((pointer) => {
                self.libraryId = pointer;
                console.info("library pointer: " + pointer);
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
     * @param {number} level
     * @param {number} type
     */
    static recordVideo(url, filename, recordSeconds, useGpu, level, type) {
        if (typeof url !== "string") {
            throw new Error("url must be string type");
        }
        if (typeof filename !== "string")
            throw new Error("filename must be string type");
        if (typeof recordSeconds !== "number")
            throw new Error("recordSeconds must be number");
        if (typeof useGpu !== "boolean")
            throw new Error("useGpu must be boolean");

        console.info(level, type, typeof level !== "number", typeof type !== "number");
        if (typeof level !== "number")
            level = INFO;
        if (typeof type !== "number")
            type = OTHER;
        ffmpeg_nodejs.recordVideo(libraryId, url, filename, recordSeconds, useGpu, level, type);
    }

    /**
     * read raw data of frame from this url video stream
     * @param {number} quality: jpeg quality
     * @param {string} type: rgb, yuv or jpeg
     * @param {number} frames: chose frames per second, default 1
     * @return {Promise<Buffer>} image buffer
     */
    async readImageBuffer(quality, type, frames) {
        return await this.syncReadImageBuffer(quality, type, frames);
    }

    /**
     * read raw data of frame from this url video stream
     * @param {number} quality: jpeg quality
     * @param {string} type: rgb, yuv or jpeg
     * @param {number} frames: chose frames per second, default 1
     * @return {Promise<Buffer>} image buffer
     */
    async syncReadImageBuffer(quality, type, frames) {
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
            let self = this;
            return new Promise((resolve, reject) => {
                ffmpeg_nodejs.syncReadImageBuffer(self.libraryId, frames, typeNo, quality).then((data) => {
                    resolve(data);
                }).catch((err) => {
                    reject(err);
                });
            });
        } else {
            return new Promise((resolve, reject) => {
                resolve(null);
                reject(null);
            });
        }
    }

    /**
     * @private
     * @param {number} quality: jpeg quality
     * @param {string} type: rgb, yuv or jpeg
     * @param {number} frames: chose frames per second, default 1
     */
    readImageStream(quality, type, frames) {
        return this.asyncReadImageBuffer(quality, type, frames)
    }

    /**
     * @param {number} quality: jpeg quality
     * @param {string} type: rgb, yuv or jpeg
     * @param {number} frames: chose frames per second, default 1
     */
    asyncReadImageBuffer(quality, type, frames) {
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
                ffmpeg_nodejs.asyncReadImageBuffer(that.libraryId, typeNo, quality, frames, (obj) => {
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

    /**
     * @param {number} quality: jpeg quality
     * @param {string} type: rgb, yuv or jpeg
     * @param {number} frames: chose frames per second, default 1
     */
    readImageStreamThreadly(quality, type, frames) {
        this.asyncReadImageBufferThreadly(quality, type, frames);
    }

    /**
     * @param {number} quality: jpeg quality
     * @param {string} type: rgb, yuv or jpeg
     * @param {number} frames: chose frames per second, default 1
     */
    asyncReadImageBufferThreadly(quality, type, frames) {
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
            ffmpeg_nodejs.asyncReadImageBufferThreadly(that.libraryId, typeNo, quality, frames, (obj) => {
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
            return ffmpeg_nodejs.destroyStream(this.libraryId);
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

    static MuxingStreamType() {
        return {
            HLS: HLS,
            MPEGTS: MPEGTS,
            FLV: FLV,
            MP4: MP4,
            RAW: RAW
        }
    }
}

module.exports = FFmpegNode;