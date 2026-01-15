# GPU Bandwidth Profiler - Python Client

Python client library for controlling the GPU Bandwidth Profiler server running on Android devices.

## Installation

### From Source

```bash
cd python
pip install .
```

### Development Installation

```bash
cd python
pip install -e .
```

### From PyPI (when published)

```bash
pip install gpu-bandwidth-profiler
```

## Quick Start

```python
from gpu_bandwidth_profiler import GPUBandwidthClient

# Connect to server
client = GPUBandwidthClient(host="localhost", port=8888)
client.connect()

# Start profiling
client.start(sampling_interval_ms=200)

# ... do some work ...

# Stop profiling
client.stop()

# Get sample data
data = client.get_data()
print(f"Collected {len(data)} samples")

client.disconnect()
```

## Command Line Tools

After installation, you can use:

```bash
gpu-bandwidth-monitor [host] [port]  # Real-time monitoring
gpu-bandwidth-view [host] [port]     # View collected data
```

## Overview

This client-server architecture allows you to:
- Run the GPU Bandwidth Profiler server on an Android device
- Control profiling and retrieve data from a Python client on your host machine
- Monitor GPU bandwidth remotely over a network connection

## Architecture

```
┌─────────────────┐         TCP/IP         ┌──────────────────┐
│  Python Client  │ ◄──────────────────► │  Server (Android) │
│   (Host PC)     │    Port 8888 (default) │   (Device)        │
└─────────────────┘                        └──────────────────┘
```

## Setup

### 1. Build and Deploy Server to Android Device

```bash
# Build server for Android (Mali backend)
bazel build --config=android_mali //src/server:gpu_bandwidth_server

# Deploy to device
adb push bazel-bin/src/server/gpu_bandwidth_server /data/local/tmp/
adb shell chmod +x /data/local/tmp/gpu_bandwidth_server
```

### 2. Start Server on Device

```bash
# Start server on device (default port 8888)
adb shell /data/local/tmp/gpu_bandwidth_server

# Or specify a custom port
adb shell /data/local/tmp/gpu_bandwidth_server 9999
```

### 3. Forward Port (if connecting from host)

```bash
# Forward device port to host
adb forward tcp:8888 tcp:8888
```

## API Reference

### GPUBandwidthClient

#### Methods

- `connect()` - Connect to the server
- `disconnect()` - Disconnect from the server
- `ping()` - Check if server is responsive
- `get_status()` - Get server status (backend name, profiling state)
- `start(sampling_interval_ms=100)` - Start bandwidth profiling
- `stop()` - Stop bandwidth profiling
- `clear_buffer()` - Clear the accumulator buffer
- `get_data()` - Get accumulated bandwidth data as list of BandwidthData
- `get_statistics(stat_start, stat_end)` - Get statistics for a time range

### Data Structures

#### BandwidthData
- `read_bandwidth_mbps: float`
- `write_bandwidth_mbps: float`
- `total_bandwidth_mbps: float`
- `timestamp_ns: int`

#### BandwidthStatistics
- `sample_count: int`
- `read_avg_mbps: float`
- `read_max_mbps: float`
- `read_median_mbps: float`
- `write_avg_mbps: float`
- `write_max_mbps: float`
- `write_median_mbps: float`
- `total_avg_mbps: float`
- `total_max_mbps: float`
- `total_median_mbps: float`
- `start_timestamp_ns: int`
- `end_timestamp_ns: int`
- `duration_sec: float`

## Examples

See `gpu_bandwidth_profiler/example.py` for a complete example.

## License

[Add your license information here]