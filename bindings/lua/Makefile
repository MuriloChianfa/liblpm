# ============================================================================
# Makefile for liblpm Lua bindings (standalone build)
# ============================================================================
#
# Usage:
#   make              - Build the Lua module
#   make test         - Run tests
#   make example      - Run basic example
#   make clean        - Remove build artifacts
#   make install      - Install to Lua cpath
#
# Configuration (override with environment variables or make arguments):
#   LUA=lua5.4        - Lua interpreter
#   LUA_CFLAGS=...    - Compiler flags for Lua headers
#   LUA_LIBS=...      - Linker flags for Lua
#   LPM_CFLAGS=...    - Compiler flags for liblpm
#   LPM_LIBS=...      - Linker flags for liblpm
#   PREFIX=/usr/local - Installation prefix
#
# ============================================================================

# Lua configuration (auto-detect if not specified)
LUA ?= lua
LUA_CONFIG ?= $(shell which lua-config 2>/dev/null || which pkg-config 2>/dev/null)

# Try to detect Lua version and flags
ifeq ($(LUA_CFLAGS),)
    LUA_CFLAGS := $(shell pkg-config --cflags lua5.4 2>/dev/null || \
                          pkg-config --cflags lua5.3 2>/dev/null || \
                          pkg-config --cflags luajit 2>/dev/null || \
                          pkg-config --cflags lua 2>/dev/null || \
                          echo "-I/usr/include/lua5.4")
endif

ifeq ($(LUA_LIBS),)
    LUA_LIBS := $(shell pkg-config --libs lua5.4 2>/dev/null || \
                        pkg-config --libs lua5.3 2>/dev/null || \
                        pkg-config --libs luajit 2>/dev/null || \
                        pkg-config --libs lua 2>/dev/null || \
                        echo "-llua5.4")
endif

# liblpm configuration
ifeq ($(LPM_CFLAGS),)
    LPM_CFLAGS := $(shell pkg-config --cflags liblpm 2>/dev/null || echo "-I/usr/local/include/lpm")
endif

ifeq ($(LPM_LIBS),)
    LPM_LIBS := $(shell pkg-config --libs liblpm 2>/dev/null || echo "-L/usr/local/lib -llpm")
endif

# Installation
PREFIX ?= /usr/local
LUA_CPATH ?= $(shell $(LUA) -e "print(package.cpath:match('([^;]+)'))" 2>/dev/null | sed 's|/[^/]*$$||' || echo "$(PREFIX)/lib/lua/5.4")

# Compiler settings
CC ?= gcc
CFLAGS ?= -Wall -Wextra -Wpedantic -O2 -fPIC
LDFLAGS ?= -shared

# Source files
SOURCES = src/liblpm.c src/liblpm_utils.c
OBJECTS = $(SOURCES:.c=.o)
TARGET = liblpm.so

# ============================================================================
# Targets
# ============================================================================

.PHONY: all clean test example install uninstall info

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LPM_LIBS) $(LUA_LIBS)

src/%.o: src/%.c
	$(CC) $(CFLAGS) $(LUA_CFLAGS) $(LPM_CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJECTS) $(TARGET)

test: $(TARGET)
	@echo "Running Lua tests..."
	@echo "Note: LD_PRELOAD is required due to ifunc resolvers in liblpm"
	LD_PRELOAD=/usr/local/lib/liblpm.so LUA_CPATH="./?.so" $(LUA) tests/test_lpm.lua

example: $(TARGET)
	@echo "Running basic example..."
	@echo "Note: LD_PRELOAD is required due to ifunc resolvers in liblpm"
	LD_PRELOAD=/usr/local/lib/liblpm.so LUA_CPATH="./?.so" $(LUA) examples/basic_example.lua

install: $(TARGET)
	@echo "Installing to $(LUA_CPATH)..."
	install -d $(LUA_CPATH)
	install -m 644 $(TARGET) $(LUA_CPATH)/$(TARGET)

uninstall:
	rm -f $(LUA_CPATH)/$(TARGET)

info:
	@echo "Lua bindings configuration:"
	@echo "  Lua:        $(LUA)"
	@echo "  LUA_CFLAGS: $(LUA_CFLAGS)"
	@echo "  LUA_LIBS:   $(LUA_LIBS)"
	@echo "  LPM_CFLAGS: $(LPM_CFLAGS)"
	@echo "  LPM_LIBS:   $(LPM_LIBS)"
	@echo "  LUA_CPATH:  $(LUA_CPATH)"
	@echo "  Target:     $(TARGET)"

# Dependencies
src/liblpm.o: src/liblpm.c
src/liblpm_utils.o: src/liblpm_utils.c
