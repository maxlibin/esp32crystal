# Crystal — AI desk buddy on the Cheap Yellow Display

Date: 2026-10-26
Status: approved, built

## Intent

Max wants a cute, lightweight AI robot buddy called **Crystal** that lives on the
ESP32 board plugged into the laptop. Max chats with Crystal by typing in a
terminal app; the answer comes from Max's ChatGPT account (via the Codex CLI)
and appears on Crystal's screen as text scrolling horizontally along the
bottom, while Crystal's face reacts. The point is charm and companionship on
the desk, not productivity: it should feel alive and lovable.

## Hardware (known)

- Board: ESP32-2432S028 "Cheap Yellow Display" (classic ESP32, no PSRAM
  assumed), 320×240 ILI9341-class TFT on SPI, XPT2046 touch, CH340 USB-serial.
- Connected to the laptop at `/dev/cu.usbserial-2130` (port can change; detect
  `/dev/cu.usbserial-*` like `eimesp32/tools/codexbar_bridge.py` does).
- No microphone. A speaker connector exists (GPIO26) but sound is out of scope.

## Goals

1. A terminal chat app on the laptop: start it, type messages, read Crystal's
   replies in the terminal too, keep conversation memory for the life of the
   session (memory resets when the app is closed).
2. Answers come from the **Codex CLI signed in with Max's ChatGPT account**
   (`codex exec` non-interactive mode), with a Crystal persona layered on top:
   warm, cute, playful, short chatty replies suited to a scrolling ticker.
3. Firmware that shows Crystal's **kawaii face** and four animated moods, and a
   scrolling reply ticker at the bottom.
4. Crystal replaces the weather/usage dashboard on the board (it is a separate
   firmware; the old project stays intact and can be re-flashed any time).

## Non-goals (v1)

- Sound of any kind (on-board speaker or laptop voice). Explicitly deferred.
- Voice input / microphone.
- Mood-per-reply (Codex choosing happy/sad/love faces). Deferred to a later
  version; the protocol should not make adding it hard.
- Touch interaction, Wi-Fi, settings UI, weather/usage dashboard features.
- Memory that persists across app restarts.
- Using the OpenAI API with an API key.

## Anti-goals (failure even if it "works")

- Pulling the ChatGPT login token out of Codex and calling OpenAI's private
  endpoints directly. Unofficial, can break, and may risk Max's account. Only
  the official `codex` CLI touches the ChatGPT login.
- A face that flickers, tears, or redraws the whole screen visibly: animation
  must feel smooth and alive.
- Crude all-caps blocky text in the ticker (the existing 5×7 font is uppercase
  only). Replies must render in proper mixed case.
- Long, robotic, assistant-style answers. Crystal is a buddy, not a manual.
- Codex acting as a coding agent on Max's files: it must not read, write, or run
  anything in Max's projects while answering chat.

## The face (decided)

Mockup chosen: option C "Kawaii face" (see
`eimesp32/.superpowers/brainstorm/*/content/eyes-style.html` and `states.html`).

- Pastel pink background (≈ `#ffe9f1`), dark plum dot eyes (≈ `#3b2a3a`) with
  a white shine highlight, pink blush ovals under the eyes (≈ `#ff9fbf`), a tiny
  curved smile.
- Bottom 40 px strip is the ticker: pink band (≈ `#ffc6da`), plum text
  (≈ `#6b2a4a`), text scrolls right-to-left.
- Approximate geometry at 320×240: eyes ~44×56 px at x≈92 and x≈184, y≈72;
  blush at y≈140; mouth centred at x≈160, y≈140. Builder may tune for looks.

## Moods (decided: all four)

| Mood | When | Face | Ticker |
|---|---|---|---|
| Idle | Default; after a reply finishes scrolling | Random blinks, occasional glance left/right | A small heart or empty |
| Thinking | From the moment Max presses Enter until the reply arrives | Eyes look up-and-aside, mouth a small flat line | Three bouncing dots |
| Talking | While the reply scrolls | Happy `^ ^` arc eyes, mouth opening/closing | Reply text scrolling |
| Sleepy | Chat app not running, or no message for a while | Closed-line eyes, floating "z Z" | "zzz…" |

