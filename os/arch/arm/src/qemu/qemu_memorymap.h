/****************************************************************************
 * arch/arm/src/qemu/qemu_memorymap.h
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

#ifndef __ARCH_ARM_SRC_QEMU_QEMU_MEMORYMAP_H
#define __ARCH_ARM_SRC_QEMU_QEMU_MEMORYMAP_H

/************************************************************************************
 * Included Files
 ************************************************************************************/

#include <tinyara/config.h>
#include "hardware/qemu_memorymap.h"

/************************************************************************************
 * Public Functions
 ************************************************************************************/

/************************************************************************************
 * Name: qemu_setupmappings
 *
 * Description:
 *   Initialize the MMU page table and set up section mappings for the QEMU
 *   virt platform.
 *
 ************************************************************************************/

int qemu_setupmappings(void);

#endif /* __ARCH_ARM_SRC_QEMU_QEMU_MEMORYMAP_H */
