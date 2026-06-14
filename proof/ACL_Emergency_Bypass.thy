theory ACL_Emergency_Bypass
  imports ACL_Pin_Monotone
begin

(* =========================================================================
   ACL_Emergency_Bypass — emergency_console=True OR zuse_session=True
                          bypasses ALL ACL checks

   Mirrors: include/vm.h       (VM.emergency_console, VM.zuse_session)
            src/vm.c           (execute_colon_word, vm_interpret_word ACL guards)
            src/vm.c           (acl_recheck — sets emergency_console=1 on entry)
            src/word_source/starforth_words.c (ZUSE-AUTHENTICATE sets zuse_session=1)
            capsules/zuse.4th  (ACL-ZUSE-BOOT calls ZUSE-AUTHENTICATE)

   Two bypass conditions exist:
     (a) emergency_console=1: fault handler / acl_recheck re-entrancy guard;
         written only by C (acl_recheck, sk_repl); active during error recovery.
     (b) zuse_session=1: superuser authenticated; written only by the
         ZUSE-AUTHENTICATE C primitive; grants god-mode — all ACL checks skipped.

   ○ CODE-MUST-MATCH: src/vm.c execute_colon_word:
       if (w && !vm->emergency_console && !vm->zuse_session) {
           if (w->acl_ttl == 0) acl_recheck(vm, w);
           else w->acl_ttl--;
           if (!w->acl_allow) { ... vm->error = 1; return; }
       }

   ○ CODE-MUST-MATCH: src/vm.c vm_interpret_word:
       if (!vm->emergency_console && !vm->zuse_session) { ... ACL check ... }

   ○ CODE-MUST-MATCH: src/vm.c acl_recheck():
       uint8_t saved_ec = vm->emergency_console;
       vm->emergency_console = 1;
       ... call ACL-RECHECK ...
       vm->emergency_console = saved_ec;
     Restores emergency_console on exit; does NOT touch zuse_session.

   ⚠ HUMAN-REVIEW: Verify that acl_recheck() always restores emergency_console
     to saved_ec even when ACL-RECHECK itself errors (the error-recovery path
     in acl_recheck must include the restore).
   ======================================================================== *)

(* =========================================================================
   Section 1: ACL check model
   ======================================================================== *)

(* Model of the two-level ACL check that guards each word execution.
   Returns True iff execution is permitted.
   Both emergency_console and zuse_session independently bypass all checks. *)
definition acl_check_permits :: "vm_state \<Rightarrow> dict_entry \<Rightarrow> bool" where
  "acl_check_permits vm e \<longleftrightarrow>
     emergency_console vm \<or> zuse_session vm \<or> de_acl_allow e"

(* =========================================================================
   Section 2: Emergency bypass lemmas
   ======================================================================== *)

(* Core guarantee: when emergency_console is True, every word is permitted
   regardless of its acl_allow field.                                      *)
lemma emergency_bypass_unconditional:
  "emergency_console vm = True \<Longrightarrow>
   acl_check_permits vm e = True"
  by (simp add: acl_check_permits_def)

(* Denied word (acl_allow=False) becomes permitted under emergency_console. *)
lemma denied_word_permitted_under_emergency:
  "emergency_console vm = True \<Longrightarrow>
   de_acl_allow e = False \<Longrightarrow>
   acl_check_permits vm e = True"
  by (simp add: acl_check_permits_def)

(* =========================================================================
   Section 2b: Zuse session bypass lemmas
   ======================================================================== *)

(* Core guarantee: when zuse_session is True, every word is permitted
   regardless of its acl_allow field (god-mode).                          *)
lemma zuse_bypass_unconditional:
  "zuse_session vm = True \<Longrightarrow>
   acl_check_permits vm e = True"
  by (simp add: acl_check_permits_def)

(* Denied word becomes permitted under zuse_session.                       *)
lemma denied_word_permitted_under_zuse:
  "zuse_session vm = True \<Longrightarrow>
   de_acl_allow e = False \<Longrightarrow>
   acl_check_permits vm e = True"
  by (simp add: acl_check_permits_def)

(* Either bypass condition alone is sufficient for full permission.        *)
lemma either_bypass_sufficient:
  "(emergency_console vm = True \<or> zuse_session vm = True) \<Longrightarrow>
   acl_check_permits vm e = True"
  by (simp add: acl_check_permits_def)

(* Without either bypass, the word's own acl_allow is definitive.         *)
lemma no_bypass_check_follows_allow:
  "emergency_console vm = False \<Longrightarrow>
   zuse_session vm = False \<Longrightarrow>
   acl_check_permits vm e = de_acl_allow e"
  by (simp add: acl_check_permits_def)

(* Legacy alias kept for backward compatibility with existing proof scripts *)
lemma no_emergency_check_follows_allow:
  "emergency_console vm = False \<Longrightarrow>
   zuse_session vm = False \<Longrightarrow>
   acl_check_permits vm e = de_acl_allow e"
  by (simp add: acl_check_permits_def)

(* =========================================================================
   Section 3: acl_recheck re-entrancy protection
   ======================================================================== *)

(* Model of emergency_console save/restore around an ACL-RECHECK call.
   ○ CODE-MUST-MATCH: acl_recheck() saves, sets True, runs RECHECK, restores. *)
definition with_emergency_bypass :: "vm_state \<Rightarrow> (vm_state \<Rightarrow> vm_state) \<Rightarrow> vm_state" where
  "with_emergency_bypass vm f =
     (let saved = emergency_console vm
      in (f (vm\<lparr>emergency_console := True\<rparr>))\<lparr>emergency_console := saved\<rparr>)"

(* Save/restore is transparent: emergency_console returns to its original value. *)
lemma emergency_bypass_restores:
  "emergency_console (with_emergency_bypass vm f) = emergency_console vm"
  by (simp add: with_emergency_bypass_def Let_def)

(* All other VM fields modified by f are preserved (the save/restore only
   touches emergency_console).                                             *)
lemma emergency_bypass_preserves_stacks:
  "data_stack  (with_emergency_bypass vm f) =
   data_stack  (f (vm\<lparr>emergency_console := True\<rparr>))"
  by (simp add: with_emergency_bypass_def Let_def)

(* Nested calls: with_emergency_bypass is safe to nest; the inner call sets
   emergency_console=True (already True), and outer save/restore still works. *)
lemma emergency_bypass_idempotent_on_console:
  "with_emergency_bypass (vm\<lparr>emergency_console := True\<rparr>) f =
   (f (vm\<lparr>emergency_console := True\<rparr>))\<lparr>emergency_console := True\<rparr>"
  by (simp add: with_emergency_bypass_def Let_def)

(* =========================================================================
   Section 4: Invariant — bypass cannot escalate acl_pinned
   ======================================================================== *)

(* The bypass only affects the CHECK (execution gate); it cannot change any
   ACL field of any DictEntry.  Dictionary state is unchanged by the bypass
   mechanism itself (though the executed FORTH code may change it).        *)
lemma emergency_bypass_preserves_dictionary:
  "dictionary (with_emergency_bypass vm (\<lambda>v. v)) = dictionary vm"
  by (simp add: with_emergency_bypass_def Let_def)

end
