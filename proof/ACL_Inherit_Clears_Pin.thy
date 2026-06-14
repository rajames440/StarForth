theory ACL_Inherit_Clears_Pin
  imports ACL_Pin_Monotone
begin

(* =========================================================================
   ACL_Inherit_Clears_Pin — ACL-INHERIT always produces acl_pinned=False
                             in the destination entry

   Mirrors: src/word_source/acl_words.c  (acl_inherit_entry)
            capsules/ACL.4th             (ACL-INHERIT)

   The inheritance operation copies acl_mode from a source entry to a
   destination entry, and then UNCONDITIONALLY clears the destination's
   acl_pinned field.  This ensures that inherited ACL cannot "accidentally
   inherit" the immutability of a privileged parent word.

   ○ CODE-MUST-MATCH: src/word_source/acl_words.c acl_inherit_entry():
       dst->acl_mode   = src->acl_mode;
       dst->acl_pinned = 0;
       dst->acl_ttl    = 0;
       dst->acl_allow  = 1;
     The destination is ALWAYS left with acl_pinned = 0 regardless of
     src->acl_pinned or dst->acl_pinned on entry.

   ⚠ HUMAN-REVIEW: Verify that no code path between acl_inherit_entry and
     the next FORTH instruction can set dst->acl_pinned = 1 (i.e., check
     that ACL-INHERIT is not immediately followed by ACL-PIN in any capsule
     boot sequence that should leave the word un-pinned).
   ======================================================================== *)

(* =========================================================================
   Section 1: ACL-INHERIT operation
   ======================================================================== *)

(* ○ CODE-MUST-MATCH: acl_inherit_entry(src, dst):
     dst->acl_mode   = src->acl_mode;
     dst->acl_pinned = 0;   ← always cleared
     dst->acl_ttl    = 0;
     dst->acl_allow  = 1;
   All other fields of dst are unchanged.                                   *)
definition acl_inherit_entry :: "dict_entry \<Rightarrow> dict_entry \<Rightarrow> dict_entry" where
  "acl_inherit_entry src dst =
     dst\<lparr>de_acl_mode    := de_acl_mode src,
         de_acl_pinned  := False,
         de_acl_ttl     := 0,
         de_acl_allow   := True\<rparr>"

(* =========================================================================
   Section 2: Structural lemmas
   ======================================================================== *)

(* Core guarantee: ACL-INHERIT always clears acl_pinned in destination.    *)
lemma inherit_clears_pin [simp]:
  "de_acl_pinned (acl_inherit_entry src dst) = False"
  by (simp add: acl_inherit_entry_def)

(* ACL-INHERIT copies the source mode.                                     *)
lemma inherit_copies_mode [simp]:
  "de_acl_mode (acl_inherit_entry src dst) = de_acl_mode src"
  by (simp add: acl_inherit_entry_def)

(* ACL-INHERIT resets TTL to 0 (first execution always rechecks).         *)
lemma inherit_resets_ttl [simp]:
  "de_acl_ttl (acl_inherit_entry src dst) = 0"
  by (simp add: acl_inherit_entry_def)

(* ACL-INHERIT sets allow=True (permissive default until ACL-RECHECK).     *)
lemma inherit_sets_allow [simp]:
  "de_acl_allow (acl_inherit_entry src dst) = True"
  by (simp add: acl_inherit_entry_def)

(* ACL-INHERIT does not change the destination's name or execution heat.  *)
lemma inherit_preserves_name [simp]:
  "de_name (acl_inherit_entry src dst) = de_name dst"
  by (simp add: acl_inherit_entry_def)
lemma inherit_preserves_heat [simp]:
  "de_heat (acl_inherit_entry src dst) = de_heat dst"
  by (simp add: acl_inherit_entry_def)

(* =========================================================================
   Section 3: No-escalation property
   ======================================================================== *)

(* A pinned source cannot propagate its pinned status to the destination.  *)
lemma pinned_src_cannot_escalate:
  "de_acl_pinned src = True \<Longrightarrow>
   de_acl_pinned (acl_inherit_entry src dst) = False"
  by simp

(* Even if both src and dst are pinned, the result is un-pinned.           *)
lemma both_pinned_clears_dst:
  "de_acl_pinned src = True \<Longrightarrow> de_acl_pinned dst = True \<Longrightarrow>
   de_acl_pinned (acl_inherit_entry src dst) = False"
  by simp

(* =========================================================================
   Section 4: Interaction with ACL-PIN
   ======================================================================== *)

(* After ACL-INHERIT the result can be re-pinned by an explicit ACL-PIN.
   The two-step sequence is intentional (INHERIT then PIN on the same entry
   is how a word inherits a mode and then becomes immutable).              *)
lemma inherit_then_pin:
  "de_acl_pinned (acl_pin (acl_inherit_entry src dst)) = True"
  by simp

(* Conversely, PIN then INHERIT always ends with un-pinned destination.
   This proves ACL-INHERIT cannot be "blocked" by pre-pinning dst.        *)
lemma pin_then_inherit:
  "de_acl_pinned (acl_inherit_entry src (acl_pin dst)) = False"
  by simp

end
