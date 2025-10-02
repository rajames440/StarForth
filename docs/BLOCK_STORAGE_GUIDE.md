# StarForth Block Storage User Guide

**Date:** 2025-10-02
**Version:** 1.1.0
**Status:** Production-Ready

---

## Overview

StarForth implements a **3-layer block storage architecture** designed for persistent storage in embedded, microkernel,
and traditional environments. The system provides 1024-byte Forth blocks with RAM caching and disk persistence.

### Key Features

- **Dual storage tiers**: RAM blocks (0-1023) and disk blocks (1024+)
- **Persistent storage**: Block data survives across program restarts
- **LRU caching**: Automatic cache management with dirty tracking
- **4KB alignment**: Optimized for modern storage devices
- **Multi-backend support**: FILE and RAM backends included, L4Re-ready
- **Hot-attach capability**: Multi-volume support for dynamic storage

---

## Quick Start

### Creating a Disk Image

Use `qemu-img` to create a raw disk image:

```bash
qemu-img create -f raw mydisk.img 500M
```

### Running StarForth with Persistent Storage

```bash
./build/starforth --disk-img=mydisk.img
```

### Basic Block Operations

```forth
\ Write data to block 2048
2048 BLOCK 1024 65 FILL   \ Fill 1024 bytes with 'A' (ASCII 65)
UPDATE                     \ Mark block as modified
FLUSH                      \ Write to disk

\ Read and display block
2048 LIST                  \ Show formatted block content

\ Verify persistence (restart StarForth)
2048 LIST                  \ Block data persists!
```

---

## Architecture

### 3-Layer Design

```
┌─────────────────────────────────────────┐
│  Layer 3: block_words.c (Forth Words)   │  ← BLOCK BUFFER UPDATE FLUSH LIST
├─────────────────────────────────────────┤
│  Layer 2: block_subsystem.c (Mapping)   │  ← RAM/Disk routing, cache, packing
├─────────────────────────────────────────┤
│  Layer 1: blkio.h (Backend Abstraction) │  ← FILE, RAM, L4Re backends
└─────────────────────────────────────────┘
```

### Block Numbering

| Range  | Storage | Purpose                 | Persistence |
|--------|---------|-------------------------|-------------|
| 0      | —       | **Reserved** (metadata) | N/A         |
| 1-1023 | RAM     | Fast temporary storage  | No          |
| 1024+  | Disk    | Persistent storage      | Yes         |

### Storage Layout

**4KB Device Alignment:**

- 3× 1KB Forth blocks packed per 4KB device sector
- Efficient for modern SSDs and flash storage
- Metadata space: 1KB per 4KB sector (341 bytes × 3 blocks)

**Example mapping:**

```
Forth Block    Device Block    Offset
-----------    ------------    ------
1024           0               0
1025           0               1024
1026           0               2048
1027           1               0
...
```

---

## Command Reference

### Core Block Words

#### `BLOCK ( u -- addr )`

Returns address of block `u` for reading.

```forth
2048 BLOCK  \ Get address of block 2048
DUP C@      \ Read first byte
.           \ Display value
DROP        \ Clean up
```

#### `BUFFER ( u -- addr )`

Returns address of block `u` for writing (marks as dirty).

```forth
2048 BUFFER      \ Get writable buffer
42 OVER C!       \ Write value 42 to first byte
DROP             \ Clean up
UPDATE           \ Mark block as modified
```

#### `UPDATE ( -- )`

Marks the current block (SCR) as modified for later flushing.

```forth
2048 BLOCK 1024 65 FILL
UPDATE           \ Mark block 2048 as dirty
```

#### `FLUSH ( -- )`

Writes all dirty blocks to disk immediately.

```forth
UPDATE
FLUSH            \ Write all pending changes to disk
```

#### `SAVE-BUFFERS ( -- )`

Writes all dirty RAM and disk blocks to storage.

```forth
\ Modify several blocks
1024 BUFFER 1024 65 FILL DROP UPDATE
1025 BUFFER 1024 66 FILL DROP UPDATE
SAVE-BUFFERS     \ Write all changes
```

#### `LIST ( u -- )`

Displays block `u` in formatted 16-line × 64-character layout.

```forth
2048 LIST        \ Show block 2048

\ Output:
\ Block 2048
\ 00: AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
\ 01: AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
\ ...
\ 15: AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
```

