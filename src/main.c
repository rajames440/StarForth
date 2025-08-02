#include <stdio.h>
#include "vm.h"
#include "word_registry.h"

int main(void) {
    VM vm;
    vm_init(&vm);
    register_forth79_words(&vm);
    vm_repl(&vm);
    return 0;
}
