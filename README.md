# Crystal

A cute AI desk buddy for the ESP32-2432S028 "Cheap Yellow Display". You chat
with Crystal in the terminal; her replies come from the Codex CLI signed in to
your ChatGPT account and scroll along the bottom of her screen while her face
reacts (idle, thinking, talking, sleepy).

## Setup (once)

```sh
npm install        # installs the Codex CLI into node_modules/
make login         # signs Codex in with your ChatGPT account
make build flash   # builds and flashes the firmware
```

## Chat

```sh
make chat                                  # uses PORT from the Makefile
make chat PORT=/dev/cu.usbserial-XXXX      # if the board shows up elsewhere
```

Type to Crystal; Ctrl-D or `/quit` leaves. Crystal dozes off when the chat app
is closed or after five quiet minutes.

## Layout

- `main/` — firmware (ESP-IDF). `link.h` documents the USB serial protocol.
- `tools/crystal_chat.py` — terminal chat app (Python 3 standard library only).
- `tools/gen_font.py` — regenerates `main/font_nunito.c` (`make font`, needs
  `.venv` with `tools/requirements.txt`). Nunito is under the SIL OFL, see
  `tools/fonts/OFL.txt`.
- `docs/superpowers/specs/` — design document.
