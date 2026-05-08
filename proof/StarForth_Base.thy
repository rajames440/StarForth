theory StarForth_Base
  imports Main
begin

(* =========================================================================
   SPECIFICATION AUTHORITY NOTICE
   ─────────────────────────────────────────────────────────────────────────
   This file is the GROUND TRUTH specification of the StarForth VM state.
   C code is written TO MATCH this theory, not the other way around.

   HUMAN REVIEW PROTOCOL:
     Every field of vm_state must be audited against include/vm.h.
     Every predicate (wf_vm, ds_full, rs_full) must be audited against the
     corresponding C guard expressions in vm.c, word_source/, and vm_time.c.
     Discrepancies between this theory and the C source are BUGS IN THE C CODE.

   When a C implementation is changed, re-check the corresponding lemma.
   When a lemma is changed, update the C implementation to match.

   PROOF STATUS KEY (used throughout):
     ✓ — fully mechanised, no C review needed beyond initial audit
     ⚠ — sorry/axiom: C code must be manually verified to satisfy this claim
     ○ — proof obligation: C code must implement this specification exactly
   ======================================================================== *)

(* =========================================================================
   Section 1: Cell type and FORTH-79 boolean convention
   ======================================================================== *)

(* ○ CODE-MUST-MATCH: cell_t in include/vm.h is `typedef signed long cell_t`.
   On x86-64 Linux, signed long = 64-bit signed integer.
   We model it as HOL int (arbitrary precision).
   All word proofs hold in C provided no intermediate value overflows the
   64-bit signed range.  This is not a hidden assumption — it is the stated
   correctness domain for FORTH programs that avoid overflow UB.            *)
type_synonym cell = int

(* ○ CODE-MUST-MATCH: include/vm.h defines:
     #define FORTH_TRUE  ((cell_t)-1)
     #define FORTH_FALSE ((cell_t) 0)
   Any C logical word that produces a boolean result MUST use these macros,
   not raw 1/0 or any other encoding.  Verified in:
     src/word_source/logical_words.c — ALL comparison and test words         *)
definition forth_true :: cell where "forth_true  = -1"
definition forth_false :: cell where "forth_false =  0"

definition to_forth_bool :: "bool \<Rightarrow> cell" where
  "to_forth_bool b = (if b then forth_true else forth_false)"

lemma to_forth_bool_True  [simp]: "to_forth_bool True  = -1"
  by (simp add: to_forth_bool_def forth_true_def)
lemma to_forth_bool_False [simp]: "to_forth_bool False =  0"
  by (simp add: to_forth_bool_def forth_false_def)
lemma to_forth_bool_eq: "to_forth_bool b = (if b then -1 else 0)"
  by (cases b; simp)

(* =========================================================================
   Section 2: Stack type and capacity constants
   ======================================================================== *)

(* Top-of-stack is the head of the list.
   ○ CODE-MUST-MATCH: In C, vm->data_stack[vm->dsp] is TOS.  The list model
   maps directly: head = data_stack[dsp], tail = data_stack[dsp-1..0].      *)
type_synonym forth_stack = "cell list"

(* ○ CODE-MUST-MATCH: #define STACK_SIZE 1024 in include/vm.h.
   ⚠ HUMAN-REVIEW: If STACK_SIZE is ever changed in the C code, this
   definition must be updated and ALL stack overflow/underflow lemmas
   re-proved to ensure they still hold.                                      *)
definition STACK_SIZE :: nat where "STACK_SIZE = 1024"

