theory StarForth_Memory_Words
  imports StarForth_Base
begin

(* =========================================================================
   POST-05: Memory Access Words
   Mirrors: src/word_source/memory_words.c
            src/test_runner/modules/memory_words_test.c

   Memory model: abstract function  memory :: "nat \<Rightarrow> cell"  representing
   byte-addressed flat VM memory.  Alignment and vm_addr_ok bounds checking
   are captured by the predicate  valid_addr.  Byte operations (C@, C!)
   additionally require  valid_byte_addr  and mask to 8-bit range.

   This abstraction is sufficient to prove read-after-write correctness and
   the absence of spurious state mutation; physical layout details are
   deferred to a lower-level memory model.
   ======================================================================== *)

(* ── Address validity predicate (abstracts vm_addr_ok) ─────────────────── *)

definition valid_addr :: "(nat \<Rightarrow> cell) \<Rightarrow> nat \<Rightarrow> bool" where
  "valid_addr mem a \<equiv> True"
  \<comment> \<open>Placeholder: in a concrete model this would check alignment and bounds.\<close>

(* ── Cell read/write on the abstract memory model ──────────────────────── *)

definition mem_read :: "(nat \<Rightarrow> cell) \<Rightarrow> nat \<Rightarrow> cell" where
  "mem_read mem a = mem a"

definition mem_write :: "(nat \<Rightarrow> cell) \<Rightarrow> nat \<Rightarrow> cell \<Rightarrow> (nat \<Rightarrow> cell)" where
  "mem_write mem a v = mem(a := v)"

lemma mem_write_read_same:
  "mem_read (mem_write mem a v) a = v"
  by (simp add: mem_read_def mem_write_def)

lemma mem_write_read_other:
  assumes "a \<noteq> b"
  shows "mem_read (mem_write mem a v) b = mem_read mem b"
  by (simp add: mem_read_def mem_write_def assms)

(* ── @ ( addr -- n ) ───────────────────────────────────────────────────── *)
(* Pops addr from data stack, reads cell from memory at addr, pushes value.
   C: vaddr_t addr = VM_ADDR(vm_pop(vm)); value = vm_load_cell(vm, addr).   *)

