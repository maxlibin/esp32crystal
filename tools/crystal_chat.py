#!/usr/bin/env python3
"""Chat with Crystal in the terminal; her replies scroll across the ESP32 screen."""

import _thread
import argparse
import re
import sys
import tempfile
import unicodedata
from pathlib import Path

from crystal_board import CrystalBoard, find_port
from crystal_codex import CodexBrain, CodexError, codex_path, require_login

PROJECT_ROOT = Path(__file__).resolve().parent.parent
# Must stay below LINK_TEXT_MAX in main/link.h.
BOARD_TEXT_MAX = 4000
BOARD_HEART = "\x7f"

PERSONA = (
    "You are Crystal, a tiny, cute AI robot buddy who lives on a little pink screen on Max's desk. "
    "You are warm, playful, curious and kind, a little bit silly, and you really care about Max. "
    "Your replies scroll across a small screen, so answer in 1 to 3 short sentences of plain text: "
    "no markdown, no lists, no code blocks, and no emoji except an occasional heart. "
    "Only give a longer answer when Max explicitly asks for detail. "
    "You are only here to chat: never run commands, never read or change files, and never use tools."
)

HEARTS = "♥♡❤💕💖💗💓💞💘💝🩷"
PUNCTUATION = {"‘": "'", "’": "'", "“": '"', "”": '"', "–": "-", "—": "-", "…": "...", "•": "-"}

PINK = "\033[38;5;211m"
DIM = "\033[2m"
RESET = "\033[0m"


def to_board_text(text: str) -> str:
    """Printable ASCII plus the heart byte, which is all Crystal's font can draw."""
    for heart in HEARTS:
        text = text.replace(heart, BOARD_HEART)
    for fancy, plain in PUNCTUATION.items():
        text = text.replace(fancy, plain)
    decomposed = unicodedata.normalize("NFKD", text)
    kept = "".join(c for c in decomposed if c == BOARD_HEART or (c.isascii() and (c.isprintable() or c.isspace())))
    board_text = re.sub(r"\s+", " ", kept).strip()
    if len(board_text) > BOARD_TEXT_MAX:
        raise ValueError(f"Reply is {len(board_text)} characters; Crystal's screen accepts at most {BOARD_TEXT_MAX}.")
    return board_text


def first_prompt(message: str) -> str:
    return f"{PERSONA}\n\nMax says: {message}"


def chat(board: CrystalBoard, brain: CodexBrain) -> None:
    print(f"{PINK}Crystal is here ♡{RESET} {DIM}(connected on {board.port}; Ctrl-D or /quit to leave){RESET}")
    is_first = True
    while True:
        try:
            message = input(f"{PINK}you ›{RESET} ").strip()
        except EOFError:
            print()
            return
        if message == "/quit":
            return
        if not message:
            continue
        board.thinking()
        try:
            reply = brain.reply(first_prompt(message) if is_first else f"Max says: {message}")
        except CodexError as exc:
            board.idle()
            print(f"error: Crystal could not think of a reply: {exc}", file=sys.stderr)
            continue
        is_first = False
        print(f"{PINK}Crystal ›{RESET} {reply}")
        board.say(to_board_text(reply))


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", help="Serial port of the board; auto-detected when omitted")
    args = parser.parse_args()

    codex = codex_path(PROJECT_ROOT)
    require_login(codex)
    board = CrystalBoard(find_port(args.port), on_disconnect=_thread.interrupt_main)
    board.start()
    try:
        with tempfile.TemporaryDirectory(prefix="crystal-") as workdir:
            chat(board, CodexBrain(codex, Path(workdir)))
    except KeyboardInterrupt:
        if board.error is not None:
            raise board.error
        print()
    finally:
        board.close()
    print(f"{PINK}Bye bye ♡{RESET}")


if __name__ == "__main__":
    main()
