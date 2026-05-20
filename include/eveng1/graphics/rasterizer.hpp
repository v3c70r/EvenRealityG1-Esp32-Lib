#pragma once

#include "../eveng1.hpp"
#include <cstdint>
#include <cstddef>

namespace eveng1 {

/// Apply dithering to convert grayscale image to 1-bit framebuffer.
/// @param fb Output framebuffer
/// @param grayscale Input grayscale image (row-major, 8-bit per pixel)
/// @param width Image width in pixels
/// @param height Image height in pixels
/// @param mode Dithering algorithm to use
void applyDither(Framebuffer& fb, const uint8_t* grayscale,
                 uint32_t width, uint32_t height, Dither mode);

}  // namespace eveng1
