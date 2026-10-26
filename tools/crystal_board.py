"""USB serial connection to Crystal's firmware (see main/link.h for the protocol)."""

import fcntl
import glob
import json
import os
import termios
import threading
from typing import Callable, List, Optional

LINE_PREFIX = "@CR:"
HEARTBEAT_SECONDS = 2.0
PORT_PATTERNS = ["/dev/cu.usbserial-*", "/dev/cu.SLAB_USBtoUART*"]


class BoardNotFoundError(RuntimeError):
    pass


class BoardDisconnectedError(RuntimeError):
    pass


def find_port(requested: Optional[str]) -> str:
    if requested:
        if not os.path.exists(requested):
            raise BoardNotFoundError(f"Serial port {requested} does not exist. Is Crystal plugged in?")
        return requested
    ports: List[str] = sorted(path for pattern in PORT_PATTERNS for path in glob.glob(pattern))
    if not ports:
        raise BoardNotFoundError(f"No Crystal board found. Looked for {', '.join(PORT_PATTERNS)}. Is it plugged in?")
    return ports[0]


def configure_serial(path: str) -> int:
    fd = os.open(path, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
    attrs = termios.tcgetattr(fd)
    attrs[0] = 0
    attrs[1] = 0
    attrs[2] = termios.CS8 | termios.CREAD | termios.CLOCAL
    attrs[3] = 0
    attrs[4] = termios.B115200
    attrs[5] = termios.B115200
    attrs[6][termios.VMIN] = 0
    attrs[6][termios.VTIME] = 0
    termios.tcsetattr(fd, termios.TCSANOW, attrs)
    # Opened non-blocking so it never waits for carrier; writes should block.
    fcntl.fcntl(fd, fcntl.F_SETFL, fcntl.fcntl(fd, fcntl.F_GETFL) & ~os.O_NONBLOCK)
    return fd


class CrystalBoard:
    """Sends mood and reply messages to Crystal and keeps a heartbeat going."""

    def __init__(self, port: str, on_disconnect: Callable[[], None]) -> None:
        self.port = port
        self._fd = configure_serial(port)
        self._lock = threading.Lock()
        self._stopped = threading.Event()
        self._on_disconnect = on_disconnect
        self.error: Optional[BoardDisconnectedError] = None
        self._heartbeat = threading.Thread(target=self._heartbeat_loop, name="crystal-heartbeat", daemon=True)

    def start(self) -> None:
        self._send({"type": "heartbeat"})
        self._heartbeat.start()

    def thinking(self) -> None:
        self._send({"type": "thinking"})

    def idle(self) -> None:
        self._send({"type": "idle"})

    def say(self, text: str) -> None:
        self._send({"type": "say", "text": text})

    def close(self) -> None:
        self._stopped.set()
        os.close(self._fd)

    def _send(self, message: dict) -> None:
        line = (LINE_PREFIX + json.dumps(message, ensure_ascii=True, separators=(",", ":")) + "\n").encode()
        with self._lock:
            try:
                view = memoryview(line)
                while view:
                    view = view[os.write(self._fd, view):]
            except OSError as exc:
                raise BoardDisconnectedError(f"Lost connection to Crystal on {self.port}: {exc}") from exc

    def _heartbeat_loop(self) -> None:
        while not self._stopped.wait(HEARTBEAT_SECONDS):
            try:
                self._send({"type": "heartbeat"})
            except BoardDisconnectedError as exc:
                self.error = exc
                self._on_disconnect()
                return
