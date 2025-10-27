(*  Title:      HOL/Auth/Event.thy
    Author:     Lawrence C Paulson, Cambridge University Computer Laboratory
    Copyright   1996  University of Cambridge

Datatype of events; function "spies"; freshness

"bad" agents have been broken by the Spy; their private keys and internal
    stores are visible to him
*)

section\<open>Theory of Events for Security Protocols\<close>

theory Event imports Message begin

consts  \<comment> \<open>Initial states of agents --- a parameter of the construction\<close>
  initState :: "agent \<Rightarrow> msg set"

datatype
  event = Says  agent agent msg
        | Gets  agent       msg
        | Notes agent       msg
       
consts 
  bad    :: "agent set"                         \<comment> \<open>compromised agents\<close>

text\<open>Spy has access to his own key for spoof messages, but Server is secure\<close>
specification (bad)
  Spy_in_bad     [iff]: "Spy \<in> bad"
  Server_not_bad [iff]: "Server \<notin> bad"
    by (rule exI [of _ "{Spy}"], simp)

primrec knows :: "agent \<Rightarrow> event list \<Rightarrow> msg set"
where
  knows_Nil:   "knows A [] = initState A"
| knows_Cons:
    "knows A (ev # evs) =
       (if A = Spy then 
        (case ev of
           Says A' B X \<Rightarrow> insert X (knows Spy evs)
         | Gets A' X \<Rightarrow> knows Spy evs
         | Notes A' X  \<Rightarrow> 
             if A' \<in> bad then insert X (knows Spy evs) else knows Spy evs)
        else
        (case ev of
           Says A' B X \<Rightarrow> 
             if A'=A then insert X (knows A evs) else knows A evs
         | Gets A' X    \<Rightarrow> 
             if A'=A then insert X (knows A evs) else knows A evs
         | Notes A' X    \<Rightarrow> 
             if A'=A then insert X (knows A evs) else knows A evs))"
(*
  Case A=Spy on the Gets event
  enforces the fact that if a message is received then it must have been sent,
  therefore the oops case must use Notes
*)

text\<open>The constant "spies" is retained for compatibility's sake\<close>

abbreviation (input)
  spies  :: "event list \<Rightarrow> msg set" where
  "spies \<equiv> knows Spy"


text \<open>Set of items that might be visible to somebody: complement of the set of fresh items\<close>
primrec used :: "event list \<Rightarrow> msg set"
where
  used_Nil:   "used []         = (UN B. parts (initState B))"
| used_Cons:  "used (ev # evs) =
                     (case ev of
                        Says A B X \<Rightarrow> parts {X} \<union> used evs
                      | Gets A X   \<Rightarrow> used evs
                      | Notes A X  \<Rightarrow> parts {X} \<union> used evs)"
    \<comment> \<open>The case for \<^term>\<open>Gets\<close> seems anomalous, but \<^term>\<open>Gets\<close> always
        follows \<^term>\<open>Says\<close> in real protocols.  Seems difficult to change.
        See \<open>Gets_correct\<close> in theory \<open>Guard/Extensions.thy\<close>.\<close>

lemma Notes_imp_used: "Notes A X \<in> set evs \<Longrightarrow> X \<in> used evs"
  by (induct evs) (auto split: event.split) 

lemma Says_imp_used: "Says A B X \<in> set evs \<Longrightarrow> X \<in> used evs"
  by (induct evs) (auto split: event.split) 


subsection\<open>Function \<^term>\<open>knows\<close>\<close>

(*Simplifying   
 parts(insert X (knows Spy evs)) = parts{X} \<union> parts(knows Spy evs).
  This version won't loop with the simplifier.*)
lemmas parts_insert_knows_A = parts_insert [of _ "knows A evs"] for A evs

lemma knows_Spy_Says [simp]:
  "knows Spy (Says A B X # evs) = insert X (knows Spy evs)"
  by simp

text\<open>Letting the Spy see "bad" agents' notes avoids redundant case-splits
      on whether \<^term>\<open>A=Spy\<close> and whether \<^term>\<open>A\<in>bad\<close>\<close>
lemma knows_Spy_Notes [simp]:
  "knows Spy (Notes A X # evs) =  
          (if A\<in>bad then insert X (knows Spy evs) else knows Spy evs)"
  by simp

lemma knows_Spy_Gets [simp]: "knows Spy (Gets A X # evs) = knows Spy evs"
  by simp

lemma knows_Spy_subset_knows_Spy_Says:
  "knows Spy evs \<subseteq> knows Spy (Says A B X # evs)"
  by (simp add: subset_insertI)

lemma knows_Spy_subset_knows_Spy_Notes:
  "knows Spy evs \<subseteq> knows Spy (Notes A X # evs)"
  by force

lemma knows_Spy_subset_knows_Spy_Gets:
  "knows Spy evs \<subseteq> knows Spy (Gets A X # evs)"
  by (simp add: subset_insertI)

text\<open>Spy sees what is sent on the traffic\<close>
lemma Says_imp_knows_Spy:
     "Says A B X \<in> set evs \<Longrightarrow> X \<in> knows Spy evs"
  by (induct evs) (auto split: event.split) 

lemma Notes_imp_knows_Spy [rule_format]:
     "Notes A X \<in> set evs \<Longrightarrow> A \<in> bad \<Longrightarrow> X \<in> knows Spy evs"
  by (induct evs) (auto split: event.split) 


text\<open>Elimination rules: derive contradictions from old Says events containing
  items known to be fresh\<close>
lemmas Says_imp_parts_knows_Spy = 
       Says_imp_knows_Spy [THEN parts.Inj, elim_format] 

lemmas knows_Spy_partsEs =
     Says_imp_parts_knows_Spy parts.Body [elim_format]

lemmas Says_imp_analz_Spy = Says_imp_knows_Spy [THEN analz.Inj]

text\<open>Compatibility for the old "spies" function\<close>
lemmas spies_partsEs = knows_Spy_partsEs
lemmas Says_imp_spies = Says_imp_knows_Spy
lemmas parts_insert_spies = parts_insert_knows_A [of _ Spy]


subsection\<open>Knowledge of Agents\<close>

lemma knows_subset_knows_Says: "knows A evs \<subseteq> knows A (Says A' B X # evs)"
  by (simp add: subset_insertI)

lemma knows_subset_knows_Notes: "knows A evs \<subseteq> knows A (Notes A' X # evs)"
  by (simp add: subset_insertI)

lemma knows_subset_knows_Gets: "knows A evs \<subseteq> knows A (Gets A' X # evs)"
  by (simp add: subset_insertI)

text\<open>Agents know what they say\<close>
lemma Says_imp_knows [rule_format]: "Says A B X \<in> set evs \<Longrightarrow> X \<in> knows A evs"
  by (induct evs) (force split: event.split)+

text\<open>Agents know what they note\<close>
lemma Notes_imp_knows [rule_format]: "Notes A X \<in> set evs \<Longrightarrow> X \<in> knows A evs"
  by (induct evs) (force split: event.split)+

text\<open>Agents know what they receive\<close>
lemma Gets_imp_knows_agents [rule_format]:
     "A \<noteq> Spy \<Longrightarrow> Gets A X \<in> set evs \<Longrightarrow> X \<in> knows A evs"
  by (induct evs) (force split: event.split)+

text\<open>What agents DIFFERENT FROM Spy know 
  was either said, or noted, or got, or known initially\<close>
lemma knows_imp_Says_Gets_Notes_initState:
  "\<lbrakk>X \<in> knows A evs; A \<noteq> Spy\<rbrakk> \<Longrightarrow> 
     \<exists>B. Says A B X \<in> set evs \<or> Gets A X \<in> set evs \<or> Notes A X \<in> set evs \<or> X \<in> initState A"
  by(induct evs) (auto split: event.split_asm if_split_asm)

text\<open>What the Spy knows -- for the time being --
  was either said or noted, or known initially\<close>
lemma knows_Spy_imp_Says_Notes_initState:
  "X \<in> knows Spy evs \<Longrightarrow> 
    \<exists>A B. Says A B X \<in> set evs \<or> Notes A X \<in> set evs \<or> X \<in> initState Spy"
  by(induct evs) (auto split: event.split_asm if_split_asm)

lemma parts_knows_Spy_subset_used: "parts (knows Spy evs) \<subseteq> used evs"
  by (induct evs) (auto simp: parts_insert_knows_A split: event.split_asm if_split_asm)

lemmas usedI = parts_knows_Spy_subset_used [THEN subsetD, intro]

lemma initState_into_used: "X \<in> parts (initState B) \<Longrightarrow> X \<in> used evs"
  by (induct evs) (auto simp: parts_insert_knows_A split: event.split)

text \<open>New simprules to replace the primitive ones for @{term used} and @{term knows}\<close>

lemma used_Says [simp]: "used (Says A B X # evs) = parts{X} \<union> used evs"
  by simp

lemma used_Notes [simp]: "used (Notes A X # evs) = parts{X} \<union> used evs"
  by simp

lemma used_Gets [simp]: "used (Gets A X # evs) = used evs"
  by simp

lemma used_nil_subset: "used [] \<subseteq> used evs"
  using initState_into_used by auto

text\<open>NOTE REMOVAL: the laws above are cleaner, as they don't involve "case"\<close>
declare knows_Cons [simp del]
        used_Nil [simp del] used_Cons [simp del]


text\<open>For proving theorems of the form \<^term>\<open>X \<notin> analz (knows Spy evs) \<longrightarrow> P\<close>
  New events added by induction to "evs" are discarded.  Provided 
  this information isn't needed, the proof will be much shorter, since
  it will omit complicated reasoning about \<^term>\<open>analz\<close>.\<close>

lemmas analz_mono_contra =
       knows_Spy_subset_knows_Spy_Says [THEN analz_mono, THEN contra_subsetD]
       knows_Spy_subset_knows_Spy_Notes [THEN analz_mono, THEN contra_subsetD]
       knows_Spy_subset_knows_Spy_Gets [THEN analz_mono, THEN contra_subsetD]


lemma knows_subset_knows_Cons: "knows A evs \<subseteq> knows A (e # evs)"
  by (cases e, auto simp: knows_Cons)

lemma initState_subset_knows: "initState A \<subseteq> knows A evs"
  by (induct evs) (use knows_subset_knows_Cons in fastforce)+

text\<open>For proving \<open>new_keys_not_used\<close>\<close>
lemma keysFor_parts_insert:
     "\<lbrakk>K \<in> keysFor (parts (insert X G));  X \<in> synth (analz H)\<rbrakk> 
      \<Longrightarrow> K \<in> keysFor (parts (G \<union> H)) | Key (invKey K) \<in> parts H"
by (force 
    dest!: parts_insert_subset_Un [THEN keysFor_mono, THEN [2] rev_subsetD]
           analz_subset_parts [THEN keysFor_mono, THEN [2] rev_subsetD]
    intro: analz_subset_parts [THEN subsetD] parts_mono [THEN [2] rev_subsetD])


lemmas analz_impI = impI [where P = "Y \<notin> analz (knows Spy evs)"] for Y evs

ML
\<open>
fun analz_mono_contra_tac ctxt = 
  resolve_tac ctxt @{thms analz_impI} THEN' 
  REPEAT1 o (dresolve_tac ctxt @{thms analz_mono_contra})
  THEN' (mp_tac ctxt)
\<close>

method_setup analz_mono_contra = \<open>
    Scan.succeed (fn ctxt => SIMPLE_METHOD (REPEAT_FIRST (analz_mono_contra_tac ctxt)))\<close>
    "for proving theorems of the form X \<notin> analz (knows Spy evs) \<longrightarrow> P"

text\<open>Useful for case analysis on whether a hash is a spoof or not\<close>
lemmas syan_impI = impI [where P = "Y \<notin> synth (analz (knows Spy evs))"] for Y evs

ML
\<open>
fun synth_analz_mono_contra_tac ctxt = 
  resolve_tac ctxt @{thms syan_impI} THEN'
  REPEAT1 o 
    (dresolve_tac ctxt
     [@{thm knows_Spy_subset_knows_Spy_Says} RS @{thm synth_analz_mono} RS @{thm contra_subsetD},
      @{thm knows_Spy_subset_knows_Spy_Notes} RS @{thm synth_analz_mono} RS @{thm contra_subsetD},
      @{thm knows_Spy_subset_knows_Spy_Gets} RS @{thm synth_analz_mono} RS @{thm contra_subsetD}])
  THEN'
  mp_tac ctxt
\<close>

method_setup synth_analz_mono_contra = \<open>
    Scan.succeed (fn ctxt => SIMPLE_METHOD (REPEAT_FIRST (synth_analz_mono_contra_tac ctxt)))\<close>
    "for proving theorems of the form X \<notin> synth (analz (knows Spy evs)) \<longrightarrow> P"

end
