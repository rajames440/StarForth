# StarForth INIT Tools

**Tools for managing init.4th ↔ disk image synchronization**

---

## Overview

StarForth provides a complete toolchain for managing the bidirectional flow between `./conf/init.4th` (
version-controlled source of truth) and disk images (`.img` files tracked via Git LFS). This enables collaborative
development where:

1. Developers work in disk images (native Forth editing via `LIST`, `EDIT`, etc.)
2. Tools extract `(- ... )` annotated blocks into `init.4th`
3. Changes to `init.4th` are reviewed in Git PRs
4. Approved changes are applied back to disk images

---

## Tools

### `tools/extract-init` (C)

**Purpose:** Scan a disk image for blocks marked with `(- ... )` headers and extract them to `init.4th`

**Usage:**

```bash
./tools/extract-init --img=disks/example.img --out=conf/init.4th \
    [--fbs=1024] [--start=0] [--end=-1] \
    [--loose] [--require-close] [--max=N]
```

**Options:**

- `--img=PATH` - Disk image to scan (required)
- `--out=PATH` - Output init.4th file (required)
- `--fbs=N` - Forth block size in bytes (default: 1024)
- `--start=N` - First block number to scan (default: 0)
- `--end=N` - Last block number to scan, -1 for EOF (default: -1)
- `--loose` - Allow BOM/whitespace before `(-` header
- `--require-close` - Require closing `)` in same block
- `--max=N` - Stop after extracting N blocks

**Output format:**

```forth
(- StarForth INIT export )
(- Source: disks/example.img )
(- FBS: 1024  Range: 0 .. 499999  Mode: loose  RequireClose: yes  Max: unlimited )

Block 2048
(- Large Letter F )
: STAR 42 EMIT ;
...

Block 3000
(- Another block )
...
```

**Detection algorithm:**

1. Read each block sequentially
2. Check for `(-` at start (with optional BOM/whitespace if `--loose`)
3. If `--require-close`, verify closing `)` exists in same block
4. Extract blocks that match criteria

---

### `tools/apply-init` (C)

**Purpose:** Parse `init.4th` and write its `Block N` sections to specific blocks in a disk image

**Usage:**

```bash
./tools/apply-init --img=disks/example.img --in=conf/init.4th \
    [--fbs=1024] [--start=0] [--end=N] \
    [--clip] [--dry-run] [--verify] [--verbose]
```

**Options:**

- `--img=PATH` - Disk image to modify (required)
- `--in=PATH` - init.4th source file (required)
- `--fbs=N` - Forth block size in bytes (default: 1024)
- `--start=N` - Guard: refuse writes to blocks < N (safety)
- `--end=N` - Guard: refuse writes to blocks > N (safety)
- `--clip` - Truncate block content if > FBS (default: error)
- `--dry-run` - Parse and show plan, don't write
- `--verify` - Read back and compare after write
- `--verbose` - Show detailed write operations

**Parsing:**

```forth
(- This is ignored - not a block header )

Block 2048
(- This is the block header comment )
: WORD1 ... ;
: WORD2 ... ;

(- These lines outside Block N are ignored )

Block 3000
(- Another block )
...
```

Lines outside `Block N` sections (like top-level comments) are ignored. Only lines between `Block N` and the next
`Block M` (or EOF) are written to block N.

**Guards:**

- `--start=1024` prevents accidentally overwriting boot area (blocks 0-1023)
- `--end=4095` prevents writing beyond a certain range
- Both are optional; by default, no guards

**Verification:**

- With `--verify`, each block is re-read after write and compared byte-for-byte
- Ensures disk write succeeded and data integrity maintained

---

### `scripts/update-init.sh` (Interactive)

**Purpose:** User-friendly wrapper for `extract-init` with interactive prompts

**Usage:**

```bash
./scripts/update-init.sh
```

**Workflow:**

1. Lists available disk images in `./disks/`
2. Prompts user to select one
3. Asks for extraction parameters (mode, FBS, range, etc.)
4. Shows full command before execution
5. Asks for confirmation
6. Runs `extract-init` and writes to `conf/init.4th`

**Use case:** After editing blocks in a disk image, run this to update `init.4th` for Git commit.

---

### `scripts/apply-init.sh` (Interactive)

**Purpose:** User-friendly wrapper for `apply-init` with interactive prompts

**Usage:**

```bash
./scripts/apply-init.sh
```

**Workflow:**

1. Lists available disk images in `./disks/`
2. Prompts user to select one
3. Asks for safety guards, verification, etc.
4. Shows full command before execution
5. Asks for confirmation
6. Runs `apply-init` to write `conf/init.4th` blocks to disk

**Use case:** After merging PR with init.4th changes, run this to update disk images.

---

## Workflow Patterns

### Pattern 1: Extract from Disk → Git