#### `LOAD ( u -- )`

Loads and interprets block `u` as Forth source code (future feature).

```forth
\ Store Forth source in block 2000
\ Then execute it:
2000 LOAD        \ Future: interprets block as source
```

#### `THRU ( u1 u2 -- )`

Loads blocks u1 through u2 sequentially.

```forth
2000 2005 THRU   \ Future: load blocks 2000-2005
```

#### `SCR ( -- addr )`

Returns address of SCR (Screen) variable holding current block number.

```forth
SCR @            \ Get current block number
```

---

## Usage Examples

### Example 1: Simple Block Storage

```forth
\ Create and store data
2048 BLOCK           \ Get block address
1024 67 FILL         \ Fill with 'C'
UPDATE FLUSH         \ Save to disk

\ Verify (restart program)
2048 LIST            \ Shows all 'C's
```

### Example 2: Structured Data Storage

```forth
\ Define a record structure (32 bytes)
\ Offset  Size  Field
\ 0       8     ID (cell)
\ 8       8     Timestamp (cell)
\ 16      8     Value (cell)
\ 24      8     Flags (cell)

: RECORD-SIZE 32 ;
: RECORDS-PER-BLOCK 1024 RECORD-SIZE / ;  ( 32 records per block )

: RECORD-ADDR ( block# record# -- addr )
  RECORD-SIZE * SWAP BLOCK + ;

: SAVE-RECORD ( id timestamp value flags block# record# -- )
  RECORD-ADDR >R          ( -- id ts val flags ) ( R: addr )
  R@ 24 + !               ( store flags )
  R@ 16 + !               ( store value )
  R@ 8 + !                ( store timestamp )
  R> !                    ( store id )
  UPDATE FLUSH ;

\ Example usage
42 12345678 100 0 2048 0 SAVE-RECORD  \ Save first record in block 2048
```

### Example 3: Block-Based Text Editor Buffer

```forth
\ Simple line-based text buffer (16 lines × 64 chars)

: LINE-SIZE 64 ;
: LINES-PER-BLOCK 16 ;

: LINE-ADDR ( block# line# -- addr )
  LINE-SIZE * SWAP BLOCK + ;

: CLEAR-LINE ( block# line# -- )
  LINE-ADDR LINE-SIZE 32 FILL  \ Fill with spaces
  UPDATE ;

: WRITE-LINE ( c-addr u block# line# -- )
  LINE-ADDR SWAP LINE-SIZE MIN MOVE
  UPDATE ;

\ Example: Clear line 5 of block 2048
2048 5 CLEAR-LINE FLUSH
```

### Example 4: Persistent Stack

```forth
\ Save stack to block storage

: STACK-BLOCK 3000 ;  \ Use block 3000 for stack storage

: SAVE-STACK ( -- )
  DEPTH DUP 128 > IF DROP 128 THEN  \ Limit to 128 items
  DUP STACK-BLOCK BUFFER !          \ Store depth
  0 DO
    I 1+ PICK                        \ Get stack item
    STACK-BLOCK BUFFER I 1+ CELLS + !
  LOOP
  UPDATE FLUSH ;

: RESTORE-STACK ( -- )
  STACK-BLOCK BLOCK @                \ Get saved depth
  DUP 0 DO
    STACK-BLOCK BLOCK I 1+ CELLS + @
  LOOP ;

\ Usage
10 20 30 40 50  \ Put data on stack
SAVE-STACK       \ Save to disk
\ Restart program
RESTORE-STACK    \ Stack now has: 10 20 30 40 50
```

---

## Command-Line Options

### Block Storage Options

```bash
# Use a disk image file
./build/starforth --disk-img=<path>

# Open disk read-only (no writes)
./build/starforth --disk-img=<path> --disk-ro

# Use RAM fallback (default: 1MB)
./build/starforth --ram-disk=<MB>

# Set Forth block size (default: 1024 bytes)
./build/starforth --fbs=<bytes>
```

### Examples

```bash
# Production mode with persistent storage
./build/starforth --disk-img=./disks/production.img

# Development mode with 64MB RAM disk
./build/starforth --ram-disk=64

# Read-only access to existing disk
./build/starforth --disk-img=./disks/production.img --disk-ro

# Custom block size (advanced)
./build/starforth --disk-img=./disks/custom.img --fbs=2048
```

