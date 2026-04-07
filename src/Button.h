#pragma once
#include <Arduino.h>
#include <functional>

class Button {
public:
    using Callback = std::function<void()>;

    void begin(uint8_t pin, bool activeLow = true);
    void loop();

    void onShortClick(Callback cb)   { _onShortClick  = cb; }
    void onLongClick(Callback cb)    { _onLongClick   = cb; }
    void onDoubleClick(Callback cb)  { _onDoubleClick = cb; }

private:
    uint8_t  _pin;
    bool     _activeLow;
    static constexpr uint32_t DEBOUNCE_MS      = 50;
    static constexpr uint32_t LONG_PRESS_MS    = 1000;
    static constexpr uint32_t DOUBLE_CLICK_MS  = 600;

    bool     _lastRaw        = false;
    bool     _stableState    = false;
    bool     _pressedState   = false;
    bool     _longFired      = false;
    bool     _waitingDouble  = false;

    uint32_t _debounceStart      = 0;
    uint32_t _pressStart         = 0;
    uint32_t _doubleClickStart   = 0;
    uint8_t  _clickCount         = 0;

    Callback _onShortClick;
    Callback _onLongClick;
    Callback _onDoubleClick;

    bool readRaw() const;
};
