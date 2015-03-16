/***************************************************************
 * This confidential and  proprietary  software may be used only
 * as authorised  by  a licensing  agreement  from  ARM  Limited
 *
 *             (C) COPYRIGHT 2013-2014 ARM Limited
 *                      ALL RIGHTS RESERVED
 *
 *  The entire notice above must be reproduced on all authorised
 *  copies and copies  may only be made to the  extent permitted
 *  by a licensing agreement from ARM Limited.
 *
 ***************************************************************/
#include <uvisor.h>
#include "svc.h"
#include "vmpu.h"
#include "unvic.h"

#define SVC_HDLRS_MAX 0x3FUL
#define SVC_HDLRS_NUM (UVISOR_ARRAY_COUNT(g_svc_vtor_tbl))
#define SVC_HDLRS_IND (SVC_HDLRS_NUM - 1)

/* FIXME this function is temporary. Writes to an address should be checked
 *       against a box's ACLs */
static void svc_bitband(uint32_t *addr, uint32_t val)
{
    DPRINTF("Executed privileged bitband access to 0x%08X\n\r", addr);
    *addr = val;
}

/* SVC handlers */
__attribute__((section(".svc_vector"))) const void *g_svc_vtor_tbl[] = {
    svc_bitband,             // 0
    unvic_set_isr,           // 1
    unvic_get_isr,           // 2
    unvic_let_isr,           // 3
    unvic_ena_irq,           // 4
    unvic_dis_irq,           // 5
    unvic_set_ena_isr,       // 6
    unvic_dis_let_isr,       // 7
};

/*******************************************************************************
 *
 * Function name:   __svc_irq
 * Brief:           SVC handlers multiplexer
 *
 * This function is the main SVC IRQ handler. Execution is multiplexed to the
 * proper handler based on the SVC opcode immediate.There are 2 kinds of SVC
 * handler:
 *
 *     1. Regular (unprivileged or privileged)
 * Regular SVC handlers are likely to be mapped to user APIs for unprivileged
 * code. They allow a maximum of 4 32bit arguments and return a single 32bit
 * argument.
 *
 *     2. Secure context (unprivileged) / interrupt (privileged) switch
 * A special SVC handler is given a shortcut to speed up execution. It is used
 * to switch the context between 2 boxes, during normnal execution
 * (unprivileged) or due to an interrupt (privileged). It accepts 4 arguments
 * generated by the asm code below.
 *
 ******************************************************************************/