---

## Performance Considerations

### Block Caching

- **Cache size**: 8 slots (8 × 4KB = 32KB cache)
- **LRU eviction**: Least recently used blocks evicted first
- **Dirty tracking**: Only modified blocks written to disk
- **Automatic flush**: On `FLUSH` or program exit

### Optimization Tips

1. **Batch operations**: Group multiple block updates, then `FLUSH` once
2. **Use UPDATE**: Mark blocks dirty only when actually modified
3. **Sequential access**: Access blocks in order for better cache locality
4. **Avoid block 0**: Reserved for metadata, never accessible

### Performance Metrics

| Operation      | Time (typical) | Notes                     |
|----------------|----------------|---------------------------|
| BLOCK (cached) | ~100ns         | Memory access only        |
| BLOCK (disk)   | ~10-100μs      | Depends on storage device |
| FLUSH (1 blk)  | ~10-100μs      | Single block write        |
| FLUSH (all)    | ~0.1-1ms       | All dirty blocks (max 8)  |

---

## Integration with L4Re

StarForth's block storage is designed for L4Re microkernel integration:

### L4Re Backend Architecture

```c
// Add these backends for L4Re:

// 1. Dataspace backend (blkio_l4ds.c)
blkio_open_l4ds(l4re_ds_t dataspace, ...);

// 2. IPC service backend (blkio_l4svc.c)
blkio_open_l4svc(l4_cap_idx_t service_cap, ...);
```

### Example L4Re Integration

```c
#include <l4/re/dataspace>
#include "blkio.h"

// Create L4Re dataspace for block storage
l4re_ds_t ds = l4re_env->mem_alloc->alloc(512 * 1024 * 1024);

// Attach to block subsystem
blkio_dev_t dev;
blkio_open_l4ds(&dev, ds, 0, 0, 1024, ...);
blk_subsys_attach_device(&dev);

// Now Forth BLOCK words work with L4Re dataspace!
```

---

## Troubleshooting

### Common Issues

#### 1. "Failed to open disk image"

**Cause**: File doesn't exist or insufficient permissions

**Solution**:

```bash
# Create disk image
qemu-img create -f raw mydisk.img 500M

# Fix permissions
chmod 644 mydisk.img
```

#### 2. "Block 0 is reserved"

**Cause**: Attempting to access block 0 (metadata block)

**Solution**: Use blocks 1+ for data storage

```forth
\ Wrong:
0 BLOCK  \ ERROR: block 0 reserved

\ Correct:
1 BLOCK  \ Use block 1 or higher
```

#### 3. Data not persisting

**Cause**: Forgot to call `UPDATE` and `FLUSH`

**Solution**: Always update and flush after modifications

```forth
2048 BLOCK 1024 65 FILL
UPDATE        \ ← Don't forget this!
FLUSH         \ ← And this!
```

#### 4. "No disk device for block"

**Cause**: No disk image attached, trying to access block 1024+

**Solution**: Run with `--disk-img` option

```bash
./build/starforth --disk-img=mydisk.img
```

---

## Advanced Topics

### Multi-Volume Support

StarForth supports multiple block devices (future feature):

```forth
\ Future API design
: ATTACH-VOLUME ( device-id -- )  \ Attach storage device
: DETACH-VOLUME ( device-id -- )  \ Detach storage device
: VOLUME-INFO ( -- blocks size )   \ Query attached storage
```

### Block Metadata

Block 0 is reserved for volume metadata:

```c
typedef struct {
    uint32_t magic;           // 0x53544652 "STFR"
    uint32_t version;         // Format version
    uint32_t total_volumes;   // Number of volumes
    uint32_t flags;           // Volume flags
    char     label[64];       // Volume label
    uint8_t  reserved[944];   // Padding to 1024 bytes
} blk_volume_meta_t;
```

### Custom Backends

To add a new storage backend:

1. Implement `blkio.h` interface:
   ```c
   int blkio_read(blkio_dev_t *dev, uint32_t block, void *buf);
   int blkio_write(blkio_dev_t *dev, uint32_t block, const void *buf);
   int blkio_flush(blkio_dev_t *dev);
   int blkio_info(blkio_dev_t *dev, blkio_info_t *info);
   int blkio_close(blkio_dev_t *dev);
   ```

2. Add to `blkio_factory_open()` in `src/blkio_factory.c`

