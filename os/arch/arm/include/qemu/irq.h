/****************************************************************************
 * arch/arm/include/qemu/irq.h
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

/* This file should never be included directly but, rather,
 * only indirectly through tinyara/irq.h
 */

#ifndef __ARCH_ARM_INCLUDE_QEMU_IRQ_H
#define __ARCH_ARM_INCLUDE_QEMU_IRQ_H

/****************************************************************************
 * Pre-processor Prototypes
 ****************************************************************************/

/* QEMU virt platform IRQ definitions.
 *
 * GIC IRQ layout (ARM Generic Interrupt Controller v2):
 *   0-15:  SGIs (Software Generated Interrupts)
 *   16-31: PPIs (Private Peripheral Interrupts)
 *   32+:   SPIs (Shared Peripheral Interrupts)
 *
 * IRQ number = GIC interrupt number
 */

/* Private Peripheral Interrupts (PPI) per CPU */

#define QEMU_IRQ_PPI              (16)
#define QEMU_IRQ_CNTP             (QEMU_IRQ_PPI + 14)   /* IRQ 30 - Physical Timer */

/* Shared Peripheral Interrupts (SPI) */

#define QEMU_IRQ_SPI              (32)
#define QEMU_IRQ_UART1            (QEMU_IRQ_SPI + 1)    /* IRQ 33 - PL011 UART */

#define NR_IRQS                   (160)                  /* Number of supported IRQs */

/****************************************************************************
 * Public Types
 ****************************************************************************/

/****************************************************************************
 * Inline functions
 ****************************************************************************/

/****************************************************************************
 * Public Data
 ****************************************************************************/

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

#ifndef __ASSEMBLY__
#ifdef __cplusplus
#define EXTERN extern "C"
extern "C"
{
#else
#define EXTERN extern
#endif

#undef EXTERN
#ifdef __cplusplus
}
#endif
#endif

#endif /* __ARCH_ARM_INCLUDE_QEMU_IRQ_H */