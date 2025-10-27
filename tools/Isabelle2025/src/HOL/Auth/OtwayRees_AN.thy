(*  Title:      HOL/Auth/OtwayRees_AN.thy
    Author:     Lawrence C Paulson, Cambridge University Computer Laboratory
    Copyright   1996  University of Cambridge
*)

section\<open>The Otway-Rees Protocol as Modified by Abadi and Needham\<close>

theory OtwayRees_AN imports Public begin

text\<open>
This simplified version has minimal encryption and explicit messages.

Note that the formalization does not even assume that nonces are fresh.
This is because the protocol does not rely on uniqueness of nonces for
security, only for freshness, and the proof script does not prove freshness
properties.

From page 11 of
  Abadi and Needham (1996).  
  Prudent Engineering Practice for Cryptographic Protocols.
  IEEE Trans. SE 22 (1)
\<close>

inductive_set otway :: "event list set"
  where
   Nil: \<comment> \<open>The empty trace\<close>
        "[] \<in> otway"

 | Fake: \<comment> \<open>The Spy may say anything he can say.  The sender field is correct,
            but agents don't use that information.\<close>
         "\<lbrakk>evsf \<in> otway;  X \<in> synth (analz (knows Spy evsf))\<rbrakk>
          \<Longrightarrow> Says Spy B X  # evsf \<in> otway"

        
 | Reception: \<comment> \<open>A message that has been sent can be received by the
                  intended recipient.\<close>
              "\<lbrakk>evsr \<in> otway;  Says A B X \<in>set evsr\<rbrakk>
               \<Longrightarrow> Gets B X # evsr \<in> otway"

 | OR1:  \<comment> \<open>Alice initiates a protocol run\<close>
         "evs1 \<in> otway
          \<Longrightarrow> Says A B \<lbrace>Agent A, Agent B, Nonce NA\<rbrace> # evs1 \<in> otway"

 | OR2:  \<comment> \<open>Bob's response to Alice's message.\<close>
         "\<lbrakk>evs2 \<in> otway;
             Gets B \<lbrace>Agent A, Agent B, Nonce NA\<rbrace> \<in>set evs2\<rbrakk>
          \<Longrightarrow> Says B Server \<lbrace>Agent A, Agent B, Nonce NA, Nonce NB\<rbrace>
                 # evs2 \<in> otway"

 | OR3:  \<comment> \<open>The Server receives Bob's message.  Then he sends a new
           session key to Bob with a packet for forwarding to Alice.\<close>
         "\<lbrakk>evs3 \<in> otway;  Key KAB \<notin> used evs3;
             Gets Server \<lbrace>Agent A, Agent B, Nonce NA, Nonce NB\<rbrace>
               \<in>set evs3\<rbrakk>
          \<Longrightarrow> Says Server B
               \<lbrace>Crypt (shrK A) \<lbrace>Nonce NA, Agent A, Agent B, Key KAB\<rbrace>,
                 Crypt (shrK B) \<lbrace>Nonce NB, Agent A, Agent B, Key KAB\<rbrace>\<rbrace>
              # evs3 \<in> otway"

 | OR4:  \<comment> \<open>Bob receives the Server's (?) message and compares the Nonces with
             those in the message he previously sent the Server.
             Need \<^term>\<open>B \<noteq> Server\<close> because we allow messages to self.\<close>
         "\<lbrakk>evs4 \<in> otway;  B \<noteq> Server;
             Says B Server \<lbrace>Agent A, Agent B, Nonce NA, Nonce NB\<rbrace> \<in>set evs4;
             Gets B \<lbrace>X, Crypt(shrK B)\<lbrace>Nonce NB,Agent A,Agent B,Key K\<rbrace>\<rbrace>
               \<in>set evs4\<rbrakk>
          \<Longrightarrow> Says B A X # evs4 \<in> otway"

 | Oops: \<comment> \<open>This message models possible leaks of session keys.  The nonces
             identify the protocol run.\<close>
         "\<lbrakk>evso \<in> otway;
             Says Server B
                      \<lbrace>Crypt (shrK A) \<lbrace>Nonce NA, Agent A, Agent B, Key K\<rbrace>,
                        Crypt (shrK B) \<lbrace>Nonce NB, Agent A, Agent B, Key K\<rbrace>\<rbrace>
               \<in>set evso\<rbrakk>
          \<Longrightarrow> Notes Spy \<lbrace>Nonce NA, Nonce NB, Key K\<rbrace> # evso \<in> otway"


