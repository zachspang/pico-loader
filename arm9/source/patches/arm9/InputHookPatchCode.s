.cpu arm946e-s
.syntax unified
.arm

.section "patch_inputhook", "ax"
.balign 4

//patch_inputhook_arm_entry
//  Called at game's input read instruction (ARM sites). Entered with a
//  plain branch so the game's lr is untouched.
//
.global patch_inputhook_arm_entry
patch_inputhook_arm_entry:
    push {lr}
    bl patch_inputhook_site_block
    pop {lr}
    ldr pc, patch_inputhook_return_addr     // patched: site + 4 (bit 0 clear)

//patch_inputhook_thumb_entry
//  Same, for Thumb sites. Entered with blx (lr = resume address | 1).
//
.global patch_inputhook_thumb_entry
patch_inputhook_thumb_entry:
    push {lr}
    blx patch_inputhook_thumb_before        // displaced instruction before the OR
    bl patch_inputhook_site_block
    blx patch_inputhook_thumb_after         // displaced instruction after the OR
    pop {pc}                                // interworks back to Thumb

//patch_inputhook_site_block
//  Six consecutive words the patcher rewrites per site:
//    +0   the game's original OR (copied verbatim, or rebuilt for Thumb)
//    +4   save scratch register Rs
//    +8   load address of the override word into Rs (patch Rs << 12)
//    +12  load the override word into Rs
//    +16  apply override: Rd = Rd & Rs
//    +20  restore scratch register Rs
//
.global patch_inputhook_site_block
patch_inputhook_site_block:
    .word 0                                 // +0:  original OR
    str r0, [sp, #-4]!                      // +4:  save Rs
    ldr r0, patch_inputhook_override_addr   // +8:  Rs = &override
    ldr r0, [r0]                            // +12: Rs = override
    ands r0, r0, r0                         // +16: combine override with real input
    ldr r0, [sp], #4                        // +20: restore Rs
    bx lr

patch_inputhook_override_addr:
    .word 0x027FFC24                        // &NTR_SHARED_MEMORY->inputOverride

// Thumb trailers: each runs one displaced instruction, then returns.
.thumb
.global patch_inputhook_thumb_before
.thumb_func
patch_inputhook_thumb_before:
    .hword 0x46C0                           // patched: displaced instruction (default nop)
    bx lr

.global patch_inputhook_thumb_after
.thumb_func
patch_inputhook_thumb_after:
    .hword 0x46C0                           // patched: displaced instruction (default nop)
    bx lr
.arm
.balign 4

.global patch_inputhook_return_addr
patch_inputhook_return_addr:
    .word 0                                 // patched (ARM sites only)

.end