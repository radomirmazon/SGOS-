#include "AudioPlayer.h"

static constexpr size_t      TASK_STACK_SIZE = 4096;
static constexpr UBaseType_t TASK_PRIORITY   = 2;
static constexpr int         AUDIO_CORE      = 0;    // Core 1 = Arduino loop, Core 0 = mikser
// Liczba próbek int16 przetwarzanych w jednej iteracji miksera (16 ms przy 16 kHz).
// Bufor miksujący zajmuje CHUNK_SAMPLES × 4 B = 1 KB (int32, tylko BSS, nie stos).
static constexpr size_t      CHUNK_SAMPLES   = 256;
static constexpr TickType_t  WRITE_TIMEOUT   = pdMS_TO_TICKS(200);
static constexpr TickType_t  STOP_TIMEOUT    = pdMS_TO_TICKS(600);

// ---------------------------------------------------------------------------
AudioPlayer::AudioPlayer(int bclkPin, int lrcPin, int dinPin, int sdPin, i2s_port_t port)
    : _bclkPin(bclkPin), _lrcPin(lrcPin), _dinPin(dinPin), _sdPin(sdPin), _port(port) {}

void AudioPlayer::begin() {
    pinMode(_sdPin, OUTPUT);
    digitalWrite(_sdPin, LOW);  // Wzmacniacz wyciszony do pierwszego play()

    i2s_config_t cfg = {};
    cfg.mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
    cfg.sample_rate          = AUDIO_SAMPLE_RATE;
    cfg.bits_per_sample      = I2S_BITS_PER_SAMPLE_16BIT;
    cfg.channel_format       = I2S_CHANNEL_FMT_ONLY_LEFT;
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

    _mutex = xSemaphoreCreateMutex();

    for (int ch = 0; ch < NUM_CHANNELS; ch++) {
        _doneSem[ch] = xSemaphoreCreateBinary();
        xSemaphoreGive(_doneSem[ch]);  // Początkowo "wolny" – play() może od razu startować
    }

    xTaskCreatePinnedToCore(
        audioTask, "AudioPlayer",
        TASK_STACK_SIZE, this,
        TASK_PRIORITY, &_taskHandle,
        AUDIO_CORE
    );
}

