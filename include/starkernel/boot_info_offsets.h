/*
 * boot_info_offsets.h — fixed byte offsets of BootInfo fields
 *
 * Read by kernel_entry.S (assembly) AND verified by _Static_assert in
 * uefi_loader.c.  Both uses share this header; only #define directives
 * are used so the file is valid for the C preprocessor AND the GAS
 * preprocessor (which runs on .S files compiled via CC).
 *
 * If you change the BootInfo struct layout in uefi.h, update the
 * constants below AND re-check the _Static_asserts (search BOOT_INFO_OFFSETS).
 *
 * BootInfo layout (64-bit, natural alignment):
 *   0:  memory_map                  ptr  (8)
 *   8:  memory_map_size             u64  (8)
 *  16:  memory_map_descriptor_size  u64  (8)
 *  24:  runtime_services            ptr  (8)
 *  32:  acpi_table                  ptr  (8)
 *  40:  framebuffer                 FramebufferInfo (32)
 *       .base                       ptr  (8)
 *       .size                       u64  (8)
 *       .width                      u32  (4)
 *       .height                     u32  (4)
 *       .pixels_per_scanline        u32  (4)
 *       .pixel_format               u32  (4)
 *  72:  uefi_boot_services_exited   u8   (1)
 *  73:  [7 bytes padding]
 *  80:  kernel_stack_base           ptr  (8)   ← BOOT_INFO_KERNEL_STACK_BASE_OFFSET
 *  88:  kernel_stack_size           u64  (8)   ← BOOT_INFO_KERNEL_STACK_SIZE_OFFSET
 *  96:  args                        KernelArgs
 */

#define BOOT_INFO_KERNEL_STACK_BASE_OFFSET  80
#define BOOT_INFO_KERNEL_STACK_SIZE_OFFSET  88
