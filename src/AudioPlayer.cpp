#include "AudioPlayer.h"

static constexpr size_t      TASK_STACK_SIZE = 4096;
static constexpr UBaseType_t TASK_PRIORITY   = 2;
static constexpr int         AUDIO_CORE      = 0;   // Core 1 = Arduino loop, Core 0 = audio
static constexpr size_t      CHUNK_BYTES     = 512;
static constexpr TickType_t  WRITE_TIMEOUT   = pdMS_TO_TICKS(200);
static constexpr TickType_t  STOP_TIMEOUT    = pdMS_TO_TICKS(600);

AudioPlayer::AudioPlayer(int bclkPin, int lrcPin, int dinPin, int sdPin, i2s_port_t port)
    : _bclkPin(bclkPin), _lrcPin(lrcPin), _dinPin(dinPin), _sdPin(sdPin), _port(port) {}

void AudioPlayer::begin() {
    pinMode(_sdPin, OUTPUT);
    digitalWrite(_sdPin, LOW);  // Wzmacniacz wyciszony do pierwszego play()

    // --- Konfiguracja I2S (legacy driver, działa na ESP-IDF 4.x i 5.x) ---
    i2s_config_t cfg = {};
    cfg.mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
    cfg.sample_rate          = AUDIO_SAMPLE_RATE;
    cfg.bits_per_sample      = I2S_BITS_PER_SAMPLE_16BIT;
    cfg.channel_format       = I2S_CHANNEL_FMT_ONLY_LEFT;  // mono na lewym kanale
    cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    cfg.intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1;
    cfg.dma_buf_count        = 4;
    cfg.dma_buf_len          = 512;
    cfg.use_apll             = false;
    cfg.tx_desc_auto_clear   = true;

    i2s_pin_config_t pins = {};
    pins.mck_io_num    = I2S_PIN_NO_CHANGE;
    pins.bck_io_num    = _bclkPin;
    pins.ws_io_num     = _lrcPin;
    pins.data_out_num  = _dinPin;
    pins.data_in_num   = I2S_PIN_NO_CHANGE;

    i2s_driver_install(_port, &cfg, 0, nullptr);
    i2s_set_pin(_port, &pins);
    i2s_zero_dma_buffer(_port);

    // --- FreeRTOS ---
    _mutex   = xSemaphoreCreateMutex();
    _doneSem = xSemaphoreCreateBinary();
    xSemaphoreGive(_doneSem);  // Początkowo "wolny"

    xTaskCreatePinnedToCore(
        audioTask, "AudioPlayer",
        TASK_STACK_SIZE, this,
        TASK_PRIORITY, &_taskHandle,
        AUDIO_CORE
    );
}

void AudioPlayer::loop(const uint8_t* data, size_t length) {
    _loopData   = data;
    _loopLength = length;
    // Looping obsługiwany bezpośrednio w audioTask (do-while bez cleanup)
    _playRaw(data, length);
}

void AudioPlayer::play(const uint8_t* data, size_t length) {
    // Przerwij aktywną pętlę i wyczyść kolejkę – _onDone ustawiony ręcznie jest zachowany
    if (_loopData) {
        _loopData   = nullptr;
        _loopLength = 0;
        _onDone     = nullptr;
    }
    _nextData   = nullptr;
    _nextLength = 0;
    _nextLoop   = false;
    _playRaw(data, length);
}

void AudioPlayer::playNext(const uint8_t* data, size_t length) {
    xSemaphoreTake(_mutex, portMAX_DELAY);
    _nextData   = data;
    _nextLength = length;
    _nextLoop   = false;
    xSemaphoreGive(_mutex);
}

void AudioPlayer::loopNext(const uint8_t* data, size_t length) {
    xSemaphoreTake(_mutex, portMAX_DELAY);
    _nextData   = data;
    _nextLength = length;
    _nextLoop   = true;
    xSemaphoreGive(_mutex);
}

void AudioPlayer::transition(const uint8_t* data, size_t length) {
    // Nie przerywamy odtwarzania – tylko podmieniamy kolejkę i wyłączamy pętlę.
    // Task skończy bieżącą iterację i bezpośrednio (gapless) przejdzie do nowych danych.
    xSemaphoreTake(_mutex, portMAX_DELAY);
    _loopData   = nullptr;
    _loopLength = 0;
    _nextData   = data;
    _nextLength = length;
    _nextLoop   = false;
    xSemaphoreGive(_mutex);
}

