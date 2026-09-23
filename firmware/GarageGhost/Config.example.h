#pragma once
#include <stdint.h>
#include <stddef.h>

// Copy this file to Config.h and fill the private copy locally.
// Config.h is ignored by Git and must never be committed.
static constexpr char WIFI_SSID[] = "YOUR_WIFI_SSID";
static constexpr char WIFI_PASSWORD[] = "YOUR_WIFI_PASSWORD";
static constexpr char GEMINI_API_KEY[] = "YOUR_GEMINI_API_KEY";
static constexpr char TAVILY_API_KEY[] = "YOUR_TAVILY_API_KEY";
static constexpr char GEMINI_MODEL[] = "gemini-3.1-flash-live-preview";

namespace cfg {
constexpr int MIC_BCLK=4, MIC_WS=5, MIC_DATA=6;
constexpr int AMP_BCLK=7, AMP_WS=8, AMP_DATA=9;
constexpr int RELAY_PINS[3]={10,11,12};
constexpr int AMP_ENABLE=13;  // MAX98357A SD: HIGH enables the amplifier, LOW mutes it.
constexpr int COIL_ENABLE=14; // Reserved; leave unconnected in the prototype wiring.
constexpr int BUTTON=15, ASR_RX=16, ASR_TX_UNUSED=17, LED=18;
constexpr uint32_t ASR_BAUD=115200;
constexpr uint32_t INPUT_RATE=16000, OUTPUT_RATE=24000;
constexpr size_t INPUT_SAMPLES=INPUT_RATE*15;
constexpr size_t OUTPUT_BYTES=OUTPUT_RATE*2*30;
constexpr size_t PREROLL_SAMPLES=INPUT_RATE*300/1000;
constexpr uint32_t FOLLOW_MS=5000, CAPTURE_MS=15000;
constexpr uint32_t SILENCE_MS=900, MIN_UTTERANCE_MS=350;
constexpr uint32_t HOLD_MS=350, DEBOUNCE_MS=25;
constexpr uint32_t RESPONSE_MS=60000, CONNECT_MS=20000;
constexpr uint32_t DIALOGUE_MS=180000, MAX_TURNS=6;
constexpr size_t WS_MAX=256*1024, JSON_BUDGET=256*1024;
constexpr size_t SEARCH_BODY_MAX=32*1024, TRANSCRIPT_MAX=8*1024;
constexpr uint8_t MAX_TOOLS=8;
// Консервативні початкові налаштування, підлаштувати за rms= у Serial.
constexpr int MIC_GAIN=4;
constexpr int VAD_RMS=550;
constexpr int VOLUME_PERCENT=15;

}
