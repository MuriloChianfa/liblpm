-- LuaRocks rockspec for liblpm Lua bindings
-- https://github.com/MuriloChianfa/liblpm
--
-- Installation:
--   luarocks make liblpm-2.0.0-1.rockspec
--
-- Requirements:
--   - liblpm C library must be installed (pkg-config detectable)
--   - Lua 5.3+ or LuaJIT 2.1+

rockspec_format = "3.0"
package = "liblpm"
version = "2.0.0-1"

source = {
    url = "git+https://github.com/MuriloChianfa/liblpm.git",
    tag = "v2.0.0"
}

description = {
    summary = "High-performance Longest Prefix Match library for Lua",
    detailed = [[
        Lua bindings for liblpm, a high-performance C library for Longest Prefix
        Match (LPM) lookups. Supports both IPv4 and IPv6 with multiple optimized
        algorithms (DIR-24-8, 8-bit stride, 16-bit wide stride).
        
        Features:
        - IPv4 and IPv6 support with algorithm selection
        - Multiple input formats: CIDR strings, dotted-decimal, byte tables
        - Batch lookup operations for high throughput
        - Automatic memory management with explicit close() option
        - Both object-oriented and functional API styles
    ]],
    homepage = "https://github.com/MuriloChianfa/liblpm",
    license = "BSL-1.0",
    labels = {"networking", "routing", "ip", "lpm", "trie"},
    maintainer = "Murilo Chianfa <murilo.chianfa@outlook.com>"
}

dependencies = {
    "lua >= 5.3"
}

external_dependencies = {
    LIBLPM = {
        header = "lpm/lpm.h",
        library = "lpm"
    }
}

build = {
    type = "builtin",
    modules = {
        liblpm = {
            sources = {
                "src/liblpm.c",
                "src/liblpm_utils.c"
            },
            libraries = {"lpm"},
            incdirs = {"$(LIBLPM_INCDIR)"},
            libdirs = {"$(LIBLPM_LIBDIR)"}
        }
    },
    copy_directories = {
        "examples",
        "tests"
    }
}

test_dependencies = {
    "lua >= 5.3"
}

-- Note: Due to ifunc resolvers in liblpm, you may need to run tests with:
-- LD_PRELOAD=/usr/local/lib/liblpm.so luarocks test
test = {
    type = "command",
    command = "LD_PRELOAD=/usr/local/lib/liblpm.so lua tests/test_lpm.lua"
}