declare Says_imp_knows_Spy [THEN analz.Inj, dest]
declare parts.Body  [dest]
declare analz_into_parts [dest]
declare Fake_parts_insert_in_Un  [dest]


text\<open>A "possibility property": there are traces that reach the end\<close>
lemma "\<lbrakk>B \<noteq> Server; Key K \<notin> used []\<rbrakk>
      \<Longrightarrow> \<exists>evs \<in> otway.
           Says B A (Crypt (shrK A) \<lbrace>Nonce NA, Agent A, Agent B, Key K\<rbrace>)
             \<in> set evs"
apply (intro exI bexI)
apply (rule_tac [2] otway.Nil
                    [THEN otway.OR1, THEN otway.Reception,
                     THEN otway.OR2, THEN otway.Reception,
                     THEN otway.OR3, THEN otway.Reception, THEN otway.OR4])
apply (possibility, simp add: used_Cons) 
done

lemma Gets_imp_Says [dest!]:
     "\<lbrakk>Gets B X \<in> set evs; evs \<in> otway\<rbrakk> \<Longrightarrow> \<exists>A. Says A B X \<in> set evs"
by (erule rev_mp, erule otway.induct, auto)



text\<open>For reasoning about the encrypted portion of messages\<close>

lemma OR4_analz_knows_Spy:
     "\<lbrakk>Gets B \<lbrace>X, Crypt(shrK B) X'\<rbrace> \<in> set evs;  evs \<in> otway\<rbrakk>
      \<Longrightarrow> X \<in> analz (knows Spy evs)"
by blast


text\<open>Theorems of the form \<^term>\<open>X \<notin> parts (spies evs)\<close> imply that
NOBODY sends messages containing X!\<close>

text\<open>Spy never sees a good agent's shared key!\<close>
lemma Spy_see_shrK [simp]:
     "evs \<in> otway \<Longrightarrow> (Key (shrK A) \<in> parts (knows Spy evs)) = (A \<in> bad)"
by (erule otway.induct, simp_all, blast+)

lemma Spy_analz_shrK [simp]:
     "evs \<in> otway \<Longrightarrow> (Key (shrK A) \<in> analz (knows Spy evs)) = (A \<in> bad)"
by auto

lemma Spy_see_shrK_D [dest!]:
     "\<lbrakk>Key (shrK A) \<in> parts (knows Spy evs);  evs \<in> otway\<rbrakk> \<Longrightarrow> A \<in> bad"
by (blast dest: Spy_see_shrK)


subsection\<open>Proofs involving analz\<close>

text\<open>Describes the form of K and NA when the Server sends this message.\<close>
lemma Says_Server_message_form:
     "\<lbrakk>Says Server B
            \<lbrace>Crypt (shrK A) \<lbrace>NA, Agent A, Agent B, Key K\<rbrace>,
              Crypt (shrK B) \<lbrace>NB, Agent A, Agent B, Key K\<rbrace>\<rbrace>
           \<in> set evs;
         evs \<in> otway\<rbrakk>
      \<Longrightarrow> K \<notin> range shrK \<and> (\<exists>i. NA = Nonce i) \<and> (\<exists>j. NB = Nonce j)"
apply (erule rev_mp)
apply (erule otway.induct, auto)
done



(****
 The following is to prove theorems of the form

  Key K \<in> analz (insert (Key KAB) (knows Spy evs)) \<Longrightarrow>
  Key K \<in> analz (knows Spy evs)

 A more general formula must be proved inductively.
****)


text\<open>Session keys are not used to encrypt other session keys\<close>

