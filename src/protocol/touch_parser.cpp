#include "eveng1/eveng1.hpp"
#include <cstdint>

namespace eveng1 {

TouchEvent parseTouchbarData(const uint8_t* data, uint16_t len, bool& outIsLeft) {
    if (len < 2) return TouchEvent::UNKNOWN;
    outIsLeft = (data[0] == 0x01);
    switch (data[1]) {
        case 0x01: return TouchEvent::SINGLE_TAP_LEFT;
        case 0x00: return TouchEvent::DOUBLE_TAP_LEFT;
        case 0x04: return TouchEvent::TRIPLE_TAP_LEFT;
        case 0x05: return TouchEvent::TRIPLE_TAP_RIGHT;
        case 0x17: return TouchEvent::LONG_PRESS_LEFT;
        case 0x18: return TouchEvent::LONG_PRESS_RELEASE;
        default: return TouchEvent::UNKNOWN;
    }
}

}  // namespace eveng1
