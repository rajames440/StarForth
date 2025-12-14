theory Physics_StateMachine
  imports Main
begin

section \<open>HOLA analytics state machine\<close>

text \<open>
  This theory provides a lightweight algebraic model for the HOLA
  shared-memory protocol used by the StarForth physics runtime. The
  invariants captured here are intentionally conservative so future
  proofs can extend them as the VM evolves.
\<close>

datatype hola_status = StatusOK | StatusBusy | StatusUnsupported

datatype hola_opcode =
    HOLA_NOP
  | HOLA_REQUEST_SNAPSHOT
  | HOLA_RESET_RING
  | HOLA_STATUS hola_status

record hola_command =
  opcode :: hola_opcode
  arg0 :: nat
  arg1 :: nat

record hola_ring =
  ring_bytes :: nat
  read_offset :: nat
  write_offset :: nat
  produce_seq :: nat
  consume_seq :: nat
  dropped_events :: nat

datatype vm_state = VMIdle | VMPolling | VMSnapshotPending | VMPublishing

record hola_state =
  ring :: hola_ring
  mailbox :: hola_command
  status :: hola_opcode
  vm_state :: vm_state
  last_timestamp_ns :: nat

abbreviation stride :: nat where "stride \<equiv> 8"
abbreviation header_units :: nat where "header_units \<equiv> 2"

definition event_units :: "nat \<Rightarrow> nat" where
  "event_units payload = header_units + ((payload + (stride - 1)) div stride)"

definition event_bytes :: "nat \<Rightarrow> nat" where
  "event_bytes payload = stride * event_units payload"

definition publish_event :: "nat \<Rightarrow> hola_ring \<Rightarrow> hola_ring" where
  "publish_event payload r =
     (let bytes = event_bytes payload in
      if ring_bytes r = 0 \<or> bytes = 0 \<or> ring_bytes r < bytes then r
      else r\<lparr>
        write_offset := (write_offset r + bytes) mod ring_bytes r,
        produce_seq := Suc (produce_seq r)
      \<rparr>)"

definition consume_event :: "nat \<Rightarrow> hola_ring \<Rightarrow> hola_ring" where
  "consume_event payload r =
     (let bytes = event_bytes payload in
      if ring_bytes r = 0 \<or> bytes = 0 \<or> ring_bytes r < bytes \<or> consume_seq r \<ge> produce_seq r
      then r
      else r\<lparr>
        read_offset := (read_offset r + bytes) mod ring_bytes r,
        consume_seq := Suc (consume_seq r)
      \<rparr>)"

definition drop_event :: "hola_ring \<Rightarrow> hola_ring" where
  "drop_event r = r\<lparr> dropped_events := Suc (dropped_events r) \<rparr>"

definition reset_ring :: "hola_ring \<Rightarrow> hola_ring" where
  "reset_ring r = r\<lparr> read_offset := 0, write_offset := 0, consume_seq := produce_seq r \<rparr>"

definition ring_invariant :: "hola_ring \<Rightarrow> bool" where
  "ring_invariant r \<longleftrightarrow> ring_bytes r > 0 \<and> ring_bytes r mod stride = 0 \<and>
    read_offset r < ring_bytes r \<and> write_offset r < ring_bytes r \<and>
    read_offset r mod stride = 0 \<and> write_offset r mod stride = 0 \<and>
    consume_seq r \<le> produce_seq r"

definition state_invariant :: "hola_state \<Rightarrow> bool" where
  "state_invariant s \<longleftrightarrow> ring_invariant (ring s)"

definition idle_mailbox :: hola_command where
  "idle_mailbox = \<lparr> opcode = HOLA_NOP, arg0 = 0, arg1 = 0 \<rparr>"

definition ack_command :: "hola_status \<Rightarrow> hola_command" where
  "ack_command st = \<lparr> opcode = HOLA_STATUS st, arg0 = 0, arg1 = 0 \<rparr>"

lemma stride_pos[simp]: "stride > 0" by simp

lemma event_units_ge_header[simp]: "event_units payload \<ge> header_units"
  unfolding event_units_def by simp

lemma event_bytes_pos[simp]: "event_bytes payload > 0"
  unfolding event_bytes_def event_units_def by simp

text \<open>
  Basic constructors (`publish_event`, `consume_event`, `drop_event`,
  `reset_ring`) are intentionally left without full invariant proofs in
  this stub. Subsequent iterations will populate locale-based lemmas
  once the runtime contracts stabilise.
\<close>

end
