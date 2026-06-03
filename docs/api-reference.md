# DM_SW_RING API Reference

## Types

### `dm_sw_ring_t`

Opaque handle to a software ring buffer instance. All API functions operate on this handle.

```c
typedef struct dm_sw_ring* dm_sw_ring_t;
```

---

### `dm_sw_ring_capacity_t`

Unsigned 32-bit integer representing a byte count (capacity, size, or number of bytes to transfer).

```c
typedef uint32_t dm_sw_ring_capacity_t;
```

---

### `dm_sw_ring_flags_t`

Bitmask controlling ring buffer behavior. Values can be combined with `|`.

```c
typedef enum
{
    dm_sw_ring_flags_drop_old_data      = (1 << 0),
    dm_sw_ring_flags_mutex_sync         = (1 << 1),
    dm_sw_ring_flags_wait_for_space     = (1 << 2),
    dm_sw_ring_flags_wait_for_data      = (1 << 3),
    dm_sw_ring_flags_wait_for_some_data = dm_sw_ring_flags_wait_for_data,
    dm_sw_ring_flags_wait_for_all_data  = dm_sw_ring_flags_wait_for_data | (1 << 4),

    dm_sw_ring_flags_default = dm_sw_ring_flags_drop_old_data
                             | dm_sw_ring_flags_mutex_sync
                             | dm_sw_ring_flags_wait_for_space
                             | dm_sw_ring_flags_wait_for_some_data,
} dm_sw_ring_flags_t;
```

| Value | Bit | Description |
|-------|-----|-------------|
| `dm_sw_ring_flags_drop_old_data` | 0 | Overwrite oldest data when buffer is full |
| `dm_sw_ring_flags_mutex_sync` | 1 | Use a DMOD mutex for thread-safe access |
| `dm_sw_ring_flags_wait_for_space` | 2 | Block writer until space is available |
| `dm_sw_ring_flags_wait_for_some_data` | 3 | Block reader until at least one byte is available |
| `dm_sw_ring_flags_wait_for_all_data` | 3+4 | Block reader until all requested bytes are available |
| `dm_sw_ring_flags_default` | — | `drop_old_data \| mutex_sync \| wait_for_space \| wait_for_some_data` |

---

## Functions

### `dm_sw_ring_create`

Create a new software ring buffer instance.

```c
dm_sw_ring_t dm_sw_ring_create(dm_sw_ring_capacity_t capacity, dm_sw_ring_flags_t flags);
```

**Parameters:**
- `capacity` – Buffer size in bytes. Must be greater than 0 and at most `INT32_MAX`.
- `flags` – Bitmask of `dm_sw_ring_flags_t` values that control synchronization and overflow behaviour.

**Returns:** A valid `dm_sw_ring_t` handle on success, or `NULL` on failure (invalid capacity or memory allocation error).

---

### `dm_sw_ring_destroy`

Destroy a ring buffer instance and release all associated resources (buffer memory, mutex, semaphores).

```c
void dm_sw_ring_destroy(dm_sw_ring_t ring);
```

**Parameters:**
- `ring` – Handle previously returned by `dm_sw_ring_create`.

> **Note:** After `dm_sw_ring_destroy` returns, the handle is invalid and must not be used.

---

### `dm_sw_ring_write`

Write bytes into the ring buffer.

```c
dm_sw_ring_capacity_t dm_sw_ring_write(dm_sw_ring_t ring, const void* data, dm_sw_ring_capacity_t length);
```

**Parameters:**
- `ring` – Ring buffer handle.
- `data` – Pointer to the source data. Must not be `NULL` when `length > 0`.
- `length` – Number of bytes to write.

**Returns:** Number of bytes actually written. This may be less than `length` if the buffer is full and neither `drop_old_data` nor `wait_for_space` is set. Returns `0` on error.

**Behaviour summary:**

| Flags set | Buffer full → |
|-----------|---------------|
| `drop_old_data` | Oldest bytes are overwritten |
| `wait_for_space` | Writer blocks until space is available |
| Neither | Returns the number of bytes that fit; remaining bytes are silently dropped |

---

### `dm_sw_ring_read`

Read and consume bytes from the ring buffer.

```c
dm_sw_ring_capacity_t dm_sw_ring_read(dm_sw_ring_t ring, void* buffer, dm_sw_ring_capacity_t length);
```

**Parameters:**
- `ring` – Ring buffer handle.
- `buffer` – Destination buffer. Must not be `NULL` when `length > 0`.
- `length` – Maximum number of bytes to read.

**Returns:** Number of bytes actually read. With `wait_for_some_data` this is at least 1 (unless `ring` is invalid). With `wait_for_all_data` the call blocks until `length` bytes have been read. Without either wait flag the call returns immediately with however many bytes are available.

---

### `dm_sw_ring_capacity`

Return the total capacity of the ring buffer in bytes.

```c
dm_sw_ring_capacity_t dm_sw_ring_capacity(dm_sw_ring_t ring);
```

**Returns:** Buffer capacity as passed to `dm_sw_ring_create`, or `0` on error.

---

### `dm_sw_ring_size`

Return the number of bytes currently stored in the ring buffer.

```c
dm_sw_ring_capacity_t dm_sw_ring_size(dm_sw_ring_t ring);
```

**Returns:** Number of bytes available to read, or `0` on error.

---

### `dm_sw_ring_available_space`

Return the number of free bytes remaining in the ring buffer.

```c
dm_sw_ring_capacity_t dm_sw_ring_available_space(dm_sw_ring_t ring);
```

**Returns:** `capacity - size`, or `0` on error.

---

### `dm_sw_ring_peek`

Copy bytes from the front of the ring buffer into `buffer` without consuming them.

```c
int32_t dm_sw_ring_peek(dm_sw_ring_t ring, void* buffer, dm_sw_ring_capacity_t length);
```

**Parameters:**
- `ring` – Ring buffer handle.
- `buffer` – Destination buffer. Must not be `NULL` when `length > 0`.
- `length` – Maximum number of bytes to peek.

**Returns:** Number of bytes copied (may be less than `length` if fewer bytes are stored), or `-1` on error (e.g. `NULL` buffer with non-zero length, or invalid handle).

> **Note:** Peek does not advance the read pointer; subsequent reads will return the same bytes.

---

### `dm_sw_ring_discard`

Remove bytes from the front of the ring buffer without copying them to a destination buffer.

```c
int32_t dm_sw_ring_discard(dm_sw_ring_t ring, dm_sw_ring_capacity_t length);
```

**Parameters:**
- `ring` – Ring buffer handle.
- `length` – Number of bytes to discard. Clamped to the current buffer size.

**Returns:** Number of bytes actually discarded, `0` if `length` is 0, or `-1` on error (invalid handle).

---

### `dm_sw_ring_clear`

Remove all bytes from the ring buffer, resetting it to an empty state.

```c
int32_t dm_sw_ring_clear(dm_sw_ring_t ring);
```

**Returns:** `0` on success, or `-1` on error (invalid handle).

---

### `dm_sw_ring_is_full`

Check whether the ring buffer is completely full.

```c
bool dm_sw_ring_is_full(dm_sw_ring_t ring);
```

**Returns:** `true` if `size == capacity`, `false` otherwise (or on error).

---

### `dm_sw_ring_is_empty`

Check whether the ring buffer contains no data.

```c
bool dm_sw_ring_is_empty(dm_sw_ring_t ring);
```

**Returns:** `true` if `size == 0`, `false` otherwise (or on error).
