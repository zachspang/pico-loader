.cpu arm946e-s
.section "patch_inputhook", "ax"
.syntax unified
.thumb

//patch_inputhook_entry
//  Called at game's input read instruction. Replaces the game's normal
//  input read with real hardware input ANDed against whatever the DSpico 
//  firmware currently holds as the USB override state. 
//  Returns overridden state in r0 and r1
//
.global patch_inputhook_entry
.type patch_inputhook_entry, %function
patch_inputhook_entry:
    push {r4, lr}
    bl dspico_getInputState // returns r4 = 4 byte response from firmware

    ldrh r1, [r0]  // read KEYINPUT
    ldrh r0, [r2]  // read XY mirror value
    orrs r1, r0

    ands r1, r4  // combine override with real input
    movs r0, r1  // put overridden input in both r0 and r1 since the first instr after returning orrs both.
    pop {r4, pc}

.pool
.balign 4

//dspico_getInputState
//  Issues the 0xEC command to the DSpico firmware and 
//  returns the 4 byte response in r4. Temporarily hands slot 1 
//  access to ARM9 (clearing EXMEMCNT bit 11) for the duration of
//  the command, then restores EXMEMCNT's state.
//
.type dspico_getInputState, %function
dspico_getInputState:
    push {r0, r1, r2, r3, r5, r6, lr}
    ldr r2, =0x04000198 // reg_base

    // Save current EXMEMCNT and give slot 1 bus access to ARM9 (clear bit 11)
    ldr r5, =0x04000204
    ldrh r6, [r5]  // r6 = saved original EXMEMCNT
    ldr r1, =0xF7FF  // ~(1 << 11)
    movs r0, r6
    ands r0, r1
    strh r0, [r5]  // clear bit 11

    movs r0, #0
    strb r0, [r2, #0x8]  // clear MCCNT0 low byte, forces ROM mode
    movs r0, #0x80
    strb r0, [r2, #0x9]  // select rom mode without irq

    movs r3, #0
1:
    ldrb r3, [r2, #0xF]
    lsrs r3, r3, #8
    bcs 1b

    // card_romSetCmd(0xEC00000000000000ull);
    str r3, [r2, #0x14]  // 00 00 00 00
    movs r3, #0xEC
    str r3, [r2, #0x10]  // EC 00 00 00

    // card_romStartXfer
    ldr r3, =0xA7446000
    str r3, [r2, #0xC]

2:
    ldrb r3, [r2, #0xE]
    lsrs r3, r3, #8
    bcc 2b

    ldr r4, =0x04100010
    ldr r4, [r4]  // read data in

    strh r6, [r5]  // restore EXMEMCNT to its original state

    pop {r0, r1, r2, r3, r5, r6, pc}

.pool
.balign 4
.end