#pragma once
#include <cstdint>

#include "PetState.h"

class GfxRenderer;

namespace pet {

// Draws the stage sprite at top-left (originX, originY), each source pixel
// scaled to a (scale x scale) filled cell. Set bits draw black (state=true).
void drawSprite(GfxRenderer& renderer, PetStage stage, int originX, int originY, int scale);

}  // namespace pet
