/****************************************************************************
 * arch/arm/src/qemu/psci.c
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to you under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
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
#include <errno.h>

#ifdef CONFIG_SMP

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: arm_psci_cpu_on
 *
 * Description:
 *   Turn on a secondary CPU. QEMU handles this through its internal
 *   PSCI emulation, so we just return success.
 *
 ****************************************************************************/

int arm_psci_cpu_on(uint32_t target_cpu, uint32_t entry_point)
{
  /* QEMU virt platform handles CPU boot through internal PSCI emulation.
   * We return success here as the actual CPU coordination is handled by
   * the bootloader and QEMU.
   */
  return 0;
}

/****************************************************************************
 * Name: arm_psci_cpu_off
 *
 * Description:
 *   Turn off the current CPU. QEMU handles this through its internal
 *   PSCI emulation.
 *
 ****************************************************************************/

int arm_psci_cpu_off(void)
{
  /* QEMU virt platform handles CPU power down through internal PSCI.
   * We return success here as the actual power management is handled
   * by QEMU.
   */
  return 0;
}

#endif /* CONFIG_SMP */
