#pragma once
#include "patches/Patch.h"

/// @brief Arm7 patch that polls the input override from the DSpico and stores it in shared memory for the arm9 input hook.
/// @details
/// Hooked into the function that copies REG_KEYXY to its shared memory mirror, which can run more than once per frame.
/// The end of that function:
///     ldr    rn, =0x027FFFA8         (0x02FFFFA8 on SDK5)
///     ...
///     strh   rX, [rn]
///     pop    {.., lr}
///     bx     lr
///
/// Is turned into this:
///     ldr    rn, =0x027FFFA8
///     ...
///     ldr    pc,= inputoverridepollarm7_entry
///     .word  inputoverridepollarm7_entry
///     bx     lr
///
/// The patch code runs the replaced strh and pop itself and returns with bx lr.
class InputOverridePollArm7Patch : public Patch
{
public:
    InputOverridePollArm7Patch(const void* arm7Start, u32 arm7Size, u32* arm7CardBusy)
        : _arm7Start(arm7Start), _arm7Size(arm7Size), _arm7CardBusy(arm7CardBusy) { }

    bool FindPatchTarget(PatchContext& patchContext) override;
    void ApplyPatch(PatchContext& patchContext) override;

private:
    const void* _arm7Start;
    u32 _arm7Size;
    u32* _arm7CardBusy;        // non zero while the save patch sends card commands
    u32* _site = nullptr;      // the strh that writes the XY mirror
    u32 _overrideAddr = 0;     // address of the override word, in the same view as the mirror
    u32 _lockAddr = 0;         // address of the SDK slot 1 lock word, in the same view as the mirror
};