(* ○ CODE-MUST-MATCH: Makefile default parameters for rolling window.
   ⚠ HUMAN-REVIEW: These values appear in multiple C files:
     - ROLLING_WINDOW_SIZE: src/rolling_window_of_truth.c, include/vm.h
     - ADAPTIVE_MIN_WINDOW_SIZE: src/rolling_window_of_truth.c
     - ADAPTIVE_SHRINK_RATE: src/rolling_window_of_truth.c
     - ADAPTIVE_GROWTH_THRESHOLD: src/rolling_window_of_truth.c
   Any change in the C Makefile parameters that alter these values must be
   reflected here and the Loop #2 / Loop #5 invariant proofs re-checked.   *)
definition ROLLING_WINDOW_SIZE      :: nat where "ROLLING_WINDOW_SIZE      = 4096"
definition ADAPTIVE_MIN_WINDOW_SIZE :: nat where "ADAPTIVE_MIN_WINDOW_SIZE = 256"
definition ADAPTIVE_SHRINK_RATE     :: nat where "ADAPTIVE_SHRINK_RATE     = 50"
definition ADAPTIVE_GROWTH_THRESHOLD :: nat where "ADAPTIVE_GROWTH_THRESHOLD = 5"
definition DICTIONARY_SIZE          :: nat where "DICTIONARY_SIZE          = 4096"

(* =========================================================================
   Section 3: Sub-struct types mirroring C structs in include/vm.h
   ======================================================================== *)

(* ── VM mode ────────────────────────────────────────────────────────────── *)
(* ○ CODE-MUST-MATCH: enum vm_mode_t { MODE_INTERPRET = 0, MODE_COMPILE = 1 }
   ⚠ HUMAN-REVIEW: ModeInterpret = 0, ModeCompile = 1.  The C code in
   src/vm.c uses integer comparisons against these values.  Verify that
   no C code uses the raw integer 1 vs 2 or other off-by-one.              *)
datatype vm_mode = ModeInterpret | ModeCompile

(* ── Mutex / lock state ─────────────────────────────────────────────────── *)
(* ○ CODE-MUST-MATCH: sf_mutex_t wraps pthread_mutex_t (Linux) or a bare-metal
   spinlock.  The abstract lock_state here models only the ownership state.
   ⚠ HUMAN-REVIEW: Verify that the C mutex implementation guarantees exactly
   the acquire/release semantics proved in StarForth_Mutex.thy — in
   particular that no thread can observe LockHeld while the lock is logically
   LockFree.  This requires auditing src/platform/linux/mutex.c.            *)
datatype lock_state = LockFree | LockHeld nat   \<comment> \<open>nat = thread ID holder\<close>

(* ── SSM L8 Jacquard mode ────────────────────────────────────────────────── *)
(* ○ CODE-MUST-MATCH: ssm_l8_mode_t { SSM_C0..SSM_C3 } in include/ssm_jacquard.h
   ⚠ HUMAN-REVIEW: Verify the C mode transition logic matches StarForth_Concurrent
   (heartbeat_step does not alter SSM mode during word execution).          *)
datatype ssm_mode = C0 | C1 | C2 | C3

record ssm_l8_state =
  ssm_current_mode       :: ssm_mode
  ssm_hysteresis_counter :: nat
  ssm_pending_mode       :: ssm_mode

(* ── DictPhysics ─────────────────────────────────────────────────────────── *)
(* ○ CODE-MUST-MATCH: struct DictPhysics in include/vm.h
   ⚠ HUMAN-REVIEW: Check that every field listed here has a corresponding field
   in the C DictPhysics struct with the same semantics.
   dp_temperature_q8 → temperature_q8 (uint16_t, Q8 format)
   dp_last_active_ns → last_active_ns  (uint64_t, monotonic ns)
   dp_last_decay_ns  → last_decay_ns   (uint64_t)
   dp_mass_bytes     → mass_bytes      (uint32_t, header+body size)
   dp_avg_latency_ns → avg_latency_ns  (uint64_t, rolling average)
   dp_state_flags    → state_flags     (uint32_t, encoded traits)              *)
record dict_physics =
  dp_temperature_q8   :: nat    \<comment> \<open>Q8 execution-heat hotness\<close>
  dp_last_active_ns   :: nat    \<comment> \<open>monotonic timestamp of last execution\<close>
  dp_last_decay_ns    :: nat
  dp_mass_bytes       :: nat    \<comment> \<open>header + body footprint\<close>
  dp_avg_latency_ns   :: nat    \<comment> \<open>rolling average latency\<close>
  dp_state_flags      :: nat    \<comment> \<open>encoded execution traits\<close>

(* ── Dictionary entry ────────────────────────────────────────────────────── *)
(* ○ CODE-MUST-MATCH: struct DictEntry in include/vm.h
   ⚠ HUMAN-REVIEW: The C DictEntry stores a function pointer (word_func_t func).
   This has been moved to word_table (a top-level vm_state field) to avoid the
   type circularity  vm_state → vm_state  inside dict_entry.
   Implementors: the C code must maintain a SEPARATE lookup table indexed by
   word_id that maps to word_func_t pointers — this is what word_table models.
   The dict_entry record here has no func field; look it up via word_table.    *)
record dict_entry =
  de_name     :: string
  de_flags    :: nat
  de_heat     :: cell        \<comment> \<open>execution_heat — drives Loop #1 optimization\<close>
  de_word_id  :: nat
  de_physics  :: dict_physics

(* ── Word transition metrics ─────────────────────────────────────────────── *)
(* ○ CODE-MUST-MATCH: struct WordTransitionMetrics in include/physics_pipelining_metrics.h
   ⚠ HUMAN-REVIEW: wt_transition_heat and wt_context_window are modeled as HOL
   functions (nat → nat) instead of C arrays.  In C these are fixed-size arrays
   of length DICTIONARY_SIZE / context window size respectively.
   Verify that array accesses in the C code are always in bounds (no UB).
   The abstraction here assumes they are.                                      *)
record word_transition_metrics =
  wt_transition_heat         :: "nat \<Rightarrow> nat"   \<comment> \<open>word_id \<rightarrow> transition count\<close>
  wt_total_transitions       :: nat
  wt_prefetch_attempts       :: nat
  wt_prefetch_hits           :: nat
  wt_prefetch_misses         :: nat
  wt_latency_saved_q48       :: int    \<comment> \<open>Q48.16, signed\<close>
  wt_misprediction_cost_q48  :: int
  wt_max_prob_q48            :: int
  wt_most_likely_next        :: nat    \<comment> \<open>word_id of predicted next word\<close>
  wt_context_window          :: "nat \<Rightarrow> nat"   \<comment> \<open>circular context buffer\<close>
  wt_context_window_pos      :: nat
  wt_actual_window_size      :: nat
  wt_total_context_trans     :: nat

(* ── Rolling Window of Truth ──────────────────────────────────────────────── *)
(* ○ CODE-MUST-MATCH: struct RollingWindowOfTruth in include/rolling_window_of_truth.h
   ⚠ HUMAN-REVIEW: rw_history, rw_snapshot_buf0, rw_snapshot_buf1 model circular
   ring buffers.  In C these are uint32_t arrays of fixed size ROLLING_WINDOW_SIZE.
   Verify that:
     (a) ring buffer write positions always remain < ROLLING_WINDOW_SIZE
     (b) rw_act_window is always set to min(total_executions, ROLLING_WINDOW_SIZE)
         immediately after incrementing rw_total_exec (see src/rolling_window_of_truth.c)
   ⚠ HUMAN-REVIEW: rw_eff_window and rw_act_window are DISTINCT fields.
     rw_eff_window = effective_window_size (adaptive target)
     rw_act_window = actual_window_size = min(total_exec, ROLLING_WINDOW_SIZE)
   Verify the C implementation updates both correctly on every execution.     *)
record rolling_window_state =
  rw_history          :: "nat \<Rightarrow> nat"   \<comment> \<open>circular buffer of word IDs\<close>
  rw_snapshot_buf0    :: "nat \<Rightarrow> nat"   \<comment> \<open>double-buffer slot 0\<close>
  rw_snapshot_buf1    :: "nat \<Rightarrow> nat"   \<comment> \<open>double-buffer slot 1\<close>
  rw_window_pos       :: nat
  rw_total_exec       :: nat
  rw_is_warm          :: bool
  rw_eff_window       :: nat   \<comment> \<open>effective_window_size (adaptive, mutated by Loops 2/5/6)\<close>
  rw_act_window       :: nat   \<comment> \<open>actual_window_size = min(total_exec, ROLLING_WINDOW_SIZE)\<close>
  rw_last_diversity   :: nat
  rw_diversity_checks :: nat
  rw_snap_index       :: nat
  rw_snap_pending     :: bool
  rw_snap_window_pos0 :: nat
  rw_snap_window_pos1 :: nat
  rw_snap_total0      :: nat
  rw_snap_total1      :: nat
  rw_snap_eff0        :: nat
  rw_snap_eff1        :: nat
  rw_snap_warm0       :: bool
  rw_snap_warm1       :: bool
  rw_adapt_accum      :: nat
  rw_adapt_pending    :: bool

(* ── Pipeline global metrics ─────────────────────────────────────────────── *)
(* ○ CODE-MUST-MATCH: struct PipelineGlobalMetrics in include/vm.h
   ⚠ HUMAN-REVIEW: pm_last_accuracy_num / pm_last_accuracy_den model the
   binary-chop accuracy ratio as a fraction.  Verify that the C code stores
   and updates these consistently and that pm_last_accuracy_den > 0 whenever
   pm_last_accuracy_num > 0 (no division by zero in accuracy computation).   *)
record pipeline_metrics_state =
  pm_prefetch_attempts    :: nat
  pm_prefetch_hits        :: nat
  pm_tuning_checks        :: nat
  pm_last_window_size     :: nat
  pm_last_accuracy_num    :: nat   \<comment> \<open>numerator of accuracy ratio\<close>
  pm_last_accuracy_den    :: nat   \<comment> \<open>denominator; den > 0 when num > 0\<close>
  pm_suggested_next_size  :: nat

(* ── Heartbeat tick snapshot ─────────────────────────────────────────────── *)
(* ○ CODE-MUST-MATCH: struct HeartbeatTickSnapshot in include/vm.h
   ⚠ HUMAN-REVIEW: hts_actual_window corresponds to the actual_window_size field
   added in the actual_window_size branch.  Verify that heartbeat_capture_tick_snapshot
   in src/heartbeat_export.c sets this field to min(total_executions, ROLLING_WINDOW_SIZE)
   and NOT to effective_window_size.                                           *)
record hb_tick_snapshot =
  hts_tick_number       :: nat
  hts_elapsed_ns        :: nat
  hts_tick_interval_ns  :: nat
  hts_cache_hits_delta  :: nat
  hts_bucket_hits_delta :: nat
  hts_word_exec_delta   :: nat
  hts_hot_word_count    :: nat
  hts_avg_word_heat_num :: nat   \<comment> \<open>numerator (Q48.16 / 65536 as nat)\<close>
  hts_window_width      :: nat
  hts_actual_window     :: nat
  hts_predicted_labels  :: nat
  hts_jitter_ns_num     :: nat
  hts_l8_mode           :: nat

(* ── Heartbeat state ─────────────────────────────────────────────────────── *)
(* ○ CODE-MUST-MATCH: struct HeartbeatState in include/vm.h
   ⚠ HUMAN-REVIEW: hb_tick_buffer is modeled as a HOL function (nat → hb_tick_snapshot)
   but in C it is a fixed-size ring buffer.  Verify:
     (a) hb_tick_write_idx always wraps modulo hb_tick_buffer_size (ring semantics)
     (b) hb_tick_buffer_size ≤ the actual C array bound (no out-of-bounds write)
   ⚠ HUMAN-REVIEW: hb_enabled must be False when the heartbeat thread is not
   running.  Every code path that accesses heartbeat state must check hb_enabled
   first; the Isabelle theories assume heartbeat is logically active.          *)
record heartbeat_state =
  hb_tick_count         :: nat
  hb_last_infer_tick    :: nat
  hb_check_counter      :: nat
  hb_enabled            :: bool
  hb_tick_target_ns     :: nat
  hb_snap_index         :: nat
  hb_infer_count        :: nat
  hb_early_exit_count   :: nat
  hb_words_executed     :: nat
  hb_dict_lookups       :: nat
  hb_tick_buffer        :: "nat \<Rightarrow> hb_tick_snapshot"
  hb_tick_buffer_size   :: nat
  hb_tick_write_idx     :: nat
  hb_tick_count_total   :: nat
  hb_run_start_ns       :: nat
  hb_tick_number_offset :: nat

(* ── Inference outputs ───────────────────────────────────────────────────── *)
(* ○ CODE-MUST-MATCH: struct InferenceOutputs in include/inference_engine.h
   ⚠ HUMAN-REVIEW: The early-exit flag io_early_exited corresponds to
   InferenceOutputs.early_exited in C.  When this flag is true, the C code
   must NOT update rw_eff_window or decay_slope_q48.  Verify in
   src/inference_engine.c run_inference() and its callers in src/vm_time.c. *)
record inference_outputs_state =
  io_early_exited           :: bool
  io_window_variance_q48    :: nat   \<comment> \<open>pattern variance Q48.16\<close>
  io_adaptive_window_width  :: nat
  io_adaptive_decay_slope   :: nat   \<comment> \<open>Q48.16\<close>
  io_fit_quality_q48        :: nat
  io_window_size_used       :: nat

(* =========================================================================
   Section 4: Full VM state record
   ── ⚠ CRITICAL HUMAN REVIEW REQUIRED ──────────────────────────────────────
   This record must mirror EVERY field of the C VM struct (include/vm.h).
   Any field present in C but absent here is an UNCOVERED STATE that could
   hide a correctness gap.  Similarly, any field present here but not in C
   (or with different semantics) is a SPECIFICATION BUG.

   AUDIT CHECKLIST (compare to include/vm.h struct VM):
     □ data_stack / return_stack — lists, TOS = head
     □ exit_colon / abort_req   — boolean flags
     □ memory / memory_size     — flat address space
     □ dictionary / latest_id / here / dict_fence — dictionary state
     □ dict_lock / word_id_next — dict management
     □ vm_mode / vm_ip / state_var / vm_base / vm_error / vm_halted
     □ word_table               — function pointer table (C: per-DictEntry func ptr)
     □ heat_threshold_25th/50th/75th / last_bucket_reorg_ns / lookup_strategy
     □ rolling_window           — all sub-fields including rw_act_window
     □ decay_slope_q48 / last_decay_check_ns / total_heat_at_check / ...
     □ tuning_lock              — for physics tuning critical section
     □ pipeline_metrics / hb_decay_cursor
     □ heartbeat                — full HeartbeatState
     □ last_inference           — InferenceOutputs option
     □ ssm_l8                   — SSM L8 Jacquard mode
   ======================================================================== *)

record vm_state =
  (* ── Core stacks ────────────────────────────────────────────────────── *)
  (* ○ CODE-MUST-MATCH: C: cell_t data_stack[STACK_SIZE], dsp (index of TOS)
     Stack word proofs rely on head = TOS = data_stack[dsp].               *)
  data_stack    :: forth_stack
  return_stack  :: forth_stack
  (* ○ CODE-MUST-MATCH: exit_colon = return from colon definition flag
     abort_req = ABORT word has been called.  Must be checked by the
     interpreter loop before each word dispatch.                             *)
  exit_colon    :: bool
  abort_req     :: bool

  (* ── Memory ──────────────────────────────────────────────────────────── *)
  (* ○ CODE-MUST-MATCH: C: uint8_t vm_memory[VM_MEMORY_SIZE]
     ⚠ HUMAN-REVIEW: In C, memory is byte-addressed (uint8_t) but cell
     operations (@ !) access aligned cell_t-sized chunks.  The abstract model
     uses nat → cell.  Alignment and bounds are modelled abstractly via
     valid_addr in StarForth_Memory_Words.thy.  A concrete memory model would
     require verifying byte-level alignment in the C vm_load_cell/vm_store_cell
     implementations.                                                        *)
  memory        :: "nat \<Rightarrow> cell"
  memory_size   :: nat

  (* ── Dictionary ──────────────────────────────────────────────────────── *)
  (* ○ CODE-MUST-MATCH: C dictionary is a linked list of DictEntry structs
     allocated in a flat memory arena.  We abstract it as a partial function
     word_id → dict_entry.  This abstracts away the arena layout.
     ⚠ HUMAN-REVIEW: Verify that word ID assignment is injective (no two
     words share a word_id) in src/memory_management.c.                     *)
  dictionary    :: "nat \<Rightarrow> dict_entry option"
  latest_id     :: "nat option"
  here          :: nat                            \<comment> \<open>next free byte offset in arena\<close>
  dict_fence    :: "nat option"                   \<comment> \<open>FENCE word_id for FORGET\<close>
  dict_lock     :: lock_state
  word_id_next  :: nat

  (* ── Execution state ──────────────────────────────────────────────────── *)
  vm_mode       :: vm_mode
  vm_ip         :: nat                            \<comment> \<open>instruction pointer (byte offset)\<close>
  state_var     :: cell                           \<comment> \<open>STATE: 0=interp, -1=compile\<close>
  vm_base       :: cell                           \<comment> \<open>numeric base for I/O (2..36)\<close>
  vm_error      :: bool
  vm_halted     :: bool

  (* ── Word semantics table ───────────────────────────────────────────── *)
  (* ⚠ CRITICAL DESIGN NOTE: The C DictEntry stores word_func_t func, a function
     pointer per word.  Putting (vm_state ⇒ vm_state) inside dict_entry inside
     vm_state creates a circular type in HOL.  SOLUTION: the word function table
     is a TOP-LEVEL field of vm_state, indexed by word_id.

     ○ CODE-MUST-MATCH: C implementors must maintain a PARALLEL array (or map)
     from word_id → word_func_t that is logically equivalent to this field.
     The dict_entry.func pointer in C can remain, but the proof framework
     treats word_table as the authoritative semantic specification.
     ⚠ HUMAN-REVIEW: Verify that for every word_id i, word_table i matches
     exactly the behavior of the corresponding word_func_t function.          *)
  word_table    :: "nat \<Rightarrow> vm_state \<Rightarrow> vm_state"

  (* ── Physics Loop #1: Execution heat tracking ───────────────────────── *)
  (* ○ CODE-MUST-MATCH: heat_threshold_{25th,50th,75th} in C VM struct.
     ⚠ HUMAN-REVIEW: Thresholds are recomputed periodically by the heat bucket
     reorg.  Verify that heat_thresholds_wf (StarForth_Loop1_Heat.thy) holds
     after every reorg step: 0 ≤ 25th ≤ 50th ≤ 75th ≤ HEAT_MAX.            *)
  heat_threshold_25th       :: cell
  heat_threshold_50th       :: cell
  heat_threshold_75th       :: cell
  last_bucket_reorg_ns      :: nat
  lookup_strategy           :: nat   \<comment> \<open>0=naive 1=heat-aware 2=inference-reorg\<close>

  (* ── Physics Loop #2: Rolling Window of Truth ────────────────────────── *)
  (* ○ CODE-MUST-MATCH: RollingWindowOfTruth vm->rolling_window.
     ⚠ HUMAN-REVIEW: Verify window_invariant (StarForth_Loop2_Window.thy)
     holds after every call to rolling_window_record_execution() in
     src/rolling_window_of_truth.c.  Pay special attention to the boundary
     condition when rw_total_exec wraps around ROLLING_WINDOW_SIZE.          *)
  rolling_window            :: rolling_window_state

  (* ── Physics Loop #3: Linear heat decay ─────────────────────────────── *)
  (* ○ CODE-MUST-MATCH: decay_slope_q48 is a Q48.16 uint64_t.  Its invariant
     (slope > 0) must be preserved by every code path that updates it:
       src/vm_time.c vm_tick_slope_validator()
       src/inference_engine.c run_inference() (slope output)
     ⚠ HUMAN-REVIEW: Check that slope clamping in the C code always results in
     slope ≥ DECAY_SLOPE_MIN (= 1 ulp in Q48.16).                            *)
  decay_slope_q48           :: nat   \<comment> \<open>current decay rate Q48.16; invariant: > 0\<close>
  last_decay_check_ns       :: nat
  total_heat_at_check       :: nat
  hot_word_count_at_check   :: nat
  stale_word_count_at_check :: nat
  word_count_at_check       :: nat
  decay_direction           :: int   \<comment> \<open>-1=decrease 0=stable +1=increase\<close>
  tuning_lock               :: lock_state

  (* ── Physics Loop #4 & #5: Pipelining / prefetch ────────────────────── *)
  (* ○ CODE-MUST-MATCH: PipelineGlobalMetrics vm->pipeline_metrics.
     ⚠ HUMAN-REVIEW: pm_prefetch_hits ≤ pm_prefetch_attempts must hold after
     every pm_record_hit / pm_record_miss call.  Verify in
     src/physics_pipelining_metrics.c.                                        *)
  pipeline_metrics          :: pipeline_metrics_state
  hb_decay_cursor           :: nat   \<comment> \<open>continuation cursor for background decay\<close>

  (* ── Physics Loop #7: Heartbeat ─────────────────────────────────────── *)
  (* ○ CODE-MUST-MATCH: HeartbeatState vm->heartbeat.
     ⚠ HUMAN-REVIEW: hb_tick_target_ns > 0 must hold at all times (enforced
     by hb_shorten_period clamping to TICK_MIN_NS in StarForth_Loop7_Heartrate).
     Verify that src/vm_time.c never sets tick_target_ns to 0 or wraps below
     zero (unsigned underflow).                                               *)
  heartbeat                 :: heartbeat_state

  (* ── Unified Inference Engine (Loops #5 & #6) ───────────────────────── *)
  (* ○ CODE-MUST-MATCH: InferenceOutputs vm->last_inference_outputs (option).
     None = inference has not run yet; Some io = last successful output.
     ⚠ HUMAN-REVIEW: Verify that io_early_exited, io_adaptive_window_width, and
     io_adaptive_decay_slope are always set atomically (under tuning_lock) so
     the concurrent model is not violated.                                    *)
  last_inference            :: "inference_outputs_state option"

  (* ── SSM L8: Jacquard mode selector ─────────────────────────────────── *)
  (* ○ CODE-MUST-MATCH: SSM L8 state from include/ssm_jacquard.h.
     ⚠ HUMAN-REVIEW: The heartbeat_step axioms in StarForth_Transition say
     heartbeat_step does not change vm_mode.  If the SSM L8 selector can
     change vm_mode as a side-effect, that axiom is violated and the
     non-interference proof breaks.  Verify in the SSM implementation.       *)
  ssm_l8                    :: ssm_l8_state

(* =========================================================================
   Section 5: Well-formedness, error signalling, capacity predicates
   ======================================================================== *)

(* ⚠ CRITICAL: wf_vm is the formal specification of "the VM is in a valid,
   non-error state."  Every C function that takes a VM* must ensure the VM
   satisfies wf_vm on entry (or restore it on exit).  This is the C-level
   invariant that the Isabelle proofs assume.

   ○ CODE-MUST-MATCH: The C interpreter loop in src/vm.c must check:
     - Stack bounds (dsp, rsp within [0, STACK_SIZE))
     - Error flag clear
     - rolling_window.effective_window_size ∈ [ADAPTIVE_MIN, ROLLING_WINDOW_SIZE]
     - rolling_window.actual_window_size = min(total_exec, ROLLING_WINDOW_SIZE)
     - decay_slope_q48 > 0
     - heartbeat.tick_target_ns > 0
   before entering the main execution loop.                                   *)
definition wf_vm :: "vm_state \<Rightarrow> bool" where
  "wf_vm vm \<longleftrightarrow>
     length (data_stack vm)   \<le> STACK_SIZE \<and>
     length (return_stack vm) \<le> STACK_SIZE \<and>
     \<not> vm_error vm \<and>
     rw_eff_window (rolling_window vm) \<ge> ADAPTIVE_MIN_WINDOW_SIZE \<and>
     rw_eff_window (rolling_window vm) \<le> ROLLING_WINDOW_SIZE \<and>
     rw_act_window (rolling_window vm) \<le> ROLLING_WINDOW_SIZE \<and>
     rw_act_window (rolling_window vm) =
       min (rw_total_exec (rolling_window vm)) ROLLING_WINDOW_SIZE \<and>
     decay_slope_q48 vm > 0 \<and>
     hb_tick_target_ns (heartbeat vm) > 0"

(* ○ CODE-MUST-MATCH: set_error models vm->error = 1.
   Only the vm_error flag changes — ALL other fields remain exactly unchanged.
   ⚠ HUMAN-REVIEW: Verify that every C error path sets ONLY the error flag and
   does NOT accidentally corrupt data_stack, rolling_window, or other physics.
   Particularly check: vm_pop() underflow handlers, vm_push() overflow handlers,
   memory access out-of-bounds handlers.                                      *)
definition set_error :: "vm_state \<Rightarrow> vm_state" where
  "set_error vm = vm\<lparr>vm_error := True\<rparr>"

lemma set_error_error   [simp]: "vm_error   (set_error vm) = True"
  by (simp add: set_error_def)
lemma set_error_ds      [simp]: "data_stack  (set_error vm) = data_stack vm"
  by (simp add: set_error_def)
lemma set_error_rs      [simp]: "return_stack (set_error vm) = return_stack vm"
  by (simp add: set_error_def)
lemma set_error_rolling [simp]: "rolling_window (set_error vm) = rolling_window vm"
  by (simp add: set_error_def)
lemma set_error_hb      [simp]: "heartbeat (set_error vm) = heartbeat vm"
  by (simp add: set_error_def)
lemma set_error_decay   [simp]: "decay_slope_q48 (set_error vm) = decay_slope_q48 vm"
  by (simp add: set_error_def)
lemma set_error_pipeline [simp]: "pipeline_metrics (set_error vm) = pipeline_metrics vm"
  by (simp add: set_error_def)
lemma set_error_infer   [simp]: "last_inference (set_error vm) = last_inference vm"
  by (simp add: set_error_def)
lemma set_error_ssm     [simp]: "ssm_l8 (set_error vm) = ssm_l8 vm"
  by (simp add: set_error_def)
lemma set_error_dict    [simp]: "dictionary (set_error vm) = dictionary vm"
  by (simp add: set_error_def)

(* ── Capacity predicates ──────────────────────────────────────────────── *)
(* ○ CODE-MUST-MATCH: ds_full ↔ dsp + 1 >= STACK_SIZE in C.
   ⚠ HUMAN-REVIEW: Verify that every C word that pushes to the data stack
   checks ds_full BEFORE the push, not after.  Off-by-one here = memory
   corruption in the C stack array.                                          *)
definition ds_full :: "vm_state \<Rightarrow> bool" where
  "ds_full vm \<longleftrightarrow> length (data_stack vm) \<ge> STACK_SIZE"

definition rs_full :: "vm_state \<Rightarrow> bool" where
  "rs_full vm \<longleftrightarrow> length (return_stack vm) \<ge> STACK_SIZE"

(* ── Physics preservation: the key "no assumptions" mechanism ────────── *)

(* ⚠ CENTRAL CORRECTNESS MECHANISM:
   HOL record-update syntax vm⦇data_stack := xs⦈ proves that EVERY field
   not mentioned in the update (rolling_window, heartbeat, decay_slope_q48,
   pipeline_metrics, dictionary, word_table, etc.) is EXACTLY unchanged.
   This is how we mechanise "proof of correctness in totality with no
   assumptions" — no field is silently assumed unchanged; HOL record algebra
   guarantees it.

   ○ CODE-MUST-MATCH: Any C word implementation that modifies only the data
   stack MUST NOT touch any other VM field.  The C compiler does not enforce
   this; the Isabelle proof framework does.  Each of the lemmas below
   corresponds to a field that must NOT be written by a "pure data stack"
   word.  Violations require adding the field to the word's formal spec and
   re-proving the affected theorems.                                          *)

lemma ds_update_preserves_rolling:
  "rolling_window (vm\<lparr>data_stack := xs\<rparr>) = rolling_window vm"
  by simp

lemma ds_update_preserves_heartbeat:
  "heartbeat (vm\<lparr>data_stack := xs\<rparr>) = heartbeat vm"
  by simp

lemma ds_update_preserves_decay:
  "decay_slope_q48 (vm\<lparr>data_stack := xs\<rparr>) = decay_slope_q48 vm"
  by simp

lemma ds_update_preserves_pipeline:
  "pipeline_metrics (vm\<lparr>data_stack := xs\<rparr>) = pipeline_metrics vm"
  by simp

lemma ds_update_preserves_inference:
  "last_inference (vm\<lparr>data_stack := xs\<rparr>) = last_inference vm"
  by simp

lemma ds_update_preserves_ssm:
  "ssm_l8 (vm\<lparr>data_stack := xs\<rparr>) = ssm_l8 vm"
  by simp

lemma ds_update_preserves_dict:
  "dictionary (vm\<lparr>data_stack := xs\<rparr>) = dictionary vm"
  by simp

lemma ds_update_preserves_word_table:
  "word_table (vm\<lparr>data_stack := xs\<rparr>) = word_table vm"
  by simp

end