text\<open>The equality makes the induction hypothesis easier to apply\<close>
lemma analz_image_freshK [rule_format]:
 "evs \<in> otway \<Longrightarrow>
   \<forall>K KK. KK \<subseteq> -(range shrK) \<longrightarrow>
          (Key K \<in> analz (Key`KK \<union> (knows Spy evs))) =
          (K \<in> KK | Key K \<in> analz (knows Spy evs))"
apply (erule otway.induct) 
apply (frule_tac [8] Says_Server_message_form)
apply (drule_tac [7] OR4_analz_knows_Spy, analz_freshK, spy_analz, auto) 
done

lemma analz_insert_freshK:
  "\<lbrakk>evs \<in> otway;  KAB \<notin> range shrK\<rbrakk> \<Longrightarrow>
      (Key K \<in> analz (insert (Key KAB) (knows Spy evs))) =
      (K = KAB | Key K \<in> analz (knows Spy evs))"
by (simp only: analz_image_freshK analz_image_freshK_simps)


text\<open>The Key K uniquely identifies the Server's message.\<close>
lemma unique_session_keys:
     "\<lbrakk>Says Server B
          \<lbrace>Crypt (shrK A) \<lbrace>NA, Agent A, Agent B, K\<rbrace>,
            Crypt (shrK B) \<lbrace>NB, Agent A, Agent B, K\<rbrace>\<rbrace>
         \<in> set evs;
        Says Server B'
          \<lbrace>Crypt (shrK A') \<lbrace>NA', Agent A', Agent B', K\<rbrace>,
            Crypt (shrK B') \<lbrace>NB', Agent A', Agent B', K\<rbrace>\<rbrace>
         \<in> set evs;
        evs \<in> otway\<rbrakk>
     \<Longrightarrow> A=A' \<and> B=B' \<and> NA=NA' \<and> NB=NB'"
apply (erule rev_mp, erule rev_mp, erule otway.induct, simp_all)
apply blast+  \<comment> \<open>OR3 and OR4\<close>
done


subsection\<open>Authenticity properties relating to NA\<close>

text\<open>If the encrypted message appears then it originated with the Server!\<close>
lemma NA_Crypt_imp_Server_msg [rule_format]:
    "\<lbrakk>A \<notin> bad;  A \<noteq> B;  evs \<in> otway\<rbrakk>
     \<Longrightarrow> Crypt (shrK A) \<lbrace>NA, Agent A, Agent B, Key K\<rbrace> \<in> parts (knows Spy evs)
       \<longrightarrow> (\<exists>NB. Says Server B
                    \<lbrace>Crypt (shrK A) \<lbrace>NA, Agent A, Agent B, Key K\<rbrace>,
                      Crypt (shrK B) \<lbrace>NB, Agent A, Agent B, Key K\<rbrace>\<rbrace>
                    \<in> set evs)"
apply (erule otway.induct, force)
apply (simp_all add: ex_disj_distrib)
apply blast+  \<comment> \<open>Fake, OR3\<close>
done


text\<open>Corollary: if A receives B's OR4 message then it originated with the
      Server. Freshness may be inferred from nonce NA.\<close>
lemma A_trusts_OR4:
     "\<lbrakk>Says B' A (Crypt (shrK A) \<lbrace>NA, Agent A, Agent B, Key K\<rbrace>) \<in> set evs;
         A \<notin> bad;  A \<noteq> B;  evs \<in> otway\<rbrakk>
      \<Longrightarrow> \<exists>NB. Says Server B
                  \<lbrace>Crypt (shrK A) \<lbrace>NA, Agent A, Agent B, Key K\<rbrace>,
                    Crypt (shrK B) \<lbrace>NB, Agent A, Agent B, Key K\<rbrace>\<rbrace>
                 \<in> set evs"
by (blast intro!: NA_Crypt_imp_Server_msg)


text\<open>Crucial secrecy property: Spy does not see the keys sent in msg OR3
    Does not in itself guarantee security: an attack could violate
    the premises, e.g. by having \<^term>\<open>A=Spy\<close>\<close>
lemma secrecy_lemma:
     "\<lbrakk>A \<notin> bad;  B \<notin> bad;  evs \<in> otway\<rbrakk>
      \<Longrightarrow> Says Server B
           \<lbrace>Crypt (shrK A) \<lbrace>NA, Agent A, Agent B, Key K\<rbrace>,
             Crypt (shrK B) \<lbrace>NB, Agent A, Agent B, Key K\<rbrace>\<rbrace>
          \<in> set evs \<longrightarrow>
          Notes Spy \<lbrace>NA, NB, Key K\<rbrace> \<notin> set evs \<longrightarrow>
          Key K \<notin> analz (knows Spy evs)"
apply (erule otway.induct, force)
apply (frule_tac [7] Says_Server_message_form)
apply (drule_tac [6] OR4_analz_knows_Spy)
apply (simp_all add: analz_insert_eq analz_insert_freshK pushes)
apply spy_analz  \<comment> \<open>Fake\<close>
apply (blast dest: unique_session_keys)+  \<comment> \<open>OR3, OR4, Oops\<close>
done


lemma Spy_not_see_encrypted_key:
     "\<lbrakk>Says Server B
            \<lbrace>Crypt (shrK A) \<lbrace>NA, Agent A, Agent B, Key K\<rbrace>,
              Crypt (shrK B) \<lbrace>NB, Agent A, Agent B, Key K\<rbrace>\<rbrace>
           \<in> set evs;
         Notes Spy \<lbrace>NA, NB, Key K\<rbrace> \<notin> set evs;
         A \<notin> bad;  B \<notin> bad;  evs \<in> otway\<rbrakk>
      \<Longrightarrow> Key K \<notin> analz (knows Spy evs)"
  by (metis secrecy_lemma)


text\<open>A's guarantee.  The Oops premise quantifies over NB because A cannot know
  what it is.\<close>
lemma A_gets_good_key:
     "\<lbrakk>Says B' A (Crypt (shrK A) \<lbrace>NA, Agent A, Agent B, Key K\<rbrace>) \<in> set evs;
         \<forall>NB. Notes Spy \<lbrace>NA, NB, Key K\<rbrace> \<notin> set evs;
         A \<notin> bad;  B \<notin> bad;  A \<noteq> B;  evs \<in> otway\<rbrakk>
      \<Longrightarrow> Key K \<notin> analz (knows Spy evs)"
  by (metis A_trusts_OR4 secrecy_lemma)



subsection\<open>Authenticity properties relating to NB\<close>

text\<open>If the encrypted message appears then it originated with the Server!\<close>
lemma NB_Crypt_imp_Server_msg [rule_format]:
 "\<lbrakk>B \<notin> bad;  A \<noteq> B;  evs \<in> otway\<rbrakk>
  \<Longrightarrow> Crypt (shrK B) \<lbrace>NB, Agent A, Agent B, Key K\<rbrace> \<in> parts (knows Spy evs)
      \<longrightarrow> (\<exists>NA. Says Server B
                   \<lbrace>Crypt (shrK A) \<lbrace>NA, Agent A, Agent B, Key K\<rbrace>,
                     Crypt (shrK B) \<lbrace>NB, Agent A, Agent B, Key K\<rbrace>\<rbrace>
                   \<in> set evs)"
apply (erule otway.induct, force, simp_all add: ex_disj_distrib)
apply blast+  \<comment> \<open>Fake, OR3\<close>
done



text\<open>Guarantee for B: if it gets a well-formed certificate then the Server
  has sent the correct message in round 3.\<close>
lemma B_trusts_OR3:
     "\<lbrakk>Says S B \<lbrace>X, Crypt (shrK B) \<lbrace>NB, Agent A, Agent B, Key K\<rbrace>\<rbrace>
           \<in> set evs;
         B \<notin> bad;  A \<noteq> B;  evs \<in> otway\<rbrakk>
      \<Longrightarrow> \<exists>NA. Says Server B
                   \<lbrace>Crypt (shrK A) \<lbrace>NA, Agent A, Agent B, Key K\<rbrace>,
                     Crypt (shrK B) \<lbrace>NB, Agent A, Agent B, Key K\<rbrace>\<rbrace>
                   \<in> set evs"
by (blast intro!: NB_Crypt_imp_Server_msg)


text\<open>The obvious combination of \<open>B_trusts_OR3\<close> with 
      \<open>Spy_not_see_encrypted_key\<close>\<close>
lemma B_gets_good_key:
     "\<lbrakk>Gets B \<lbrace>X, Crypt (shrK B) \<lbrace>NB, Agent A, Agent B, Key K\<rbrace>\<rbrace>
          \<in> set evs;
         \<forall>NA. Notes Spy \<lbrace>NA, NB, Key K\<rbrace> \<notin> set evs;
         A \<notin> bad;  B \<notin> bad;  A \<noteq> B;  evs \<in> otway\<rbrakk>
      \<Longrightarrow> Key K \<notin> analz (knows Spy evs)"
by (blast dest: B_trusts_OR3 Spy_not_see_encrypted_key)

end
