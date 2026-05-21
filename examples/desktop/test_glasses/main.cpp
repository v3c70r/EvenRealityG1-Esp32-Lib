#include "eveng1/devices/g1_display.hpp"
#include "eveng1/transport/ble_bluez.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <csignal>

static volatile bool running = true;

void signalHandler(int) {
    running = false;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <glasses-mac-address>" << std::endl;
        std::cerr << "Example: " << argv[0] << " AA:BB:CC:DD:EE:FF" << std::endl;
        return 1;
    }

    const char* address = argv[1];

    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    std::cout << "EvenG1Lib Phase 2 Test" << std::endl;
    std::cout << "Connecting to: " << address << std::endl;

    auto transport = std::make_unique<eveng1::BleBlueZ>();
    eveng1::G1Display display(std::move(transport));

    std::cout << "Connecting..." << std::endl;
    if (!display.connect(address)) {
        std::cerr << "Failed to connect to glasses" << std::endl;
        return 1;
    }
    std::cout << "Connected!" << std::endl;

    // Test 1: Clear screen
    std::cout << "Test 1: Clear screen..." << std::endl;
    display.clear(false);
    display.present();
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Test 2: Fill screen
    std::cout << "Test 2: Fill screen..." << std::endl;
    display.clear(true);
    display.present();
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Test 3: Draw rectangles
    std::cout << "Test 3: Draw rectangles..." << std::endl;
    display.clear(false);
    display.fillRect(10, 10, 100, 50);
    display.fillRect(150, 10, 50, 100);
    display.fillRect(300, 30, 200, 80);
    display.present();
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Test 4: Draw lines
    std::cout << "Test 4: Draw lines..." << std::endl;
    display.clear(false);
    display.drawLine(0, 0, 575, 135);
    display.drawLine(575, 0, 0, 135);
    display.drawLine(288, 0, 288, 135);
    display.drawLine(0, 68, 575, 68);
    display.present();
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Test 5: Draw text
    std::cout << "Test 5: Draw text..." << std::endl;
    display.clear(false);
    display.drawText(10, 10, "Hello G1!");
    display.drawText(10, 30, "Phase 2 Test");
    display.drawText(10, 50, "EvenG1Lib");
    display.present();
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Test 6: Text protocol (0x4E)
    std::cout << "Test 6: Text protocol..." << std::endl;
    display.showText("Fast text via 0x4E protocol!", 1, 1);
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Test 7: Heartbeat
    std::cout << "Test 7: Heartbeat (5 seconds)..." << std::endl;
    for (int i = 0; i < 5 && running; i++) {
        display.heartbeat();
        std::cout << "  Heartbeat " << (i + 1) << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    // Test 8: Checkerboard pattern
    std::cout << "Test 8: Checkerboard..." << std::endl;
    display.clear(false);
    for (uint16_t y = 0; y < 136; y += 16) {
        for (uint16_t x = 0; x < 576; x += 16) {
            if ((x / 16 + y / 16) % 2 == 0) {
                display.fillRect(x, y, 16, 16);
            }
        }
    }
    display.present();
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Test 9: Dithered gradient
    std::cout << "Test 9: Dithered gradient..." << std::endl;
    display.clear(false);
    for (uint16_t x = 0; x < 576; x++) {
        uint8_t gray = static_cast<uint8_t>(x * 255 / 575);
        for (uint16_t y = 0; y < 136; y++) {
            display.framebuffer().setPixel(x, y, gray > 127);
        }
    }
    display.present(eveng1::Dither::ORDERED_4X4);
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Cleanup
    std::cout << "Disconnecting..." << std::endl;
    display.disconnect();
    std::cout << "Done!" << std::endl;

    return 0;
}