// ---------------------------------------------------------------------------
bool AudioPlayer::isAnyPlaying() const {
    for (int ch = 0; ch < NUM_CHANNELS; ch++) {
        if (_channels[ch].playing) return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
void AudioPlayer::play(const uint8_t* data, size_t length, int channel) {
    // Wyczyść pętlę i kolejkę tego kanału, inne kanały pozostają nienaruszone
    xSemaphoreTake(_mutex, portMAX_DELAY);
    _channels[channel].loopData   = nullptr;
    _channels[channel].loopLength = 0;
    _channels[channel].nextData   = nullptr;
    _channels[channel].nextLength = 0;
    _channels[channel].nextLoop   = false;
    xSemaphoreGive(_mutex);

    _playRaw(data, length, channel);
}

void AudioPlayer::loop(const uint8_t* data, size_t length, int channel) {
    xSemaphoreTake(_mutex, portMAX_DELAY);
    _channels[channel].loopData   = data;
    _channels[channel].loopLength = length;
    xSemaphoreGive(_mutex);

    _playRaw(data, length, channel);
}

void AudioPlayer::playNext(const uint8_t* data, size_t length, int channel) {
    xSemaphoreTake(_mutex, portMAX_DELAY);
    _channels[channel].nextData   = data;
    _channels[channel].nextLength = length;
    _channels[channel].nextLoop   = false;
    xSemaphoreGive(_mutex);
}

void AudioPlayer::loopNext(const uint8_t* data, size_t length, int channel) {
    xSemaphoreTake(_mutex, portMAX_DELAY);
    _channels[channel].nextData   = data;
    _channels[channel].nextLength = length;
    _channels[channel].nextLoop   = true;
    xSemaphoreGive(_mutex);
}

void AudioPlayer::transition(const uint8_t* data, size_t length, int channel) {
    xSemaphoreTake(_mutex, portMAX_DELAY);
    _channels[channel].loopData   = nullptr;
    _channels[channel].loopLength = 0;
    _channels[channel].nextData   = data;
    _channels[channel].nextLength = length;
    _channels[channel].nextLoop   = false;
    xSemaphoreGive(_mutex);
}

void AudioPlayer::stop(int channel) {
    xSemaphoreTake(_mutex, portMAX_DELAY);
    _channels[channel].loopData   = nullptr;
    _channels[channel].loopLength = 0;
    _channels[channel].nextData   = nullptr;
    _channels[channel].nextLength = 0;
    _channels[channel].nextLoop   = false;
    _channels[channel].playing    = false;
    xSemaphoreGive(_mutex);
    // audioTask wykryje !playing && wasActive i zwolni _doneSem[channel]
}

void AudioPlayer::stopAll() {
    for (int ch = 0; ch < NUM_CHANNELS; ch++) {
        stop(ch);
    }
}

// ---------------------------------------------------------------------------
void AudioPlayer::_playRaw(const uint8_t* data, size_t length, int channel) {
    // Gdy wywołanie pochodzi z callbacku wewnątrz audioTask – pomijamy czekanie
    // na semafor (task sam właśnie zwolnił kanał) i nie budzimy taska ponownie.
    const bool fromTask = (xTaskGetCurrentTaskHandle() == _taskHandle);

    if (!fromTask) {
        _channels[channel].playing = false;              // Sygnał do taska: zatrzymaj kanał
        xSemaphoreTake(_doneSem[channel], STOP_TIMEOUT); // Czekaj na potwierdzenie od taska
    }

    xSemaphoreTake(_mutex, portMAX_DELAY);
    _channels[channel].audioData   = data;
    _channels[channel].audioLength = length;
    _channels[channel].audioOffset = 0;
    _channels[channel].playing     = true;
    xSemaphoreGive(_mutex);

    if (!fromTask) {
        digitalWrite(_sdPin, HIGH);       // Aktywuj wzmacniacz (jeśli był wyciszony)
        xTaskNotifyGive(_taskHandle);     // Obudź task jeśli spał
    }
    // fromTask: task jest już aktywny – wykryje playing=true w następnej iteracji pętli
}

// ---------------------------------------------------------------------------
// audioTask – działa na Core 0; miesza próbki ze wszystkich kanałów i wysyła do I2S
// ---------------------------------------------------------------------------
void AudioPlayer::audioTask(void* param) {
    auto* self = static_cast<AudioPlayer*>(param);

    // Bufory statyczne – nie zajmują stosu zadania
    static int32_t         mixBuf[CHUNK_SAMPLES];          // akumulator miksera
    static int16_t         outBuf[CHUNK_SAMPLES];          // wynik po clampingu
    static const int16_t   silence[CHUNK_SAMPLES] = {};    // cisza do flush DMA

    for (;;) {
        // Czekaj na pierwszy sygnał z play()
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        // Odrzuć nadmiarowe powiadomienia (np. gdy wiele kanałów startuje jednocześnie)
        ulTaskNotifyTake(pdTRUE, 0);

        digitalWrite(self->_sdPin, HIGH);  // Aktywuj wzmacniacz

        // Pętla miksera – działa dopóki co najmniej jeden kanał jest aktywny
        bool anyActive = true;
        while (anyActive) {
            anyActive = false;
            memset(mixBuf, 0, CHUNK_SAMPLES * sizeof(int32_t));

            // Flagi do obsługi po zapisie do I2S (poza sekcją krytyczną)
            bool needDone[NUM_CHANNELS]    = {};
            bool naturalDone[NUM_CHANNELS] = {};

            for (int ch = 0; ch < NUM_CHANNELS; ch++) {
                Channel& c = self->_channels[ch];

                // Kanał zatrzymany zewnętrznie (stop/play z innego wątku) –
                // zwolnij semafor aby odblokować ewentualnie czekający _playRaw()
                if (!c.playing && c.wasActive) {
                    c.wasActive  = false;
                    needDone[ch] = true;
                    continue;
                }

                if (!c.playing) continue;

                anyActive   = true;
                c.wasActive = true;

                // Odczyt wskaźnika i długości pod mutexem
                xSemaphoreTake(self->_mutex, portMAX_DELAY);
                const uint8_t* data   = (const uint8_t*)c.audioData;
                size_t         length = c.audioLength;
                size_t         offset = c.audioOffset;
                xSemaphoreGive(self->_mutex);

                // Oblicz ile próbek wymiksować z tego kanału
                const size_t bps         = (self->_bitDepth == BITS_8) ? 1u : 2u;
                const size_t remainBytes = (length > offset) ? (length - offset) : 0u;
                const size_t toMix       = (remainBytes / bps < CHUNK_SAMPLES)
                                           ? (remainBytes / bps) : CHUNK_SAMPLES;

                // Dodaj próbki kanału do bufora int32 (int32 zabezpiecza przed
                // przepełnieniem przy sumowaniu wielu kanałów int16)
                if (self->_bitDepth == BITS_8) {
                    for (size_t i = 0; i < toMix; i++) {
                        // uint8 (0–255) → int16 signed → dodaj do akumulatora
                        mixBuf[i] += (int32_t)(((int16_t)data[offset + i] - 128) << 8);
                    }
                } else {
                    const auto* src16 = reinterpret_cast<const int16_t*>(data + offset);
                    for (size_t i = 0; i < toMix; i++) {
                        mixBuf[i] += (int32_t)src16[i];
                    }
                }

                offset += toMix * bps;

                // Zaktualizuj offset i obsłuż koniec klipu pod mutexem
                xSemaphoreTake(self->_mutex, portMAX_DELAY);
                c.audioOffset = offset;

                if (offset >= length) {
                    if (c.nextData != nullptr) {
                        // Gapless chain: następny zakolejkowany dźwięk
                        c.audioData   = c.nextData;
                        c.audioLength = c.nextLength;
                        c.audioOffset = 0;
                        if (c.nextLoop) {
                            c.loopData   = c.nextData;
                            c.loopLength = c.nextLength;
                            c.nextLoop   = false;
                        }
                        c.nextData   = nullptr;
                        c.nextLength = 0;
                    } else if (c.loopData != nullptr) {
                        // Zapętlenie: wróć do początku
                        c.audioData   = c.loopData;
                        c.audioLength = c.loopLength;
                        c.audioOffset = 0;
                    } else {
                        // Naturalne zakończenie kanału
                        c.playing       = false;
                        c.wasActive     = false;
                        needDone[ch]    = true;
                        naturalDone[ch] = true;
                    }
                }
                xSemaphoreGive(self->_mutex);
            }

            // Ogranicz wynik miksera do zakresu int16 i wyślij do I2S
            for (size_t i = 0; i < CHUNK_SAMPLES; i++) {
                const int32_t s = mixBuf[i];
                outBuf[i] = (int16_t)(s > 32767 ? 32767 : s < -32768 ? -32768 : s);
            }
            size_t written = 0;
            i2s_write(self->_port, outBuf, sizeof(outBuf), &written, WRITE_TIMEOUT);

            // Zwolnij semafory i wywołaj callbacki po zapisie (nie pod mutexem)
            for (int ch = 0; ch < NUM_CHANNELS; ch++) {
                if (!needDone[ch]) continue;

                xSemaphoreGive(self->_doneSem[ch]);

                // Callback tylko przy naturalnym zakończeniu (nie przy stop/play z zewnątrz)
                if (naturalDone[ch]) {
                    // Kopiuj przed wywołaniem – callback może zmieniać onDone
                    auto cb = self->_channels[ch].onDone;
                    if (cb) cb();
                    // Callback mógł wznowić ten kanał przez play() – sprawdź
                    if (self->_channels[ch].playing) anyActive = true;
                }
            }

            // Odrzuć powiadomienia nagromadzone podczas przetwarzania chunka
            ulTaskNotifyTake(pdTRUE, 0);
        }

        // Flush: wyślij ciszę aby opróżnić bufor DMA, a następnie wycisz wzmacniacz
        size_t dummy = 0;
        i2s_write(self->_port, silence, sizeof(silence), &dummy, pdMS_TO_TICKS(50));
        i2s_zero_dma_buffer(self->_port);
        digitalWrite(self->_sdPin, LOW);
    }
}
