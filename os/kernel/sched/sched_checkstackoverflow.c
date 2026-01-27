/****************************************************************************
 *
 * Copyright 2025 Samsung Electronics All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 * either express or implied. See the License for the specific
 * language governing permissions and limitations under the License.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <tinyara/config.h>
#include <tinyara/arch.h>
#include <tinyara/sched.h>
#include <tinyara/irq.h>

#include <assert.h>
#include <debug.h>
#include <stdint.h>

#include "sched/sched.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/
/* For use with EABI and floating point, the stack must be aligned to 8-byte
 * addresses.
 */

#ifdef __ARM_EABI__
#define STACK_ALIGNMENT     8
#else
#define STACK_ALIGNMENT     4
#endif

/* Stack alignment macros */

#define STACK_ALIGN_MASK    (STACK_ALIGNMENT - 1)
#define STACK_ALIGN_DOWN(a) ((a) & ~STACK_ALIGN_MASK)
#define STACK_ALIGN_UP(a)   (((a) + STACK_ALIGN_MASK) & ~STACK_ALIGN_MASK)

/****************************************************************************
 * Private Type Declarations
 ****************************************************************************/

/****************************************************************************
 * Global Variables
 ****************************************************************************/

/****************************************************************************
 * Private Variables
 ****************************************************************************/

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: sched_checkstackoverflow
 *
 * Description:
 *   This function checks stack overflow condition for a thread.
 *
 * Inputs:
 *   rtcb - Points to the TCB that is ready-to-run
 *
 ****************************************************************************/

void sched_checkstackoverflow(FAR struct tcb_s *rtcb)
{
	if (*(uint32_t *)(rtcb->stack_base_ptr) != STACK_COLOR) {

		irqstate_t flags = enter_critical_section();

		lldbg_noarg("\n###############    STACK OVERFLOW at pid %d ", rtcb->pid);
#if CONFIG_TASK_NAME_SIZE > 0
		lldbg_noarg("(%s) ", rtcb->name);
#endif
		lldbg_noarg("###################\n");
		PANIC();

		leave_critical_section(flags);
	}
}

void sched_checkintstackoverflow()
{
    uint32_t istackbase;
#if CONFIG_ARCH_INTERRUPTSTACK > 3
    istackbase = 0;
#else
    istackbase = 0xFFFFFFFF;
#endif

#if CONFIG_ARCH_INTERRUPTSTACK > 7
#ifdef CONFIG_SMP
    /* Initialize istackbase based on the interrupt stack size and proper alignment value (~7) */
    istackbase = ((uint32_t)arm_intstack_alloc());
#else
    istackbase = (uint32_t)&g_intstackalloc;
#endif
	istackbase = STACK_ALIGN_UP(istackbase);
 
	if (*(uint32_t *)(istackbase) != INTSTACK_COLOR) {
		irqstate_t flags = enter_critical_section();

        lldbg_noarg("\n###############    INTERRUPT STACK OVERFLOW ");
        lldbg_noarg("###################\n");
        PANIC();

        leave_critical_section(flags);

	}
#endif
}