**Scenario:** You've been working in a disk image and want to commit changes to Git.

```bash
# 1. Work in disk (using StarForth REPL)
./build/starforth --disk-img=disks/mywork.img
ok> 2048 LIST
ok> 2048 EDIT
... (edit block) ...

# 2. Extract marked blocks to init.4th
./scripts/update-init.sh
# (select disk, confirm extraction)

# 3. Review changes
git diff conf/init.4th

# 4. Commit
git add conf/init.4th
git commit -m "Add new INIT words: FOO and BAR"
git push
```

---

### Pattern 2: Apply from Git → Disk

**Scenario:** A PR was merged with init.4th changes, and you need to update your disk.

```bash
# 1. Pull latest changes
git pull origin master

# 2. Apply to your disk
./scripts/apply-init.sh
# (select disk, confirm application)

# 3. Verify in REPL
./build/starforth --disk-img=disks/mywork.img
ok> 2048 LIST
ok> ( verify new code is there )
```

---

### Pattern 3: Command-Line Automation (CI/CD)

**Extract:**

```bash
./tools/extract-init \
    --img=disks/production.img \
    --out=conf/init.4th \
    --fbs=1024 \
    --start=0 \
    --end=-1 \
    --loose \
    --require-close
```

**Apply:**

```bash
./tools/apply-init \
    --img=disks/production.img \
    --in=conf/init.4th \
    --fbs=1024 \
    --start=1024 \
    --clip \
    --verify \
    --verbose
```

---

## Block Naming Convention

**Recommended disk naming:** `<hostname>-<username>-<version>.img`

Examples:

- `workstation-alice-1.0.img`
- `laptop-bob-2.3.img`
- `starship-rajames-1.0.img`

**Tracked in Git:**

- `disks/*.img` → Git LFS (binary, large)
- `conf/init.4th` → Git (text, diffable)

---

## The `(-` Metadata Marker

### Purpose

The `(-` word serves dual purposes:

1. **Runtime:** Forth comment (like `(` but with dash)
2. **Tooling:** Metadata marker for block extraction

### Syntax

```forth
(- <description> )
```

Example:

```forth
Block 2048
(- String utilities: COUNT, COMPARE, SEARCH )
: COUNT  DUP 1+ SWAP C@ ;
: COMPARE ... ;
```

### Why `(-` instead of `(`?

1. Distinguishes extraction-worthy blocks from regular comments
2. Avoids false positives (many Forth blocks use `(` for normal comments)
3. Easy to search: `grep "^(-" ` finds all metadata markers
4. Doesn't conflict with standard Forth syntax

### Implementation

Located in `src/word_source/starforth_words.c:102-120`:

```c
void starforth_word_paren_dash(VM *vm) {
    /* Consume input from "(- " to first ")" */
    int depth = 1;
    while (vm->input_pos < vm->input_length && depth > 0) {
        char c = vm->input_buffer[vm->input_pos++];
        if (c == '(') depth++;
        else if (c == ')') depth--;
    }
    if (depth > 0) {
        log_message(LOG_WARN, "(- comment not terminated");
    }
    log_message(LOG_DEBUG, "(- comment parsed (init.4th metadata marker)");
}
```

Registered in both FORTH and STARFORTH vocabularies.

---

## Safety Features

### 1. Guard Rails

**Problem:** Accidentally overwriting boot area (blocks 0-1023) or system ranges

**Solution:** `--start` and `--end` guards

```bash
# Only allow writes to blocks 1024-4095
./tools/apply-init --img=disk.img --in=init.4th --start=1024 --end=4095
```

If init.4th contains `Block 512`, apply-init will refuse:

```
Refusing to write block 512 (< start guard 1024)
```

### 2. Verification

**Problem:** Disk write might fail silently (bad sector, filesystem issue)

**Solution:** `--verify` flag

```bash
./tools/apply-init --img=disk.img --in=init.4th --verify
```

After writing each block:

1. Seek back to block start
2. Read full block (1024 bytes)
3. Compare with what was written
4. Error if mismatch

### 3. Dry Run

**Problem:** Want to see what would happen without committing

**Solution:** `--dry-run` flag

```bash
./tools/apply-init --img=disk.img --in=init.4th --dry-run --verbose
```

Output:

```
[plan] Block 2048  (171 byte payload)
[plan] Block 3000  (56 byte payload)
[plan] Block 3001  (30 byte payload)
Sections parsed: 3; would write 3 block(s)
```

No actual writes occur.

### 4. Content Length Checks

**Problem:** Block text exceeds FBS (1024 bytes)

**Solution:** Error by default, `--clip` to truncate

Without `--clip`:

```
Block 2048: content 1127 > FBS 1024 (use --clip to truncate)
ERROR: ...
```

With `--clip`:

```
Wrote block 2048 (1024/1024 bytes used, padded 0)
(Warning: 103 bytes truncated)
```