static void __attribute__((naked)) __svc_irq(void)
{
    asm volatile(
        "tst    lr, #4\n"                   // privileged/unprivileged mode
        "it     eq\n"
        "beq    called_from_priv\n"

    /* the code here serves calls from unprivileged code and is mirrored below
     * for the privileged case; only minor changes exists between the two */
    "called_from_unpriv:\n"
        "mrs    r0, PSP\n"                  // stack pointer
        "ldrt   r1, [r0, #24]\n"            // stacked pc
        "add    r1, r1, #-2\n"              // pc at SVC call
        "ldrbt  r2, [r1]\n"                 // SVC immediate
        /***********************************************************************
         *  ATTENTION
         ***********************************************************************
         * the special handlers
         *   (b00 custom_table_unpriv)
         *    b01 unvic_svc_cx_out
         *    b10 svc_cx_switch_in
         *    b11 svc_cx_switch_out
         * are called here with 3 arguments:
         *    r0 - PSP
         *    r1 - pc of SVCall
         *    r2 - immediate value in SVC opcode
         * these arguments are defined by the asm code you are reading; when
         * changing this code make sure the same format is used or changed
         * accordingly
         **********************************************************************/
        "lsr    r3, r2, #6\n"               // mode bits
        "adr    r12, jump_table_unpriv\n"   // address of jump table
        "ldr    pc, [r12, r3, lsl #2]\n"    // branch to handler
        ".align 4\n"                        // the jump table must be aligned
    "jump_table_unpriv:\n"
        ".word  custom_table_unpriv\n"
        ".word  svc_cx_isr_out\n"
        ".word  svc_cx_switch_in\n"
        ".word  svc_cx_switch_out\n"

    ".thumb_func\n"                            // needed for correct referencing
    "custom_table_unpriv:\n"
        /* there is no need to mask the lower 6 bits of the SVC# because
         * custom_table_unpriv is only when SVC# <= 0x3F */
        "cmp    r2, %[svc_hdlrs_num]\n"     // check SVC table overflow
        "ite    ls\n"                       // note: this ITE order speeds it up
        "ldrls  r1, =g_svc_vtor_tbl\n"      // fetch handler from table
        "bxhi   lr\n"                       // abort if overflowing SVC table
        "add    r1, r1, r2, LSL #2\n"       // SVC table offset
        "ldr    r1, [r1]\n"                 // SVC handler
        "push   {lr}\n"                     // save lr for later
        "ldr    lr, =svc_thunk_unpriv\n"    // after handler return to thunk
        "push   {r1}\n"                     // save SVC handler to fetch args
        "ldrt   r3, [r0, #12]\n"            // fetch args (unprivileged)
        "ldrt   r2, [r0, #8]\n"             // pass args from stack (unpriv)
        "ldrt   r1, [r0, #4]\n"             // pass args from stack (unpriv)
        "ldrt   r0, [r0, #0]\n"             // pass args from stack (unpriv)
        "pop    {pc}\n"                     // execute handler (return to thunk)

    ".thumb_func\n"                            // needed for correct referencing
    "svc_thunk_unpriv:\n"
        "mrs    r1, PSP\n"                  // unpriv stack may have changed
        "strt   r0, [r1]\n"                 // store result on stacked r0
        "pop    {pc}\n"                     // return from SVCall

    "called_from_priv:\n"
        "mrs    r0, MSP\n"                  // stack pointer
        "ldr    r1, [r0, #24]\n"            // stacked pc
        "add    r1, r1, #-2\n"              // pc at SVC call
        "ldrb   r2, [r1]\n"                 // SVC immediate
        /***********************************************************************
         *  ATTENTION
         ***********************************************************************
         * the special handlers
         *   (b00 custom_table_priv)
         *    b01 unvic_svc_cx_in
         * are called here with 3 arguments:
         *    r0 - MSP
         *    r1 - pc of SVCall
         *    r2 - immediate value in SVC opcode
         * these arguments are defined by the asm code you are reading; when
         * changing this code make sure the same format is used or changed
         * accordingly
         **********************************************************************/
        "lsr    r3, r2, #6\n"               // mode bits
        "adr    r12, jump_table_priv\n"     // address of jump table
        "ldr    pc, [r12, r3, lsl #2]\n"    // branch to handler
        ".align 4\n"                        // the jump table must be aligned
    "jump_table_priv:\n"
        ".word  custom_table_priv\n"
        ".word  svc_cx_isr_in\n"

    ".thumb_func\n"                            // needed for correct referencing
    "custom_table_priv:\n"
        /* there is no need to mask the lower 6 bits of the SVC# because
         * custom_table_unpriv is only when SVC# <= 0x3F */
        "cmp    r2, %[svc_hdlrs_num]\n"     // check SVC table overflow
        "ite    ls\n"                       // note: this ITE order speeds it up
        "ldrls  r1, =g_svc_vtor_tbl\n"      // fetch handler from table
        "bxhi   lr\n"                       // abort if overflowing SVC table
        "add    r1, r1, r2, LSL #2\n"       // SVC table offset
        "ldr    r1, [r1]\n"                 // SVC handler
        "push   {lr}\n"                     // save lr for later
        "ldr    lr, =svc_thunk_priv\n"      // after handler return to thunk
        "push   {r1}\n"                     // save SVC handler to fetch args
        "ldm    r0, {r0-r3}\n"              // pass args from stack
        "pop    {pc}\n"                     // execute handler (return to thunk)

    ".thumb_func\n"                            // needed for correct referencing
    "svc_thunk_priv:\n"
        "str    r0, [sp, #4]\n"             // store result on stacked r0
        "pop    {pc}\n"                     // return from SVCall
        :: [svc_hdlrs_num] "i" (SVC_HDLRS_IND)
    );
}

/*******************************************************************************
 *
 * Function name:   svc_init
 * Brief:           SVC initialization
 *
 ******************************************************************************/
void svc_init(void)
{
    /* register SVC handler */
    ISR_SET(SVCall_IRQn, &__svc_irq);

    /* sanity checks */
    assert(SVC_HDLRS_NUM <= SVC_HDLRS_MAX);
}
