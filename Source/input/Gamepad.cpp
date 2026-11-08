#include "Gamepad.h"
#include <cmath>

#if AT_WITH_GAMEPAD
 #define SDL_MAIN_HANDLED 1
 #include <SDL3/SDL.h>
 #include <SDL3/SDL_main.h>
#endif

namespace at
{
#if AT_WITH_GAMEPAD

struct GamepadService::Impl : public juce::Thread
{
    explicit Impl (GamepadService& o) : juce::Thread ("AutoTheremin Gamepad"), owner (o) {}
    ~Impl() override { stopThread (2000); }

    GamepadService& owner;
    mutable juce::SpinLock lock;
    GamepadSnapshot snap;
    juce::String name;

    SDL_Gamepad* pad = nullptr;
    SDL_JoystickID padId = 0;
    float roll = 0, centre = 0;
    bool rollInit = false;
    uint32_t lastLight = 0xFFFFFFFF;
    juce::uint32 lastLightMs = 0;
    bool prevDown[BtnCount] {};
    uint32_t presses[BtnCount] {};

    void openFirst()
    {
        int count = 0;
        if (SDL_JoystickID* ids = SDL_GetGamepads (&count))
        {
            for (int i = 0; i < count && pad == nullptr; ++i) open (ids[i]);
            SDL_free (ids);
        }
    }

    void open (SDL_JoystickID id)
    {
        if (pad != nullptr) return;
        pad = SDL_OpenGamepad (id);
        if (pad == nullptr) return;
        padId = id;
        if (SDL_GamepadHasSensor (pad, SDL_SENSOR_ACCEL)) SDL_SetGamepadSensorEnabled (pad, SDL_SENSOR_ACCEL, true);
        if (SDL_GamepadHasSensor (pad, SDL_SENSOR_GYRO))  SDL_SetGamepadSensorEnabled (pad, SDL_SENSOR_GYRO, true);
        rollInit = false;
        lastLight = 0xFFFFFFFF;
        const char* n = SDL_GetGamepadName (pad);
        const juce::SpinLock::ScopedLockType sl (lock);
        name = n != nullptr ? juce::String (n) : juce::String ("Gamepad");
    }

    void close()
    {
        if (pad != nullptr) SDL_CloseGamepad (pad);
        pad = nullptr; padId = 0;
        const juce::SpinLock::ScopedLockType sl (lock);
        snap.connected = false;
        name = {};
    }

