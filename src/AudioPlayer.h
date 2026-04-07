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
    // Liczba wirtualnych kanałów audio miksowanych razem przed wysłaniem do I2S.
    // Aby dodać kanały, wystarczy zwiększyć tę stałą – reszta skaluje się automatycznie.
    static constexpr int NUM_CHANNELS = 2;

    // sdPin  – MAX98357A SD (HIGH = on, LOW = mute/shutdown)
    // port   – I2S_NUM_0 lub I2S_NUM_1
    AudioPlayer(int bclkPin, int lrcPin, int dinPin, int sdPin,
                i2s_port_t port = I2S_NUM_0);

    // Inicjalizuje I2S i uruchamia zadanie miksera na Core 0
    void begin();

    // Format danych wejściowych – wspólny dla wszystkich kanałów
    enum BitDepth { BITS_8, BITS_16 };

    // BITS_8  – próbki uint8 bez znaku (0–255), konwertowane do int16 przed miksem
    // BITS_16 – próbki int16 little-endian, miksowane bezpośrednio
    void setBitDepth(BitDepth bd) { _bitDepth = bd; }

    // --- Funkcje odtwarzania (channel = 0..NUM_CHANNELS-1, domyślnie 0) ---
    // Wszystkie kanały są odtwarzane jednocześnie; próbki są miksowane programowo.

    // Odtwarza dźwięk na kanale; przerywa aktywne odtwarzanie na tym kanale.
    // Pozostałe kanały nie są przerywane.
    void play(const uint8_t* data, size_t length, int channel = 0);

    // Kolejkuje następny dźwięk do odtworzenia bezpośrednio po bieżącym (gapless),
    // bez przerwy i bez cyklu mute/unmute wzmacniacza.
    void playNext(const uint8_t* data, size_t length, int channel = 0);

    // Jak playNext, ale po przejściu dźwięk jest odtwarzany w pętli.
    void loopNext(const uint8_t* data, size_t length, int channel = 0);

    // Gapless przejście z aktywnej pętli do jednorazowego dźwięku na kanale.
    // Kończy bieżącą iterację pętli, następnie odgrywa nowy dźwięk bez przerwy.
    void transition(const uint8_t* data, size_t length, int channel = 0);

    // Odtwarza dźwięk w nieskończoność na kanale; przerwać przez play() lub stop().
    void loop(const uint8_t* data, size_t length, int channel = 0);

    // Zatrzymuje odtwarzanie na wskazanym kanale.
    // Pozostałe kanały nie są przerywane.
    void stop(int channel = 0);

    // Zatrzymuje wszystkie kanały. Wzmacniacz zostanie wyciszony gdy ostatni kanał skończy.
    void stopAll();

    // Czy wskazany kanał aktualnie odtwarza dźwięk?
    bool isPlaying(int channel = 0) const { return _channels[channel].playing; }

    // Czy jakikolwiek kanał aktualnie odtwarza dźwięk?
    bool isAnyPlaying() const;

    // Callback wywoływany po naturalnym zakończeniu odtwarzania na kanale (z Core 0).
    // Bezpieczne wywołanie play() / loop() wewnątrz callbacku.
    void setOnDone(std::function<void()> cb, int channel = 0) {
        _channels[channel].onDone = std::move(cb);
    }
    void clearOnDone(int channel = 0) { _channels[channel].onDone = nullptr; }

private:
    int        _bclkPin, _lrcPin, _dinPin, _sdPin;
    i2s_port_t _port;
    BitDepth   _bitDepth = BITS_8;

    // Stan jednego wirtualnego kanału audio
    struct Channel {
        volatile const uint8_t* audioData   = nullptr;
        volatile size_t          audioLength = 0;
        volatile size_t          audioOffset = 0;    // bieżąca pozycja w bajtach
        volatile bool            playing     = false;
        bool                     wasActive   = false; // śledzone wyłącznie przez audioTask

        volatile const uint8_t* loopData   = nullptr;
        volatile size_t          loopLength = 0;
        volatile const uint8_t* nextData   = nullptr;
        volatile size_t          nextLength = 0;
        volatile bool            nextLoop   = false;

        std::function<void()> onDone;
    };

    Channel           _channels[NUM_CHANNELS];
    SemaphoreHandle_t _doneSem[NUM_CHANNELS]; // jeden semafor na kanał
    TaskHandle_t      _taskHandle = nullptr;
    SemaphoreHandle_t _mutex      = nullptr;

    // Wewnętrzne uruchomienie kanału (bez resetowania stanu pętli).
    // Gdy wywoływane z audioTask (callback), pomija oczekiwanie na semafor.
    void _playRaw(const uint8_t* data, size_t length, int channel);

    static void audioTask(void* param);
};
