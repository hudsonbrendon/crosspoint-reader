#pragma once
#include <cstdint>

#include "PetState.h"

// Chicken sprite data sourced from trilwu/crosspet (master branch),
// https://github.com/trilwu/crosspet/blob/master/src/pet/PetSpriteData.h
// One representative frame per stage: kChicken[stage][frame=0][row].
// Bit convention (identical to crosspet): bit 23 = leftmost column (col 0),
// bit 0 = rightmost (col 23); only bits [23:0] are used (top 8 bits unused).
// Stage→crosspet index: EGG=0, HATCHLING=1, YOUNGSTER=2, COMPANION=3, ELDER=4.

namespace pet {

// 24x24 1bpp sprite: row[r], bit (23 - col) is the pixel at (col, row).
inline constexpr int kSpriteSize = 24;

// One frame (frame 0) per stage for the chicken (pet type 0).
// Index by static_cast<int>(PetStage): 0=EGG..4=ELDER.
// Marked static constexpr so it lives in flash (not DRAM).
static constexpr uint32_t kStageSprites[5][kSpriteSize] = {
    // [0] EGG — frame 0 (center)
    {
        0x000000, 0x000000, 0x00FC00, 0x010200, 0x020100, 0x040080,
        0x040080, 0x040080, 0x040080, 0x042080, 0x046080, 0x042080,
        0x020100, 0x010200, 0x00FC00, 0x000000, 0x000000, 0x000000,
        0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000,
    },
    // [1] HATCHLING — frame 0 (normal)
    {
        0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x004000,
        0x00E000, 0x01F000, 0x03F800, 0x02EC00, 0x03FC00, 0x01F800,
        0x00F000, 0x006000, 0x000000, 0x000000, 0x000000, 0x000000,
        0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000,
    },
    // [2] YOUNGSTER — frame 0 (stand)
    {
        0x000000, 0x000000, 0x000000, 0x00A000, 0x01F000, 0x03F800,
        0x02EC00, 0x03FE00, 0x07FF00, 0x07FF00, 0x07FF00, 0x03FE00,
        0x01FC00, 0x00F800, 0x005000, 0x00D800, 0x000000, 0x000000,
        0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000,
    },
    // [3] COMPANION — frame 0 (normal)
    {
        0x000000, 0x00A800, 0x015400, 0x00F800, 0x01FC00, 0x03FE00,
        0x06FB00, 0x0F7780, 0x0F7780, 0x0FFFF0, 0x0FFFF0, 0x0FFFF0,
        0x07FFE0, 0x03FFC0, 0x01FF80, 0x00FF00, 0x00CC00, 0x01CE00,
        0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000,
    },
    // [4] ELDER — frame 0 (center)
    {
        0x000000, 0x00D000, 0x016800, 0x00F800, 0x01FC00, 0x03FE00,
        0x06FB00, 0x0F7780, 0x0F7780, 0x0FFFF0, 0x0FFFF0, 0x0FFFF0,
        0x07FFE0, 0x03FFC0, 0x01FF80, 0x00FF00, 0x00CC00, 0x01CE00,
        0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000,
    },
};

// Returns the 24-row sprite for the given stage. Stage index clamped to [0, 4].
inline constexpr const uint32_t* spriteForStage(PetStage stage) {
    int idx = static_cast<int>(stage);
    if (idx < 0) idx = 0;
    if (idx > 4) idx = 4;
    return kStageSprites[idx];
}

}  // namespace pet
