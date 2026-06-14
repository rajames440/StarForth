theory ACL_TTL_Bounded
  imports ACL_Pin_Monotone
begin

(* =========================================================================
   ACL_TTL_Bounded — ACL-TTL-COMPUTE output is bounded; TTL invariants

   Mirrors: capsules/ACL.4th Block 2063  (ACL-TTL-COMPUTE)
            src/word_source/acl_words.c  (forth_acl_ttl_store)
            src/vm.c                     (acl_recheck hot/cold path)

   ACL-TTL-COMPUTE produces an adaptive TTL from execution heat:
     ttl = min(heat / 4 + ACL_BASE_TTL, ACL_MAX_TTL)

   This theory proves:
     • Output is always ≥ ACL_BASE_TTL (minimum service)
     • Output is always ≤ ACL_MAX_TTL  (bounded cost amortisation)
     • TTL hot-path decrement is safe (TTL stays ≥ 0 when pre-condition holds)
     • STRICT mode holds TTL at 0 after every ACL-RECHECK

   ○ CODE-MUST-MATCH: capsules/ACL.4th Block 2063:
       : ACL-TTL-COMPUTE ( xt -- ttl )
         ACL-HEAT@            ( -- heat )
         4 /                  ( -- heat/4 )
         ACL-BASE-TTL +       ( -- raw-ttl )
         ACL-MAX-TTL MIN ;    ( -- ttl, capped )

   ○ CODE-MUST-MATCH: capsules/ACL.4th Block 2060:
       16 CONSTANT ACL-BASE-TTL
       65535 CONSTANT ACL-MAX-TTL
   ======================================================================== *)

(* =========================================================================
   Section 1: TTL constants
   ======================================================================== *)

definition ACL_BASE_TTL :: nat where "ACL_BASE_TTL = 16"
definition ACL_MAX_TTL  :: nat where "ACL_MAX_TTL  = 65535"

(* ○ CODE-MUST-MATCH: include/vm.h #define ACL_TTL_OPEN UINT32_MAX
   Used as the fail-open TTL when ACL-RECHECK is not yet in the dictionary.
   Modelled as a large natural number; actual C value is 2^32 - 1.        *)
definition ACL_TTL_OPEN :: nat where "ACL_TTL_OPEN = 4294967295"

(* =========================================================================
   Section 2: ACL-TTL-COMPUTE
   ======================================================================== *)

(* ○ CODE-MUST-MATCH: capsules/ACL.4th ACL-TTL-COMPUTE formula.
   heat is taken as a nat (de_heat cast to nat; heat ≥ 0 by Loop #1).    *)
definition acl_ttl_compute :: "nat \<Rightarrow> nat" where
  "acl_ttl_compute heat = min (heat div 4 + ACL_BASE_TTL) ACL_MAX_TTL"

(* Lower bound: result is always ≥ ACL_BASE_TTL.                           *)
lemma acl_ttl_compute_ge_base:
  "acl_ttl_compute heat \<ge> ACL_BASE_TTL"
  by (simp add: acl_ttl_compute_def ACL_BASE_TTL_def)

(* Upper bound: result is always ≤ ACL_MAX_TTL.                            *)
lemma acl_ttl_compute_le_max:
  "acl_ttl_compute heat \<le> ACL_MAX_TTL"
  by (simp add: acl_ttl_compute_def)

(* Monotone with heat: more heat → longer TTL (up to the cap).             *)
lemma acl_ttl_compute_monotone:
  "h1 \<le> h2 \<Longrightarrow> acl_ttl_compute h1 \<le> acl_ttl_compute h2"
  by (simp add: acl_ttl_compute_def div_le_mono)

(* Specific test vector matching the POST test:
   heat=2560 → 2560/4 + 16 = 640 + 16 = 656 (within [16, 65535]).
   ○ CODE-MUST-MATCH: acl_words_test.c test_acl_adaptive_ttl asserts
     ACL-TTL-COMPUTE with heat=2560 produces 656.                         *)
lemma acl_ttl_compute_2560:
  "acl_ttl_compute 2560 = 656"
  by (simp add: acl_ttl_compute_def ACL_BASE_TTL_def ACL_MAX_TTL_def)

(* Zero-heat gives base TTL (new words with no execution history).         *)
lemma acl_ttl_compute_zero_heat:
  "acl_ttl_compute 0 = ACL_BASE_TTL"
  by (simp add: acl_ttl_compute_def ACL_BASE_TTL_def ACL_MAX_TTL_def)

(* =========================================================================
   Section 3: TTL hot-path decrement
   ======================================================================== *)

(* ○ CODE-MUST-MATCH: src/vm.c execute_colon_word / vm_interpret_word:
     if (w->acl_ttl == 0)
         acl_recheck(vm, w);
     else
         w->acl_ttl--;
   The hot path only fires when acl_ttl > 0.                               *)
definition acl_ttl_hot_decrement :: "dict_entry \<Rightarrow> dict_entry" where
  "acl_ttl_hot_decrement e =
     (if de_acl_ttl e > 0 then e\<lparr>de_acl_ttl := de_acl_ttl e - 1\<rparr> else e)"

(* Precondition: hot path only fires when TTL > 0.                         *)
lemma hot_decrement_decreases_ttl:
  "de_acl_ttl e > 0 \<Longrightarrow>
   de_acl_ttl (acl_ttl_hot_decrement e) = de_acl_ttl e - 1"
  by (simp add: acl_ttl_hot_decrement_def)

(* TTL stays ≥ 0 (natural arithmetic guarantees this; no underflow).       *)
lemma hot_decrement_nonneg:
  "de_acl_ttl (acl_ttl_hot_decrement e) \<ge> 0"
  by simp

(* Hot decrement does not change acl_pinned.                               *)
lemma hot_decrement_preserves_pinned:
  "de_acl_pinned (acl_ttl_hot_decrement e) = de_acl_pinned e"
  by (simp add: acl_ttl_hot_decrement_def)

(* Hot decrement does not change acl_allow.                                *)
lemma hot_decrement_preserves_allow:
  "de_acl_allow (acl_ttl_hot_decrement e) = de_acl_allow e"
  by (simp add: acl_ttl_hot_decrement_def)

(* =========================================================================
   Section 4: STRICT-mode ACL-RECHECK keeps TTL at 0
   ======================================================================== *)

(* ○ CODE-MUST-MATCH: capsules/ACL.4th ACL-RECHECK STRICT branch:
     1 OVER ACL-ALLOW!
     0 OVER ACL-TTL!
     DROP EXIT
   In STRICT mode, TTL is set back to 0 after every recheck.              *)
definition acl_recheck_strict :: "dict_entry \<Rightarrow> dict_entry" where
  "acl_recheck_strict e = e\<lparr>de_acl_allow := True, de_acl_ttl := 0\<rparr>"

lemma strict_recheck_ttl_zero [simp]:
  "de_acl_ttl (acl_recheck_strict e) = 0"
  by (simp add: acl_recheck_strict_def)

lemma strict_recheck_allows [simp]:
  "de_acl_allow (acl_recheck_strict e) = True"
  by (simp add: acl_recheck_strict_def)

(* STRICT mode: every call triggers the cold path (TTL = 0 after recheck). *)
lemma strict_mode_always_rechecks:
  "de_acl_mode e = ACL_MODE_STRICT \<Longrightarrow>
   de_acl_ttl (acl_recheck_strict e) = 0"
  by simp

end
