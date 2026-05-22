/****************************************************************************
 * arch/arm/src/qemu/qemu_allocateheap.c
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied.  See the License for the specific language
 * governing permissions and limitations under the License.
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

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Following NuttX approach for QEMU:
 * - Heap starts after idle thread stack (g_idle_topstack)
 * - Heap ends at CONFIG_RAM_END (RAM end minus page table)
 *
 * Memory Layout (RAM at 0x40000000):
 *   0x40000000: .text, .data, .bss
 *   _ebss: End of BSS
 *   IDLE stack
 *   g_idle_topstack: End of idle stack
 *   BOOT_STACK_RESERVE: Reserved space for boot stack (32KB)
 *   HEAP START (after boot stack reserve)
 *   HEAP (grows upward)
 *   CONFIG_RAM_END: HEAP END (before page table)
 *   Page table at top of RAM
 *
 * NOTE: Boot stack reserve is added to prevent boot stack from corrupting
 * heap metadata during early boot before idle task takes over.
 */

/* Boot stack reserve size - prevents boot stack from corrupting heap metadata */
#define BOOT_STACK_RESERVE  (32 * 1024)  /* 32KB */

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: up_allocate_heap
 *
 * Description:
 *   This function will be called to dynamically set aside the heap region.
 *
 ****************************************************************************/

void up_allocate_heap(FAR void **heap_start, size_t *heap_size)
{
  /* Heap starts after idle thread stack plus boot stack reserve
   * This prevents boot stack from corrupting heap metadata during early boot
   */
  *heap_start = (FAR void *)(g_idle_topstack + BOOT_STACK_RESERVE);
  
  /* Heap size is from stack top to end of usable RAM (excluding page table) */
  *heap_size = (size_t)(QEMU_RAM_USABLE_END - ((uint32_t)g_idle_topstack + BOOT_STACK_RESERVE));
}

/****************************************************************************
 * Name: up_allocate_kheap
 *
 * Description:
 *   QEMU-specific kernel heap allocation.
 *   Heap starts after idle stack and extends to end of usable RAM.
 *   This is the function called by os_start.c for CONFIG_MM_KERNEL_HEAP.
 *
 ****************************************************************************/

#ifdef CONFIG_MM_KERNEL_HEAP
void up_allocate_kheap(FAR void **heap_start, size_t *heap_size)
{
  /* QEMU RAM configuration:
   * RAM: 0x40000000 - 0x4FFFFFFF (256MB)
   * Stack: g_idle_topstack (set in arm_head.S, typically 0x40100800)
   * Page table at top of RAM (last 16KB)
   *
   * Memory layout:
   *   0x40000000: .text, .data, .bss
   *   g_idle_topstack: End of idle stack
   *   BOOT_STACK_RESERVE: Reserved space for boot stack (32KB)
   *   HEAP START (after boot stack reserve)
   *   HEAP (grows upward)
   *   QEMU_RAM_USABLE_END: HEAP END (before page table)
   */
  
  /* Heap starts after idle thread stack plus boot stack reserve
   * This prevents boot stack from corrupting heap metadata during early boot
   */
  *heap_start = (FAR void *)(g_idle_topstack + BOOT_STACK_RESERVE);
  
  /* Heap size is from stack top to end of usable RAM (excluding page table) */
  *heap_size = (size_t)(QEMU_RAM_USABLE_END - ((uint32_t)g_idle_topstack + BOOT_STACK_RESERVE));
}
#endif