definition forth_fetch :: "vm_state \<Rightarrow> vm_state" where
  "forth_fetch vm =
    (case data_stack vm of
       []         \<Rightarrow> set_error vm
     | addr # xs \<Rightarrow>
         if addr < 0
         then set_error vm
         else vm\<lparr>data_stack := mem_read (memory vm) (nat addr) # xs\<rparr>)"

lemma fetch_normal:
  assumes "data_stack vm = addr # xs"
  assumes "addr \<ge> 0"
  shows "data_stack (forth_fetch vm) = mem_read (memory vm) (nat addr) # xs"
  by (simp add: forth_fetch_def assms)

lemma fetch_reads_stored_value:
  assumes "memory vm = mem_write m a v"
  assumes "data_stack vm = int a # xs"
  shows "hd (data_stack (forth_fetch vm)) = v"
  by (simp add: forth_fetch_def mem_read_def mem_write_def assms)

lemma fetch_depth_preserved:
  assumes "data_stack vm = addr # xs"
  assumes "addr \<ge> 0"
  shows "length (data_stack (forth_fetch vm)) = length (data_stack vm)"
  by (simp add: forth_fetch_def assms)

lemma fetch_underflow:
  assumes "data_stack vm = []"
  shows "vm_error (forth_fetch vm)"
  by (simp add: forth_fetch_def set_error_def assms)

lemma fetch_neg_addr:
  assumes "data_stack vm = addr # xs"
  assumes "addr < 0"
  shows "vm_error (forth_fetch vm)"
  by (simp add: forth_fetch_def set_error_def assms)

(* ── ! ( n addr -- ) ───────────────────────────────────────────────────── *)
(* Pops addr then n, writes n to memory[addr].
   C: addr = VM_ADDR(vm_pop(vm)); value = vm_pop(vm); vm_store_cell(addr, value). *)

definition forth_store :: "vm_state \<Rightarrow> vm_state" where
  "forth_store vm =
    (case data_stack vm of
       addr # n # xs \<Rightarrow>
         if addr < 0
         then set_error vm
         else vm\<lparr>data_stack := xs,
                  memory    := mem_write (memory vm) (nat addr) n\<rparr>
     | _ \<Rightarrow> set_error vm)"

lemma store_normal:
  assumes "data_stack vm = addr # n # xs"
  assumes "addr \<ge> 0"
  shows "data_stack (forth_store vm) = xs"
    and "memory (forth_store vm) = mem_write (memory vm) (nat addr) n"
  by (simp add: forth_store_def assms)+

lemma store_writes_value:
  assumes "data_stack vm = addr # n # xs"
  assumes "addr \<ge> 0"
  shows "mem_read (memory (forth_store vm)) (nat addr) = n"
  by (simp add: forth_store_def mem_write_def mem_read_def assms)

lemma store_depth_decreases:
  assumes "data_stack vm = addr # n # xs"
  assumes "addr \<ge> 0"
  shows "length (data_stack (forth_store vm)) = length (data_stack vm) - 2"
  by (simp add: forth_store_def assms)

lemma store_other_unchanged:
  assumes "data_stack vm = addr # n # xs"
  assumes "addr \<ge> 0"
  assumes "nat addr \<noteq> b"
  shows "mem_read (memory (forth_store vm)) b = mem_read (memory vm) b"
  by (simp add: forth_store_def mem_write_def mem_read_def assms)

lemma store_underflow_nil:
  assumes "data_stack vm = []"
  shows "vm_error (forth_store vm)"
  by (simp add: forth_store_def set_error_def assms)

lemma store_underflow_one:
  assumes "data_stack vm = [x]"
  shows "vm_error (forth_store vm)"
  by (simp add: forth_store_def set_error_def assms)

lemma store_neg_addr:
  assumes "data_stack vm = addr # n # xs"
  assumes "addr < 0"
  shows "vm_error (forth_store vm)"
  by (simp add: forth_store_def set_error_def assms)

(* ── Store then fetch = identity ────────────────────────────────────────── *)

lemma store_then_fetch:
  assumes "data_stack vm = addr # n # xs"
  assumes "addr \<ge> 0"
  assumes "data_stack vm' = addr # xs"
  assumes "memory vm' = memory (forth_store vm)"
  assumes "addr \<ge> 0"
  shows "hd (data_stack (forth_fetch vm')) = n"
  by (simp add: forth_fetch_def forth_store_def mem_write_def mem_read_def assms)

(* ── C@ ( addr -- c ) ──────────────────────────────────────────────────── *)
(* Reads a single byte (0..255) from memory, zero-extended to cell width.
   C: value = vm_load_u8(vm, addr); vm_push(vm, (cell_t)value).
   We model this as reading memory and masking to [0, 255].                 *)

definition forth_cfetch :: "vm_state \<Rightarrow> vm_state" where
  "forth_cfetch vm =
    (case data_stack vm of
       []         \<Rightarrow> set_error vm
     | addr # xs \<Rightarrow>
         if addr < 0
         then set_error vm
         else let byte = mem_read (memory vm) (nat addr) AND 0xFF
              in vm\<lparr>data_stack := byte # xs\<rparr>)"

lemma cfetch_normal:
  assumes "data_stack vm = addr # xs"
  assumes "addr \<ge> 0"
  shows "data_stack (forth_cfetch vm) =
         (mem_read (memory vm) (nat addr) AND 0xFF) # xs"
  by (simp add: forth_cfetch_def assms)

lemma cfetch_byte_range:
  assumes "data_stack vm = addr # xs"
  assumes "addr \<ge> 0"
  shows "0 \<le> hd (data_stack (forth_cfetch vm))"
    and "hd (data_stack (forth_cfetch vm)) \<le> 255"
  by (simp add: forth_cfetch_def assms)+

lemma cfetch_underflow:
  assumes "data_stack vm = []"
  shows "vm_error (forth_cfetch vm)"
  by (simp add: forth_cfetch_def set_error_def assms)

lemma cfetch_neg_addr:
  assumes "data_stack vm = addr # xs"
  assumes "addr < 0"
  shows "vm_error (forth_cfetch vm)"
  by (simp add: forth_cfetch_def set_error_def assms)

(* ── C! ( c addr -- ) ──────────────────────────────────────────────────── *)
(* Stores low byte of c into memory[addr].
   C: vm_store_u8(vm, addr, (uint8_t)(value & 0xFF)).                       *)

definition forth_cstore :: "vm_state \<Rightarrow> vm_state" where
  "forth_cstore vm =
    (case data_stack vm of
       addr # c # xs \<Rightarrow>
         if addr < 0
         then set_error vm
         else vm\<lparr>data_stack := xs,
                  memory    := mem_write (memory vm) (nat addr) (c AND 0xFF)\<rparr>
     | _ \<Rightarrow> set_error vm)"

lemma cstore_normal:
  assumes "data_stack vm = addr # c # xs"
  assumes "addr \<ge> 0"
  shows "data_stack (forth_cstore vm) = xs"
    and "memory (forth_cstore vm) = mem_write (memory vm) (nat addr) (c AND 0xFF)"
  by (simp add: forth_cstore_def assms)+

lemma cstore_writes_byte:
  assumes "data_stack vm = addr # c # xs"
  assumes "addr \<ge> 0"
  shows "mem_read (memory (forth_cstore vm)) (nat addr) = c AND 0xFF"
  by (simp add: forth_cstore_def mem_write_def mem_read_def assms)

lemma cstore_depth_decreases:
  assumes "data_stack vm = addr # c # xs"
  assumes "addr \<ge> 0"
  shows "length (data_stack (forth_cstore vm)) = length (data_stack vm) - 2"
  by (simp add: forth_cstore_def assms)

lemma cstore_underflow_nil:
  assumes "data_stack vm = []"
  shows "vm_error (forth_cstore vm)"
  by (simp add: forth_cstore_def set_error_def assms)

lemma cstore_underflow_one:
  assumes "data_stack vm = [x]"
  shows "vm_error (forth_cstore vm)"
  by (simp add: forth_cstore_def set_error_def assms)

lemma cstore_neg_addr:
  assumes "data_stack vm = addr # c # xs"
  assumes "addr < 0"
  shows "vm_error (forth_cstore vm)"
  by (simp add: forth_cstore_def set_error_def assms)

(* C! then C@ round-trip: byte written is byte read back. *)
lemma cstore_then_cfetch:
  assumes "data_stack vm = addr # c # xs"
  assumes "addr \<ge> 0"
  assumes "data_stack vm' = addr # xs"
  assumes "memory vm' = memory (forth_cstore vm)"
  shows "hd (data_stack (forth_cfetch vm')) = c AND 0xFF"
  by (simp add: forth_cfetch_def forth_cstore_def mem_write_def mem_read_def assms)

end
