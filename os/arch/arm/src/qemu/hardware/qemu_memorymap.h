/****************************************************************************
 *
 * Copyright 2024 Samsung Electronics All Rights Reserved.
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
/************************************************************************************
 * arch/arm/src/qemu/hardware/qemu_memorymap.h
 *
 *   Copyright (C) 2014 Gregory Nutt. All rights reserved.
 *   Author: Gregory Nutt <gnutt@nuttx.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name NuttX nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ************************************************************************************/

#ifndef __ARCH_ARM_SRC_QEMU_HARDWARE_QEMU_MEMORYMAP_H
#define __ARCH_ARM_SRC_QEMU_HARDWARE_QEMU_MEMORYMAP_H

/************************************************************************************
 * Included Files
 ************************************************************************************/

#include <tinyara/config.h>

/************************************************************************************
 * Pre-processor Definitions
 ************************************************************************************/

/* QEMU virt Memory Map - IDENTITY MAPPING (matching NuttX approach)
 * Reference: https://www.qemu.org/docs/master/system/arm/virt.html
 *
 * QEMU -kernel loads ELF to RAM, ignoring FLASH addresses in linker script.
 * We use identity mapping (virtual = physical) like NuttX:
 *
 *  Virtual          Physical        Size            Description
 *  ------           ------          -------         -----------
 *  0x0000_0000      0x0000_0000     128MB           FLASH (QEMU loads here)
 *  0x0800_0000      0x0800_0000     96MB            I/O (GIC, UART, etc.)
 *  0x1000_0000      0x1000_0000     768MB           PCIe
 *  0x4000_0000      0x4000_0000     256MB           SDRAM (QEMU default)
 */

/* Legacy definitions for GIC */
#define QEMU_GIC_DIST_BASE  0x08000000
#define QEMU_GIC_CPU_BASE   0x08010000

/* UART base addresses */
#define QEMU_UART1_BASE     0x09000000
#define QEMU_UART2_BASE     0x09010000

/* Fix VSIZE macro */
#define QEMU_SDRAM_VSIZE    QEMU_SDRAM_SECSIZE

/* Physical section base addresses (where QEMU puts things) */
#define QEMU_FLASH_PSECTION      0x00000000  /* QEMU loads code/vectors here */
#define QEMU_IO_PSECTION         0x08000000  /* 0x08000000-0x0DFFFFFF */
#define QEMU_PCIE_PSECTION       0x10000000  /* 0x10000000-0x3FFFFFFF */
#define QEMU_SDRAM_PSECTION      0x40000000  /* 0x40000000-0x4FFFFFFF */

/* Virtual section base addresses - IDENTITY MAPPING (virtual = physical)
 * This matches NuttX approach - QEMU loads to physical addresses,
 * and MMU maps virtual to same physical addresses.
 */
#define QEMU_FLASH_VSECTION      QEMU_FLASH_PSECTION     /* 1:1 mapping */
#define QEMU_IO_VSECTION         QEMU_IO_PSECTION        /* 1:1 mapping */
#define QEMU_PCIE_VSECTION       QEMU_PCIE_PSECTION      /* 1:1 mapping */
#define QEMU_SDRAM_VSECTION      QEMU_SDRAM_PSECTION     /* 1:1 mapping */

/* Sizes of memory regions in bytes */
#define QEMU_FLASH_SECSIZE       (128*1024*1024)   /* 128MB */
#define QEMU_IO_SECSIZE          (96*1024*1024)    /* 96MB - covers GIC, UART, RTC, virtio */
#define QEMU_PCIE_SECSIZE        (768*1024*1024)   /* 768MB */
#define QEMU_SDRAM_SECSIZE       (256*1024*1024)   /* 256MB */

/* Section count macro - rounds up to nearest 1MB section */
#define _NSECTIONS(b)            (((b) + 0x000fffff) >> 20)

/* Number of 1MB sections for each region */
#define QEMU_FLASH_NSECTIONS     _NSECTIONS(QEMU_FLASH_SECSIZE)
#define QEMU_IO_NSECTIONS        _NSECTIONS(QEMU_IO_SECSIZE)
#define QEMU_PCIE_NSECTIONS      _NSECTIONS(QEMU_PCIE_SECSIZE)
#define QEMU_SDRAM_NSECTIONS     _NSECTIONS(QEMU_SDRAM_SECSIZE)

/* MMU Flags for each region */
#define QEMU_FLASH_MMUFLAGS      MMU_ROMFLAGS      /* Read-only, cacheable */
#define QEMU_IO_MMUFLAGS         MMU_IOFLAGS       /* Non-cacheable, device */
#define QEMU_PCIE_MMUFLAGS       MMU_IOFLAGS       /* Non-cacheable, device */
#define QEMU_SDRAM_MMUFLAGS      MMU_MEMFLAGS      /* Read/write, cacheable */

