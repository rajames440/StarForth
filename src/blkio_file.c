/*
  StarForth — Steady-State Virtual Machine Runtime

  Copyright (c) 2023–2025 Robert A. James
  All rights reserved.

  This file is part of the StarForth project.

  Licensed under the StarForth License, Version 1.0 (the "License");
  you may not use this file except in compliance with the License.

  You may obtain a copy of the License at:
      https://github.com/star.4th@proton.me/StarForth/LICENSE.txt

  This software is provided "AS IS", WITHOUT WARRANTY OF ANY KIND,
  express or implied, including but not limited to the warranties of
  merchantability, fitness for a particular purpose, and noninfringement.

 See the License for the specific language governing permissions and
 limitations under the License.

 StarForth — Steady-State Virtual Machine Runtime
  Copyright (c) 2023–2025 Robert A. James
  All rights reserved.

  This file is part of the StarForth project.

  Licensed under the StarForth License, Version 1.0 (the "License");
  you may not use this file except in compliance with the License.

  You may obtain a copy of the License at:
       https://github.com/star.4th@proton.me/StarForth/LICENSE.txt

  This software is provided "AS IS", WITHOUT WARRANTY OF ANY KIND,
  express or implied, including but not limited to the warranties of
  merchantability, fitness for a particular purpose, and noninfringement.

  See the License for the specific language governing permissions and
  limitations under the License.

 */

// -----------------------------------------------------------------------------
// StarForth - File-backed Block I/O backend (portable)
//  - Uses lseek()+read()/write() shims (no pread/pwrite requirement).
//  - Deterministic full-block I/O; rejects short I/O.
//  - Single-process/serialized access expected for correctness.
// -----------------------------------------------------------------------------

#include "blkio.h"

#include <unistd.h>     // read, write, lseek, close
#include <fcntl.h>      // open flags
#include <sys/stat.h>   // fstat
#include <sys/types.h>  // off_t
#include <errno.h>
#include <string.h>     // memset
#include <stdint.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

typedef struct {
    int fd;
    char path[PATH_MAX];
    uint32_t total_blocks;
    uint32_t fbs;
    uint8_t read_only;
} blkio_file_state_t;

/**
 * @brief Return the byte size required for a @c blkio_file_state_t.
 *
 * Used by @c blkio_factory_open() to verify the caller's state buffer is
 * large enough before calling @c blkio_file_init_state().
 *
 * @return sizeof(blkio_file_state_t)
 */
size_t blkio_file_state_size(void) { return sizeof(blkio_file_state_t); }

/**
 * @brief Initialise a @c blkio_file_state_t in caller-provided memory.
 *
 * Copies the path into state (truncated to PATH_MAX-1), sets @c fbs,
 * @c total_blocks, and @c read_only, and writes the state pointer to
 * @c *out_opaque. The file descriptor is initialised to -1; the actual
 * @c open() call happens in @c file_open() (the vtable open method).
 * @c create_if_missing and @c truncate_to_size are accepted for API
 * compatibility but currently unused (creation is handled in @c file_open()).
 *
 * @param state_mem          Caller-allocated state buffer
 * @param state_len          Size of @c state_mem (must be >= blkio_file_state_size())
 * @param path               Path to the disk image file
 * @param total_blocks       Total block count (0 = derive from file size at open)
 * @param fbs                FORTH block size in bytes
 * @param read_only          1 = open read-only, 0 = read-write
 * @param create_if_missing  Ignored (reserved for future use)
 * @param truncate_to_size   Ignored (reserved for future use)
 * @param out_opaque         Output: set to @c state_mem on success
 * @return @c BLKIO_OK on success, @c BLKIO_EINVAL on bad arguments
 */
int blkio_file_init_state(void *state_mem, size_t state_len,
                          const char *path,
                          uint32_t total_blocks,
                          uint32_t fbs,
                          uint8_t read_only,
                          uint8_t create_if_missing,
                          uint8_t truncate_to_size,
                          void **out_opaque) {
    (void) create_if_missing;
    (void) truncate_to_size;

    if (!state_mem || state_len < sizeof(blkio_file_state_t) || !path || !out_opaque)
        return BLKIO_EINVAL;

    blkio_file_state_t *st = (blkio_file_state_t *) state_mem;
    memset(st, 0, sizeof(*st));
    st->fd = -1;

    size_t n = 0;
    while (path[n] && n < (PATH_MAX - 1)) {
        st->path[n] = path[n];
        n++;
    }
    st->path[n] = '\0';
    if (st->path[0] == '\0') return BLKIO_EINVAL;

    st->fbs = (fbs ? fbs : BLKIO_FORTH_BLOCK_SIZE);
    st->total_blocks = total_blocks; // may be 0 (derive later)
    st->read_only = read_only ? 1u : 0u;

    *out_opaque = (void *) st;
    return BLKIO_OK;
}

