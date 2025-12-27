# Agent Notes (2025-12-26)

## Current Status
- Interpreter now gated; vocab bootstrap is C-only, so no VM scripts run before primitives finish registering.
- VM arena has page-sized guard bands; dictionary code records the last registered word (name/XT/HERE/latest) and logs it automatically during guard trips or halting panics.
- Parity build still crashes while `register_arithmetic_words()` runs (XT fetch at 0xA0000). We now have instrumentation to pinpoint the culprit, but the split into vm_core/vm_bootstrap/vm_runtime has not yet started.

## Picking Up The Thread
1. **Use the new guard hooks:** Call `vm_dictionary_log_last_word()` and watch the guard asserts (`REGISTER_CHECK_ARENA`) during the arithmetic module to see which word corrupts memory.
2. **Add hard invariants:** Inside `vm_create_word()` / threaded stores, assert that every pointer stays inside the expected region and panic immediately on failures.
3. **Minimal bootstrap harness:** Build a reduced path that only runs until primitive registration so iteration is fast and noise from heartbeat/inference is eliminated.
4. **Only after the corruption is fixed** should we proceed with the vm_core/vm_bootstrap/vm_runtime split mandated by M7.5.

Keep the guard output handy—if the arena canary goes off, it now prints the last known good word and HERE/latest so the failing primitive is obvious.
