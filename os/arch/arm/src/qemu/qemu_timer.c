/****************************************************************************
 * arch/arm/src/qemu/qemu_timer.c
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

#include <tinyara/arch.h>

#include "chip.h"
#include "up_internal.h"
#include "up_arch.h"

#include "cp15.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* ARM Generic Timer:
 *   Counter Frequency Register (CNTFRQ)
 *   Physical Timer Control Register (CNTP_CTL)
 *   Physical Timer Value Register (CNTP_TVAL)
 *   Physical Timer Interrupt Status (CNTP_CTL_ISTATUS)
 *   Physical Timer Interrupt (PPI 14 -> IRQ 30)
 *
 * Register addresses for the system coprocessor:
 *   CNTFRQ:   p15, 0, r0, c14, c0, 0
 *   CNTP_CTL: p15, 0, r0, c14, c2, 1
 *   CNTP_TVAL:p15, 0, r0, c14, c2, 0
 */

#define CNTFRQ          "p15, 0, %0, c14, c0, 0"
#define CNTP_CTL        "p15, 0, %0, c14, c2, 1"
#define CNTP_TVAL       "p15, 0, %0, c14, c2, 0"

#define read_sysreg(reg) \
  ({ uint32_t _val; __asm__ __volatile__("mrc " reg : "=r"(_val)); _val; })

#define write_sysreg(reg, val) \
  __asm__ __volatile__("mcr " reg : : "r"(val))

#define CNTP_CTL_ENABLE     (1 << 0)
#define CNTP_CTL_IMASK      (1 << 1)
#define CNTP_CTL_ISTATUS    (1 << 2)

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: qemu_timer_isr
 *
 * Description:
 *   Timer interrupt handler for the ARM Generic Timer.
 *
 ****************************************************************************/

static int qemu_timer_isr(int irq, void *context, void *arg)
{
  /* Clear the timer interrupt by writing to TVAL */

  uint32_t tval = (uint32_t)(CONFIG_USEC_PER_TICK * 24);  /* 24MHz timer */
  write_sysreg(CNTP_TVAL, tval);

  /* Process the system tick */

  sched_process_timer();

  return 0;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: up_timer_initialize
 *
 * Description:
 *   Initialize the timer interrupt.
 *
 ****************************************************************************/

void up_timer_initialize(void)
{
  uint32_t freq;
  uint32_t tval;

  /* Set the counter frequency to 24MHz (QEMU virt default) */

  freq = 24000000;
  write_sysreg(CNTFRQ, freq);

  /* Set the timer interval for one system tick */

  tval = (uint32_t)((uint64_t)freq * CONFIG_USEC_PER_TICK / 1000000);
  write_sysreg(CNTP_TVAL, tval);

  /* Attach the timer interrupt handler */

  (void)irq_attach(IRQ_CNTP, qemu_timer_isr, NULL);

  /* Enable the timer, mask interrupt until attached */

  write_sysreg(CNTP_CTL, CNTP_CTL_ENABLE);
}