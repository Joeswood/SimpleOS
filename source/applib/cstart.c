/**
 * Process init - C
 *
 */
#include "lib_syscall.h"
#include <stdlib.h>

int main (int argc, char ** argv);

extern uint8_t __bss_start__[], __bss_end__[];

/**
 * @brief application init
 */
void cstart (int argc, char ** argv) {
    // Note: must clear bss section here (newlib dependence reason)
    uint8_t * start = __bss_start__;
    while (start < __bss_end__) {
        *start++ = 0;
    }

    exit(main(argc, argv));
}