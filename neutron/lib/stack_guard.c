/* ===========================================================================
 * Neutron Kernel — Stack Protector Support
 * ===========================================================================
 * Provides __stack_chk_guard and __stack_chk_fail for -fstack-protector-strong.
 * The guard value is a fixed canary — in a full implementation this would
 * be randomized at boot using hardware RNG.
 * =========================================================================== */

#include <neutron/types.h>
#include <neutron/console.h>

/* Stack canary value — deliberately non-zero with a null byte to catch
 * string-based overflows. A production kernel would randomize this. */
u64 __stack_chk_guard = 0x00000AFF0BCDULL;

void __stack_chk_fail(void)
{
    console_puts("\n*** NEUTRON KERNEL PANIC: STACK SMASHING DETECTED ***\n");
    console_puts("A stack buffer overflow was detected. Halting.\n");

    /* Halt — this is a fatal security violation */
    __asm__ volatile("msr daifset, #0xf");
    for (;;)
        __asm__ volatile("wfi");
}
