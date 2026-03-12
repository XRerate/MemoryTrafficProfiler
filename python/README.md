# Memory Traffic Profiler - Python Client

Python client library for controlling the Memory Traffic Profiler server running on Android devices.

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

## Quick Start

```python
from memory_traffic_profiler import MemoryTrafficClient

# Connect to server
client = MemoryTrafficClient(host="localhost", port=8888)
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

## Overview

This client-server architecture allows you to:
- Run the Memory Traffic Profiler server on an Android device
- Control profiling and retrieve data from a Python client on your host machine
- Monitor memory traffic (GPU, CPU, NPU) remotely over a network connection

## Architecture

```
┌─────────────────┐         TCP/IP         ┌──────────────────┐
│  Python Client  │ ◄──────────────────► │  Server (Android) │
│   (Host PC)     │    Port 8888 (default) │   (Device)        │
└─────────────────┘                        └──────────────────┘
```

## API Reference

### MemoryTrafficClient

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

See `memory_traffic_profiler/example.py` for a complete example.

## License

[Add your license information here]
