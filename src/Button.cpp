#include "Button.h"

void Button::begin(uint8_t pin, bool activeLow) {
    _pin       = pin;
    _activeLow = activeLow;

    pinMode(_pin, activeLow ? INPUT_PULLUP : INPUT_PULLDOWN);

    _lastRaw      = readRaw();
    _stableState  = _lastRaw;
    _pressedState = false;
    _longFired    = false;
}

void Button::loop() {
    bool raw = readRaw();
    uint32_t now = millis();

    // Debounce: reset timer on any raw change
    if (raw != _lastRaw) {
        _lastRaw       = raw;
        _debounceStart = now;
    }

    // Stable state confirmed after debounce period
    if ((now - _debounceStart) >= DEBOUNCE_MS && raw != _stableState) {
        _stableState = raw;

        if (_stableState) {
            // Button pressed
            _pressStart   = now;
            _pressedState = true;
            _longFired    = false;
        } else {
            // Button released
            if (_pressedState && !_longFired) {
                _clickCount++;
                if (_clickCount == 1) {
                    _waitingDouble    = true;
                    _doubleClickStart = now;
                } else if (_clickCount >= 2) {
                    _waitingDouble = false;
                    _clickCount    = 0;
                    if (_onDoubleClick) _onDoubleClick();
                }
            }
            _pressedState = false;
        }
    }

    // Long press: fire once while still held
    if (_pressedState && !_longFired) {
        if ((now - _pressStart) >= LONG_PRESS_MS) {
            _longFired     = true;
            _waitingDouble = false;
            _clickCount    = 0;
            if (_onLongClick) _onLongClick();
        }
    }

    // Double-click window expired — treat as single short click
    if (_waitingDouble && !_pressedState && (now - _doubleClickStart) >= DOUBLE_CLICK_MS) {
        _waitingDouble = false;
        _clickCount    = 0;
        if (_onShortClick) _onShortClick();
    }
}

bool Button::readRaw() const {
    bool level = digitalRead(_pin);
    return _activeLow ? !level : level;
}
