const ffmpeg_nodejs = require('./build/Release/ffmpeg_nodejs');

const exportObj = {};

/**
 * @param {string} input_filename
 */
exportObj.initReadingVideo = (input_filename) => {
    if (typeof input_filename !== "string")
        throw Error("input_filename must be string type");
    return ffmpeg_nodejs.initReadingVideo(input_filename);
}

/**
 * @param {string} input_filename
 * @param {number} jpeg quality percent, default 80
 * @return {Buffer} output file data
 */
exportObj.video2JpegStream = (quality) => {
    if (typeof quality !== "number") quality = 80;
    else if (quality <= 0) quality = 80;
    else if (quality >= 100) quality = 100;
    return ffmpeg_nodejs.video2JpegStream(parseInt(quality));
};

exportObj.destroyStream = () => {
    return ffmpeg_nodejs.destroyStream();
};

module.exports = exportObj;