import curses
import select
import socket
import struct

DEFAULT_PORT = 8080
HEADER = "!BIH"
HDR_LEN = struct.calcsize(HEADER)
PKT_MSG, PKT_SYS = 1, 2
MAX_BODY = 1024
NAME_LEN = 32
HISTORY_LIMIT = 2000

assert HDR_LEN == 7


def highlighted(text):
    return f"\033[1;31m{text}\033[0m"


def recv_exact(sock, n):
    data = b""
    while len(data) < n:
        chunk = sock.recv(n - len(data))
        if not chunk:
            raise ConnectionError("server closed")
        data += chunk
    return data


def recv_packet(sock):
    rtype, sender_id, length = struct.unpack(HEADER, recv_exact(sock, HDR_LEN))
    return rtype, sender_id, recv_exact(sock, length)


def send_packet(sock, ptype, sender_id, body):
    if len(body) > MAX_BODY:
        raise ValueError("body too long")
    sock.sendall(struct.pack(HEADER, ptype, sender_id, len(body)) + body)


def ask_server():
    print("=== Register ===")
    host = input("Server IP: ").strip()
    return host or "127.0.0.1"


def register(sock):
    print("Username is your password; reuse it to keep your id.")
    while True:
        name = input("Username: ").strip()
        if (
            name
            and len(name) < NAME_LEN
            and not any(ch.isspace() for ch in name)
        ):
            break
        print(f"1-{NAME_LEN - 1} characters, no spaces.")

    send_packet(sock, PKT_SYS, 0, b"request_id " + name.encode())
    rtype, _, body = recv_packet(sock)
    parts = body.decode(errors="replace").split(" ")

    if rtype != PKT_SYS or len(parts) != 3 or parts[0] != "ok":
        raise ConnectionError(body.decode(errors="replace") or "registration failed")

    return name, int(parts[1]), int(parts[2])


def add_entry(history, entry):
    history.append(entry)
    if len(history) > HISTORY_LIMIT:
        del history[: len(history) - HISTORY_LIMIT]


def process_packet(rtype, sid, body, roster, client_id, history):
    if rtype == PKT_SYS:
        parts = body.decode(errors="replace").split(" ", 3)
        if parts[0] == "join" and len(parts) == 4:
            jid = int(parts[1])
            jcolor = int(parts[2])
            jname = parts[3]
            roster[jid] = (jname, jcolor)
            if jid != client_id:
                add_entry(history, ("join", jname, jcolor))
        elif parts[0] == "leave" and len(parts) >= 2:
            lid = int(parts[1])
            info = roster.pop(lid, None)
            who = info[0] if info else f"id {lid}"
            add_entry(history, ("leave", who))
    else:
        uname, ucolor = roster.get(sid, (f"id {sid}", 15))
        add_entry(history, ("chat", uname, ucolor, body.decode(errors="replace")))


class Colors:
    def __init__(self):
        self.pairs = {}
        self.enabled = curses.has_colors()
        self.bg = 0
        if self.enabled:
            curses.start_color()
            try:
                curses.use_default_colors()
                self.bg = -1
            except curses.error:
                pass

    def attr(self, color):
        if not self.enabled:
            return 0

        if color not in self.pairs:
            pair = len(self.pairs) + 1
            if pair >= curses.COLOR_PAIRS:
                return 0
            index = color % curses.COLORS if curses.COLORS else 0
            curses.init_pair(pair, index, self.bg)
            self.pairs[color] = pair

        return curses.color_pair(self.pairs[color])


def wrap_segments(segments, width):
    width = max(1, width)
    chars = [(ch, attr) for text, attr in segments for ch in text]
    lines = []

    while chars:
        cut = min(width, len(chars))
        if cut < len(chars):
            for i in range(cut - 1, 0, -1):
                if chars[i][0] == " ":
                    cut = i
                    break

        line = chars[:cut]
        chars = chars[cut:]
        while chars and chars[0][0] == " ":
            chars = chars[1:]

        merged = []
        for ch, attr in line:
            if merged and merged[-1][1] == attr:
                merged[-1] = (merged[-1][0] + ch, attr)
            else:
                merged.append((ch, attr))
        lines.append(merged)

    return lines or [[("", 0)]]


def entry_segments(entry, colors):
    kind = entry[0]

    if kind == "chat":
        _, uname, ucolor, text = entry
        return [(uname, colors.attr(ucolor)), (": ", 0), (text, 0)]
    if kind == "join":
        _, jname, jcolor = entry
        return [(f"++ {jname} joined", colors.attr(jcolor))]
    if kind == "leave":
        _, who = entry
        return [(f"-- {who} disconnected --", colors.attr(curses.COLOR_RED) | curses.A_BOLD)]
    if kind == "error":
        _, text = entry
        return [(text, colors.attr(curses.COLOR_RED) | curses.A_BOLD)]

    _, text = entry
    return [(text, 0)]


