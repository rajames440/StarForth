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
*/

/**
 * @file mama_forth_words.c
 * @brief Mama FORTH vocabulary implementation for kernel capsule system (M7.1)
 *
 * Implements the MAMA vocabulary - kernel-only words for the LithosAnanke
 * capsule birth protocol. This file is ONLY compiled for __STARKERNEL__ builds.
 *
 * The MAMA vocabulary provides:
 *   - Capsule directory enumeration
 *   - Baby VM birth from production capsules
 *   - Experiment execution on Mama
 *   - VM registry queries
 */

#ifdef __STARKERNEL__

#include "starkernel/capsule.h"
#include "starkernel/capsule_birth.h"
#include "starkernel/capsule_loader.h"
#include "starkernel/capsule_run.h"
#include "starkernel/capsule_loader.h"
#include "starkernel/repl.h"
#include "starkernel/capsule_generated.h"
#include "starkernel/console.h"
#include "starkernel/arch.h"
#include "vm.h"
#include "word_registry.h"
#include "word_source/include/vocabulary_words.h"

/* Forward declarations for functions defined in vm_bootstrap.c and vocabulary_words.c */
extern void vm_bootstrap_root_vocabulary(VM *vm, const char *name);
extern void vocabulary_word_forth(VM *vm);
extern void vocabulary_word_definitions(VM *vm);

/* ============================================================================
 * Capsule Directory Words
 * ============================================================================ */

/**
 * @brief CAPSULE-COUNT ( -- n )
 * Push number of capsules in the capsule directory.
 */
void mama_word_capsule_count(VM *vm)
{
    vm_push(vm, (cell_t)capsule_get_desc_count());
}

/**
 * @brief CAPSULE@ ( idx -- desc )
 * Get capsule descriptor address by index.
 * Returns 0 if index is out of bounds.
 */
void mama_word_capsule_fetch(VM *vm)
{
    if (vm->dsp < 0) {
        vm->error = 1;
        return;
    }

    cell_t idx = vm_pop(vm);

    if ((uint32_t)idx >= capsule_get_desc_count()) {
        vm_push(vm, 0);  /* Out of bounds */
        return;
    }

    /* Push address of descriptor */
    vm_push(vm, (cell_t)(uintptr_t)&capsule_get_descriptors()[idx]);
}

/**
 * @brief CAPSULE-HASH@ ( desc -- hash )
 * Get content hash from capsule descriptor.
 */
void mama_word_capsule_hash_fetch(VM *vm)
{
    if (vm->dsp < 0) {
        vm->error = 1;
        return;
    }

    cell_t desc_addr = vm_pop(vm);
    const CapsuleDesc *desc = (const CapsuleDesc *)(uintptr_t)desc_addr;

    if (!desc) {
        vm_push(vm, 0);
        return;
    }

    vm_push(vm, (cell_t)desc->content_hash);
}

/**
 * @brief CAPSULE-FLAGS@ ( desc -- flags )
 * Get flags from capsule descriptor.
 */
void mama_word_capsule_flags_fetch(VM *vm)
{
    if (vm->dsp < 0) {
        vm->error = 1;
        return;
    }

    cell_t desc_addr = vm_pop(vm);
    const CapsuleDesc *desc = (const CapsuleDesc *)(uintptr_t)desc_addr;

    if (!desc) {
        vm_push(vm, 0);
        return;
    }

    vm_push(vm, (cell_t)desc->flags);
}

/**
 * @brief CAPSULE-LEN@ ( desc -- len )
 * Get payload length from capsule descriptor.
 */
void mama_word_capsule_len_fetch(VM *vm)
{
    if (vm->dsp < 0) {
        vm->error = 1;
        return;
    }

    cell_t desc_addr = vm_pop(vm);
    const CapsuleDesc *desc = (const CapsuleDesc *)(uintptr_t)desc_addr;

    if (!desc) {
        vm_push(vm, 0);
        return;
    }

    vm_push(vm, (cell_t)desc->length);
}

