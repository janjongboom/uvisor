/*
 * Copyright (c) 2013-2016, ARM Limited, All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef __VMPU_H__
#define __VMPU_H__

#include "vmpu_unpriv_access.h"
#include "api/inc/vmpu_exports.h"
#include <stdint.h>

/* Check if the address is in public/physical flash/SRAM. */
/* Note: Instead of using the '<' check on
 *          __uvisor_config.{flash, sram}_end
 *       we use the '<=' check on
 *          __uvisor_config.{flash, sram}_end - 4
 *       because unaligned accesses at a physical memory boundary have undefined
 *       behavior and must be avoided. */

/* Public flash
 * This portion of memory includes the physical flash up to where the private
 * boxes configuration table starts. */
/* Note: At the moment we assume uVisor RO code and data must be in the same
 *       memory as the rest of the OS/app. */
static UVISOR_FORCEINLINE int vmpu_public_flash_addr(uint32_t addr)
{
    return ((addr >= (uint32_t) __uvisor_config.flash_start) &&
            (addr <= ((uint32_t) __uvisor_config.secure_start - 4)));
}

/* Physical flash */
static UVISOR_FORCEINLINE int vmpu_flash_addr(uint32_t addr)
{
    return ((addr >= (uint32_t) __uvisor_config.flash_start) &&
            (addr <= ((uint32_t) __uvisor_config.flash_end - 4)));
}

/* Public SRAM
 * This portion of memory includes the physical SRAM where the public box
 * memories and the page heap section are. */
/* Note: This check is consistent both if uVisor shares the SRAM with the
 *       OS/app and if uVisor uses a separate memory (e.g. a TCM). */
static UVISOR_FORCEINLINE int vmpu_public_sram_addr(uint32_t addr)
{
    return ((addr >= (uint32_t) __uvisor_config.public_sram_start) &&
            (addr <= ((uint32_t) __uvisor_config.public_sram_end - 4)));
}

/* Physical SRAM */
static UVISOR_FORCEINLINE int vmpu_sram_addr(uint32_t addr)
{
    return ((addr >= (uint32_t) __uvisor_config.sram_start) &&
            (addr <= ((uint32_t) __uvisor_config.sram_end - 4)));
}

#define VMPU_REGION_SIZE(p1, p2) ((p1 >= p2) ? 0 : \
                                             ((uint32_t) (p2) - (uint32_t) (p1)))

#define VMPU_SCB_BFSR  (*((volatile uint8_t *) &SCB->CFSR + 1))
#define VMPU_SCB_MMFSR (*((volatile uint8_t *) &SCB->CFSR))

/* opcode encoding for ldr/str instructions
 *
 * for str instructions we expect the following:
 *   Operand | Purpose | Symbol | Value
 *   ----------------------------------
 *   imm5    | offset  | none   | 0x0
 *   Rn      | addr    | r0     | 0x0
 *   Rt      | src     | r1     | 0x1
 *
 * for ldr instructions we expect the following:
 *   Operand | Purpose | Symbol | Value
 *   ----------------------------------
 *   imm5    | offset  | none   | 0x0
 *   Rn      | addr    | r0     | 0x0
 *   Rt      | dst     | r0     | 0x0
 *
 *   |-------------|---------------------------------------------|--------|
 *   |             | Opcode base    | imm5       | Rn    | Rt    |        |
 *   | instruction |---------------------------------------------| Opcode |
 *   |             | 15 14 13 12 11 | 10 9 8 7 6 | 5 4 3 | 2 1 0 |        |
 *   |-------------|---------------------------------------------|--------|
 *   | str         |  0  1  1  0  0 |  0 0 0 0 0 | 0 0 0 | 0 0 1 | 0x6001 |
 *   | strh        |  1  0  0  0  0 |  0 0 0 0 0 | 0 0 0 | 0 0 1 | 0x8001 |
 *   | strb        |  0  1  1  1  0 |  0 0 0 0 0 | 0 0 0 | 0 0 1 | 0x7001 |
 *   | ldr         |  0  1  1  0  1 |  0 0 0 0 0 | 0 0 0 | 0 0 0 | 0x6800 |
 *   | ldrh        |  1  0  0  0  1 |  0 0 0 0 0 | 0 0 0 | 0 0 0 | 0x8800 |
 *   | ldrb        |  0  1  1  1  1 |  0 0 0 0 0 | 0 0 0 | 0 0 0 | 0x7800 |
 *   |-------------|---------------------------------------------|--------|
 *
 */
#define VMPU_OPCODE16_LOWER_R0_R1_MASK  0x01
#define VMPU_OPCODE16_LOWER_R0_R0_MASK  0x00
#define VMPU_OPCODE16_UPPER_STR_MASK    0x60
#define VMPU_OPCODE16_UPPER_STRH_MASK   0x80
#define VMPU_OPCODE16_UPPER_STRB_MASK   0x70
#define VMPU_OPCODE16_UPPER_LDR_MASK    0x68
#define VMPU_OPCODE16_UPPER_LDRH_MASK   0x88
#define VMPU_OPCODE16_UPPER_LDRB_MASK   0x78

/* bit-banding regions boundaries */
#define VMPU_SRAM_START           0x20000000
#define VMPU_SRAM_BITBAND_START   0x22000000
#define VMPU_SRAM_BITBAND_END     0x23FFFFFF
#define VMPU_PERIPH_START         0x40000000
#define VMPU_PERIPH_MASK          0xFFF00000
#define VMPU_PERIPH_BITBAND_START 0x42000000
#define VMPU_PERIPH_BITBAND_END   0x43FFFFFF
#define VMPU_PERIPH_BITBAND_MASK  0xFE000000
#define VMPU_PERIPH_FULL_MASK     0xFC000000

