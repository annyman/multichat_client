import selectors
import socket
import struct
import sys

DEFAULT_PORT = 8080
HEADER = "!BIH"
HDR_LEN = struct.calcsize(HEADER)
PKT_MSG, PKT_SYS = 1, 2
MAX_BODY = 1024
NAME_LEN = 32

assert HDR_LEN == 7


def colored(text, color):
    return f"\033[38;5;{color}m{text}\033[0m"


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
    if not host:
        host = "127.0.0.1"
    port = input(f"Port ({DEFAULT_PORT}): ").strip()
    try:
        return host, int(port) if port else DEFAULT_PORT
    except ValueError:
        raise SystemExit("Invalid port.")


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


def handle_sys(body, roster, client_id):
    parts = body.decode(errors="replace").split(" ", 3)

    if parts[0] == "join" and len(parts) == 4:
        jid = int(parts[1])
        color = int(parts[2])
        name = parts[3]
        roster[jid] = (name, color)
        if jid != client_id:
            print(colored(f"++ {name} joined", color))
    elif parts[0] == "leave" and len(parts) >= 2:
        lid = int(parts[1])
        info = roster.pop(lid, None)
        who = info[0] if info else f"id {lid}"
        print(highlighted(f"-- {who} disconnected --"))


def main():
    host, port = ask_server()
    try:
        sock = socket.create_connection((host, port))
    except OSError as e:
        raise SystemExit(f"Could not connect to {host}:{port}: {e}")

    with sock:
        name, client_id, color = register(sock)
        roster = {client_id: (name, color)}
        print(f"Registered as {colored(name, color)} (id {client_id})")
        print("Type a message, or 'close' to quit.")

        sel = selectors.DefaultSelector()
        sel.register(sock, selectors.EVENT_READ, "sock")
        sel.register(sys.stdin, selectors.EVENT_READ, "stdin")

        rx = b""
        while True:
            for key, _ in sel.select():
                if key.data == "stdin":
                    line = sys.stdin.readline()
                    if not line:
                        return
                    msg = line.rstrip("\n")
                    send_packet(sock, PKT_MSG, client_id, msg.encode())
                    if msg == "close":
                        return
                    continue

                chunk = sock.recv(65536)
                if not chunk:
                    print(highlighted("Connection closed by server."))
                    return

                rx += chunk
                while len(rx) >= HDR_LEN:
                    rtype, sid, length = struct.unpack(HEADER, rx[:HDR_LEN])
                    if length > MAX_BODY:
                        print(highlighted("Invalid packet from server."))
                        return
                    if len(rx) < HDR_LEN + length:
                        break

                    body = rx[HDR_LEN:HDR_LEN + length]
                    rx = rx[HDR_LEN + length:]

                    if rtype == PKT_SYS:
                        handle_sys(body, roster, client_id)
                    else:
                        uname, ucolor = roster.get(sid, (f"id {sid}", 15))
                        print(f"{colored(uname, ucolor)}: {body.decode(errors='replace')}")


if __name__ == "__main__":
    main()