def build_lines(history, width, colors):
    lines = []
    for entry in history:
        lines.extend(wrap_segments(entry_segments(entry, colors), width))
    return lines


def tui_loop(stdscr, sock, client_id, name, color):
    try:
        curses.curs_set(1)
    except curses.error:
        pass
    stdscr.keypad(True)
    stdscr.timeout(50)

    colors = Colors()
    roster = {client_id: (name, color)}
    history = [
        ("info", f"Registered as {name} (id {client_id})"),
        ("info", "Type a message. PgUp/PgDn or arrows to scroll. 'close' quits."),
    ]

    rx = b""
    input_buf = ""
    top = 0
    follow = True

    while True:
        h, w = stdscr.getmaxyx()
        view_h = max(1, h - 1)

        lines = build_lines(history, w, colors)
        max_top = max(0, len(lines) - view_h)
        if follow:
            top = max_top
        else:
            top = min(max(top, 0), max_top)

        stdscr.erase()

        for row in range(view_h):
            idx = top + row
            if idx >= len(lines):
                break
            col = 0
            for text, attr in lines[idx]:
                if col >= w:
                    break
                try:
                    stdscr.addstr(row, col, text[: w - col], attr)
                except curses.error:
                    pass
                col += len(text)

        avail = max(1, w - 2)
        bar = "> " + input_buf[-avail:]
        try:
            stdscr.addnstr(h - 1, 0, bar.ljust(w), w, curses.A_REVERSE)
            stdscr.move(h - 1, min(len(bar), w - 1))
        except curses.error:
            pass
        stdscr.refresh()

        if select.select([sock], [], [], 0)[0]:
            closed = False
            while True:
                try:
                    chunk = sock.recv(65536)
                except BlockingIOError:
                    break
                except OSError:
                    closed = True
                    break
                if not chunk:
                    closed = True
                    break
                rx += chunk

            while len(rx) >= HDR_LEN:
                rtype, sid, length = struct.unpack(HEADER, rx[:HDR_LEN])
                if length > MAX_BODY:
                    closed = True
                    break
                if len(rx) < HDR_LEN + length:
                    break

                body = rx[HDR_LEN:HDR_LEN + length]
                rx = rx[HDR_LEN + length:]
                process_packet(rtype, sid, body, roster, client_id, history)

            if closed:
                return "Connection closed by server."

        try:
            ch = stdscr.get_wch()
        except curses.error:
            ch = -1

        if ch == -1:
            continue

        if ch in (curses.KEY_ENTER, 10, 13):
            ch = "\n"
        elif ch in (curses.KEY_BACKSPACE, 127, 8):
            ch = "\x7f"
        elif ch == 3:
            ch = "\x03"

        if isinstance(ch, str):
            if ch == "\n":
                msg = input_buf
                input_buf = ""
                if not msg:
                    continue

                data = msg.encode()
                if len(data) > MAX_BODY:
                    add_entry(history, ("error", "Message too long."))
                    continue

                try:
                    send_packet(sock, PKT_MSG, client_id, data)
                except OSError:
                    return "Send failed."

                if msg == "close":
                    return None
                follow = True
            elif ch == "\x7f":
                input_buf = input_buf[:-1]
            elif ch == "\x03":
                return None
            elif ch.isprintable() and len(input_buf) < MAX_BODY:
                input_buf += ch
        else:
            if ch == curses.KEY_UP:
                follow = False
                top -= 1
            elif ch == curses.KEY_DOWN:
                if not follow:
                    top += 1
                    if top >= max_top:
                        follow = True
            elif ch == curses.KEY_PPAGE:
                follow = False
                top -= view_h
            elif ch == curses.KEY_NPAGE:
                if not follow:
                    top += view_h
                    if top >= max_top:
                        follow = True


def main():
    host = ask_server()
    try:
        sock = socket.create_connection((host, DEFAULT_PORT))
    except OSError as e:
        raise SystemExit(f"Could not connect to {host}:{DEFAULT_PORT}: {e}")

    with sock:
        name, client_id, color = register(sock)
        sock.setblocking(False)

        try:
            message = curses.wrapper(tui_loop, sock, client_id, name, color)
        except KeyboardInterrupt:
            message = None

        if message:
            print(highlighted(message))


if __name__ == "__main__":
    main()
