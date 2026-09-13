.cpu arm946e-s
.section "patch_inputhook", "ax"
.syntax unified
.thumb

.global patch_inputhook_entry
.type patch_inputhook_entry, %function
patch_inputhook_entry:
    push {r2, lr}

    @ Read real hardware inputs into r4
    ldr r0, =0x04000130
    ldrh r4, [r0]

    @ Read the override buffer from memory
    ldr r0, =0x027FFC40
    ldrh r1, [r0, #0]        

    @ Check if the patch has overwritten the input yet. 
    @ The first nibble normally is never written to and should be 0 on startup
    @ so this patch uses it as a flag for if the buffer has been initialized
    ldr r2, =0xF000
    cmp r1, r2
    bhs 1f
    
    @ If this is the first run of the patch load 0xF3FF into 0x027FFC40 to initialize all buttons as released
    ldr r1, =0xF3FF
    strh r1, [r0]          
1:

    @ Combine override with hardware inputs
    ands r1, r4

    pop {r2, pc}

.pool
.balign 4

.global patch_inputhook_readSectors_address
patch_inputhook_readSectors_address:
    .word 0

.pool
.end