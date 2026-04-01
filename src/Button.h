#pragma once
#include <Arduino.h>
#include <functional>

class Button {
public:
    using Callback = std::function<void()>;

    void begin(uint8_t pin, bool activeLow = true);
    void loop();

    void onShortClick(Callback cb)  { _onShortClick = cb; }
    void onLongClick(Callback cb)   { _onLongClick = cb; }

private:
    uint8_t  _pin;
    bool     _activeLow;
    static constexpr uint32_t DEBOUNCE_MS   = 50;
    static constexpr uint32_t LONG_PRESS_MS = 800;

    bool     _lastRaw        = false;
    bool     _stableState    = false;
    bool     _pressedState   = false;
    bool     _longFired      = false;

    uint32_t _debounceStart  = 0;
    uint32_t _pressStart     = 0;

    Callback _onShortClick;
    Callback _onLongClick;

    bool readRaw() const;
};
