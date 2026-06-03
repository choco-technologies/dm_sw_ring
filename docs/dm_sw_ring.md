# DM_SW_RING Module Overview

## Introduction

DM_SW_RING is a DMOD (Dynamic Modular System) module that provides a lightweight, general-purpose software ring buffer (also known as a circular buffer or FIFO) for embedded systems. It stores arbitrary binary data at byte granularity and supports optional thread-safety through mutex synchronization and semaphore-based blocking for producers and consumers.

## Architecture

```
┌─────────────────────────────────────────┐
│           Application Code              │
└────────────────┬────────────────────────┘
                 │  dm_sw_ring API
┌────────────────▼────────────────────────┐
│            dm_sw_ring                   │
│  (Software ring buffer implementation)  │
│                                         │
│  - Byte-level circular buffer           │
│  - Optional mutex synchronization       │
│  - Optional blocking on read/write      │
│  - Peek / discard / clear operations    │
└─────────────────────────────────────────┘
```

## Key Concepts

### Ring Buffer Mechanics

The buffer maintains a contiguous block of memory of a fixed `capacity` (in bytes). Internally it tracks a `head` (read pointer), a `tail` (write pointer), and a `count` (number of bytes currently stored). When the tail wraps around past the end of the buffer it restarts from index 0, forming the "ring".

### Behavior Flags

The behavior of the ring buffer is controlled by a bitmask of `dm_sw_ring_flags_t` values supplied at creation time:

| Flag | Description |
|------|-------------|
| `dm_sw_ring_flags_drop_old_data` | When the buffer is full, the oldest bytes are silently overwritten to make room for new data |
| `dm_sw_ring_flags_mutex_sync` | A DMOD mutex is created to serialize concurrent access; without this flag all operations use a critical section |
| `dm_sw_ring_flags_wait_for_space` | A writer blocks (on a semaphore) until enough space becomes available instead of dropping data immediately |
| `dm_sw_ring_flags_wait_for_some_data` | A reader blocks until at least one byte is available |
| `dm_sw_ring_flags_wait_for_all_data` | A reader blocks until **all** requested bytes are available |
| `dm_sw_ring_flags_default` | Combination of `drop_old_data`, `mutex_sync`, `wait_for_space`, and `wait_for_some_data` |

> **Note:** `wait_for_space` and `drop_old_data` are complementary: if `wait_for_space` is set, a writer that finds the buffer full will block rather than discard. If neither flag is set, a write to a full buffer returns 0 bytes written.

### Thread Safety

When `dm_sw_ring_flags_mutex_sync` is set, a mutex is created at buffer construction time. Every public API function acquires this mutex before operating on the internal state and releases it afterwards. Blocking semaphore operations (`wait_for_space` / `wait_for_data`) are performed *outside* the mutex to avoid deadlocks.

Without `dm_sw_ring_flags_mutex_sync`, a DMOD critical section (interrupt-safe spinlock) is used instead, which is sufficient for single-threaded or interrupt-driven environments.

## Module Files

```
dm_sw_ring/
├── docs/              # Documentation (markdown format)
├── include/
│   └── dm_sw_ring.h  # Public API: types, flags, function declarations
├── src/
│   └── dm_sw_ring.c  # Implementation
├── tests/
│   ├── CMakeLists.txt
│   └── test_dm_sw_ring.c
├── CMakeLists.txt
├── dm_sw_ring.dmr    # DMOD resource file
└── manifest.dmm      # DMOD manifest
```
