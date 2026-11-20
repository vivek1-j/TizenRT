/****************************************************************************
 *
 * Copyright 2026 Samsung Electronics All Rights Reserved.
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

#include "arm_internal.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

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
 * Name: up_checkstackoverflow
 *
 * Description:
 *   This function checks stack overflow condition for a thread.
 *
 * Inputs:
 *   rtcb - Points to the TCB that is ready-to-run
 *
 ****************************************************************************/

/* NuttX 12.13.0 adaptation: the TCB no longer carries adj_stack_ptr,
 * and stack_base_ptr has inverted semantics - arm_createstack() now
 * stores the allocated (bottom) end of the stack there and colors the
 * region with STACK_COLOR.  The guard word is the first colored word,
 * STACKFRAME_ALIGN_UP(stack_base_ptr): arm_stack_color() and
 * arm_stack_check() both start from that aligned address, and on the
 * non-TLS allocation path (kmm_malloc) the raw base has no contractual
 * alignment, so reading it directly could test an uncolored word.
 * The old
 * adj_stack_ptr-based recompute is gone and so is the MEMORY
 * CORRUPTION branch, which cross-checked the two former sources.
 */

void up_checkstackoverflow(FAR struct tcb_s *rtcb)
{
	FAR uint32_t *guard;

	guard = (FAR uint32_t *)STACKFRAME_ALIGN_UP((uintptr_t)rtcb->stack_base_ptr);

	if (*guard != STACK_COLOR) {
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