/* ============================================================================
 * VM Birth and Experiment Words
 * ============================================================================ */

/**
 * @brief BIRTH ( c-addr u -- )
 * Birth a named VM from its capsule.  Idempotent — if a live VM with
 * that name already exists (case-insensitive), logs and returns.
 *
 * Name mapping: S" Artemis" → capsule "artemis:init.4th"
 * Exception:    S" Hera"    → rejected (cannot re-birth Mama)
 */
void mama_word_birth(VM *vm)
{
    char            name_buf[VM_NAME_MAX];
    char            capsule_name[VM_NAME_MAX + 12]; /* name + ":init.4th" + NUL */
    VMRegistryEntry existing;
    uint32_t        i, j;
    cell_t          u, caddr;
    const char     *src;
    char            lower[VM_NAME_MAX];
    CapsuleRunResult result;
    uint32_t        new_vm_id;

    if (vm->dsp < 1) {
        vm->error = 1;
        return;
    }

    u     = vm_pop(vm);
    caddr = vm_pop(vm);

    if (u <= 0 || (uint32_t)u >= VM_NAME_MAX) {
        console_println("BIRTH: name too long or empty");
        return;
    }

    /* caddr is a VM address; S" ( -- c-addr u ) stores chars directly at caddr */
    {
        const uint8_t *p = vm_ptr(vm, (vaddr_t)caddr);
        if (!p) { vm->error = 1; return; }
        src = (const char *)p;
    }
    for (i = 0; i < (uint32_t)u; i++) name_buf[i] = src[i];
    name_buf[u] = '\0';

    /* Idempotency: live VM with same name → skip */
    if (capsule_vm_find_by_name_nocase(name_buf, &existing) == 0 &&
        existing.state == VM_STATE_LIVE) {
        console_puts("BIRTH: ");
        console_puts(name_buf);
        console_println(" already live (idempotent)");
        return;
    }

    /* Lowercase name for capsule path */
    for (i = 0; name_buf[i]; i++) {
        char c = name_buf[i];
        lower[i] = (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
    }
    lower[i] = '\0';

    /* Hera cannot be re-birthed */
    if (lower[0]=='h' && lower[1]=='e' && lower[2]=='r' && lower[3]=='a' && lower[4]=='\0') {
        console_println("BIRTH: cannot re-birth Hera");
        return;
    }

    /* Build "lower:init.4th" */
    j = 0;
    for (i = 0; lower[i]; i++) capsule_name[j++] = lower[i];
    capsule_name[j++] = ':';
    { const char *suf = "init.4th"; for (i = 0; suf[i]; i++) capsule_name[j++] = suf[i]; }
    capsule_name[j] = '\0';

    new_vm_id = 0;

    /* Switch console prefix to the baby's name so its init capsule output
     * appears tagged [Hermes] / [Artemis] rather than [Hera]. */
    {
        const char *saved_prefix = console_get_vm_name();
        console_set_vm_name(name_buf);

        result = capsule_birth_baby(
            capsule_name,
            capsule_get_directory(),
            capsule_get_descriptors(),
            capsule_get_names(),
            capsule_get_arena(),
            &new_vm_id,
            (void **)0
        );

        console_set_vm_name(saved_prefix);  /* restore [Hera] (or whoever called BIRTH) */
    }

    if (result == CAPSULE_RUN_OK) {
        capsule_vm_registry_set_name(new_vm_id, name_buf);
        console_puts("BIRTH: ");
        console_puts(name_buf);
        console_println(" live");
    } else {
        console_puts("BIRTH: ");
        console_puts(name_buf);
        console_println(" FAILED");
    }
    /* D3: stack clean on exit */
}

/**
 * @brief START ( c-addr u -- )
 * Enter a named VM's REPL loop synchronously.  The calling VM blocks
 * inside sk_repl_run() until the target halts (via STOP or BYE).
 * Cannot start a LIVE, DEAD, or STILLBORN VM.
 */
void mama_word_start(VM *vm)
{
    char             name_buf[VM_NAME_MAX];
    VMRegistryEntry  entry;
    uint32_t         i;
    cell_t           u, caddr;
    const char      *src;
    const char      *caller_name;
    VM              *target;

    if (vm->dsp < 1) {
        vm->error = 1;
        return;
    }

    u     = vm_pop(vm);
    caddr = vm_pop(vm);

    if (u <= 0 || (uint32_t)u >= VM_NAME_MAX) {
        console_println("START: name too long or empty");
        return;
    }

    /* caddr is a VM address; S" ( -- c-addr u ) stores chars directly at caddr */
    {
        const uint8_t *p = vm_ptr(vm, (vaddr_t)caddr);
        if (!p) { vm->error = 1; return; }
        src = (const char *)p;
    }
    for (i = 0; i < (uint32_t)u; i++) name_buf[i] = src[i];
    name_buf[u] = '\0';

    if (capsule_vm_find_by_name_nocase(name_buf, &entry) != 0) {
        console_puts("START: ");
        console_puts(name_buf);
        console_println(" not found");
        return;
    }

    if (entry.state == VM_STATE_LIVE) {
        console_puts("START: ");
        console_puts(name_buf);
        console_println(" already live");
        return;
    }

    if (entry.state == VM_STATE_DEAD || entry.state == VM_STATE_STILLBORN) {
        console_puts("START: ");
        console_puts(name_buf);
        console_println(" dead/stillborn — BIRTH first");
        return;
    }

    target = (VM *)entry.vm_ptr;
    if (!target) {
        console_puts("START: ");
        console_puts(name_buf);
        console_println(" no VM pointer");
        return;
    }

    /* Switch console prefix to target VM's name */
    caller_name = console_get_vm_name();
    console_set_vm_name(entry.name);

    capsule_vm_set_state(entry.vm_id, VM_STATE_LIVE);

    console_puts("START: entering ");
    console_println(entry.name);

    /* Run target's REPL — blocks until target->halted */
    sk_repl_run(target);

    /* Target halted (STOP or BYE) — restore caller's context */
    capsule_vm_set_state(entry.vm_id, VM_STATE_STOPPED);
    console_set_vm_name(caller_name);

    console_puts("START: ");
    console_puts(entry.name);
    console_println(" stopped");
    /* Stack clean on exit */
}

/**
 * @brief STOP ( -- )
 * Self-stop: set vm->halted so sk_repl_run() exits on the next iteration.
 * State is updated to VM_STATE_STOPPED by the START word after the REPL
 * returns.  STOP is registered in every VM's dictionary (including children)
 * so any VM can stop itself.
 */
void mama_word_stop(VM *vm)
{
    vm->halted = 1;
    /* sk_repl_run's while(!vm->halted) loop exits after this word returns */
}

/**
 * @brief USE ( c-addr u -- )
 * Redirect system-wide REPL input to a named VM without touching the C
 * call stack.  The console prefix changes to [VMName].
 * USE Hera (vm_id 0) resets dispatch to the default (Mama's VM, NULL slot).
 */
void mama_word_use(VM *vm)
{
    char            name_buf[VM_NAME_MAX];
    VMRegistryEntry entry;
    uint32_t        i;
    cell_t          u, caddr;
    const char     *src;

    if (vm->dsp < 1) {
        vm->error = 1;
        return;
    }

    u     = vm_pop(vm);
    caddr = vm_pop(vm);

    if (u <= 0 || (uint32_t)u >= VM_NAME_MAX) {
        console_println("USE: name too long or empty");
        return;
    }

    /* caddr is a VM address; S" ( -- c-addr u ) stores chars directly at caddr */
    {
        const uint8_t *p = vm_ptr(vm, (vaddr_t)caddr);
        if (!p) { vm->error = 1; return; }
        src = (const char *)p;
    }
    for (i = 0; i < (uint32_t)u; i++) name_buf[i] = src[i];
    name_buf[u] = '\0';

    if (capsule_vm_find_by_name_nocase(name_buf, &entry) != 0) {
        console_puts("USE: ");
        console_puts(name_buf);
        console_println(" not found");
        return;
    }

    if (entry.state == VM_STATE_DEAD || entry.state == VM_STATE_STILLBORN) {
        console_puts("USE: ");
        console_puts(name_buf);
        console_println(" dead/stillborn");
        return;
    }

    /* vm_id 0 = Hera — restore default dispatch (NULL = use REPL's own vm) */
    if (entry.vm_id == 0) {
        sk_repl_set_active_vm((void *)0);
    } else {
        sk_repl_set_active_vm((VM *)entry.vm_ptr);
    }

    console_set_vm_name(entry.name);

    console_puts("USE: now using ");
    console_println(entry.name);
    /* Stack clean on exit */
}

/**
 * @brief KILL ( c-addr u -- )
 * Destroy a named VM unconditionally.  Hera cannot be killed.
 * Idempotent: killing an already-dead VM is a no-op.
 */
void mama_word_kill(VM *vm)
{
    char        name_buf[VM_NAME_MAX];
    uint32_t    i;
    cell_t      u, caddr;
    const char *src;

    if (vm->dsp < 1) {
        vm->error = 1;
        return;
    }

    u     = vm_pop(vm);
    caddr = vm_pop(vm);

    if (u <= 0 || (uint32_t)u >= VM_NAME_MAX) {
        console_println("KILL: name too long or empty");
        return;
    }

    /* caddr is a VM address; S" ( -- c-addr u ) stores chars directly at caddr */
    {
        const uint8_t *p = vm_ptr(vm, (vaddr_t)caddr);
        if (!p) { vm->error = 1; return; }
        src = (const char *)p;
    }
    for (i = 0; i < (uint32_t)u; i++) name_buf[i] = src[i];
    name_buf[u] = '\0';

    capsule_vm_kill(name_buf);
    /* Stack clean on exit */
}

/**
 * @brief VM-STEP ( c-addr u -- )
 * Give one REPL quantum to a named VM.
 * Prints the VM's prompt, reads one line, executes it, returns to caller.
 * This is the Compudynamics context-switch primitive — Hera yields one
 * REPL turn to the named VM without surrendering the outer loop.
 */
static void mama_word_vm_step(VM *vm)
{
    char        name_buf[VM_NAME_MAX];
    uint32_t    i;
    cell_t      u, caddr;
    const char *src;
    VMRegistryEntry entry;
    VM         *target;
    const char *saved_name;

    if (vm->dsp < 1) { vm->error = 1; return; }

    u     = vm_pop(vm);
    caddr = vm_pop(vm);

    if (u <= 0 || (uint32_t)u >= VM_NAME_MAX) {
        console_println("VM-STEP: name too long or empty");
        return;
    }

    {
        const uint8_t *p = vm_ptr(vm, (vaddr_t)caddr);
        if (!p) { vm->error = 1; return; }
        src = (const char *)p;
    }
    for (i = 0; i < (uint32_t)u; i++) name_buf[i] = src[i];
    name_buf[u] = '\0';

    if (capsule_vm_find_by_name_nocase(name_buf, &entry) != 0) {
        console_puts("VM-STEP: VM not found: ");
        console_println(name_buf);
        return;
    }

    target = (VM *)entry.vm_ptr;
    if (!target || entry.state == VM_STATE_DEAD || entry.state == VM_STATE_STILLBORN) {
        console_puts("VM-STEP: VM not available: ");
        console_println(name_buf);
        return;
    }

    saved_name = console_get_vm_name();
    console_set_vm_name(entry.name);
    sk_repl_step(target);
    console_set_vm_name(saved_name);
    /* Stack clean on exit */
}

/**
 * @brief VM-EXEC ( cmd-caddr cmd-u vm-name-caddr vm-name-u -- )
 * Inject a command string into a named VM and execute it immediately —
 * no readline, no blocking.  This is the autonomous Compudynamics primitive:
 * Hera can drive a FORTH command into any child VM without surrendering the
 * outer loop.  Console prefix is saved/restored around the call.
 *
 * Stack order: push cmd string first, then VM name.  Example:
 *   S" DOE-WORK" S" Hermes" VM-EXEC
 */
static void mama_word_vm_exec(VM *vm)
{
    char        vm_name[VM_NAME_MAX];
    char        cmd_buf[1025];
    uint32_t    i;
    cell_t      vm_u, vm_caddr, cmd_u, cmd_caddr;
    const char *src;
    VMRegistryEntry entry;
    VM         *target;
    const char *saved_name;

    if (vm->dsp < 3) { vm->error = 1; return; }

    /* TOS: vm-name-u, vm-name-caddr, cmd-u, cmd-caddr */
    vm_u     = vm_pop(vm);
    vm_caddr = vm_pop(vm);
    cmd_u    = vm_pop(vm);
    cmd_caddr = vm_pop(vm);

    if (vm_u <= 0 || (uint32_t)vm_u >= VM_NAME_MAX) {
        console_println("VM-EXEC: VM name too long or empty");
        return;
    }
    if (cmd_u < 0 || (uint32_t)cmd_u > 1024) {
        console_println("VM-EXEC: command too long");
        return;
    }

    {
        const uint8_t *p = vm_ptr(vm, (vaddr_t)vm_caddr);
        if (!p) { vm->error = 1; return; }
        src = (const char *)p;
    }
    for (i = 0; i < (uint32_t)vm_u; i++) vm_name[i] = src[i];
    vm_name[vm_u] = '\0';

    {
        const uint8_t *p = vm_ptr(vm, (vaddr_t)cmd_caddr);
        if (!p) { vm->error = 1; return; }
        src = (const char *)p;
    }
    for (i = 0; i < (uint32_t)cmd_u; i++) cmd_buf[i] = src[i];
    cmd_buf[cmd_u] = '\0';

    if (capsule_vm_find_by_name_nocase(vm_name, &entry) != 0) {
        console_puts("VM-EXEC: VM not found: ");
        console_println(vm_name);
        return;
    }

    target = (VM *)entry.vm_ptr;
    if (!target || entry.state == VM_STATE_DEAD || entry.state == VM_STATE_STILLBORN) {
        console_puts("VM-EXEC: VM not available: ");
        console_println(vm_name);
        return;
    }

    saved_name = console_get_vm_name();
    console_set_vm_name(entry.name);
    vm_interpret(target, cmd_buf);
    console_set_vm_name(saved_name);

    if (target->error) {
        console_puts("VM-EXEC: ERROR in ");
        console_println(vm_name);
        target->error = 0;
    }
    /* Stack clean on exit */
}

/**
 * @brief CAPSULE-BIRTH ( capsule-id -- vm-id )
 * Birth a baby VM from a production (p) capsule.
 * Returns the new VM's ID, or -1 on failure.
 */
void mama_word_capsule_birth(VM *vm)
{
    if (vm->dsp < 0) {
        vm->error = 1;
        return;
    }

    cell_t capsule_idx = vm_pop(vm);
    uint32_t new_vm_id = 0;

    if ((uint64_t)capsule_idx >= capsule_get_desc_count()) {
        vm_push(vm, (cell_t)-1);
        return;
    }
    const char *cap_name = capsule_get_names()[(uint32_t)capsule_idx].name;

    CapsuleRunResult result = capsule_birth_baby(
        cap_name,
        capsule_get_directory(),
        capsule_get_descriptors(),
        capsule_get_names(),
        capsule_get_arena(),
        &new_vm_id,
        (void **)0  /* Don't need VM context back */
    );

    if (result == CAPSULE_RUN_OK) {
        vm_push(vm, (cell_t)new_vm_id);
    } else {
        vm_push(vm, (cell_t)-1);  /* Birth failed */
    }
}

/**
 * @brief CAPSULE-RUN ( capsule-id -- )
 * Run an experiment (e) capsule on Mama.
 */
void mama_word_capsule_run(VM *vm)
{
    if (vm->dsp < 0) {
        vm->error = 1;
        return;
    }

    cell_t capsule_idx = vm_pop(vm);

    if ((uint64_t)capsule_idx >= capsule_get_desc_count()) {
        vm->error = 1;
        return;
    }
    const char *cap_name = capsule_get_names()[(uint32_t)capsule_idx].name;

    /* Run experiment on Mama (this VM) */
    capsule_run_experiment(
        vm,
        cap_name,
        capsule_get_directory(),
        capsule_get_descriptors(),
        capsule_get_names(),
        capsule_get_arena(),
        (uint64_t *)0  /* Don't need run_id back */
    );
}

/* ============================================================================
 * VM Registry Words
 * ============================================================================ */

/**
 * @brief MAMA-VM-ID ( -- 0 )
 * Push Mama's VM ID (always 0).
 */
void mama_word_mama_vm_id(VM *vm)
{
    vm_push(vm, 0);
}

/**
 * @brief VM-COUNT ( -- n )
 * Push number of registered VMs.
 */
void mama_word_vm_count(VM *vm)
{
    vm_push(vm, (cell_t)capsule_vm_registry_count());
}

/* ============================================================================
 * Diagnostic Words
 * ============================================================================ */

/**
 * @brief CAPSULE-TEST ( -- )
 * Print diagnostic message confirming capsule system is active.
 */
void mama_word_capsule_test(VM *vm)
{
    (void)vm;
    console_println("Mama FORTH Capsule System (M7.1)");
    console_puts("  Capsules: ");
    console_println("");
    console_puts("  VMs: ");
    console_println("");
}

/* ============================================================================
 * Vocabulary Registration
 * ============================================================================ */

/**
 * @brief EXEC ( c-addr u -- )
 * Execute a named capsule in the current VM — same path as mama init auto-run.
 */
void mama_word_exec(VM *vm)
{
    char         name_buf[VM_NAME_MAX];
    uint32_t     i;
    cell_t       u, caddr;
    const char  *src;
    CapsuleRunResult result;
    int          saved_dsp;
    int          saved_rsp;

    if (vm->dsp < 1) { vm->error = 1; return; }

    u     = vm_pop(vm);
    caddr = vm_pop(vm);

    if (u <= 0 || (uint32_t)u >= VM_NAME_MAX) {
        console_println("EXEC: name too long or empty");
        return;
    }

    {
        const uint8_t *p = vm_ptr(vm, (vaddr_t)caddr);
        if (!p) { vm->error = 1; return; }
        src = (const char *)p;
    }
    for (i = 0; i < (uint32_t)u; i++) name_buf[i] = src[i];
    name_buf[u] = '\0';

    /* Save both stacks so a crashing capsule cannot corrupt the caller.
     * DSP: any partial pushes by the capsule are discarded.
     * RSP: DO-LOOP indices pushed by the caller (e.g. the DoE loop) are
     * preserved; a capsule that crashed mid->R/R> cannot corrupt them. */
    saved_dsp = vm->dsp;
    saved_rsp = vm->rsp;

    result = capsule_exec_init(
        vm,
        name_buf,
        capsule_get_directory(),
        capsule_get_descriptors(),
        capsule_get_names(),
        capsule_get_arena());

    /* Restore stacks and clear error/exit flags so the DoE loop survives
     * a workload crash and continues cleanly to the next run. */
    vm->dsp        = saved_dsp;
    vm->rsp        = saved_rsp;
    vm->error      = 0;
    vm->exit_colon = 0;

    if (result != CAPSULE_RUN_OK) {
        console_puts("EXEC: failed: ");
        console_println(name_buf);
    }
}

/**
 * @brief CONNECT-ARTEMIS ( -- ) — Enter Artemis's REPL, birthing it first if needed.
 */
static void mama_word_connect_artemis(VM *vm __attribute__((unused)))
{
    VMRegistryEntry  entry;
    VM              *artemis;
    const char      *saved_name;

    if (capsule_vm_find_by_name_nocase("Artemis", &entry) != 0 ||
        entry.state == VM_STATE_DEAD ||
        entry.state == VM_STATE_STILLBORN) {
        uint32_t new_vm_id = 0;
        const char *saved = console_get_vm_name();
        CapsuleRunResult r;

        console_set_vm_name("Artemis");
        r = capsule_birth_baby(
            "artemis:init.4th",
            capsule_get_directory(),
            capsule_get_descriptors(),
            capsule_get_names(),
            capsule_get_arena(),
            &new_vm_id, (void **)0);
        console_set_vm_name(saved);

        if (r != CAPSULE_RUN_OK) {
            console_println("CONNECT-ARTEMIS: birth failed");
            return;
        }
        capsule_vm_registry_set_name(new_vm_id, "Artemis");
        if (capsule_vm_find_by_name_nocase("Artemis", &entry) != 0) {
            console_println("CONNECT-ARTEMIS: registry error");
            return;
        }
    }

    artemis = (VM *)entry.vm_ptr;
    if (!artemis) {
        console_println("CONNECT-ARTEMIS: no VM pointer");
        return;
    }

    saved_name = console_get_vm_name();
    console_set_vm_name(entry.name);
    capsule_vm_set_state(entry.vm_id, VM_STATE_LIVE);

    sk_repl_run(artemis);

    capsule_vm_set_state(entry.vm_id, VM_STATE_STOPPED);
    console_set_vm_name(saved_name);
}

/**
 * @brief BYE ( -- ) — Hera-only: reap all children then cold-restart the machine.
 *
 * In child VMs this word is never registered; children use the standard
 * system_word_bye which sets vm->halted and returns to the parent's REPL.
 */
static void mama_word_bye(VM *vm __attribute__((unused)))
{
    console_println("BYE: reaping children");
    capsule_vm_kill_all_nonmama();
    console_println("BYE: cold restart");
    arch_cold_reset();
}

/**
 * @brief CONNECT-HERMES ( -- ) — Enter Hermes's REPL, birthing it first if needed.
 *
 * Idempotent: if Hermes is already born (LIVE or STOPPED) it is entered
 * directly without re-birthing.  On BYE from Hermes, control returns here.
 */
static void mama_word_connect_hermes(VM *vm __attribute__((unused)))
{
    VMRegistryEntry  entry;
    VM              *hermes;
    const char      *saved_name;

    /* Birth if not found or previously dead/stillborn */
    if (capsule_vm_find_by_name_nocase("Hermes", &entry) != 0 ||
        entry.state == VM_STATE_DEAD ||
        entry.state == VM_STATE_STILLBORN) {
        uint32_t new_vm_id = 0;
        const char *saved = console_get_vm_name();
        CapsuleRunResult r;

        console_set_vm_name("Hermes");
        r = capsule_birth_baby(
            "hermes:init.4th",
            capsule_get_directory(),
            capsule_get_descriptors(),
            capsule_get_names(),
            capsule_get_arena(),
            &new_vm_id, (void **)0);
        console_set_vm_name(saved);

        if (r != CAPSULE_RUN_OK) {
            console_println("CONNECT-HERMES: birth failed");
            return;
        }
        capsule_vm_registry_set_name(new_vm_id, "Hermes");
        if (capsule_vm_find_by_name_nocase("Hermes", &entry) != 0) {
            console_println("CONNECT-HERMES: registry error");
            return;
        }
    }

    hermes = (VM *)entry.vm_ptr;
    if (!hermes) {
        console_println("CONNECT-HERMES: no VM pointer");
        return;
    }

    saved_name = console_get_vm_name();
    console_set_vm_name(entry.name);
    capsule_vm_set_state(entry.vm_id, VM_STATE_LIVE);

    sk_repl_run(hermes);

    capsule_vm_set_state(entry.vm_id, VM_STATE_STOPPED);
    console_set_vm_name(saved_name);
}

/**
 * @brief Register Mama FORTH vocabulary words with the VM
 *
 * Creates the MAMA vocabulary and registers all capsule-related words.
 *
 * @param vm Pointer to the VM instance
 */
void register_mama_forth_words(VM *vm)
{
    /* Register words in FORTH vocabulary first */
    register_word(vm, "BYE", mama_word_bye);
    register_word(vm, "CONNECT-HERMES", mama_word_connect_hermes);
    register_word(vm, "CONNECT-ARTEMIS", mama_word_connect_artemis);
    register_word(vm, "BIRTH", mama_word_birth);
    register_word(vm, "KILL", mama_word_kill);
    register_word(vm, "START", mama_word_start);
    register_word(vm, "STOP", mama_word_stop);
    register_word(vm, "USE", mama_word_use);
    register_word(vm, "EXEC", mama_word_exec);
    register_word(vm, "CAPSULE-COUNT", mama_word_capsule_count);
    register_word(vm, "CAPSULE@", mama_word_capsule_fetch);
    register_word(vm, "CAPSULE-HASH@", mama_word_capsule_hash_fetch);
    register_word(vm, "CAPSULE-FLAGS@", mama_word_capsule_flags_fetch);
    register_word(vm, "CAPSULE-LEN@", mama_word_capsule_len_fetch);
    register_word(vm, "CAPSULE-BIRTH", mama_word_capsule_birth);
    register_word(vm, "CAPSULE-RUN", mama_word_capsule_run);
    register_word(vm, "MAMA-VM-ID", mama_word_mama_vm_id);
    register_word(vm, "VM-COUNT", mama_word_vm_count);
    register_word(vm, "VM-STEP", mama_word_vm_step);
    register_word(vm, "VM-EXEC", mama_word_vm_exec);
    register_word(vm, "CAPSULE-TEST", mama_word_capsule_test);

    /* Create and switch to MAMA vocabulary */
    vm_bootstrap_root_vocabulary(vm, "MAMA");

    /* Re-register in MAMA vocabulary context */
    register_word(vm, "BYE", mama_word_bye);
    register_word(vm, "CONNECT-HERMES", mama_word_connect_hermes);
    register_word(vm, "CONNECT-ARTEMIS", mama_word_connect_artemis);
    register_word(vm, "BIRTH", mama_word_birth);
    register_word(vm, "KILL", mama_word_kill);
    register_word(vm, "START", mama_word_start);
    register_word(vm, "STOP", mama_word_stop);
    register_word(vm, "USE", mama_word_use);
    register_word(vm, "EXEC", mama_word_exec);
    register_word(vm, "CAPSULE-COUNT", mama_word_capsule_count);
    register_word(vm, "CAPSULE@", mama_word_capsule_fetch);
    register_word(vm, "CAPSULE-HASH@", mama_word_capsule_hash_fetch);
    register_word(vm, "CAPSULE-FLAGS@", mama_word_capsule_flags_fetch);
    register_word(vm, "CAPSULE-LEN@", mama_word_capsule_len_fetch);
    register_word(vm, "CAPSULE-BIRTH", mama_word_capsule_birth);
    register_word(vm, "CAPSULE-RUN", mama_word_capsule_run);
    register_word(vm, "MAMA-VM-ID", mama_word_mama_vm_id);
    register_word(vm, "VM-COUNT", mama_word_vm_count);
    register_word(vm, "VM-STEP", mama_word_vm_step);
    register_word(vm, "VM-EXEC", mama_word_vm_exec);
    register_word(vm, "CAPSULE-TEST", mama_word_capsule_test);
    register_word(vm, "EXEC", mama_word_exec);

    /* Return to FORTH vocabulary */
    vocabulary_word_forth(vm);
    vocabulary_word_definitions(vm);
}

#endif /* __STARKERNEL__ */
