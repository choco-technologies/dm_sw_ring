# DM_SW_RING Documentation

Welcome to the DM_SW_RING module documentation.

## Contents

- **[dm_sw_ring.md](dm_sw_ring.md)** - Module overview and architecture
- **[api-reference.md](api-reference.md)** - Complete API documentation
- **[examples.md](examples.md)** - Usage examples

## Quick Reference

```c
#include "dm_sw_ring.h"

// Create a 64-byte ring buffer with default settings (drop-old, mutex, blocking)
dm_sw_ring_t ring = dm_sw_ring_create(64, dm_sw_ring_flags_default);

// Write bytes into the buffer
uint8_t data[] = {0xAA, 0xBB, 0xCC};
dm_sw_ring_capacity_t written = dm_sw_ring_write(ring, data, sizeof(data));

// Read bytes from the buffer
uint8_t buf[3];
dm_sw_ring_capacity_t read = dm_sw_ring_read(ring, buf, sizeof(buf));

// Peek without consuming
dm_sw_ring_peek(ring, buf, 1);

// Discard bytes without reading
dm_sw_ring_discard(ring, 1);

// Inspect state
dm_sw_ring_capacity_t cap   = dm_sw_ring_capacity(ring);
dm_sw_ring_capacity_t used  = dm_sw_ring_size(ring);
dm_sw_ring_capacity_t free_ = dm_sw_ring_available_space(ring);
bool full  = dm_sw_ring_is_full(ring);
bool empty = dm_sw_ring_is_empty(ring);

// Destroy when done
dm_sw_ring_destroy(ring);
```