- After a reply has scrolled through once, Crystal returns to Idle (the reply
  does not loop).
- Long replies scroll in full; there is no truncation on screen.
- Crystal wakes from Sleepy when Max sends a message (and when the chat app
  starts).

## How it fits together (decided)

```
[terminal chat app]  --codex exec-->  [Codex CLI, ChatGPT login]
        |
        | USB serial, newline-delimited messages
        v
[Crystal firmware on ESP32]  -> face + ticker
```

- New, separate project folder: `~/Desktop/workspace/crystal/` (firmware +
  laptop app). `eimesp32` is left untouched.
- Firmware: C on ESP-IDF, same toolchain and `Makefile` style as `eimesp32`
  (`eim run "idf.py ..."`, `make build/flash/monitor`, `PORT ?= ...`).
- Laptop app: Python 3, standard library only, in the style of
  `eimesp32/tools/codexbar_bridge.py` (termios serial setup at 115200, port
  auto-detect). A `make chat` target starts it.
- Codex CLI is a prerequisite Max installs once and signs into with ChatGPT.

## Builder's calls (my calls, not Max's — flag if wrong)

- **Code reuse:** copy, don't share, the proven bits from
  `eimesp32/main/hello_world_main.c`: pin map, SPI/LCD init sequence, backlight
  PWM, `lcd_set_addr_window`, UART line reader pattern. No dependency between
  the two projects.
- **Rendering:** the classic ESP32 has ~520 KB SRAM and no PSRAM is assumed, so
  no full 320×240×16-bit framebuffer. Redraw only what changes (eye/mouth
  regions, ticker strip) using small DMA-capable buffers, e.g. one 320×40
  buffer for the ticker pushed each frame. Target ≥ 30 fps ticker scroll.
- **Font:** add a nicer mixed-case bitmap font (roughly 12–20 px tall, rounded
  if possible) covering printable ASCII plus a heart glyph. Non-ASCII
  characters from Codex are transliterated or dropped on the laptop side
  before sending; the firmware only ever receives ASCII + the heart.
- **Serial protocol (laptop → board):** one JSON object per line prefixed
  `@CR:` with a `type` of `heartbeat` (every 2 s), `thinking`, `idle`, or `say`
  (with `text`); documented in `main/link.h`. A future `"face"` field
  (mood-per-reply) is a pure addition. The board decides when talking ends
  (after one full scroll) and returns to Idle on its own.
- **Sleep timing:** Sleepy when no heartbeat for ~10 s (app closed / unplugged)
  or no chat message for ~5 min. Tunable constants.
- **Codex invocation:** `codex exec` in non-interactive mode, run from an empty
  scratch directory with a read-only sandbox and no approvals, low reasoning
  effort for snappy replies, git-repo check skipped. Builder must verify the
  exact flags against the installed Codex version (`codex exec --help`).
  Conversation memory: prefer Codex's own session resume for `exec` if the
  installed version supports it; otherwise the app keeps the transcript and
  sends it with each turn.
- **Persona prompt:** Crystal is a cute, warm desk buddy; replies are 1–3 short
  sentences, plain text, no markdown, no emoji except an occasional heart;
  longer answers only when Max explicitly asks for detail.
- **Errors:** follow Max's global rules — no silent fallbacks. If the board is
  not found at startup, the app exits with a clear message naming the ports it
  looked for. If Codex is missing, not logged in, or a call fails, the terminal
  prints the command, exit code, and stderr; Crystal returns to Idle. If the
  board disconnects mid-session, the app stops with a clear error.
- **Install:** Codex CLI pinned as a project npm dependency (no global
  install), then `make login` with ChatGPT. Codex runs with
  `--ignore-user-config --ignore-rules`, a read-only sandbox, and an empty temp
  directory, so personal config, MCP servers and files stay out of the chat.
  Documented in the project README.
- **Testing:** an end-to-end smoke test with the real board and real Codex
  (send one message, confirm a reply prints and the board receives the
  thinking → talking messages). No mocks.

## Left to the builder

File layout inside the project, exact pixel geometry and animation timings,
font choice, frame scheduling, protocol field names, persona wording, and how
the terminal UI looks (prompt symbol, colours), within the constraints above.
