.cpu arm7tdmi
.section "patch_inputoverridepollarm7", "ax"
.syntax unified
.arm

INPUT_OVERRIDE_LOCK_ID = 0xBF // last arm7 lock id, should never be used by the game
WAIT_BOUND = 0x800 // transfer wait timeout, irqs are disabled while waiting
START_DELAY = 600 // number of polls to skip after boot

// Replaces the final strh and pop {.., lr} of the XY mirror writer.
// Polls the input override from the DSpico with command 0xEC and stores it in shared
// memory for the arm9 input hook. Skipped when the card is in use.
.global inputoverridepollarm7_entry
.global inputoverridepollarm7_strh
inputoverridepollarm7_entry:
inputoverridepollarm7_strh:
    .word 0 // displaced strh

    push {r0-r7}

    // disable irqs
    mrs r7, cpsr
    orr r3, r7, #0x80
    msr cpsr_c, r3

    // skip the first START_DELAY polls after boot
    ldr r3, start_delay
    cmp r3, #0
    subne r3, r3, #1
    strne r3, start_delay
    bne end

    // skip when the arm7 save patch is using the card
    ldr r3, inputoverridepollarm7_card_busy_addr
    ldr r3, [r3]
    cmp r3, #0
    bne end

    // OS_TryLockCard
    ldr r6, inputoverridepollarm7_lock_addr
    mov r4, #INPUT_OVERRIDE_LOCK_ID
    swp r3, r4, [r6] // atomically swap our id into the lock word
    cmp r3, #0
    bne end // already locked
    strh r4, [r6, #4] // owner id

    ldr r5, =0x04000204 // REG_EXMEMSTAT
    ldrh r3, [r5]
    tst r3, #0x800
    beq unlock // slot 1 not mapped to arm7

    ldr r2, =0x04000198 // reg_base
    ldrh r1, [r2, #0x8] // r1 = REG_AUXSPICNT, restored at the end

    ldrb r3, [r2, #0xF]
    tst r3, #0x80
    bne drain // leftover transfer still busy

    tst r1, #0x20C0
    bne unlock // save chip busy or selected

    tst r1, #0x8000
    beq unlock // slot disabled

    bic r3, r1, #0x4000
    strh r3, [r2, #0x8] // disable transfer ready irq

    // card_romSetCmd(0xEC00000000000000ull);
    mov r3, #0
    str r3, [r2, #0x14] // 00 00 00 00
    mov r3, #0xEC
    str r3, [r2, #0x10] // EC 00 00 00

    // card_romStartXfer
    ldr r3, =0xA7446000
    str r3, [r2, #0xC] // REG_MCCNT1 = MCCNT1_ENABLE | settings;

    mov r4, #WAIT_BOUND
2:
    ldrb r3, [r2, #0xE]
    tst r3, #0x80
    bne 3f // data ready
    subs r4, r4, #1
    bne 2b
    b drain // timeout

3:
    ldr r0, =0x04100010 // REG_MCD1
    ldr r0, [r0]

    // card_romWaitBusy
    mov r4, #WAIT_BOUND
4:
    ldrb r3, [r2, #0xF]
    tst r3, #0x80
    beq 5f
    subs r4, r4, #1
    bne 4b
    b drain // timeout

5:
    strh r1, [r2, #0x8] // restore REG_AUXSPICNT
    cmp r0, #0
    mvneq r0, #0 // 0 (no DSpico) becomes 0xFFFFFFFF, no override
    ldr r3, =0xFFFFF000
    orr r0, r0, r3 // only buttons A to Y (bits 0-11) can be overridden
    ldr r3, inputoverridepollarm7_override_addr
    str r0, [r3]
    b unlock

drain:
    // read out remaining data until the transfer finishes
    mov r4, #WAIT_BOUND
    ldr r0, =0x04100010 // REG_MCD1
6:
    ldrb r3, [r2, #0xF]
    tst r3, #0x80
    beq 7f // not busy
    ldrb r3, [r2, #0xE]
    tst r3, #0x80
    ldrne r3, [r0] // discard data word
    subs r4, r4, #1
    bne 6b
7:
    strh r1, [r2, #0x8] // restore REG_AUXSPICNT

unlock:
    // OS_UnlockCard
    mov r3, #0
    strh r3, [r6, #4] // owner id
    str r3, [r6]

end:
    msr cpsr_c, r7 // restore irqs
    pop {r0-r7}

.global inputoverridepollarm7_pop
inputoverridepollarm7_pop:
    .word 0 // displaced pop {.., lr}

    bx lr

.global inputoverridepollarm7_override_addr
inputoverridepollarm7_override_addr:
    .word 0

.global inputoverridepollarm7_lock_addr
inputoverridepollarm7_lock_addr:
    .word 0

.global inputoverridepollarm7_card_busy_addr
inputoverridepollarm7_card_busy_addr:
    .word 0

start_delay:
    .word START_DELAY // counts down to 0

.pool
.end
