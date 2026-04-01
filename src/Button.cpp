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
                if (_onShortClick) _onShortClick();
            }
            _pressedState = false;
        }
    }

    // Long press: fire once while still held
    if (_pressedState && !_longFired) {
        if ((now - _pressStart) >= LONG_PRESS_MS) {
            _longFired = true;
            if (_onLongClick) _onLongClick();
        }
    }
}

bool Button::readRaw() const {
    bool level = digitalRead(_pin);
    return _activeLow ? !level : level;
}
