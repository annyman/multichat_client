# multichat_client

Terminal client for `multichat_server`, ported from Python to C.

The client speaks a small binary protocol over TCP (port `8080`): a 7-byte
big-endian header (`uint8` type, `uint32` sender id, `uint16` body length)
followed by a body of at most 1024 bytes. Usernames act as passwords; reusing
one restores the same client id and color.

## Requirements

- A C11 compiler (`cc`, `gcc`, or `clang`)
- The ncurses wide-character development files
  - Fedora: `sudo dnf install ncurses-devel`
  - Debian/Ubuntu: `sudo apt install libncurses-dev`
- [`just`](https://github.com/casey/just) (optional; plain `cc` commands also work)

## Build and run

```sh
just            # same as: just build
just run        # build, then launch the client
just test       # build and run the unit tests
just clean
```

Without `just`:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -O2 -Iinclude src/*.c \
    -o multichat_client -lncursesw
./multichat_client
```

If ncurses lives in a non-standard location, point the build at it:

```sh
NCURSES_CFLAGS=-I/path/to/include \
NCURSES_LIBS="-L/path/to/lib -lncursesw" just build
```

## Usage

The client prompts for the server IP (default `127.0.0.1`) and a username,
then opens a full-screen TUI with scrollable chat history and an input bar at
the bottom.

- `PgUp` / `PgDn` or the arrow keys scroll the history.
- `Enter` sends the current message.
- `Backspace` edits the input line.
- `Ctrl-C` or sending `close` quits.

## Layout

```
include/            public headers
  protocol.h        wire constants, packet header encode/decode
  net.h             TCP connect, framing, send/recv helpers
  util.h            allocation and UTF-8 <-> wide-string helpers
  history.h         chat history entries and bounded buffer
  roster.h          client-id -> (name, color) roster
  chat.h            incoming-packet processing
  wrap.h            styled word wrapping
  colors.h          curses color-pair management
  tui.h             curses event loop
src/                matching implementations plus main.c
tests/test_core.c   unit tests for protocol, UTF-8, wrapping, packets, chat
Justfile            build/test/run/clean/install recipes
client.py           original Python reference implementation
```

## Notes on the port

The Python client used `curses` with wide-character input; the C version uses
`ncursesw` and `wchar_t` throughout so multi-byte input, wrapping, and
backspace behave the same way. Invalid UTF-8 received from the server is
replaced with `U+FFFD`, matching Python's `decode(errors="replace")`. The
history keeps the most recent 2000 entries.
