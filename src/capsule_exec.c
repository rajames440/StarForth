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

/*
 * capsule_exec.c - Hosted capsule file loader
 *
 * Reads a .4th file from CAPSULES_DIR, walks "Block N" headers,
 * and drives vm_interpret() on each block's lines in order.
 *
 * Design constraints:
 *   - vm_interpret() has a 255-byte input buffer cap (INPUT_BUFFER_SIZE=256).
 *     Standard Forth block lines are ≤64 chars, so we process line-by-line.
 *   - Block numbers can be >1023 (doe.4th uses 2049+); we never touch
 *     the block storage subsystem — just drive vm_interpret() directly.
 *   - VM mode (compile/interpret) persists across vm_interpret() calls,
 *     so multi-line colon definitions assemble correctly.
 */

#include "../include/capsule_exec.h"
#include "../include/log.h"
#include "../include/platform_alloc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef CAPSULES_DIR
#define CAPSULES_DIR "./capsules"
#endif

/* Maximum file size we will load (1 MB — plenty for any capsule) */
#define CAPSULE_MAX_FILE_SIZE (1024 * 1024)

/* ------------------------------------------------------------------
 * is_block_header  — detect "Block <integer>\n" lines
 *
 * Returns the block number if the line starting at `p` is a block
 * header, or -1 if it is not. `*content_start` is set to the byte
 * after the newline (first byte of the block's content).
 * ------------------------------------------------------------------ */
static int is_block_header(const char *p, const char *end,
                            const char **content_start)
{
    if (p + 6 > end) return -1;
    if (strncmp(p, "Block ", 6) != 0) return -1;

    const char *q = p + 6;
    if (q >= end || *q < '0' || *q > '9') return -1;

    int num = 0;
    while (q < end && *q >= '0' && *q <= '9') {
        num = num * 10 + (*q - '0');
        q++;
    }

    /* Must be followed immediately by \n (or \r\n) */
    if (q < end && *q == '\r') q++;
    if (q >= end || *q != '\n') return -1;

    if (content_start) *content_start = q + 1;
    return num;
}

/* ------------------------------------------------------------------
 * interpret_block  — feed one block to vm_interpret line by line
 * ------------------------------------------------------------------ */
static void interpret_block(VM *vm, const char *block_start,
                             const char *block_end)
{
    const char *line = block_start;
    char linebuf[256];

    while (line < block_end && !vm->error && !vm->halted) {
        /* Find end of this line */
        const char *eol = line;
        while (eol < block_end && *eol != '\n') eol++;

        /* Strip trailing \r */
        const char *end = eol;
        if (end > line && *(end - 1) == '\r') end--;

        size_t len = (size_t)(end - line);

        /* Skip blank lines */
        if (len == 0) {
            line = eol + 1;
            continue;
        }

        /* Clamp to buffer (lines > 254 chars are malformed capsules) */
        if (len > sizeof(linebuf) - 1) {
            len = sizeof(linebuf) - 1;
            log_message(LOG_WARN, "capsule_exec: line truncated (>%zu bytes)",
                        sizeof(linebuf) - 1);
        }

        memcpy(linebuf, line, len);
        linebuf[len] = '\0';

        vm_interpret(vm, linebuf);

        line = eol + 1;
    }
}

/* ------------------------------------------------------------------
 * capsule_exec_file  — public API
 * ------------------------------------------------------------------ */
int capsule_exec_file(VM *vm, const char *name, size_t name_len)
{
    if (!vm || !name || name_len == 0) return -1;

    /* Build path: CAPSULES_DIR "/" name NUL */
    const char *dir = CAPSULES_DIR;
    size_t dir_len  = strlen(dir);
    size_t path_len = dir_len + 1 + name_len + 1;

    char *path = (char *)sf_malloc(path_len);
    if (!path) {
        log_message(LOG_ERROR, "capsule_exec: out of memory for path");
        return -1;
    }
    memcpy(path, dir, dir_len);
    path[dir_len] = '/';
    memcpy(path + dir_len + 1, name, name_len);
    path[path_len - 1] = '\0';

    FILE *fp = fopen(path, "r");
    if (!fp) {
        log_message(LOG_ERROR, "capsule_exec: cannot open '%s'", path);
        sf_free(path);
        return -1;
    }
    sf_free(path);

    /* Read entire file */
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return -1;
    }
    long fsize = ftell(fp);
    rewind(fp);

    if (fsize < 0 || fsize > CAPSULE_MAX_FILE_SIZE) {
        log_message(LOG_ERROR, "capsule_exec: file too large or unreadable");
        fclose(fp);
        return -1;
    }

    char *buf = (char *)sf_malloc((size_t)fsize + 1);
    if (!buf) {
        log_message(LOG_ERROR, "capsule_exec: out of memory for file buffer");
        fclose(fp);
        return -1;
    }

    size_t nread = fread(buf, 1, (size_t)fsize, fp);
    fclose(fp);
    buf[nread] = '\0';

    const char *p   = buf;
    const char *end = buf + nread;

    /* Walk the file: find block headers and interpret each block's content */
    int blocks_executed = 0;

    while (p < end) {
        /* Find line start — scan to next \n boundary for header detection */
        const char *line_start = p;

        /* Check if this line is a "Block N" header */
        const char *content_start = NULL;
        int block_num = is_block_header(line_start, end, &content_start);

        if (block_num >= 0) {
            /* Find where this block's content ends: at the next "Block " line
               or at EOF */
            const char *next_block = content_start;
            while (next_block < end) {
                /* Scan to next line start */
                const char *nl = next_block;
                while (nl < end && *nl != '\n') nl++;
                if (nl >= end) {
                    next_block = end;
                    break;
                }
                const char *next_line = nl + 1;
                if (is_block_header(next_line, end, NULL) >= 0) {
                    next_block = next_line;
                    break;
                }
                next_block = next_line;
            }

            log_message(LOG_DEBUG, "capsule_exec: executing Block %d", block_num);
            interpret_block(vm, content_start, next_block);
            blocks_executed++;

            p = next_block;
        } else {
            /* Skip to next line */
            while (p < end && *p != '\n') p++;
            if (p < end) p++;
        }

        if (vm->halted) break;
    }

    sf_free(buf);

    if (blocks_executed == 0) {
        log_message(LOG_WARN, "capsule_exec: no blocks found in capsule");
    } else {
        log_message(LOG_INFO, "capsule_exec: executed %d blocks", blocks_executed);
    }

    return vm->error ? -1 : 0;
}
