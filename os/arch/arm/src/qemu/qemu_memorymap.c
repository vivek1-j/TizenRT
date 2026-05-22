/****************************************************************************
 * arch/arm/src/qemu/qemu_memorymap.c
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

#include "mmu.h"

#include "hardware/qemu_memorymap.h"
#include "qemu_memorymap.h"

/****************************************************************************
 * Private Data
 ****************************************************************************/

#ifndef CONFIG_ARCH_ROMPGTABLE
/* This table describes how to map 1MB sections to the physical
 * address space of the QEMU virt platform.
 *
 * Memory Map:
 *   FLASH:  0x00000000 - 0x07FFFFFF  (128MB, cacheable, ROM)
 *   I/O:    0x08000000 - 0x0DFFFFFF  (96MB, non-cacheable, device)
 *   PCIe:   0x10000000 - 0x3FFFFFFF  (768MB, non-cacheable, device)
 *   SDRAM:  0x40000000 - 0x47FFFFFF  (128MB, cacheable, RAM)
 */

static const struct section_mapping_s g_section_mapping[] =
{
  /* FLASH region */
  {
    QEMU_FLASH_PSECTION,
    QEMU_FLASH_VSECTION,
    QEMU_FLASH_MMUFLAGS,
    QEMU_FLASH_NSECTIONS
  },
  /* I/O region (GIC, UART, RTC, virtio, etc.) */
  {
    QEMU_IO_PSECTION,
    QEMU_IO_VSECTION,
    QEMU_IO_MMUFLAGS,
    QEMU_IO_NSECTIONS
  },
  /* PCIe region */
  {
    QEMU_PCIE_PSECTION,
    QEMU_PCIE_VSECTION,
    QEMU_PCIE_MMUFLAGS,
    QEMU_PCIE_NSECTIONS
  },
  /* SDRAM/DRAM region */
  {
    QEMU_SDRAM_PSECTION,
    QEMU_SDRAM_VSECTION,
    QEMU_SDRAM_MMUFLAGS,
    QEMU_SDRAM_NSECTIONS
  },
  /* NOTE: No explicit high vector mapping needed.
   * ARM hardware handles high vectors (0xFFFF0000) automatically via
   * internal alias when SCTLR.V=1. See NuttX QEMU port for reference.
   */
};

/* The number of entries in the mapping table */

#define NMAPPINGS \
  (sizeof(g_section_mapping) / sizeof(struct section_mapping_s))

const size_t g_num_mappings = NMAPPINGS;

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int qemu_setupmappings(void)
{
  /* Map all memory regions. High vectors (0xFFFF0000) are handled
   * by ARM hardware alias when SCTLR.V=1, no explicit mapping needed.
   */
  mmu_l1_map_regions(g_section_mapping, g_num_mappings);
  return 0;
}

#endif /* CONFIG_ARCH_ROMPGTABLE */
