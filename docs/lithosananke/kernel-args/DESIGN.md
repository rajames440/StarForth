# Kernel Command-Line Arguments (KernelArgs)

**Branch:** `lithosananke`
**Status:** Design complete — awaiting implementation

## Motivation

Memory sizing and runtime behavior are currently compile-time constants:

```c
#define KERNEL_HEAP_SIZE  (2ULL * 1024 * 1024 * 1024)   /* kernel_main.c */
.space 0x200000                                           /* kernel_entry.S */
```

This means tuning stack depth or heap size for a given machine requires a
full rebuild.  The JVM model (`-Xmx`, `-Xss`) is the right analogy: bake in
safe defaults, allow runtime overrides via boot-time command line.  The same
mechanism also lets the kernel self-start the DoE and control log verbosity
without relying on external socat injection from the Makefile scripts.

## UEFI Mechanism

`EFI_LOADED_IMAGE_PROTOCOL->LoadOptions` is a UCS-2 string passed to the
EFI application by the firmware or boot manager.  It is available to the
loader before `ExitBootServices()` at no extra cost — the loader already
holds `loaded_image` (see `uefi_loader.c:160`).  We convert it to ASCII
(simple 8-bit truncation; all valid switches are ASCII) and parse it into
a `KernelArgs` struct that travels to the kernel via `BootInfo`.

## New Files

| File | Purpose |
|------|---------|
| `include/starkernel/kernel_args.h` | `KernelArgs` struct + default macros |
| `src/starkernel/boot/cmdline.c` | UCS-2→ASCII conversion + tokeniser + parser |
| `include/starkernel/cmdline.h` | Parser API (`cmdline_parse`) |

## Changed Files

| File | Change |
|------|--------|
| `include/starkernel/uefi.h` | Add `KernelArgs args` + `void *kernel_stack_base` + `uint64_t kernel_stack_size` to `BootInfo` |
| `src/starkernel/boot/uefi_loader.c` | Call `cmdline_parse(loaded_image->LoadOptions, ...)`, allocate dynamic stack, fill `BootInfo` |
| `src/starkernel/arch/amd64/kernel_entry.S` | Read `boot_info->kernel_stack_base`; fall back to BSS stack if zero |
| `src/starkernel/kernel_main.c` | Use `boot_info->args.heap_size`; honour `run_doe` and `log_level` |
| `Makefile.starkernel` | Pass `--doe` via OVMF boot entry args; remove socat EXEC-DOE injection |

## `KernelArgs` Struct

```c
/* include/starkernel/kernel_args.h */

#define KARGS_DEFAULT_STACK_SIZE   (2ULL  * 1024 * 1024)         /* 2 MB  */
#define KARGS_DEFAULT_HEAP_SIZE    (2ULL  * 1024 * 1024 * 1024)  /* 2 GB  */
#define KARGS_DEFAULT_LOG_LEVEL    1   /* info */
#define KARGS_DEFAULT_RUN_DOE      0

typedef struct {
    /* Memory */
    uint64_t stack_size;    /* --stack=<size>       e.g. 2M 4M 8M        */
    uint64_t heap_size;     /* --heap=<size>        e.g. 2G 4G            */

    /* Runtime behaviour */
    int      run_doe;       /* --doe                flag, default off      */
    int      log_level;     /* --log-level=<level>  debug=0 info=1 warn=2 error=3 */

    /* Extension: raw unparsed cmdline preserved verbatim (null-terminated) */
    char     raw[512];
} KernelArgs;
```

All fields have explicit defaults.  Unknown switches are ignored and
appended to `raw` so future kernels can act on them while old kernels boot
safely.

## `BootInfo` Changes

Two fields are added **directly** to `BootInfo` (not nested inside
`KernelArgs`) so `kernel_entry.S` can read them at fixed, known offsets
without any C header dependency:

```c
typedef struct {
    EFI_MEMORY_DESCRIPTOR *memory_map;
    UINTN                  memory_map_size;
    UINTN                  memory_map_descriptor_size;
    EFI_RUNTIME_SERVICES  *runtime_services;
    void                  *acpi_table;
    FramebufferInfo        framebuffer;
    UINT8                  uefi_boot_services_exited;

    /* NEW — kernel stack (loader-allocated; zero = use BSS fallback) */
    void                  *kernel_stack_base;   /* low address of allocation */
    uint64_t               kernel_stack_size;

    /* NEW — all other parsed arguments */
    KernelArgs             args;
} BootInfo;
```

Keeping `kernel_stack_base` / `kernel_stack_size` at the top-level avoids
having to compute a nested struct offset inside assembly.