/* ROM Table region boundaries */
#define VMPU_ROMTABLE_START 0xE00FF000UL
#define VMPU_ROMTABLE_MASK  0xFFFFF000UL

/* bit-banding aliases macros
 * physical address ---> bit-banded alias
 * bit-banded alias ---> physical address
 * bit-banded alias ---> specific bit accessed at physical address */
#define VMPU_SRAM_BITBAND_ADDR_TO_ALIAS(addr, bit)   (VMPU_SRAM_BITBAND_START + 32U * (((uint32_t) (addr)) - \
                                                     VMPU_SRAM_START) + 4U * ((uint32_t) (bit)))
#define VMPU_SRAM_BITBAND_ALIAS_TO_ADDR(alias)       ((((((uint32_t) (alias)) - VMPU_SRAM_BITBAND_START) >> 5) & \
                                                     ~0x3U) + VMPU_SRAM_START)
#define VMPU_SRAM_BITBAND_ALIAS_TO_BIT(alias)        (((((uint32_t) (alias)) - VMPU_SRAM_BITBAND_START) - \
                                                     ((VMPU_SRAM_BITBAND_ALIAS_TO_ADDR(alias) - \
                                                     VMPU_SRAM_START) << 5)) >> 2)
#define VMPU_PERIPH_BITBAND_ADDR_TO_ALIAS(addr, bit) (VMPU_PERIPH_BITBAND_START + 32U * (((uint32_t) (addr)) - \
                                                     VMPU_PERIPH_START) + 4U * ((uint32_t) (bit)))
#define VMPU_PERIPH_BITBAND_ALIAS_TO_ADDR(alias)     ((((((uint32_t) (alias)) - VMPU_PERIPH_BITBAND_START) >> 5) & \
                                                     ~0x3U) + VMPU_PERIPH_START)
#define VMPU_PERIPH_BITBAND_ALIAS_TO_BIT(alias)      (((((uint32_t) (alias)) - VMPU_PERIPH_BITBAND_START) - \
                                                     ((VMPU_PERIPH_BITBAND_ALIAS_TO_ADDR(alias) - \
                                                     VMPU_PERIPH_START) << 5)) >> 2)

extern void vmpu_acl_add(uint8_t box_id, void *addr,
                         uint32_t size, UvisorBoxAcl acl);
extern void vmpu_acl_irq(uint8_t box_id, void *function, uint32_t isr_id);
extern int  vmpu_acl_dev(UvisorBoxAcl acl, uint16_t device_id);
extern int  vmpu_acl_mem(UvisorBoxAcl acl, uint32_t addr, uint32_t size);
extern int  vmpu_acl_reg(UvisorBoxAcl acl, uint32_t addr, uint32_t rmask,
                         uint32_t wmask);
extern int  vmpu_acl_bit(UvisorBoxAcl acl, uint32_t addr);

extern void vmpu_switch(uint8_t src_box, uint8_t dst_box);

extern void vmpu_load_box(uint8_t box_id);

extern int vmpu_fault_recovery_bus(uint32_t pc, uint32_t sp, uint32_t fault_addr, uint32_t fault_status);

uint32_t vmpu_fault_find_acl(uint32_t fault_addr, uint32_t size);

extern void vmpu_acl_stack(uint8_t box_id, uint32_t bss_size, uint32_t stack_size);
extern uint32_t vmpu_acl_static_region(uint8_t region, void* base, uint32_t size, UvisorBoxAcl acl);

extern void vmpu_arch_init(void);
extern void vmpu_arch_init_hw(void);
extern int  vmpu_init_pre(void);
extern void vmpu_init_post(void);

/* Handle system exceptions and interrupts. Return the EXC_RETURN desired for
 * returning from exception mode. */
extern uint32_t vmpu_sys_mux_handler(uint32_t lr, uint32_t msp);

/* contains the total number of boxes
 * boxes are enumerated from 0 to (g_vmpu_box_count - 1) and the following
 * condition must hold: g_vmpu_box_count < UVISOR_MAX_BOXES */
extern uint32_t  g_vmpu_box_count;
bool g_vmpu_boxes_counted;

extern int vmpu_box_namespace_from_id(int box_id, char *box_name, size_t length);

/** Determine if the passed size can be mapped to an exact region size
 * depending on underlying MPU implementation. Note that the size must be an
 * exact match to a MPU region size.
 * With the ARMv7-M MPU that would be 32 <= 2^N <= 512M.
 * With the K64F MPU that would be 32 <= 32*N <= 512M.
 *
 * @note `size` is limited to 512M, even though larger sizes are allowed by the
 *       MPU, but don't make sense from a security perspective.
 * @retval 1 The region size is valid
 * @retval 0 The region size is not valid
 */
extern int vmpu_is_region_size_valid(uint32_t size);

/**
 * @param addr Input address to be rounded up
 * @param size Region size chosen for alignment
 *
 * @retval >0 The passed address rounded up to the next possible alignment
 *            of the passed region size
 * @retval 0 The region size was invalid, or address rounding overflowed
 *
 */
extern uint32_t vmpu_round_up_region(uint32_t addr, uint32_t size);

static UVISOR_FORCEINLINE bool vmpu_is_box_id_valid(int box_id)
{
    /* Return true if the box_id is valid.
     * This function checks box_id against UVISOR_MAX_BOXES if boxes have not
     * been enumerated yet, otherwise it checks against g_vmpu_box_count. */
    return (box_id >= 0 && (g_vmpu_boxes_counted ? (box_id < g_vmpu_box_count) : (box_id < UVISOR_MAX_BOXES)));
}

#endif/*__VMPU_H__*/