/**
 * @brief Perform a complete read of @c len bytes at absolute offset @c off_abs.
 *
 * Uses @c lseek() + @c read() loops to handle short reads and EINTR.
 * Not safe for concurrent readers/writers on the same file descriptor.
 * Returns -1 on any I/O error or unexpected EOF.
 *
 * @param fd      Open file descriptor
 * @param buf     Destination buffer (must be >= @c len bytes)
 * @param len     Number of bytes to read
 * @param off_abs Absolute byte offset within the file
 * @return 0 on success, -1 on error (errno set)
 */
static int full_read_at(int fd, void *buf, size_t len, off_t off_abs) {
    uint8_t *p = (uint8_t *) buf;
    size_t done = 0;
    while (done < len) {
        if (lseek(fd, off_abs + (off_t) done, SEEK_SET) == (off_t) - 1) return -1;
        ssize_t r = read(fd, p + done, len - done);
        if (r < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (r == 0) {
            errno = EIO;
            return -1;
        } /* unexpected EOF */
        done += (size_t) r;
    }
    return 0;
}

/**
 * @brief Perform a complete write of @c len bytes at absolute offset @c off_abs.
 *
 * Uses @c lseek() + @c write() loops to handle short writes and EINTR.
 * Not safe for concurrent readers/writers on the same file descriptor.
 * Returns -1 on any I/O error.
 *
 * @param fd      Open file descriptor
 * @param buf     Source buffer (must be >= @c len bytes)
 * @param len     Number of bytes to write
 * @param off_abs Absolute byte offset within the file
 * @return 0 on success, -1 on error (errno set)
 */
static int full_write_at(int fd, const void *buf, size_t len, off_t off_abs) {
    const uint8_t *p = (const uint8_t *) buf;
    size_t done = 0;
    while (done < len) {
        if (lseek(fd, off_abs + (off_t) done, SEEK_SET) == (off_t) - 1) return -1;
        ssize_t w = write(fd, p + done, len - done);
        if (w < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (w == 0) {
            errno = EIO;
            return -1;
        } /* should not happen */
        done += (size_t) w;
    }
    return 0;
}

/** @brief Open the file and attach it to the device handle. */
static int file_open(blkio_dev_t *dev, const blkio_params_t *p);

/** @brief Close the file descriptor held by the device. */
static int file_close(blkio_dev_t * dev);

/** @brief Read one FORTH block from the file into @c dst. */
static int file_read(blkio_dev_t *dev, uint32_t fblock, void *dst);

/** @brief Write one FORTH block from @c src into the file. */
static int file_write(blkio_dev_t *dev, uint32_t fblock, const void *src);

/** @brief Flush (fsync) pending writes to the file. */
static int file_flush(blkio_dev_t * dev);

/** @brief Return device info (block size, count, physical size, read-only flag). */
static int file_info(blkio_dev_t * dev, blkio_info_t * out);

static const blkio_vtable_t BLKIO_FILE_VT = {
    .open = file_open,
    .close = file_close,
    .read = file_read,
    .write = file_write,
    .flush = file_flush,
    .info = file_info
};

/**
 * @brief Return the vtable for the file-backed block I/O backend.
 * @return Pointer to the static @c blkio_vtable_t for file devices
 */
const blkio_vtable_t *blkio_file_vtable(void) { return &BLKIO_FILE_VT; }

/**
 * @brief Open the underlying file and populate the device handle.
 *
 * Opens @c st->path with @c O_RDONLY or @c O_RDWR|O_CREAT depending on
 * @c st->read_only. Derives @c total_blocks from the file size if the state
 * value is 0 — the file size must then be a multiple of @c fbs. Sets
 * @c dev->state, @c dev->forth_block_size, and @c dev->total_blocks on success.
 *
 * @param dev Uninitialised device handle
 * @param p   Parameters including the pre-initialised opaque state pointer
 * @return @c BLKIO_OK on success, @c BLKIO_EIO or @c BLKIO_EINVAL on failure
 */
static int file_open(blkio_dev_t *dev, const blkio_params_t *p) {
    if (!dev || !p || !p->opaque) return BLKIO_EINVAL;

    blkio_file_state_t *st = (blkio_file_state_t *) p->opaque;
    if (st->fd != -1) {
        close(st->fd);
        st->fd = -1;
    }

    int oflags = st->read_only ? O_RDONLY : O_RDWR;
    if (!st->read_only) oflags |= O_CREAT;
#ifdef O_CLOEXEC
    oflags |= O_CLOEXEC;
#endif

    const int fd = open(st->path, oflags, 0644);
    if (fd < 0) return BLKIO_EIO;
    st->fd = fd;

    struct stat sb;
    if (fstat(fd, &sb) < 0) {
        close(fd);
        st->fd = -1;
        return BLKIO_EIO;
    }

    /* If caller didn’t specify blocks, derive from file size (must be multiple of fbs). */
    if (st->total_blocks == 0) {
        if (st->fbs == 0) {
            close(fd);
            st->fd = -1;
            return BLKIO_EINVAL;
        }
        if ((uint64_t) sb.st_size % (uint64_t) st->fbs != 0) {
            close(fd);
            st->fd = -1;
            return BLKIO_EINVAL;
        }
        uint64_t blocks = (uint64_t) sb.st_size / (uint64_t) st->fbs;
        if (blocks == 0 || blocks > 0xffffffffULL) {
            close(fd);
            st->fd = -1;
            return BLKIO_EINVAL;
        }
        st->total_blocks = (uint32_t) blocks;
    }

    dev->state = st;
    dev->forth_block_size = st->fbs;
    dev->total_blocks = st->total_blocks;
    return BLKIO_OK;
}

/**
 * @brief Close the file descriptor held by the device.
 *
 * @param dev Device handle (must have valid state)
 * @return @c BLKIO_OK on success, @c BLKIO_EIO if @c close() fails
 */
static int file_close(blkio_dev_t *dev) {
    if (!dev || !dev->state) return BLKIO_EINVAL;
    blkio_file_state_t *st = (blkio_file_state_t *) dev->state;
    if (st->fd >= 0) {
        int rc = close(st->fd);
        st->fd = -1;
        return (rc == 0) ? BLKIO_OK : BLKIO_EIO;
    }
    return BLKIO_OK;
}

/**
 * @brief Read one FORTH block from the file backend.
 *
 * @param dev    Open file device
 * @param fblock Logical block number (must be < @c dev->total_blocks)
 * @param dst    Destination buffer (must be >= @c fbs bytes)
 * @return @c BLKIO_OK, @c BLKIO_EINVAL if @c fblock is out of range, or @c BLKIO_EIO
 */
static int file_read(blkio_dev_t *dev, uint32_t fblock, void *dst) {
    if (!dev || !dst) return BLKIO_EINVAL;
    if (fblock >= dev->total_blocks) return BLKIO_EINVAL;

    blkio_file_state_t *st = (blkio_file_state_t *) dev->state;
    if (st->fd < 0) return BLKIO_ECLOSED;

    const off_t off = (off_t)((uint64_t) fblock * (uint64_t) st->fbs);
    if (full_read_at(st->fd, dst, st->fbs, off) < 0) return BLKIO_EIO;
    return BLKIO_OK;
}

/**
 * @brief Write one FORTH block to the file backend.
 *
 * @param dev    Open file device (must not be read-only)
 * @param fblock Logical block number (must be < @c dev->total_blocks)
 * @param src    Source buffer (must be >= @c fbs bytes)
 * @return @c BLKIO_OK, @c BLKIO_ENOSUP if read-only, @c BLKIO_EINVAL if out of range,
 *         or @c BLKIO_EIO on write failure
 */
static int file_write(blkio_dev_t *dev, uint32_t fblock, const void *src) {
    if (!dev || !src) return BLKIO_EINVAL;
    if (fblock >= dev->total_blocks) return BLKIO_EINVAL;

    blkio_file_state_t *st = (blkio_file_state_t *) dev->state;
    if (st->fd < 0) return BLKIO_ECLOSED;
    if (st->read_only) return BLKIO_ENOSUP;

    const off_t off = (off_t)((uint64_t) fblock * (uint64_t) st->fbs);
    if (full_write_at(st->fd, src, st->fbs, off) < 0) return BLKIO_EIO;
    return BLKIO_OK;
}

/**
 * @brief Flush pending writes via @c fsync() on Linux/POSIX; no-op elsewhere.
 *
 * @param dev Open file device
 * @return @c BLKIO_OK on success, @c BLKIO_EIO if @c fsync() fails
 */
static int file_flush(blkio_dev_t *dev) {
    if (!dev) return BLKIO_EINVAL;
    blkio_file_state_t *st = (blkio_file_state_t *) dev->state;
#if defined(__linux__) || defined(__APPLE__) || defined(__unix__)
    if (st->fd < 0) return BLKIO_ECLOSED;
    if (fsync(st->fd) < 0) return BLKIO_EIO;
    return BLKIO_OK;
#else
    (void) st;
    return BLKIO_OK;
#endif
}

/**
 * @brief Return device info for the file backend.
 *
 * Physical sector size is reported as 512 (best-effort guess; no ioctl used).
 * Physical size in bytes comes from @c fstat().
 *
 * @param dev Open file device
 * @param out Output struct to populate
 * @return @c BLKIO_OK on success, @c BLKIO_EIO if @c fstat() fails
 */
static int file_info(blkio_dev_t *dev, blkio_info_t *out) {
    if (!dev || !out) return BLKIO_EINVAL;
    blkio_file_state_t *st = (blkio_file_state_t *) dev->state;

    struct stat sb;
    if (fstat(st->fd, &sb) < 0) return BLKIO_EIO;

    out->forth_block_size = st->fbs;
    out->total_blocks = st->total_blocks;
    out->phys_sector_size = 512; // best-effort guess
    out->phys_size_bytes = (uint64_t) sb.st_size;
    out->read_only = st->read_only;
    return BLKIO_OK;
}