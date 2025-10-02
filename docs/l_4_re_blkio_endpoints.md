## StarForth Block I/O — L4Re Porting Endpoints

This document explains how the existing `blkio` API maps cleanly onto L4Re, what you need to implement, and where your
VM and Forth block words connect. The idea is: the **northbound API never changes**; you only swap in different
backends (dataspace or IPC service) when porting to L4Re.

---

### 1. Northbound API (unchanged)

Your VM and block words always call:

```c
int  blkio_open (blkio_dev_t *dev, const blkio_vtable_t *vt, const blkio_params_t *p);
int  blkio_close(blkio_dev_t *dev);
int  blkio_read (blkio_dev_t *dev, uint32_t fblock, void *dst);
int  blkio_write(blkio_dev_t *dev, uint32_t fblock, const void *src);
int  blkio_flush(blkio_dev_t *dev);
int  blkio_info (blkio_dev_t *dev, blkio_info_t *out);
```

No matter what backend you use, these remain stable. That’s your ABI.

---

### 2. In-process L4Re Dataspace Backend

Use when your VM can directly attach a dataspace. This gives you the fastest path: no IPC.

**Symbols you implement:**

```c
size_t                 blkio_l4ds_state_size(void);
int                    blkio_l4ds_init_state(void *state_mem, size_t state_len,
                                             l4_cap_idx_t ds_cap,
                                             l4_addr_t attach_addr,
                                             l4_size_t attach_size,
                                             uint32_t total_blocks,
                                             uint32_t fbs,
                                             uint8_t  read_only,
                                             void   **out_opaque);
const blkio_vtable_t  *blkio_l4ds_vtable(void);
```

**State struct:**

```c
typedef struct {
  l4_cap_idx_t ds;     /* dataspace cap */
  void        *base;   /* attached VA */
  l4_size_t    size;   /* mapped length */
  uint32_t     fbs;    /* block size */
  uint32_t     blocks; /* #blocks */
  uint8_t      ro;
} blkio_l4ds_state_t;
```

**Ops:**

- `open`: `l4re_rm_attach()` dataspace, derive block count.
- `read/write`: `memcpy` from attached region.
- `flush`: NOP (or `l4re_ds_op` if needed).
- `close`: `l4re_rm_detach()`.

---

### 3. Out-of-process Block Server (IPC Backend)

Use when storage is owned by a different task (e.g., AHCI, NVMe, eMMC server).

**Client-side symbols:**

```c
size_t                 blkio_l4svc_state_size(void);
int                    blkio_l4svc_init_state(void *state_mem, size_t state_len,
                                             l4_cap_idx_t server_cap,
                                             l4_cap_idx_t bounce_ds,
                                             uint32_t fbs,
                                             uint8_t  read_only,
                                             void   **out_opaque);
const blkio_vtable_t  *blkio_l4svc_vtable(void);
```

**IPC protocol (example):**

```c
enum bsvc_op {
  BSVC_INFO  = 1,
  BSVC_READ  = 2,
  BSVC_WRITE = 3,
  BSVC_FLUSH = 4,
  BSVC_PING  = 5
};
```

Transport choices:

- Shared dataspace (preferred): server + client map the same page(s).
- Inline IPC (simpler, less efficient): block payload in IPC message.

**Ops mapping:**

- `read`: send op + fblock, server fills shared buffer.
- `write`: copy into shared buffer, send op + fblock.
- `info`: query block size, count, flags.

---

### 4. Factory Helpers on L4Re

You can add overloads to `blkio_factory` for clarity:

```c
int blkio_factory_open_l4ds (blkio_dev_t *dev, l4_cap_idx_t ds_cap, uint32_t fbs, uint8_t ro);
int blkio_factory_open_file (blkio_dev_t *dev, const char *path, uint32_t fbs, uint8_t ro);
int blkio_factory_open_l4svc(blkio_dev_t *dev, l4_cap_idx_t server_cap, l4_cap_idx_t bounce_ds, uint32_t fbs, uint8_t ro);
```

This way `main.c` can pick a backend cleanly depending on CLI args and environment.

---

### 5. CLI Semantics on L4Re

- `--disk-img=<path>` → Either:
    - VFS+file server that returns a dataspace → **l4ds backend**
    - Or connect to a block server → **l4svc backend**
- `--ram-disk=<MB>` → Allocate RAM dataspace, use **l4ds backend**.

Fallback: if no `--disk-img`, create RAM disk from allocator.

---

### 6. Composite Backends (RAM + Disk, 4 KiB packing)

Your design needs:

- Blocks 0–1023 → RAM backend.
- Blocks ≥1024 → Disk backend.
- All wrapped in a composite vtable that dispatches based on `fblock`.

Later: add an outer wrapper that packs 3×1 KiB + 1 KiB metadata into 4 KiB pages for device alignment. Still the same
five ops.

---

## TL;DR — Endpoints to Implement for L4Re

**New backends:**

```c
/* In-proc dataspace */
blkio_l4ds_state_size();
blkio_l4ds_init_state(...);
blkio_l4ds_vtable();

/* IPC client/server */
blkio_l4svc_state_size();
blkio_l4svc_init_state(...);
blkio_l4svc_vtable();
```

**Optional factory helpers:**

```c
blkio_factory_open_l4ds(...);
blkio_factory_open_l4svc(...);
```

**Everything else stays the same**: VM calls only `blkio_read`, `blkio_write`, etc. You swap backends, not call sites.