## Assembly Trampoline (`kernel_entry.S`)

```asm
kernel_main:
    /* rdi = BootInfo* */
    mov  rax, [rdi + BOOT_INFO_STACK_BASE_OFFSET]
    test rax, rax
    jnz  .dynamic_stack
    lea  rax, [rip + g_kernel_stack_top]    /* BSS fallback */
    jmp  .stack_ready
.dynamic_stack:
    mov  rcx, [rdi + BOOT_INFO_STACK_SIZE_OFFSET]
    add  rax, rcx                           /* top = base + size */
.stack_ready:
    and  rax, -16
    sub  rax, 8
    mov  rsp, rax
    xor  rbp, rbp
    jmp  kernel_main_impl
```

`BOOT_INFO_STACK_BASE_OFFSET` and `BOOT_INFO_STACK_SIZE_OFFSET` are defined
as integer constants in a small `boot_info_offsets.h` that is included by
both C (to `_Static_assert` correctness) and the assembly file.

## Parser (`cmdline.c`)

```c
void cmdline_parse(const CHAR16 *load_options, BootInfo *boot_info);
```

Steps:
1. Convert UCS-2 → ASCII into a local 512-byte buffer (truncate at 511).
2. Copy verbatim into `boot_info->args.raw`.
3. Fill defaults into `boot_info->args`.
4. Tokenise on spaces; for each token:
   - `--stack=<N>[KMG]`  → `boot_info->kernel_stack_size` + `args.stack_size`
   - `--heap=<N>[KMG]`   → `args.heap_size`
   - `--doe`             → `args.run_doe = 1`
   - `--log-level=<s>`   → `args.log_level` (debug/info/warn/error → 0-3)
   - anything else       → silently ignored (forward-compat)
5. Allocate stack via `BS->AllocatePages(AllocateAnyPages, EfiLoaderData, pages)`.
   Store result in `boot_info->kernel_stack_base`.

Size suffix parser handles `K`/`M`/`G` (case-insensitive); bare numbers are
treated as bytes.

## Loader Integration

In `uefi_loader.c`, after the existing `HandleProtocol` call for
`loaded_image` and before `ExitBootServices`:

```c
cmdline_parse(loaded_image->LoadOptions, &boot_info);
```

`boot_info` is already on the loader stack and passed to the kernel entry;
no structural change needed.

## `kernel_main.c` Integration

```c
/* Heap — use runtime size if provided */
uint64_t heap_sz = boot_info->args.heap_size
                 ? boot_info->args.heap_size
                 : KARGS_DEFAULT_HEAP_SIZE;
kmalloc_init(heap_sz);

/* DoE — self-start if requested */
#ifdef STARFORTH_ENABLE_VM
if (boot_info->args.run_doe) {
    vm_interpret(mama, "EXEC-DOE");
}
#endif
```

Log level feeds into the existing `LOG-DEBUG` / `LOG-INFO` etc. word
infrastructure via a new `sk_set_log_level(int)` call.

## Makefile Integration

The `qemu` target currently injects `EXEC-DOE` via socat after detecting
`ok>`.  With `--doe` supported natively:

```makefile
# Pass --doe via OVMF boot entry OptionalData
QEMU_KERNEL_ARGS ?= --doe --log-level=info --stack=2M --heap=2G
```

The ISO build embeds these in the UEFI boot entry so the kernel receives
them from firmware.  The socat injection dance is removed.  The `SK_CMD`
compile-time flag remains available for one-off testing.

## Passing Args in QEMU

OVMF reads boot entry `OptionalData` from the UEFI variable store.  The
cleanest approach for the Makefile: use a small Python/shell script to
write the boot entry with the desired args into `OVMF_VARS.fd` before
launching QEMU.  Alternatively, `efibootmgr` syntax baked into the ISO's
`startup.nsh` script works with the OVMF shell.

## Extension Points

Adding a new switch requires:
1. A field in `KernelArgs` with a default macro.
2. One `else if` branch in `cmdline_parse()`.
3. One read site in the kernel (usually `kernel_main.c`).

No changes to `BootInfo`, `kernel_entry.S`, or the loader calling code.

## Invariants

- If `LoadOptions` is NULL or empty, all defaults apply — identical to
  current behaviour.  Existing QEMU launch commands continue to work.
- The BSS stack (`g_kernel_stack`) remains in the binary as a fallback.
  It can be removed in a future cleanup once dynamic allocation is proven.
- `KernelArgs.raw` is always null-terminated; max 511 chars.
- Unknown arguments never cause a boot failure.
