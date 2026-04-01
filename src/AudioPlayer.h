#pragma once
#include <Arduino.h>
#include <driver/i2s.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <functional>

// Audio data format: PCM 16-bit signed, mono, little-endian
// Sample rate: 16000 Hz  →  2 bytes per sample, 32 000 bytes/sec
static constexpr uint32_t AUDIO_SAMPLE_RATE = 16000;

class AudioPlayer {
public:
    // sdPin  – MAX98357A SD (HIGH = on, LOW = mute/shutdown)
    // port   – I2S_NUM_0 lub I2S_NUM_1
    AudioPlayer(int bclkPin, int lrcPin, int dinPin, int sdPin,
                i2s_port_t port = I2S_NUM_0);

    // Inicjalizuje I2S i uruchamia zadanie na Core 0
    void begin();

    // Format danych wejściowych
    enum BitDepth { BITS_8, BITS_16 };

    // Przełącza format wejściowy (domyślnie BITS_8)
    // BITS_8  – próbki uint8 bez znaku (0–255), konwertowane do int16 przed wysłaniem do I2S
    // BITS_16 – próbki int16 little-endian, wysyłane bezpośrednio
    void setBitDepth(BitDepth bd) { _bitDepth = bd; }

    // Rozpoczyna odtwarzanie tablicy PCM; bezpieczne wywołanie z loop()
    // Jeśli coś jest odtwarzane – zatrzymuje i zaczyna nowy dźwięk
    void play(const uint8_t* data, size_t length);

    // Kolejkuje następny dźwięk do odtworzenia natychmiast po bieżącym,
    // bez przerwy (bez czyszczenia DMA i bez cyklu mute/unmute wzmacniacza).
    // Można wywołać w dowolnym momencie podczas odtwarzania.
    void playNext(const uint8_t* data, size_t length);

    // Jak playNext, ale po przejściu dźwięk jest odtwarzany w pętli
    // aż do wywołania play() lub stop().
    void loopNext(const uint8_t* data, size_t length);

    // Gapless przejście z aktywnej pętli do jednorazowego dźwięku.
    // Kończy bieżącą iterację pętli, następnie odgrywa nowy dźwięk BEZ
    // przerwy (bez cyklu mute/unmute). Nie wywołuj play() – to jest zamiennik.
    void transition(const uint8_t* data, size_t length);

    // Odtwarza dźwięk w nieskończoność; przerwać przez play() lub stop()
    void loop(const uint8_t* data, size_t length);

    // Zatrzymuje odtwarzanie i wycisza wzmacniacz
    void stop();

    bool isPlaying() const { return _playing; }

    // Callback wywoływany po zakończeniu odtwarzania (z Core 0 – audio task).
    // Można bezpiecznie wywołać play() wewnątrz callbacku, aby zakolejkować
    // następny dźwięk.
    // Przykład:
    //   player.setOnDone([&]() { player.play(nextSnd, nextSndLen); });
    void setOnDone(std::function<void()> cb) { _onDone = std::move(cb); }
    void clearOnDone() { _onDone = nullptr; }

private:
    int        _bclkPin, _lrcPin, _dinPin, _sdPin;
    i2s_port_t _port;
    BitDepth   _bitDepth = BITS_8;

    std::function<void()> _onDone;

    volatile const uint8_t* _loopData   = nullptr;
    volatile size_t          _loopLength = 0;

    volatile const uint8_t* _nextData   = nullptr;
    volatile size_t          _nextLength = 0;
    volatile bool            _nextLoop   = false;

    TaskHandle_t      _taskHandle = nullptr;
    SemaphoreHandle_t _mutex      = nullptr;
    SemaphoreHandle_t _doneSem    = nullptr;

    volatile const uint8_t* _audioData   = nullptr;
    volatile size_t          _audioLength = 0;
    volatile bool            _playing     = false;

    // Wewnętrzne odtwarzanie bez resetowania stanu pętli
    void _playRaw(const uint8_t* data, size_t length);

    static void audioTask(void* param);
};
