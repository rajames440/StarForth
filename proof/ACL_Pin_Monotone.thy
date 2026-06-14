theory ACL_Pin_Monotone
  imports StarForth_Base
begin

(* =========================================================================
   ACL_Pin_Monotone — acl_pinned is a set-only (monotone) field

   Mirrors: include/vm.h            (DictEntry.acl_pinned)
            src/word_source/acl_words.c (forth_acl_pin, forth_acl_mode_store,
                                         forth_acl_ttl_store, forth_acl_allow_store)
            capsules/ACL.4th        (ACL-PIN, ACL-INHERIT)

   ○ CODE-MUST-MATCH: The following C functions must never write acl_pinned=0
     after it has been written to 1:
       forth_acl_pin         — sets acl_pinned=1 unconditionally
       forth_acl_mode_store  — no-op if acl_pinned (guard: if (e->acl_pinned) return)
       forth_acl_ttl_store   — no-op if acl_pinned
       forth_acl_allow_store — no-op if acl_pinned
     Only acl_inherit_entry() may clear acl_pinned (on the DESTINATION entry),
     and that is intentional: the ACL_Inherit_Clears_Pin theory proves this is safe.

   ⚠ HUMAN-REVIEW: Verify no C code path writes entry->acl_pinned = 0 except
     acl_inherit_entry().  grep -rn "acl_pinned = 0" src/ should show only that
     one site.
   ======================================================================== *)

(* =========================================================================
   Section 1: ACL mode constants
   ======================================================================== *)

(* ○ CODE-MUST-MATCH: include/vm.h
     #define ACL_MODE_TTL    0
     #define ACL_MODE_STRICT 1                                              *)
definition ACL_MODE_TTL    :: nat where "ACL_MODE_TTL    = 0"
definition ACL_MODE_STRICT :: nat where "ACL_MODE_STRICT = 1"

(* =========================================================================
   Section 2: ACL-PIN operation
   ======================================================================== *)

(* ○ CODE-MUST-MATCH: src/word_source/acl_words.c forth_acl_pin():
     e->acl_pinned = 1;
   The function sets acl_pinned to True and touches no other field.        *)
definition acl_pin :: "dict_entry \<Rightarrow> dict_entry" where
  "acl_pin e = e\<lparr>de_acl_pinned := True\<rparr>"

(* =========================================================================
   Section 3: Monotonicity lemmas
   ======================================================================== *)

(* Core monotonicity: acl_pin sets de_acl_pinned to True.                  *)
lemma acl_pin_sets_pinned [simp]:
  "de_acl_pinned (acl_pin e) = True"
  by (simp add: acl_pin_def)

(* Once pinned, stays pinned: applying acl_pin to an already-pinned entry
   is idempotent.                                                           *)
lemma acl_pin_idempotent:
  "acl_pin (acl_pin e) = acl_pin e"
  by (simp add: acl_pin_def)

(* acl_pin does not alter any non-pinned field.                            *)
lemma acl_pin_preserves_name   [simp]: "de_name     (acl_pin e) = de_name e"
  by (simp add: acl_pin_def)
lemma acl_pin_preserves_flags  [simp]: "de_flags    (acl_pin e) = de_flags e"
  by (simp add: acl_pin_def)
lemma acl_pin_preserves_heat   [simp]: "de_heat     (acl_pin e) = de_heat e"
  by (simp add: acl_pin_def)
lemma acl_pin_preserves_ttl    [simp]: "de_acl_ttl  (acl_pin e) = de_acl_ttl e"
  by (simp add: acl_pin_def)
lemma acl_pin_preserves_allow  [simp]: "de_acl_allow(acl_pin e) = de_acl_allow e"
  by (simp add: acl_pin_def)
lemma acl_pin_preserves_mode   [simp]: "de_acl_mode (acl_pin e) = de_acl_mode e"
  by (simp add: acl_pin_def)

(* =========================================================================
   Section 4: Guard lemmas (pinned entry resists mutation)
   ======================================================================== *)

(* ○ CODE-MUST-MATCH: forth_acl_mode_store() guard:
     if (e->acl_pinned) return;
   A pinned entry's mode is immutable.                                     *)
definition acl_mode_store :: "nat \<Rightarrow> dict_entry \<Rightarrow> dict_entry" where
  "acl_mode_store mode e =
     (if de_acl_pinned e then e else e\<lparr>de_acl_mode := mode\<rparr>)"

lemma pinned_mode_immutable:
  "de_acl_pinned e \<Longrightarrow> acl_mode_store mode e = e"
  by (simp add: acl_mode_store_def)

(* ○ CODE-MUST-MATCH: forth_acl_ttl_store() guard:
     if (e->acl_pinned) return;                                            *)
definition acl_ttl_store :: "nat \<Rightarrow> dict_entry \<Rightarrow> dict_entry" where
  "acl_ttl_store ttl e =
     (if de_acl_pinned e then e else e\<lparr>de_acl_ttl := ttl\<rparr>)"

lemma pinned_ttl_immutable:
  "de_acl_pinned e \<Longrightarrow> acl_ttl_store ttl e = e"
  by (simp add: acl_ttl_store_def)

(* ○ CODE-MUST-MATCH: forth_acl_allow_store() guard:
     if (e->acl_pinned) return;                                            *)
definition acl_allow_store :: "bool \<Rightarrow> dict_entry \<Rightarrow> dict_entry" where
  "acl_allow_store flag e =
     (if de_acl_pinned e then e else e\<lparr>de_acl_allow := flag\<rparr>)"

lemma pinned_allow_immutable:
  "de_acl_pinned e \<Longrightarrow> acl_allow_store flag e = e"
  by (simp add: acl_allow_store_def)

(* Applying any guarded store to a pinned entry leaves it completely
   unchanged — every field is preserved.                                    *)
lemma pinned_entry_fully_immutable:
  assumes "de_acl_pinned e"
  shows "acl_mode_store mode e = e \<and>
         acl_ttl_store ttl e     = e \<and>
         acl_allow_store flag e  = e"
  using assms
  by (simp add: acl_mode_store_def acl_ttl_store_def acl_allow_store_def)

(* =========================================================================
   Section 5: Monotone reachability across arbitrary store sequences
   ======================================================================== *)

(* Once a word is pinned, no sequence of guarded stores can un-pin it.     *)
lemma pinned_stable_under_mode_store:
  "de_acl_pinned e \<Longrightarrow> de_acl_pinned (acl_mode_store mode e)"
  by (simp add: acl_mode_store_def)

lemma pinned_stable_under_ttl_store:
  "de_acl_pinned e \<Longrightarrow> de_acl_pinned (acl_ttl_store ttl e)"
  by (simp add: acl_ttl_store_def)

lemma pinned_stable_under_allow_store:
  "de_acl_pinned e \<Longrightarrow> de_acl_pinned (acl_allow_store flag e)"
  by (simp add: acl_allow_store_def)

end
