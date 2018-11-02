const ffmpeg_nodejs = require('./build/Release/ffmpeg_nodejs');

const exportObj = {};

/**
 * @param {string} input_filename
 * @return {Promise<void>} promise
 */
exportObj.initReadingVideo = (input_filename) => {
    if (typeof input_filename !== "string")
        throw Error("input_filename must be string type");
    return ffmpeg_nodejs.initReadingVideo(input_filename);
}

/**
 * @param {string} input_filename
 * @param {number} jpeg quality percent, default 80
 * @param {number} frames_per_second default 25
 * @param {number} chose_frames_per_second default 5
 * @param {Function} callback callback function with param returned buffer
 * @return {Promise<Buffer>} buffer
 */
exportObj.video2JpegStream = (quality, frames_per_second, chose_frames_per_second) => {
    if (typeof quality !== "number") quality = 80;
    else if (quality <= 0) quality = 80;
    else if (quality >= 100) quality = 100;
    
    if (typeof frames_per_second !== "number" || frames_per_second <= 0) frames_per_second = 25;

    if (typeof chose_frames_per_second !== "number" || chose_frames_per_second <= 0 
        || chose_frames_per_second > frames_per_second) chose_frames_per_second = 5;

    return ffmpeg_nodejs.video2JpegStream(parseInt(quality), parseInt(frames_per_second), parseInt(chose_frames_per_second));
};

exportObj.destroyStream = () => {
    return ffmpeg_nodejs.destroyStream();
};

module.exports = exportObj;