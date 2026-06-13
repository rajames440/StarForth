#pragma once
#include "../../include/vm.h"

/* Register all ACL C primitive words into the VM dictionary */
void register_acl_words(VM *vm);

/* Direct C entry point for birth inheritance (no stack manipulation needed) */
void acl_inherit_entry(DictEntry *src, DictEntry *dst);