    void run() override
    {
        SDL_SetMainReady();
        SDL_SetHint (SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
        SDL_SetHint (SDL_HINT_JOYSTICK_ENHANCED_REPORTS, "1");
        SDL_SetHint (SDL_HINT_NO_SIGNAL_HANDLERS, "1");
        if (! SDL_Init (SDL_INIT_GAMEPAD))
        {
            DBG ("SDL_Init failed: " << SDL_GetError());
            return;
        }
        owner.available.store (true);
        openFirst();

        int rescan = 0;
        while (! threadShouldExit())
        {
            // This is our own thread, so pumping only touches our own message queue,
            // never the host's.
            SDL_PumpEvents();
            SDL_Event e;
            while (SDL_PollEvent (&e))
            {
                if (e.type == SDL_EVENT_GAMEPAD_ADDED && pad == nullptr) open (e.gdevice.which);
                else if (e.type == SDL_EVENT_GAMEPAD_REMOVED && pad != nullptr && e.gdevice.which == padId) close();
            }
            if (pad == nullptr && ++rescan > 250) { rescan = 0; openFirst(); }
            if (pad != nullptr) poll();
            wait (2);
        }
        close();
        SDL_Quit();
    }

    static float axis (SDL_Gamepad* p, SDL_GamepadAxis a)
    {
        return juce::jlimit (-1.0f, 1.0f, (float) SDL_GetGamepadAxis (p, a) / 32767.0f);
    }

    void poll()
    {
        GamepadSnapshot s;
        s.connected = SDL_GamepadConnected (pad);
        s.isDualSense = SDL_GetGamepadType (pad) == SDL_GAMEPAD_TYPE_PS5;
        s.lx = axis (pad, SDL_GAMEPAD_AXIS_LEFTX);
        s.ly = axis (pad, SDL_GAMEPAD_AXIS_LEFTY);
        s.rx = axis (pad, SDL_GAMEPAD_AXIS_RIGHTX);
        s.ry = axis (pad, SDL_GAMEPAD_AXIS_RIGHTY);
        s.l2 = juce::jlimit (0.0f, 1.0f, axis (pad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER));
        s.r2 = juce::jlimit (0.0f, 1.0f, axis (pad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER));

        static const SDL_GamepadButton map[BtnCount] = {
            SDL_GAMEPAD_BUTTON_SOUTH, SDL_GAMEPAD_BUTTON_EAST, SDL_GAMEPAD_BUTTON_WEST, SDL_GAMEPAD_BUTTON_NORTH,
            SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER,
            SDL_GAMEPAD_BUTTON_DPAD_UP, SDL_GAMEPAD_BUTTON_DPAD_DOWN, SDL_GAMEPAD_BUTTON_DPAD_LEFT, SDL_GAMEPAD_BUTTON_DPAD_RIGHT,
            SDL_GAMEPAD_BUTTON_START, SDL_GAMEPAD_BUTTON_BACK, SDL_GAMEPAD_BUTTON_TOUCHPAD, SDL_GAMEPAD_BUTTON_GUIDE };
        for (int b = 0; b < BtnCount; ++b)
        {
            const bool down = SDL_GetGamepadButton (pad, map[b]);
            if (down) s.buttons |= (1u << b);
            if (down && ! prevDown[b]) ++presses[b];
            prevDown[b] = down;
            s.pressCount[b] = presses[b];
        }

        if (SDL_GetNumGamepadTouchpads (pad) > 0)
        {
            s.hasTouchpad = true;
            bool down = false; float x = 0, y = 0, pr = 0;
            if (SDL_GetGamepadTouchpadFinger (pad, 0, 0, &down, &x, &y, &pr))
            {
                s.touchDown = down;
                s.touchX = juce::jlimit (0.0f, 1.0f, x);
                s.touchY = juce::jlimit (0.0f, 1.0f, y);
            }
        }

        if (SDL_GamepadSensorEnabled (pad, SDL_SENSOR_ACCEL))
        {
            float a[3] {};
            if (SDL_GetGamepadSensorData (pad, SDL_SENSOR_ACCEL, a, 3))
            {
                s.hasMotion = true;
                // roll = left/right tilt, independent of how far the controller is angled towards you
                const float r = juce::radiansToDegrees (std::atan2 (a[0], std::sqrt (a[1] * a[1] + a[2] * a[2])));
                if (! rollInit) { roll = r; rollInit = true; }
                roll += 0.09f * (r - roll);   // ~20 ms smoothing at 500 Hz
                if (owner.recentreRequest.exchange (false)) centre = roll;
                s.tilt = roll - centre;
            }
        }

        const uint32_t want = owner.lightRequest.load();
        const auto now = juce::Time::getMillisecondCounter();
        if (want != lastLight && now - lastLightMs > 40)
        {
            SDL_SetGamepadLED (pad, (Uint8) ((want >> 16) & 0xFF), (Uint8) ((want >> 8) & 0xFF), (Uint8) (want & 0xFF));
            lastLight = want; lastLightMs = now;
        }

        const juce::SpinLock::ScopedLockType sl (lock);
        snap = s;
    }
};

GamepadService::GamepadService() : impl (std::make_unique<Impl> (*this))
{
    impl->startThread (juce::Thread::Priority::normal);
}

GamepadService::~GamepadService() { impl.reset(); }

GamepadSnapshot GamepadService::read() const
{
    const juce::SpinLock::ScopedLockType sl (impl->lock);
    return impl->snap;
}

juce::String GamepadService::getDeviceName() const
{
    const juce::SpinLock::ScopedLockType sl (impl->lock);
    return impl->name;
}

#else  // ---------------------------------------------------------------- no SDL

struct GamepadService::Impl {};
GamepadService::GamepadService() {}
GamepadService::~GamepadService() {}
GamepadSnapshot GamepadService::read() const { return {}; }
juce::String GamepadService::getDeviceName() const { return {}; }

#endif
} // namespace at
