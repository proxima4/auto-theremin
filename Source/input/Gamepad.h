#pragma once
// PS5 DualSense (and other gamepad) input, read on a private background thread via SDL3.
// One shared service for all plugin instances; every value is published through atomics.
#include <atomic>
#include <juce_core/juce_core.h>

namespace at
{
enum GamepadButton : int
{
    BtnCross = 0, BtnCircle, BtnSquare, BtnTriangle,
    BtnL1, BtnR1, BtnDpadUp, BtnDpadDown, BtnDpadLeft, BtnDpadRight,
    BtnOptions, BtnCreate, BtnTouchClick, BtnPS, BtnCount
};

struct GamepadSnapshot
{
    bool connected = false, isDualSense = false, hasTouchpad = false, hasMotion = false;
    float lx = 0, ly = 0, rx = 0, ry = 0, l2 = 0, r2 = 0;   // sticks -1..1 (y down = +), triggers 0..1
    bool touchDown = false; float touchX = 0.5f, touchY = 0.5f;
    float tilt = 0;          // roll angle in degrees, re-centred
    uint32_t buttons = 0;
    uint32_t pressCount[BtnCount] {};
};

class GamepadService
{
public:
    GamepadService();
    ~GamepadService();

    GamepadSnapshot read() const;
    juce::String getDeviceName() const;
    bool isAvailable() const { return available; }   // false if built without SDL / init failed

    void recentreTilt() { recentreRequest.store (true); }
    /** Light bar colour (DualSense). Packed 0xRRGGBB. */
    void setLight (uint32_t rgb) { lightRequest.store (rgb); }

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
    std::atomic<bool> available { false };
    std::atomic<bool> recentreRequest { false };
    std::atomic<uint32_t> lightRequest { 0xFFB040 };
    friend struct Impl;
};
} // namespace at
