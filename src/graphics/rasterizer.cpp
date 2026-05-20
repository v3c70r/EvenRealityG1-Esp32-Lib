#include "eveng1/eveng1.hpp"
#include <algorithm>
#include <cstring>

namespace eveng1 {

static constexpr uint8_t DITHER_2X2[2][2] = {
    {0, 2},
    {3, 1}
};

static constexpr uint8_t DITHER_4X4[4][4] = {
    { 0, 8, 2,10},
    {12, 4,14, 6},
    { 3,11, 1, 9},
    {15, 7,13, 5}
};

struct RasterizerState {
    float error[576];
};

void applyDither(Framebuffer& fb, const uint8_t* grayscale,
                 uint32_t width, uint32_t height, Dither mode) {
    if (mode == Dither::NONE) {
        for (uint32_t y = 0; y < height; y++) {
            for (uint32_t x = 0; x < width; x++) {
                fb.setPixel(x, y, grayscale[y * width + x] > 127);
            }
        }
        return;
    }

    if (mode == Dither::ORDERED_2X2) {
        for (uint32_t y = 0; y < height; y++) {
            for (uint32_t x = 0; x < width; x++) {
                uint8_t threshold = (DITHER_2X2[y % 2][x % 2] * 255) / 4;
                fb.setPixel(x, y, grayscale[y * width + x] > threshold);
            }
        }
        return;
    }

    if (mode == Dither::ORDERED_4X4) {
        for (uint32_t y = 0; y < height; y++) {
            for (uint32_t x = 0; x < width; x++) {
                uint8_t threshold = (DITHER_4X4[y % 4][x % 4] * 255) / 16;
                fb.setPixel(x, y, grayscale[y * width + x] > threshold);
            }
        }
        return;
    }

    if (mode == Dither::FLOYD_STEINBERG) {
        std::vector<float> buf(width * height);
        for (uint32_t i = 0; i < width * height; i++) {
            buf[i] = static_cast<float>(grayscale[i]);
        }
        for (uint32_t y = 0; y < height; y++) {
            for (uint32_t x = 0; x < width; x++) {
                float old = buf[y * width + x];
                bool newPixel = old > 127;
                fb.setPixel(x, y, newPixel);
                float err = old - (newPixel ? 255.0f : 0.0f);
                if (x + 1 < width)
                    buf[y * width + x + 1] += err * 7.0f / 16.0f;
                if (y + 1 < height) {
                    if (x > 0)
                        buf[(y+1) * width + x - 1] += err * 3.0f / 16.0f;
                    buf[(y+1) * width + x] += err * 5.0f / 16.0f;
                    if (x + 1 < width)
                        buf[(y+1) * width + x + 1] += err * 1.0f / 16.0f;
                }
            }
        }
        return;
    }
}

}  // namespace eveng1
