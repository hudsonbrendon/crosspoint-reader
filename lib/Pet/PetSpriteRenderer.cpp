#include "PetSpriteRenderer.h"

#include <GfxRenderer.h>

#include "PetSpriteData.h"

namespace pet {

void drawSprite(GfxRenderer& renderer, PetStage stage, int originX, int originY, int scale) {
  if (scale < 1) scale = 1;
  const uint32_t* rows = spriteForStage(stage);
  for (int row = 0; row < kSpriteSize; ++row) {
    const uint32_t bits = rows[row];
    for (int col = 0; col < kSpriteSize; ++col) {
      if (bits & (1u << (23 - col))) {
        renderer.fillRect(originX + col * scale, originY + row * scale, scale, scale, true);
      }
    }
  }
}

}  // namespace pet
