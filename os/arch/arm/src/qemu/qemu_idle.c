/****************************************************************************
 * arch/arm/src/qemu/qemu_idle.c
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

#include "chip.h"
#include "up_internal.h"

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: up_idle
 *
 * Description:
 *   The idle thread is the lowest priority task and runs when no other
 *   task is ready to run.  This function is called from the idle task.
 *
 ****************************************************************************/

void up_idle(void)
{
  /* The WFI (Wait For Interrupt) instruction puts the CPU in low power
   * state.  This is the proper thing to do in a real system, but on QEMU
   * it doesn't hurt to spin.
   */

  __asm__ __volatile__("wfi" : : : "memory");
}