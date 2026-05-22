/****************************************************************************
 * arch/arm/src/qemu/qemu_irq.c
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

#include <tinyara/irq.h>
#include <tinyara/arch.h>

#include "chip.h"
#include "up_internal.h"
#include "up_arch.h"
#include "gic.h"
#include "sctlr.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Size of the interrupt stack allocation */

#define INTSTACK_ALLOC (CONFIG_SMP_NCPUS * INTSTACK_SIZE)

/****************************************************************************
 * Public Data
 ****************************************************************************/

/* g_current_regs[] holds a reference to the current interrupt level
 * register storage structure.  It is non-NULL only during interrupt
 * processing.  Access to g_current_regs[] must be through the macro
 * CURRENT_REGS for portability.
 */

volatile uint32_t *g_current_regs[CONFIG_SMP_NCPUS];

#if defined(CONFIG_SMP) && CONFIG_ARCH_INTERRUPTSTACK > 7
/* In the SMP configuration, we will need custom IRQ and FIQ stacks.
 * These definitions provide the aligned stack allocations.
 */

static uint64_t g_irqstack_alloc[INTSTACK_ALLOC >> 3];
static uint64_t g_fiqstack_alloc[INTSTACK_ALLOC >> 3];

/* These are arrays that point to the top of each interrupt stack */

uintptr_t g_irqstack_top[CONFIG_SMP_NCPUS] =
{
  (uintptr_t)g_irqstack_alloc + INTSTACK_SIZE,
#if CONFIG_SMP_NCPUS > 1
  (uintptr_t)g_irqstack_alloc + (2 * INTSTACK_SIZE),
#endif
#if CONFIG_SMP_NCPUS > 2
  (uintptr_t)g_irqstack_alloc + (3 * INTSTACK_SIZE),
#endif
#if CONFIG_SMP_NCPUS > 3
  (uintptr_t)g_irqstack_alloc + (4 * INTSTACK_SIZE)
#endif
};

uintptr_t g_fiqstack_top[CONFIG_SMP_NCPUS] =
{
  (uintptr_t)g_fiqstack_alloc + INTSTACK_SIZE,
#if CONFIG_SMP_NCPUS > 1
  (uintptr_t)g_fiqstack_alloc + 2 * INTSTACK_SIZE,
#endif
#if CONFIG_SMP_NCPUS > 2
  (uintptr_t)g_fiqstack_alloc + 3 * INTSTACK_SIZE,
#endif
#if CONFIG_SMP_NCPUS > 3
  (uintptr_t)g_fiqstack_alloc + 4 * INTSTACK_SIZE
#endif
};
#endif

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: up_irqinitialize
 *
 * Description:
 *   This function is called by up_initialize() during the bring-up of the
 *   system.  It is the responsibility of this function to put the interrupt
 *   subsystem into the working and ready state.
 *
 ****************************************************************************/

void up_irqinitialize(void)
{
#ifdef CONFIG_ARCH_LOWVECTORS
  /* Low vectors: Set VBAR to vector table address.
   * The vector table is linked at the start of DRAM.
   */
  extern uint32_t _vector_start[];
  cp15_wrvbar((uint32_t)_vector_start);
#else
  /* High vectors (default): Hardware maps vectors to 0xFFFF0000.
   * No VBAR setup needed - the ARM processor automatically uses
   * the fixed high vector address when SCTLR.V=1.
   */
#endif

  /* The following operations need to be atomic, but since this function is
   * called early in the initialization sequence, we expect to have exclusive
   * access to the GIC.
   */

  /* Initialize the Generic Interrupt Controller (GIC) for CPU0.
   * In SMP mode, we want arm_gic0_initialize to be called only once.
   */

  if (sched_getcpu() == 0)
    {
      arm_gic0_initialize();  /* Initialization unique to CPU0 */
    }

  arm_gic_initialize();   /* Initialization common to all CPUs */

#ifndef CONFIG_SUPPRESS_INTERRUPTS
  /* And finally, enable interrupts */

  //arm_color_intstack();
  up_irq_enable();
#endif
}

/****************************************************************************
 * Name: up_get_intstackbase
 *
 * Description:
 *   Return a pointer to the "alloc" the correct interrupt stack allocation
 *   for the current CPU.
 *
 ****************************************************************************/

#if defined(CONFIG_SMP) && CONFIG_ARCH_INTERRUPTSTACK > 7
uintptr_t up_get_intstackbase(int cpu)
{
  return g_irqstack_top[cpu] - INTSTACK_SIZE;
}

/****************************************************************************
 * Name: arm_intstack_top
 *
 * Description:
 *   Return a pointer to the top the correct interrupt stack allocation
 *   for the current CPU.
 *
 ****************************************************************************/

uintptr_t arm_intstack_top(void)
{
  return g_irqstack_top[up_cpu_index()];
}

/****************************************************************************
 * Name: arm_intstack_alloc
 *
 * Description:
 *   Return a pointer to the "alloc" the correct interrupt stack allocation
 *   for the current CPU.
 *
 ****************************************************************************/

uintptr_t arm_intstack_alloc(void)
{
  return g_irqstack_top[up_cpu_index()] - INTSTACK_SIZE;
}
#endif

/****************************************************************************
 * Name: qemu_irq_initialize
 *
 * Description:
 *   Initialize the IRQ subsystem for the QEMU virt platform.
 *
 ****************************************************************************/

void qemu_irq_initialize(void)
{
  /* Initialize the GIC distributor and CPU interfaces */

  arm_gic_initialize();

  /* Set interrupt priorities for the GIC */

  up_prioritize_irq(IRQ_UART1, 0x7f);
  up_prioritize_irq(IRQ_CNTP, 0x40);

#ifndef CONFIG_SUPPRESS_INTERRUPTS
  /* Enable interrupt sources at the GIC distributor */

  up_enable_irq(IRQ_UART1);
  up_enable_irq(IRQ_CNTP);
#endif
}

/****************************************************************************
 * Name: up_cpu_up
 *
 * Description:
 *   Boot a secondary CPU for SMP operation.
 *   For QEMU virt platform, this is a stub as QEMU handles CPU boot.
 *
 ****************************************************************************/

int up_cpu_up(int cpu)
{
  /* QEMU virt platform handles secondary CPU boot through PSCI emulation.
   * The actual CPU startup is handled by the bootloader and QEMU.
   * Return success to allow SMP initialization to proceed.
   */
  return 0;
}

/****************************************************************************
 * Name: up_get_console_dev
 *
 * Description:
 *   Return the console device for early output.
 *   This is a stub for QEMU platform.
 *
 ****************************************************************************/

void *up_get_console_dev(void)
{
  /* Return NULL - QEMU uses polled UART output via up_putc */
  return NULL;
}

/****************************************************************************
 * Name: arm_cpu_boot
 *
 * Description:
 *   Secondary CPU boot entry point for QEMU virt platform.
 *   QEMU handles secondary CPU boot through PSCI emulation.
 *   This is a stub to satisfy the linker.
 *
 ****************************************************************************/

void arm_cpu_boot(void)
{
  /* QEMU virt platform handles secondary CPU boot through PSCI.
   * The actual CPU startup is handled by QEMU's PSCI emulation.
   * This stub is called but does nothing - the CPU continues
   * execution from the standard boot sequence.
   */
}