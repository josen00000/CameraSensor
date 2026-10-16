# Camera Sensor

[![sfc_core CI](https://github.com/josen00000/CameraSensor/actions/workflows/core.yml/badge.svg)](https://github.com/josen00000/CameraSensor/actions/workflows/core.yml)

This repository contains the **StallFreeCapture** plugin and the **CaptureSandbox** host project. StallFreeCapture will provide an asynchronous camera system for Unreal Engine 5.8 that captures scene images without stalling the game thread.

## Overview

The plugin will capture three image types per shot: RGB color, object segmentation mask, and depth. Captures will happen in the background using GPU-accelerated readback and worker threads for compression and file I/O. A lock-free bounded queue will manage the data pipeline with configurable policies for when the queue fills: block until space is available, drop the oldest frame, or reject new captures.

The system will be measured against a naive synchronous baseline, tracking per-shot game thread cost, capture-to-disk latency, and frame drops at various capture rates. The lock-free queue will be proven thread-safe under race detectors, and deterministic scene simulation will ensure reproducible captures from the same seed.

## Components

- **StallFreeCapture plugin**: Will provide the core capture system, lock-free queue, and integration with Unreal's rendering pipeline
- **CaptureSandbox**: Host Unreal project for testing and benchmarking the capture system
- **sfc_core**: Cross-platform C++20 library that will provide the lock-free queue, deterministic random number generation, fixed-step clock, and latency measurement utilities
