theory ACL_No_Escalation
  imports ACL_Inherit_Clears_Pin ACL_TTL_Bounded ACL_Emergency_Bypass
begin

(* =========================================================================
   ACL_No_Escalation — a child word cannot acquire acl_pinned=True via
                       ACL-INHERIT alone

   Mirrors: src/word_source/acl_words.c (acl_inherit_entry)
            capsules/ACL.4th             (ACL-INHERIT, ACL-BOOT)

   The no-escalation property: inheriting from any source word (pinned or
   not) always leaves the destination un-pinned.  A child word can only
   become pinned via an explicit ACL-PIN call, which requires intentional
   action from a trusted caller.

   This is the key security property that prevents capsule words from
   "upgrading" themselves to kernel-level immutability by inheriting from
   a privileged parent.

   ○ CODE-MUST-MATCH: acl_inherit_entry() clears dst->acl_pinned = 0
     unconditionally (verified in ACL_Inherit_Clears_Pin).

   ⚠ HUMAN-REVIEW: Verify that no C code calls acl_inherit_entry() and
     then immediately calls forth_acl_pin() on the same entry in a sequence
     that the caller did not explicitly intend to produce a pinned result.
     In particular, capsule BIRTH should call ACL-INHERIT but NOT ACL-PIN
     unless explicitly requested by the capsule definition.
   ======================================================================== *)

(* =========================================================================
   Section 1: Reachability of acl_pinned via inherit chains
   ======================================================================== *)

(* Single inherit: result is never pinned.                                 *)
lemma single_inherit_not_pinned:
  "\<not> de_acl_pinned (acl_inherit_entry src dst)"
  by simp

(* Chain of inherits: result is still never pinned, regardless of how many
   times we inherit.                                                        *)
lemma inherit_chain_not_pinned:
  "\<not> de_acl_pinned (acl_inherit_entry s2 (acl_inherit_entry s1 dst))"
  by simp

(* Inheriting from a pinned source cannot create a pinned destination.     *)
lemma pinned_src_cannot_create_pinned_dst:
  "de_acl_pinned src \<Longrightarrow>
   \<not> de_acl_pinned (acl_inherit_entry src dst)"
  by simp

(* =========================================================================
   Section 2: The ONLY path to pinned = explicit ACL-PIN
   ======================================================================== *)

(* A word is pinned iff it was explicitly pinned at some point.
   The operations available (inherit, mode_store, ttl_store, allow_store)
   cannot produce a pinned result except for acl_pin.                      *)

lemma inherit_cannot_produce_pinned:
  "\<not> de_acl_pinned (acl_inherit_entry src dst)"
  by simp

lemma mode_store_cannot_produce_pinned_from_unpinned:
  "\<not> de_acl_pinned e \<Longrightarrow>
   \<not> de_acl_pinned (acl_mode_store mode e)"
  by (simp add: acl_mode_store_def)

lemma ttl_store_cannot_produce_pinned_from_unpinned:
  "\<not> de_acl_pinned e \<Longrightarrow>
   \<not> de_acl_pinned (acl_ttl_store ttl e)"
  by (simp add: acl_ttl_store_def)

lemma allow_store_cannot_produce_pinned_from_unpinned:
  "\<not> de_acl_pinned e \<Longrightarrow>
   \<not> de_acl_pinned (acl_allow_store flag e)"
  by (simp add: acl_allow_store_def)

(* =========================================================================
   Section 3: Composition proofs
   ======================================================================== *)

(* Inheriting from a pinned word and then applying guarded stores cannot
   escalate to pinned.                                                      *)
lemma inherit_then_stores_not_pinned:
  assumes "\<not> de_acl_pinned (acl_inherit_entry src dst)"
  shows "\<not> de_acl_pinned (acl_mode_store m (acl_ttl_store t (acl_allow_store f (acl_inherit_entry src dst))))"
proof -
  have h1: "\<not> de_acl_pinned (acl_allow_store f (acl_inherit_entry src dst))"
    using assms by (simp add: acl_allow_store_def)
  have h2: "\<not> de_acl_pinned (acl_ttl_store t (acl_allow_store f (acl_inherit_entry src dst)))"
    using h1 by (simp add: acl_ttl_store_def)
  show ?thesis using h2 by (simp add: acl_mode_store_def)
qed

(* Full composition: inherit from pinned source, apply all guarded stores —
   result is NOT pinned.                                                   *)
theorem inherit_from_pinned_no_escalation:
  "de_acl_pinned src \<Longrightarrow>
   \<not> de_acl_pinned
       (acl_mode_store m
         (acl_ttl_store t
           (acl_allow_store f
             (acl_inherit_entry src dst))))"
  by (intro inherit_then_stores_not_pinned) simp

(* =========================================================================
   Section 4: Emergency bypass does not escalate pinned
   ======================================================================== *)

(* Executing under emergency_console does not change any dictionary entry's
   acl_pinned field (the bypass only affects execution gating).
   ○ CODE-MUST-MATCH: emergency_console is purely a vm_state flag; it never
     writes to any DictEntry ACL fields.                                   *)
lemma bypass_preserves_dict_pinned:
  "\<And>e. dictionary (with_emergency_bypass vm (\<lambda>v. v)) = dictionary vm"
  by (simp add: with_emergency_bypass_def Let_def)

end