3. Test with block words

---

## API Reference

### C API (Block Subsystem)

```c
#include "block_subsystem.h"

// Initialize block subsystem
int blk_subsys_init(VM *vm, uint8_t *ram_base, size_t ram_size);

// Attach storage device
int blk_subsys_attach_device(struct blkio_dev *dev);

// Get block buffer (read-only or writable)
uint8_t *blk_get_buffer(uint32_t block_num, int writable);

// Mark block as modified
int blk_update(uint32_t block_num);

// Flush blocks to disk
int blk_flush(uint32_t block_num);  // 0 = flush all

// Check if block number is valid
int blk_is_valid(uint32_t block_num);

// Get total available blocks
uint32_t blk_get_total_blocks(void);

// Shutdown block subsystem
int blk_subsys_shutdown(void);
```

### Return Codes

```c
enum {
    BLK_OK           =  0,    // Success
    BLK_EINVAL       = -1,    // Invalid parameter
    BLK_ERANGE       = -2,    // Block out of range
    BLK_EIO          = -3,    // I/O error
    BLK_ENODEV       = -4,    // No device attached
    BLK_ERESERVED    = -5,    // Block 0 reserved
    BLK_EDIRTY       = -6     // Unflushed dirty blocks
};
```

---

## Best Practices

### 1. Always Flush After Critical Operations

```forth
\ Good practice
2048 BLOCK 1024 65 FILL
UPDATE FLUSH  \ Ensure data is written

\ Risky (data may be lost on crash)
2048 BLOCK 1024 65 FILL
\ Forgot UPDATE and FLUSH!
```

### 2. Use RAM Blocks for Temporary Data

```forth
\ Fast temporary storage (blocks 1-1023)
100 BUFFER 1024 0 FILL  \ Quick temporary buffer
\ No need for FLUSH - RAM only

\ Persistent storage (blocks 1024+)
2048 BUFFER 1024 0 FILL
UPDATE FLUSH  \ Required for persistence
```

### 3. Validate Block Numbers

```forth
: SAFE-BLOCK ( u -- addr | 0 )
  DUP 0= IF DROP 0 EXIT THEN    \ Reject block 0
  DUP 1024 < IF DROP 0 EXIT THEN \ Reject RAM blocks if you want disk only
  BLOCK ;
```

### 4. Handle Errors Gracefully

```forth
: TRY-BLOCK ( u -- addr flag )
  BLOCK DUP 0<> ;  \ Returns address and success flag

: SAFE-OPERATION ( u -- )
  TRY-BLOCK IF
    \ Block available, do work
    1024 65 FILL UPDATE FLUSH
  ELSE
    ." Block not available" CR
  THEN ;
```

---

## Testing

### Manual Testing

```forth
\ Test 1: Write and read
2048 BLOCK 1024 65 FILL UPDATE FLUSH
2048 LIST  \ Should show all 'A's

\ Test 2: Persistence (restart program between commands)
2048 BLOCK 1024 66 FILL UPDATE FLUSH
\ Restart
2048 LIST  \ Should show all 'B's

\ Test 3: Multiple blocks
2048 BLOCK 1024 65 FILL UPDATE
2049 BLOCK 1024 66 FILL UPDATE
2050 BLOCK 1024 67 FILL UPDATE
FLUSH
2048 LIST  \ 'A's
2049 LIST  \ 'B's
2050 LIST  \ 'C's
```

### Automated Testing

Run the block word tests:

```bash
./build/starforth --run-tests | grep "Block Words"
```

---

## Changelog

### Version 1.1.0 (2025-10-02)

- ✅ Block persistence implemented (FILE backend)
- ✅ LIST command with formatted output
- ✅ FILL now works with block buffers
- ✅ 3-layer architecture complete
- ✅ LRU caching with dirty tracking
- ✅ 4KB device alignment
- ✅ qemu-img compatibility

### Version 1.0.0

- Basic block words (BLOCK, BUFFER, UPDATE)
- RAM-only storage
- In-memory cache

---

## Further Reading

- **ARCHITECTURE.md**: VM internals and memory model
- **docs/l_4_re_blkio_endpoints.md**: L4Re integration details
- **FORTH-79.txt**: Block words specification
- **src/block_subsystem.c**: Implementation details

---

**End of Block Storage User Guide**