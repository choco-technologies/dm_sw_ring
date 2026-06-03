# DM_SW_RING - DMOD Software Ring Buffer Module

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

A DMOD (Dynamic Modular System) module providing a general-purpose software ring buffer (circular buffer) implementation for embedded systems.

## Features

- **Byte-level Ring Buffer**: Stores arbitrary binary data byte-by-byte with configurable capacity
- **Overwrite Mode**: Optionally discard oldest data when the buffer is full (`dm_sw_ring_flags_drop_old_data`)
- **Thread Safety**: Optional mutex-based synchronization for use in multi-threaded environments (`dm_sw_ring_flags_mutex_sync`)
- **Blocking Writes**: Optionally block a writer until space becomes available (`dm_sw_ring_flags_wait_for_space`)
- **Blocking Reads**: Optionally block a reader until at least some data is available (`dm_sw_ring_flags_wait_for_some_data`) or until all requested data is available (`dm_sw_ring_flags_wait_for_all_data`)
- **Non-destructive Peek**: Inspect buffered data without consuming it
- **Discard and Clear**: Remove data from the buffer without reading it
- **DMOD Integration**: Follows the DMOD module lifecycle (`dmod_init` / `dmod_deinit`)

## Quick Start

### Installation

Using `dmf-get` from the DMOD release package:

```bash
dmf-get install dm_sw_ring
```

### Basic Usage

```c
#include "dm_sw_ring.h"

// Create a 64-byte ring buffer with default settings
dm_sw_ring_t ring = dm_sw_ring_create(64, dm_sw_ring_flags_default);

// Write data
uint8_t tx[] = {1, 2, 3, 4};
dm_sw_ring_capacity_t written = dm_sw_ring_write(ring, tx, sizeof(tx));

// Read data
uint8_t rx[4];
dm_sw_ring_capacity_t read = dm_sw_ring_read(ring, rx, sizeof(rx));

// Destroy when done
dm_sw_ring_destroy(ring);
```

## Building

### Prerequisites

- CMake 3.18 or higher
- C99-compatible compiler
- DMOD framework (automatically fetched via CMake FetchContent)

### Build Commands

```bash
# Configure
cmake -B build

# Build
cmake --build build
```

## Documentation

Comprehensive documentation is available in the `docs/` directory:

- **[dm_sw_ring.md](docs/dm_sw_ring.md)** - Module overview and architecture
- **[api-reference.md](docs/api-reference.md)** - Complete API documentation
- **[examples.md](docs/examples.md)** - Usage examples

View documentation using `dmf-man`:

```bash
dmf-man dm_sw_ring          # Main documentation
dmf-man dm_sw_ring api      # API reference
dmf-man dm_sw_ring examples # Usage examples
```

## Project Structure

```
dm_sw_ring/
├── docs/              # Documentation (markdown format)
├── include/           # Public headers
│   └── dm_sw_ring.h  # Main API (types, flags, function declarations)
├── src/
│   └── dm_sw_ring.c  # Core implementation
├── tests/
│   ├── CMakeLists.txt         # Test build configuration
│   └── test_dm_sw_ring.c      # Unit tests
├── CMakeLists.txt    # Build configuration
├── dm_sw_ring.dmr    # DMOD resource file
└── manifest.dmm      # DMOD manifest
```

## Contributing

Contributions are welcome! Please:

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Submit a pull request

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Authors

- Patryk Kubiak - Initial work

## Related Projects

- [DMOD](https://github.com/choco-technologies/dmod) - Dynamic Modular System framework
- [DMGPIO](https://github.com/choco-technologies/dmgpio) - DMOD GPIO Driver Module
- [DMHAMAN](https://github.com/choco-technologies/dmhaman) - DMOD Handler Manager

## Support

For issues, questions, or contributions:

- Open an issue on GitHub
- Check the documentation in `docs/`
- Use `dmf-man dm_sw_ring` for command-line help