void AudioPlayer::_playRaw(const uint8_t* data, size_t length) {
    // Zatrzymaj bieżące odtwarzanie i poczekaj aż zadanie zwolni
    _playing = false;
    xSemaphoreTake(_doneSem, STOP_TIMEOUT);

    // Ustaw nowe dane
    xSemaphoreTake(_mutex, portMAX_DELAY);
    _audioData   = data;
    _audioLength = length;
    _playing     = true;
    xSemaphoreGive(_mutex);

    digitalWrite(_sdPin, HIGH);    // Aktywuj wzmacniacz
    xTaskNotifyGive(_taskHandle);  // Obudź zadanie audio
}

void AudioPlayer::stop() {
    _loopData   = nullptr;
    _loopLength = 0;
    _nextData   = nullptr;
    _nextLength = 0;
    _nextLoop   = false;
    _onDone     = nullptr;
    _playing = false;
    // Zadanie samo wyciszy wzmacniacz po zakończeniu bieżącej porcji
}

// ---------------------------------------------------------------------------
// Zadanie FreeRTOS – działa na Core 0
// ---------------------------------------------------------------------------
void AudioPlayer::audioTask(void* param) {
    auto* self = static_cast<AudioPlayer*>(param);

    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);  // Czekaj na sygnał z play()

        // Pętla wewnętrzna: przy aktywnym loop lub zakolejkowanym playNext
        // restartuje dane BEZ cleanup – brak przerwy i cyklu mute/unmute.
        bool gaplessNext = false;
        do {
            gaplessNext = false;

            xSemaphoreTake(self->_mutex, portMAX_DELAY);
            const uint8_t* data   = (const uint8_t*)self->_audioData;
            size_t         length = self->_audioLength;
            xSemaphoreGive(self->_mutex);

            // Wyślij dane do I2S porcjami
            size_t offset = 0;
            if (self->_bitDepth == AudioPlayer::BITS_8) {
                // Konwersja uint8 (0–255) → int16 signed przed wysłaniem do I2S
                int16_t conv[CHUNK_BYTES];
                while (offset < length && self->_playing) {
                    size_t samples = min(CHUNK_BYTES, length - offset);
                    for (size_t i = 0; i < samples; i++) {
                        conv[i] = ((int16_t)data[offset + i] - 128) << 8;
                    }
                    size_t written = 0;
                    i2s_write(self->_port, conv, samples * sizeof(int16_t), &written, WRITE_TIMEOUT);
                    offset += samples;
                }
            } else {
                while (offset < length && self->_playing) {
                    size_t toWrite = min(CHUNK_BYTES, length - offset);
                    size_t written = 0;
                    i2s_write(self->_port, data + offset, toWrite, &written, WRITE_TIMEOUT);
                    if (written > 0) offset += written;
                }
            }

            // Przesuń zakolejkowany playNext/loopNext → bieżący (gapless chain)
            if (self->_nextData != nullptr) {
                xSemaphoreTake(self->_mutex, portMAX_DELAY);
                self->_audioData   = self->_nextData;
                self->_audioLength = self->_nextLength;
                if (self->_nextLoop) {
                    self->_loopData   = self->_nextData;
                    self->_loopLength = self->_nextLength;
                    self->_nextLoop   = false;
                }
                self->_nextData    = nullptr;
                self->_nextLength  = 0;
                gaplessNext = true;
                xSemaphoreGive(self->_mutex);
            }

        } while ((self->_loopData != nullptr || gaplessNext) && self->_playing);

        // Cleanup tylko przy prawdziwym zatrzymaniu (nie przy loop)
        static const uint8_t silence[CHUNK_BYTES] = {};
        size_t dummy = 0;
        i2s_write(self->_port, silence, sizeof(silence), &dummy, pdMS_TO_TICKS(50));
        i2s_zero_dma_buffer(self->_port);

        self->_playing = false;
        digitalWrite(self->_sdPin, LOW);   // Wycisz wzmacniacz

        xSemaphoreGive(self->_doneSem);    // Sygnalizuj zakończenie

        // Wywołaj callback po zwolnieniu semafora – play() można bezpiecznie
        // wywołać wewnątrz, powiadomi ten sam task przez ulTaskNotifyTake
        if (self->_onDone) self->_onDone();
    }
}
