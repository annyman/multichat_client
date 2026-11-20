# Build, test, and install the C chat client.

cc := "clang"
cflags := env("CFLAGS", "-std=c11 -Wall -Wextra -Wpedantic -O2")
ncurses_cflags := env("NCURSES_CFLAGS", "")
ncurses_libs := env("NCURSES_LIBS", "-lncursesw")

target := "build"
binary := "multichat_client"
core_sources := "src/protocol.c src/net.c src/util.c src/wrap.c src/history.c src/roster.c src/chat.c"

default: build

# Compile the client.
build:
	mkdir -p {{target}}
	{{cc}} {{cflags}} -Iinclude {{ncurses_cflags}} src/*.c -o {{target}}/{{binary}} {{ncurses_libs}}

# Build and run the client.
run: build
    ./{{target}}/{{binary}}

# Build and run the unit tests (no curses required).
test:
    {{cc}} {{cflags}} -Iinclude tests/test_core.c {{core_sources}} -o tests/test_core
    ./tests/test_core

# Remove build artefacts.
clean:
    rm -f {{target}} tests/test_core

# Install the binary (override with PREFIX=...).
install: build
    install -Dm755 {{target}}/{{binary}} {{env("PREFIX", "/usr/local")}}/bin/{{binary}}
