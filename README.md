# Camera Sensor

[![sfc_core CI](https://github.com/josen00000/CameraSensor/actions/workflows/core.yml/badge.svg)](https://github.com/josen00000/CameraSensor/actions/workflows/core.yml)

Ship the SimHarness scenario (P1) publicly first. Then build a camera for SimHarness that saves three images per shot: the normal picture (RGB), a mask of which pixels belong to which object, and a depth image.

The game never pauses for a shot. The GPU copies each image back in the background, and worker threads compress and save the files. If the disk cannot keep up, a bounded queue either holds the data (lossless mode) or drops the oldest frames and logs it (real-time mode). A small GPU compute pass packs depth and counts each object's visible pixels.

The camera is measured against a naive synchronous version: game-thread cost per shot, time from shot to file on disk, and frames dropped at 10 Hz and 30 Hz. The queue is proven thread-safe, and labels are reproducible from the seed.