/* Legacy aliases for backward compatibility */
#define QEMU_DEVICE_SECTION      QEMU_IO_PSECTION
#define QEMU_DEVICE_SIZE         QEMU_IO_SECSIZE
#define QEMU_DEVICE_PSECTION     QEMU_IO_PSECTION
#define QEMU_DEVICE_VSECTION     QEMU_IO_VSECTION
#define QEMU_DEVICE_NSECTIONS    QEMU_IO_NSECTIONS
#define QEMU_DEVICE_MMUFLAGS     QEMU_IO_MMUFLAGS

#define QEMU_DRAM_PSECTION       QEMU_SDRAM_PSECTION
#define QEMU_DRAM_VSECTION       QEMU_SDRAM_VSECTION
#define QEMU_DRAM_NSECTIONS      QEMU_SDRAM_NSECTIONS
#define QEMU_DRAM_MMUFLAGS       QEMU_SDRAM_MMUFLAGS

/* Legacy SDRAM definitions */
#define QEMU_SDRAM_START         QEMU_SDRAM_PSECTION
#define QEMU_SDRAM_SIZE          QEMU_SDRAM_SECSIZE
#define QEMU_SDRAM_END           (QEMU_SDRAM_START + QEMU_SDRAM_SIZE)
#define QEMU_SDRAM_VSTART        QEMU_SDRAM_VSECTION

/* NOTE: Using identity mapping - vectors at virtual 0x00000000
 * With CONFIG_ARCH_LOWVECTORS=y, CPU fetches vectors from 0x00000000
 * MMU identity maps this to physical 0x00000000 where QEMU loads code
 */

/************************************************************************************
 * Text Section and Page Table Base Address Definitions
 *
 * These definitions follow the pattern from other ARMv7-A platforms (imx6, amebasmart)
 * and are required by arm_head.S and arm_cpuhead.S for MMU initialization.
 ************************************************************************************/

/* Text section mapping for MMU - FLASH region (where QEMU loads code)
 * Using identity mapping: virtual = physical = 0x00000000
 */
#define TIZENRT_TEXT_PADDR       (QEMU_FLASH_PSECTION & 0xfff00000)
#define TIZENRT_TEXT_VADDR       (QEMU_FLASH_VSECTION & 0xfff00000)
#define TIZENRT_TEXT_SIZE        (QEMU_FLASH_SECSIZE)

/* Page table size - 16KB total for L1 and L2 page tables */
#define PGTABLE_SIZE             0x00004000
#define ALL_PGTABLE_SIZE         PGTABLE_SIZE

/* Page table base addresses - place at end of DRAM (following imx6/amebasmart pattern) */
#define PGTABLE_BASE_PADDR       (QEMU_SDRAM_START + QEMU_SDRAM_SIZE - ALL_PGTABLE_SIZE)
#define PGTABLE_BASE_VADDR       (QEMU_SDRAM_VSTART + QEMU_SDRAM_SECSIZE - ALL_PGTABLE_SIZE)

/* Number of 1MB sections for text (used by arm_head.S) */
#define _textlen                 ((TIZENRT_TEXT_SIZE + 0x000fffff) >> 20)

/* IDLE stack base address for CONFIG_BOOT_SDRAM_DATA
 * Place stack at beginning of SDRAM region (after FLASH/code)
 * This allows heap to grow from end of stack to top of RAM
 * Stack base is at 1MB into SDRAM to leave room for code/data
 */
#define IDLE_STACK_VBASE         (QEMU_SDRAM_START + 0x00100000)

/* IDLE_STACK_TOP - Top of idle thread stack (used by arm_head.S for QEMU)
 * This follows NuttX pattern - simple macro without conditionals
 * Stack grows downward from this address
 */
#define IDLE_STACK_TOP           (IDLE_STACK_VBASE + CONFIG_IDLETHREAD_STACKSIZE)

/* QEMU_RAM_USABLE_END - End of usable RAM (excluding page table)
 * Following NuttX approach: heap ends before page table at top of RAM
 * Note: Cannot use CONFIG_RAM_END as it's defined in config.h
 */
#define QEMU_RAM_USABLE_END      (QEMU_SDRAM_START + QEMU_SDRAM_SIZE - ALL_PGTABLE_SIZE)

#endif /* __ARCH_ARM_SRC_QEMU_HARDWARE_QEMU_MEMORYMAP_H */
