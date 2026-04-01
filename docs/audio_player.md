# AudioPlayer – MAX98357A I2S na ESP32-S3

## Opis pinów modułu MAX98357A

| Pin | Kierunek | Opis |
|-----|----------|------|
| **VDD** | Zasilanie | Napięcie zasilania: 2,5 V – 5,5 V. Moduły breakout akceptują 3,3 V i 5 V. |
| **GND** | Masa | Masa układu i głośnika. |
| **SD** | Wejście | _Shutdown / L–R select._ Steruje trybem pracy:<br>• bezpośrednio do GND → układ wyłączony (shutdown)<br>• przez R 100 kΩ do GND → tylko kanał prawy<br>• pozostawiony niepodłączony (floating) → średnia L+R<br>• przez R 100 kΩ do VDD → tylko kanał lewy<br>• bezpośrednio do VDD → normalna praca (L+R)<br>W projekcie: GPIO18 steruje bezpośrednio (HIGH = praca, LOW = shutdown). |
| **GAIN** | Wejście | Ustawia zysk wzmacniacza. Szczegóły w sekcji [Zysk wzmacniacza](#zysk-wzmacniacza-pin-gain). |
| **DIN** | Wejście | I2S – szeregowe dane audio (dane PCM z ESP32). |
| **BCLK** | Wejście | I2S – _Bit Clock_, zegar przesyłu każdego bitu. Częstotliwość: `sample_rate × bity × kanały` = 16 000 × 16 × 2 = **512 kHz**. |
| **LRC** | Wejście | I2S – _Left/Right Clock_ (LRCLK / WS), wskazuje kanał L lub P. Częstotliwość równa sample rate: **16 kHz**. |
| **OUT+** | Wyjście | Wyjście głośnikowe – biegun dodatni (BTL, klasa D). |
| **OUT−** | Wyjście | Wyjście głośnikowe – biegun ujemny (BTL, klasa D). |

> **Uwaga:** Wyjście OUT+ / OUT− to wzmacniacz mostkowy klasy D (BTL). Nie podłączać do masy – grozi uszkodzeniem układu. Głośnik łączyć wyłącznie między OUT+ a OUT−.

---

## Schemat podłączenia

```
ESP32-S3       MAX98357A
─────────────────────────
GPIO15   →    BCLK   (I2S zegar bitu)
GPIO16   →    LRC    (I2S zegar słowa)
GPIO17   →    DIN    (I2S dane)
GPIO18   →    SD     (HIGH = aktywny, LOW = wyciszony)
GND      →    GAIN   (9 dB stały zysk – bez GPIO)
3.3V/5V  →    VDD
GND      →    GND
```

### Zajęte GPIO w projekcie

| GPIO | Funkcja |
|------|---------|
| 5    | Motor – speed |
| 6    | Motor – dir |
| 8    | I2C SDA (PCA9685) |
| 9    | I2C SCL (PCA9685) |
| 15   | MAX98357A BCLK |
| 16   | MAX98357A LRC |
| 17   | MAX98357A DIN |
| 18   | MAX98357A SD |

---

## Format danych audio

| Parametr | Wartość |
|----------|---------|
| Kodowanie | PCM 16-bit signed |
| Kanały | 1 (mono) |
| Częstotliwość próbkowania | **16 000 Hz** |
| Typ tablicy | `uint8_t[]` |
| Przepustowość | 32 000 bajtów/sek |
| Przykład: dźwięk 3 sek | ~96 KB |

---

## Architektura

```
Core 1 (Arduino loop)          Core 0 (FreeRTOS)
──────────────────────         ─────────────────────────
audio.play(data, len)  ──►    AudioPlayer::audioTask()
audio.stop()           ──►      i2s_channel_write() × N
audio.isPlaying()              → SD pin HIGH/LOW
                               → semafory FreeRTOS
```

- `play()` jest **nieblokujące** – wraca natychmiast, dźwięk gra w tle
- `stop()` ustawia flagę; zadanie kończy bieżącą porcję i wycisza wzmacniacz
- Wywołanie `play()` podczas odtwarzania → bezpiecznie przerywa i zaczyna nowy dźwięk

---

## API klasy

```cpp
AudioPlayer audio(bclkPin, lrcPin, dinPin, sdPin);

audio.begin();                        // setup() – inicjalizuje I2S, tworzy task
audio.play(const uint8_t* data, size_t length);  // nieblokujące, anuluje pętlę
audio.loop(const uint8_t* data, size_t length);  // odtwarza w kółko
audio.stop();                         // nieblokujące, anuluje pętlę
bool playing = audio.isPlaying();

audio.setOnDone(std::function<void()> cb);  // callback po zakończeniu dźwięku
audio.clearOnDone();                        // usuwa callback
```

### Callback `setOnDone`

Wywoływany automatycznie po zakończeniu odtwarzania (z Core 0 – audio task).
Wewnątrz callbacku można bezpiecznie wywołać `play()`, aby zakolejkować następny dźwięk.

**Zapętlenie przez `loop()` (zalecane):**
```cpp
audio.loop(ambientSound, ambientSoundLen);  // gra aż do play() lub stop()

// Przerwanie pętli i odtworzenie innego dźwięku:
audio.play(chevronSound, chevronSoundLen);  // pętla anulowana automatycznie

// Zatrzymanie:
audio.stop();
```

**Ręczne zapętlenie przez callback:**
```cpp
audio.setOnDone([&]() {
    audio.play(loopSound, loopSoundLen);
});
audio.play(loopSound, loopSoundLen);
```

**Sekwencja dźwięków:**
```cpp
audio.setOnDone([&]() {
    static int step = 0;
    switch (step++) {
        case 0: audio.play(sound1, sound1Len); break;
        case 1: audio.play(sound2, sound2Len); break;
        default:
            audio.clearOnDone();
            step = 0;
            break;
    }
});
audio.play(sound0, sound0Len);
```

> **Uwaga:** Callback działa na Core 0. Nie należy w nim wykonywać długich operacji –
> tylko `play()` / `clearOnDone()` lub ustawienie flagi dla `loop()`.

---

## Przygotowanie plików audio

### 1. Konwersja WAV → surowe PCM

```bash
# Wymaga SoX (sox.sourceforge.net)
sox input.wav -r 16000 -c 1 -e signed -b 16 output.raw
```

### 2. Konwersja na tablicę C

```bash
# Linux / Git Bash / WSL
xxd -i output.raw > src/snd_chevron.h
```

Wynikowy plik `snd_chevron.h`:
```c
unsigned char output_raw[] = { 0x00, 0x00, ... };
unsigned int  output_raw_len = 12345;
```

### 3. Użycie w kodzie

```cpp
#include "snd_chevron.h"
#include "snd_wormhole.h"

// W setup():
audio.begin();

// W loop() lub metodzie klasy:
audio.play(output_raw, output_raw_len);

// Zatrzymanie przed nowym dźwiękiem (play() robi to automatycznie):
audio.stop();
```

---

## Zysk wzmacniacza (pin GAIN)

| Podłączenie GAIN | Zysk |
|------------------|------|
| GND (domyślnie)  | 9 dB |
| Floating         | 12 dB |
| VDD              | 15 dB |

Zmiana zysku przez GPIO wymaga rezystora (patrz datasheet MAX98357A, tabela pin description).
