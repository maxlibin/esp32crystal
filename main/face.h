#pragma once

#include <stdint.h>

#include "buddy.h"
#include "render.h"

// Everything above the ticker strip belongs to the face.
#define FACE_HEIGHT 200
#define FACE_STRIP_ROWS 20

// The shapes that make up Crystal's kawaii face for the current mood and moment.
ShapeList face_shapes(const BuddyState *state, int64_t now_ms);

// Screen area whose pixels differ between two face poses.
Rect face_dirty_rect(const ShapeList *before, const ShapeList *after);

// Redraws the given face area strip by strip using a DMA buffer of
// LCD_WIDTH * FACE_STRIP_ROWS pixels.
void face_draw(Rect area, const ShapeList *shapes, uint16_t *strip_buffer);
