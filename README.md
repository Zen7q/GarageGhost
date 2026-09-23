# GarageGhost

GarageGhost is a Ukrainian-speaking voice assistant for automating, modernizing, and digitizing a garage workshop. The name describes the idea: a helpful "ghost" lives in the garage, answers questions, searches for current information, and controls workshop lighting while the user's hands are busy.

The first hardware version runs on an ESP32-S3-WROOM-1 N16R8 board with 16 MB flash and 8 MB PSRAM. An offline ASRPRO module listens only for the Ukrainian wake words **"друже"** and **"брате"**. A physical button provides push-to-talk and immediate cancellation. Speech processing and the assistant response use cloud services only while Wi-Fi is available.

## Current capabilities

- Local wake-word activation through ASRPRO; no continuous microphone stream is sent to the cloud.
- Push-to-talk activation with the same microphone path.
- A short button tap cancels an accidental activation, pending cloud request, or audio reply.
- Ukrainian speech input and spoken Ukrainian responses through Gemini Live audio.
- Live web search through a separate search tool when current information is required.
- Explicit Gemini function calls for three independently controlled light zones.
- Non-blocking audio capture, network handling, playback, button processing, and relay control.
- PSRAM-backed audio and JSON buffers with fixed size limits.
- Safe relay startup: all four relay inputs remain inactive until a valid light command is accepted.

## Lighting zones

| Relay | Function | Load |
| --- | --- | --- |
| K1 | Desk backlight | 230 V AC |
| K2 | Wall backlight | 12 V DC from a separate supply |
| K3 | Main ceiling light | 230 V AC |
| K4 | Reserved | Permanently inactive in firmware |

The relay module uses **active-low inputs**. GPIO LOW energizes a relay, and GPIO HIGH releases it. The firmware initializes every relay GPIO HIGH before network or audio startup.

## Hardware

- ESP32-S3-WROOM-1 N16R8 development board with PSRAM enabled
- INMP441 I2S digital microphone
- MAX98357A I2S DAC/amplifier
- 4 ohm, 3 W speaker
- ASRPRO offline voice-recognition module
- BSS138 four-channel 3.3 V to 5 V bidirectional level-shifter module
- CW-022 compatible four-channel 5 V relay module with low-level trigger inputs
- Normally-open momentary button and status LED
- Isolated regulated 5 V / 3 A power supply
- Separate 12 V supply for the wall lighting circuit

The complete English one-sheet wiring diagram is available in [hardware/GarageGhost_Wiring_EN.pdf](hardware/GarageGhost_Wiring_EN.pdf). Wire identifiers are printed at both ends where space allows.

> **Electrical safety:** K1 and K3 switch 230 V AC. Keep mains conductors physically separated from all low-voltage wiring, use a closed flame-retardant enclosure, provide suitable fusing and strain relief, and have the mains side installed and inspected by a qualified electrician. Verify the COM/NO terminal order printed on the actual relay board.

## System flow

1. ASRPRO recognizes "друже" or "брате" locally and sends a short UART event to the ESP32-S3. Holding the button starts the same interaction manually.
2. The ESP32 opens a five-second speech-onset window, records 16 kHz mono PCM from the INMP441, and ends the utterance after sustained silence or the configured maximum duration.
3. Audio is sent over TLS to the Gemini Live API. The assistant is instructed to respond in Ukrainian.
4. A light request produces a validated `set_lights` function call. The firmware accepts only `desk`, `wall`, `ceiling`, or `all`, and only an explicit Boolean `on` value.
5. A current-information request can produce a validated `web_search` call. The ESP32 queries the configured search provider and returns a compact set of HTTPS sources to Gemini.
6. Gemini's PCM audio response streams to the MAX98357A. A short press of the button invalidates the current interaction generation, silences playback, and blocks late network events from changing a relay.

## Firmware layout

```text
firmware/GarageGhost/
  GarageGhost.ino       Arduino sketch entry point
  App.cpp                state machine, audio tasks, network and relay logic
  Config.example.h       pin map and private configuration template
  CoreLogic.h            timing, button, wake-word and light-state logic
  Protocol.h             strict Gemini function-call validation
  SearchPolicy.h         search query and result validation
  TrustAnchors.h         TLS root certificates
  WsCodec.h              bounded WebSocket frame decoder
  WsTransport.h          TLS WebSocket transport
hardware/
  GarageGhost_Wiring_EN.pdf
tests/
  test_logic.cpp         native state, protocol and decoder tests
  validate.py            Arduino compile and native-test runner
```

The enclosure and all 3D models are intentionally maintained outside this repository.

## Arduino IDE setup

1. Install the current Arduino IDE 2.x and the Espressif **esp32** board package.
2. Install **ArduinoJson 7.x** with Library Manager.
3. Open `firmware/GarageGhost/GarageGhost.ino`.
4. Select an ESP32-S3 board profile and configure:
   - Flash size: **16 MB**
   - PSRAM: **OPI PSRAM**
   - Partition scheme: a layout with at least a 3 MB application partition
   - USB CDC on boot: enabled when using the native USB serial port
5. Copy `Config.example.h` to `Config.h` in the same folder.
6. Enter the Wi-Fi, Gemini API, and search API credentials in the private `Config.h` file.
7. Compile and upload the sketch.

`Config.h` is ignored by Git. Do not rename `Config.example.h` in the repository or commit the private copy.

## Cloud configuration

Create a Gemini API key in Google AI Studio and a search-provider API key for the `web_search` tool. The firmware sends the Gemini key in the secure WebSocket request header and never prints it to Serial. Search requests use HTTPS, reject non-public result URLs, limit excerpt sizes, and return only the minimum data required for the spoken answer.

The configured Gemini Live model is a preview model and may be renamed or retired by Google. If the API rejects the model name, update `GEMINI_MODEL` in the private `Config.h` after checking the current Gemini Live model list.

## ASRPRO setup

Train or configure two offline commands on the ASRPRO module:

| Spoken phrase | UART line sent to ESP32 |
| --- | --- |
| друже | `WAKE:1\n` |
| брате | `WAKE:2\n` |

The parser requires an exact, bounded line. Other serial text is ignored. Connect ASRPRO TX to ESP32 RX through the resistor shown in the diagram and share the low-voltage ground.

## Button behavior

- **Hold:** start push-to-talk and keep the interaction active while speaking.
- **Release after hold:** finish manual capture when the utterance is ready.
- **Short press:** cancel wake-word activation, recording, cloud processing, follow-up listening, or playback.

After a spoken response the assistant permits a short follow-up window. The same short press closes that window immediately, preventing nearby conversation from becoming a new request.

## Validation

`tests/validate.py` can compile the firmware with Arduino CLI and run the native protocol tests. It does not flash hardware and does not contact any live API. The native suite covers button debounce and cancellation, speech timing, active-low light state changes, strict function-call schemas, WebSocket fragmentation and malformed frames, microphone sample conversion, ASRPRO UART framing, URL restrictions, UTF-8 boundaries, and deterministic decoder fuzzing.

Example with a local Arduino CLI installation:

```bash
python3 tests/validate.py \
  --cli /path/to/arduino-cli \
  --config /path/to/arduino-cli.yaml \
  --arduinojson /path/to/ArduinoJson/src
```

Hardware-in-the-loop checks are still required before connecting real lighting loads: confirm each GPIO, verify that K4 never energizes, measure idle relay levels, test cancellation during a delayed response, and check microphone/speaker noise with the final power supply and enclosure.
