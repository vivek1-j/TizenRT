/****************************************************************************
 * arch/arm/src/qemu/qemu_boot.c
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <tinyara/config.h>

#include <stdint.h>
#include <assert.h>
#include <debug.h>
#include <string.h>

#include <arch/board/board.h>

#include "chip.h"
#include "arm.h"
#include "mmu.h"
#include "scu.h"
#include "up_internal.h"

#include "qemu_memorymap.h"
#include "qemu_irq.h"

#include "sctlr.h"

/* External function to zero .bss */
extern void arm_data_initialize(void);

/* Linker symbols for .data section (declared in up_internal.h) */
/* extern const uint32_t _eronly; */  /* Load address of .data in ROM */
/* extern uint32_t _sdata; */   /* Start of .data in RAM */
/* extern uint32_t _edata; */   /* End of .data in RAM */

/****************************************************************************
 * Name: qemu_data_initialize
 *
 * Description:
 *   Copy .data section from ROM to RAM for QEMU SDRAM boot.
 *   This is needed because QEMU loads the ELF but .data ends up uninitialized
 *   when booting from SDRAM.
 ****************************************************************************/

static void qemu_data_initialize(void)
{
    const uint32_t *src = &_eronly;
    uint32_t *dst = &_sdata;
    const uint32_t *end = &_edata;

    /* Copy .data section from ROM to RAM */
    while (dst < (uint32_t *)end) {
        *dst++ = *src++;
    }
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: arm_boot
 *
 * Description:
 *   Complete boot operations started in arm_head.S
 *
 ****************************************************************************/

void arm_boot(void)
{
#ifdef CONFIG_ARCH_PERF_EVENTS
  /* Perf init */

  up_perf_init(0);
#endif

  /* Copy .data section from ROM to RAM (QEMU-specific fix for SDRAM boot) */
  /* This MUST be done FIRST, before any code that uses initialized data */
  /* including qemu_setupmappings() which uses region descriptor arrays */

  qemu_data_initialize();

  /* Zero BSS section */

  arm_data_initialize();

  /* Set up MMU page table mappings (now uses initialized data) */

  qemu_setupmappings();

  /* Initialize the GIC */

  qemu_irq_initialize();

#ifdef CONFIG_SMP
  /* Enable SMP cache coherency for CPU0 */

  arm_enable_smp(0);
#endif

  /* Configure the FPU */

  arm_fpuconfig();

  /* Perform early serial initialization (REQUIRED for console) */
  /* Note: This must be called unconditionally to initialize UART hardware */

  up_earlyserialinit();

#ifdef CONFIG_SYSLOG_CONSOLE
  /* Register the serial console */

  up_putc(0);  /* Dummy output to kick-start the serial driver */
#endif

  /* Note: arm_serialinit() is called later via board_initialize() */
  /*       to ensure heap semaphore is ready before VFS registration */
}