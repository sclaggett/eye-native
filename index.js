// index.js: This code is the JavaScript bridge between the functions that this
// module exports and the C++ wrapper defined in Wrapper.h.

const native = require('bindings')('eyenative');

/**
 * The initializeFfmpeg() function should be called before anything else to 
 * set the location of the FFmpeg executable.
 */
function initializeFfmpeg(ffmpegPath) {
  native.initializeFfmpeg(ffmpegPath);
}

/**
 * Use the functions in this section to create a new video file, queue frames to be
 * written to that file, check periodically to see which frames have been processed,
 * and close the file when finished.
 */

function createVideoOutput(width, height, fps, encoder, outputPath) {
  return native.createVideoOutput(width, height, fps, encoder, outputPath);
}

function queueNextFrame(buffer, width, height) {
  return native.queueNextFrame(buffer, width, height);
}

function checkCompletedFrames() {
  return native.checkCompletedFrames();
}

function closeVideoOutput() {
  native.closeVideoOutput();
}

/**
 * Use the functions in this section to open an existing video file, read the frames,
 * and close when finished.
 */

// openVideoInput
// readNextFrame
// closeVideoInput

/**
 * The functions in this section give us the ability to process a video in the main thread
 * and show a preview of it in a BrowserWindow, all without having to use the Electron
 * framework to pass the image between them. The channel should be created by the main
 * thread, and the browser window should open it, read each frame, and close it when
 * finished.
 */

function createPreviewChannel() {
  return native.createPreviewChannel();
}

function openPreviewChannel(name) {
  return native.openPreviewChannel(name);
}

function getNextFrame(width, height) {
  return native.getNextFrame(width, height);
}

function closePreviewChannel() {
  native.closePreviewChannel();
}

module.exports = {
  initializeFfmpeg,
  createVideoOutput,
  queueNextFrame,
  checkCompletedFrames,
  closeVideoOutput,
  createPreviewChannel,
  openPreviewChannel,
  getNextFrame,
  closePreviewChannel
};