---

## Building the Tools

**Automatic:**

Scripts (`update-init.sh`, `apply-init.sh`) check timestamps and rebuild if source is newer than binary.

**Manual:**

```bash
# extract-init
gcc -std=c99 -O2 -Wall -Wextra -Werror \
    -o tools/extract-init tools/extract_init.c

# apply-init
gcc -std=gnu99 -D_FILE_OFFSET_BITS=64 -O2 -Wall -Wextra -Werror \
    -o tools/apply-init tools/apply_init.c
```

**Requirements:**

- C99 compiler (gcc or clang)
- POSIX file APIs (fseeko, ftello)
- 64-bit file offset support

---

## Testing

### Unit Test (Round-Trip)

```bash
# 1. Apply init.4th to disk
./tools/apply-init --img=test.img --in=conf/init.4th --fbs=1024 --verify

# 2. Extract back
./tools/extract-init --img=test.img --out=/tmp/extracted.4th --fbs=1024 --loose

# 3. Compare (ignoring metadata header)
tail -n +4 /tmp/extracted.4th > /tmp/extracted-body.4th
diff -u conf/init.4th /tmp/extracted-body.4th

# Should be identical (or only whitespace diffs)
```

### Integration Test (Full Workflow)

```bash
# 1. Edit a block in disk image
echo ": TEST 42 . ; " | ./build/starforth --disk-img=test.img <<EOF
2048 BLOCK
( edit commands... )
EOF

# 2. Extract to init.4th
./tools/extract-init --img=test.img --out=conf/init.4th --fbs=1024 --loose --require-close

# 3. Verify TEST word is in init.4th
grep "TEST" conf/init.4th

# 4. Apply to fresh disk
cp test.img test-fresh.img
./tools/apply-init --img=test-fresh.img --in=conf/init.4th --fbs=1024 --verify

# 5. Verify TEST word works
echo "TEST" | ./build/starforth --disk-img=test-fresh.img
# Should print: 42
```

---

## Troubleshooting

### "Picked 0 blocks"

**Cause:** No blocks found with `(-` headers

**Fix:**

1. Check disk actually has blocks with `(-` at start
2. Try `--loose` if blocks have BOM or leading whitespace
3. Remove `--require-close` if `)` is on next line

### "Refusing to write block N"

**Cause:** Block number violates guard rails

**Fix:**

1. Check `--start` and `--end` values
2. Ensure init.4th block numbers are in allowed range
3. Remove guards if intentional

### "Content exceeds FBS"

**Cause:** Block text is longer than 1024 bytes

**Fix:**

1. Split block into multiple blocks
2. Use `--clip` to truncate (NOT RECOMMENDED for code)
3. Increase FBS if using custom block size

### "Verify failed at block N"

**Cause:** Read-back doesn't match what was written

**Fix:**

1. Check disk image file permissions
2. Verify filesystem not full
3. Check for disk errors (fsck, SMART)
4. Try without `--verify` to isolate issue

---

## Advanced Usage

### Custom Block Size

**Scenario:** Disk uses 2048-byte blocks

```bash
# Extract
./tools/extract-init --img=bigdisk.img --out=init.4th --fbs=2048

# Apply
./tools/apply-init --img=bigdisk.img --in=init.4th --fbs=2048
```

**Note:** FBS must match between extract/apply and must match what StarForth VM expects.

### Partial Range Extraction

**Scenario:** Only extract blocks 1024-2047

```bash
./tools/extract-init \
    --img=disk.img \
    --out=partial.4th \
    --start=1024 \
    --end=2047
```

### Limited Extraction

**Scenario:** Extract only first 5 marked blocks

```bash
./tools/extract-init \
    --img=disk.img \
    --out=init.4th \
    --max=5
```

---

## See Also

- [INIT System Documentation](INIT_SYSTEM.md) - How INIT word loads init.4th at boot
- [Block Storage Guide](BLOCK_STORAGE_GUIDE.md) - 3-layer block architecture
- [Git LFS Documentation](https://git-lfs.github.com/) - Large file storage

---

## Code References

### extract_init.c

- `tools/extract_init.c:66-76` - Header detection logic
- `tools/extract_init.c:78-83` - Block text trimming
- `tools/extract_init.c:85-91` - Closing `)` check
- `tools/extract_init.c:193-212` - Main extraction loop

### apply_init.c

- `tools/apply_init.c:85-98` - Block header parsing
- `tools/apply_init.c:114-155` - Block write with verification
- `tools/apply_init.c:193-219` - Section finalization (guards, verify)
- `tools/apply_init.c:221-236` - Main parser loop

### Shell Scripts

- `scripts/update-init.sh` - Interactive extract wrapper
- `scripts/apply-init.sh` - Interactive apply wrapper

---

**End of INIT Tools Documentation**