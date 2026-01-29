/**
 * @file liblpm.c
 * @brief Lua bindings for liblpm - High-Performance Longest Prefix Match library
 *
 * This module provides Lua bindings for the liblpm C library, supporting both
 * IPv4 and IPv6 longest prefix match operations with multiple algorithm options.
 *
 * Supports: Lua 5.3, 5.4, and LuaJIT 2.1+
 *
 * @author Murilo Chianfa
 * @license Boost Software License 1.0
 */

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include <lpm.h>

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>

/* Compatibility macros for different Lua versions */
#if LUA_VERSION_NUM < 502
    #define luaL_newlib(L, l) (lua_newtable(L), luaL_register(L, NULL, l))
    #define luaL_setfuncs(L, l, n) luaL_register(L, NULL, l)
#endif

#if LUA_VERSION_NUM < 503
    /* lua_isinteger doesn't exist in Lua 5.1/5.2 */
    #define lua_isinteger(L, idx) (lua_type(L, idx) == LUA_TNUMBER)
#endif

/* Metatable names */
#define LPM_TABLE_MT "liblpm.table"

/* Module version */
#define LPM_LUA_VERSION "2.0.0"

/* Maximum batch size to prevent excessive memory allocation */
#define LPM_MAX_BATCH_SIZE 100000

/* ============================================================================
 * Userdata Structure
 * ============================================================================ */

typedef struct {
    lpm_trie_t *trie;
    bool is_ipv6;
    bool closed;
} lua_lpm_table_t;

/* ============================================================================
 * Forward declarations for utility functions
 * ============================================================================ */

/* IP address parsing (implemented in liblpm_utils.c) */
int lpm_lua_parse_ipv4(const char *str, uint8_t out[4]);
int lpm_lua_parse_ipv6(const char *str, uint8_t out[16]);
int lpm_lua_parse_ipv4_cidr(const char *str, uint8_t prefix[4], uint8_t *prefix_len);
int lpm_lua_parse_ipv6_cidr(const char *str, uint8_t prefix[16], uint8_t *prefix_len);

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * Get the lua_lpm_table_t userdata from the stack at the given index.
 * Raises a Lua error if the argument is not a valid lpm table.
 */
static lua_lpm_table_t *check_lpm_table(lua_State *L, int idx) {
    lua_lpm_table_t *t = (lua_lpm_table_t *)luaL_checkudata(L, idx, LPM_TABLE_MT);
    if (t->closed) {
        luaL_error(L, "attempt to use a closed lpm table");
    }
    return t;
}

/**
 * Get the lua_lpm_table_t userdata without checking if closed.
 * Used for operations that are valid on closed tables (like is_closed).
 */
static lua_lpm_table_t *get_lpm_table(lua_State *L, int idx) {
    return (lua_lpm_table_t *)luaL_checkudata(L, idx, LPM_TABLE_MT);
}

/**
 * Parse an IPv4 address from various Lua formats:
 * - String: "192.168.1.1"
 * - Table: {192, 168, 1, 1}
 * - Binary string: 4-byte string
 */
static int parse_ipv4_address(lua_State *L, int idx, uint8_t out[4]) {
    if (lua_isstring(L, idx)) {
        size_t len;
        const char *str = lua_tolstring(L, idx, &len);
        
        /* Binary string (4 bytes) */
        if (len == 4) {
            memcpy(out, str, 4);
            return 0;
        }
        
        /* Dotted-decimal string */
        if (lpm_lua_parse_ipv4(str, out) != 0) {
            return -1;
        }
        return 0;
    }
    else if (lua_istable(L, idx)) {
        for (int i = 0; i < 4; i++) {
            lua_rawgeti(L, idx, i + 1);
            if (!lua_isinteger(L, -1)) {
                lua_pop(L, 1);
                return -1;
            }
            lua_Integer val = lua_tointeger(L, -1);
            lua_pop(L, 1);
            if (val < 0 || val > 255) {
                return -1;
            }
            out[i] = (uint8_t)val;
        }
        return 0;
    }
    
    return -1;
}

/**
 * Parse an IPv6 address from various Lua formats:
 * - String: "2001:db8::1"
 * - Table: {0x20, 0x01, 0x0d, 0xb8, ...} (16 bytes)
 * - Binary string: 16-byte string
 */
static int parse_ipv6_address(lua_State *L, int idx, uint8_t out[16]) {
    if (lua_isstring(L, idx)) {
        size_t len;
        const char *str = lua_tolstring(L, idx, &len);
        
        /* Binary string (16 bytes) */
        if (len == 16) {
            memcpy(out, str, 16);
            return 0;
        }
        
        /* Colon-hex string */
        if (lpm_lua_parse_ipv6(str, out) != 0) {
            return -1;
        }
        return 0;
    }
    else if (lua_istable(L, idx)) {
        for (int i = 0; i < 16; i++) {
            lua_rawgeti(L, idx, i + 1);
            if (!lua_isinteger(L, -1)) {
                lua_pop(L, 1);
                return -1;
            }
            lua_Integer val = lua_tointeger(L, -1);
            lua_pop(L, 1);
            if (val < 0 || val > 255) {
                return -1;
            }
            out[i] = (uint8_t)val;
        }
        return 0;
    }
    
    return -1;
}

/**
 * Parse a prefix (address + prefix length) from various formats:
 * - CIDR string: "192.168.0.0/16"
 * - Address + separate prefix_len argument
 */
static int parse_prefix(lua_State *L, int idx, bool is_ipv6, 
                        uint8_t *prefix, uint8_t *prefix_len) {
    int addr_size = is_ipv6 ? 16 : 4;
    
    /* Check if it's a CIDR string */
    if (lua_isstring(L, idx)) {
        const char *str = lua_tostring(L, idx);
        
        /* Check for '/' indicating CIDR notation */
        if (strchr(str, '/') != NULL) {
            if (is_ipv6) {
                return lpm_lua_parse_ipv6_cidr(str, prefix, prefix_len);
            } else {
                return lpm_lua_parse_ipv4_cidr(str, prefix, prefix_len);
            }
        }
        
        /* Plain address, need prefix_len from next argument */
        if (is_ipv6) {
            if (lpm_lua_parse_ipv6(str, prefix) != 0) {
                return -1;
            }
        } else {
            if (lpm_lua_parse_ipv4(str, prefix) != 0) {
                return -1;
            }
        }
        
        /* Get prefix length from next argument */
        if (!lua_isinteger(L, idx + 1)) {
            return -1;
        }
        lua_Integer len = lua_tointeger(L, idx + 1);
        int max_len = is_ipv6 ? 128 : 32;
        if (len < 0 || len > max_len) {
            return -1;
        }
        *prefix_len = (uint8_t)len;
        return 1; /* Consumed 2 arguments */
    }
    else if (lua_istable(L, idx)) {
        /* Table of bytes */
        if (is_ipv6) {
            if (parse_ipv6_address(L, idx, prefix) != 0) {
                return -1;
            }
        } else {
            if (parse_ipv4_address(L, idx, prefix) != 0) {
                return -1;
            }
        }
        
        /* Get prefix length from next argument */
        if (!lua_isinteger(L, idx + 1)) {
            return -1;
        }
        lua_Integer len = lua_tointeger(L, idx + 1);
        int max_len = is_ipv6 ? 128 : 32;
        if (len < 0 || len > max_len) {
            return -1;
        }
        *prefix_len = (uint8_t)len;
        return 1; /* Consumed 2 arguments */
    }
    
    return -1;
}

/* ============================================================================
 * Table Creation Functions
 * ============================================================================ */

/**
 * lpm.new_ipv4([algorithm]) -> table
 *
 * Create a new IPv4 LPM table.
 *
 * @param algorithm (optional) "dir24" (default) or "stride8"
 * @return LPM table userdata
 */
static int l_new_ipv4(lua_State *L) {
    const char *algo = luaL_optstring(L, 1, "dir24");
    
    lpm_trie_t *trie = NULL;
    
    if (strcmp(algo, "dir24") == 0) {
        trie = lpm_create_ipv4_dir24();
    } else if (strcmp(algo, "stride8") == 0) {
        trie = lpm_create_ipv4_8stride();
    } else {
        return luaL_error(L, "invalid algorithm '%s' (expected 'dir24' or 'stride8')", algo);
    }
    
    if (trie == NULL) {
        return luaL_error(L, "failed to create IPv4 LPM table: out of memory");
    }
    
    lua_lpm_table_t *t = (lua_lpm_table_t *)lua_newuserdata(L, sizeof(lua_lpm_table_t));
    t->trie = trie;
    t->is_ipv6 = false;
    t->closed = false;
    
    luaL_getmetatable(L, LPM_TABLE_MT);
    lua_setmetatable(L, -2);
    
    return 1;
}

/**
 * lpm.new_ipv6([algorithm]) -> table
 *
 * Create a new IPv6 LPM table.
 *
 * @param algorithm (optional) "wide16" (default) or "stride8"
 * @return LPM table userdata
 */
static int l_new_ipv6(lua_State *L) {
    const char *algo = luaL_optstring(L, 1, "wide16");
    
    lpm_trie_t *trie = NULL;
    
    if (strcmp(algo, "wide16") == 0) {
        trie = lpm_create_ipv6_wide16();
    } else if (strcmp(algo, "stride8") == 0) {
        trie = lpm_create_ipv6_8stride();
    } else {
        return luaL_error(L, "invalid algorithm '%s' (expected 'wide16' or 'stride8')", algo);
    }
    
    if (trie == NULL) {
        return luaL_error(L, "failed to create IPv6 LPM table: out of memory");
    }
    
    lua_lpm_table_t *t = (lua_lpm_table_t *)lua_newuserdata(L, sizeof(lua_lpm_table_t));
    t->trie = trie;
    t->is_ipv6 = true;
    t->closed = false;
    
    luaL_getmetatable(L, LPM_TABLE_MT);
    lua_setmetatable(L, -2);
    
    return 1;
}

/* ============================================================================
 * Table Methods
 * ============================================================================ */

/**
 * table:insert(prefix, prefix_len, next_hop) -> boolean, [error]
 * table:insert(cidr_string, next_hop) -> boolean, [error]
 *
 * Insert a route into the LPM table.
 *
 * @param prefix IP prefix (string, table of bytes, or CIDR string)
 * @param prefix_len Prefix length (0-32 for IPv4, 0-128 for IPv6)
 * @param next_hop Next hop value (unsigned 32-bit integer, max 30 bits for dir24)
 * @return true on success, nil + error message on failure
 */
static int l_insert(lua_State *L) {
    lua_lpm_table_t *t = check_lpm_table(L, 1);
    
    uint8_t prefix[16] = {0};
    uint8_t prefix_len = 0;
    
    int consumed = parse_prefix(L, 2, t->is_ipv6, prefix, &prefix_len);
    if (consumed < 0) {
        lua_pushnil(L);
        lua_pushstring(L, "invalid prefix format");
        return 2;
    }
    
    /* Get next_hop from the appropriate position */
    int nh_idx = (consumed == 0) ? 3 : 4;
    if (!lua_isinteger(L, nh_idx)) {
        lua_pushnil(L);
        lua_pushstring(L, "next_hop must be an integer");
        return 2;
    }
    
    lua_Integer next_hop = lua_tointeger(L, nh_idx);
    if (next_hop < 0 || next_hop > 0x3FFFFFFF) {
        lua_pushnil(L);
        lua_pushstring(L, "next_hop must be between 0 and 0x3FFFFFFF (30-bit value)");
        return 2;
    }
    
    int result = lpm_add(t->trie, prefix, prefix_len, (uint32_t)next_hop);
    if (result != 0) {
        lua_pushnil(L);
        lua_pushstring(L, "insert failed");
        return 2;
    }
    
    lua_pushboolean(L, 1);
    return 1;
}

/**
 * table:delete(prefix, prefix_len) -> boolean, [error]
 * table:delete(cidr_string) -> boolean, [error]
 *
 * Delete a route from the LPM table.
 *
 * @param prefix IP prefix (string, table of bytes, or CIDR string)
 * @param prefix_len Prefix length (required unless using CIDR notation)
 * @return true on success, nil + error message on failure
 */
static int l_delete(lua_State *L) {
    lua_lpm_table_t *t = check_lpm_table(L, 1);
    
    uint8_t prefix[16] = {0};
    uint8_t prefix_len = 0;
    
    int consumed = parse_prefix(L, 2, t->is_ipv6, prefix, &prefix_len);
    if (consumed < 0) {
        lua_pushnil(L);
        lua_pushstring(L, "invalid prefix format");
        return 2;
    }
    
    int result = lpm_delete(t->trie, prefix, prefix_len);
    if (result != 0) {
        lua_pushnil(L);
        lua_pushstring(L, "delete failed (prefix not found)");
        return 2;
    }
    
    lua_pushboolean(L, 1);
    return 1;
}

/**
 * table:lookup(address) -> next_hop or nil
 *
 * Look up an IP address in the LPM table.
 *
 * @param address IP address (string, table of bytes, or binary string)
 * @return next_hop value if found, nil if not found
 */
static int l_lookup(lua_State *L) {
    lua_lpm_table_t *t = check_lpm_table(L, 1);
    
    uint32_t result;
    
    if (t->is_ipv6) {
        uint8_t addr[16];
        if (parse_ipv6_address(L, 2, addr) != 0) {
            return luaL_error(L, "invalid IPv6 address format");
        }
        result = lpm_lookup_ipv6(t->trie, addr);
    } else {
        uint8_t addr[4];
        if (parse_ipv4_address(L, 2, addr) != 0) {
            return luaL_error(L, "invalid IPv4 address format");
        }
        /* Convert to host byte order uint32 */
        uint32_t addr32 = ((uint32_t)addr[0] << 24) | ((uint32_t)addr[1] << 16) |
                          ((uint32_t)addr[2] << 8) | (uint32_t)addr[3];
        result = lpm_lookup_ipv4(t->trie, addr32);
    }
    
    if (result == LPM_INVALID_NEXT_HOP) {
        lua_pushnil(L);
        return 1;
    }
    
    lua_pushinteger(L, result);
    return 1;
}

/**
 * table:lookup_batch(addresses) -> table of results
 *
 * Look up multiple IP addresses in the LPM table.
 *
 * @param addresses Table of IP addresses
 * @return Table where results[i] = next_hop or nil for addresses[i]
 */
static int l_lookup_batch(lua_State *L) {
    lua_lpm_table_t *t = check_lpm_table(L, 1);
    
    luaL_checktype(L, 2, LUA_TTABLE);
    
    /* Get table length */
    lua_len(L, 2);
    lua_Integer count = lua_tointeger(L, -1);
    lua_pop(L, 1);
    
    if (count <= 0) {
        lua_newtable(L);
        return 1;
    }
    
    if (count > LPM_MAX_BATCH_SIZE) {
        return luaL_error(L, "batch size too large (max %d)", LPM_MAX_BATCH_SIZE);
    }
    
    /* Allocate arrays for addresses and results */
    uint32_t *next_hops = (uint32_t *)malloc(count * sizeof(uint32_t));
    if (next_hops == NULL) {
        return luaL_error(L, "out of memory");
    }
    
    /* Create result table */
    lua_createtable(L, (int)count, 0);
    
    if (t->is_ipv6) {
        uint8_t (*addrs)[16] = (uint8_t (*)[16])malloc(count * 16);
        if (addrs == NULL) {
            free(next_hops);
            return luaL_error(L, "out of memory");
        }
        
        /* Parse all addresses */
        for (lua_Integer i = 0; i < count; i++) {
            lua_rawgeti(L, 2, i + 1);
            if (parse_ipv6_address(L, -1, addrs[i]) != 0) {
                free(addrs);
                free(next_hops);
                return luaL_error(L, "invalid IPv6 address at index %d", (int)(i + 1));
            }
            lua_pop(L, 1);
        }
        
        /* Batch lookup */
        lpm_lookup_batch_ipv6(t->trie, (const uint8_t (*)[16])addrs, next_hops, count);
        
        free(addrs);
    } else {
        uint32_t *addrs = (uint32_t *)malloc(count * sizeof(uint32_t));
        if (addrs == NULL) {
            free(next_hops);
            return luaL_error(L, "out of memory");
        }
        
        /* Parse all addresses */
        for (lua_Integer i = 0; i < count; i++) {
            lua_rawgeti(L, 2, i + 1);
            uint8_t addr_bytes[4];
            if (parse_ipv4_address(L, -1, addr_bytes) != 0) {
                free(addrs);
                free(next_hops);
                return luaL_error(L, "invalid IPv4 address at index %d", (int)(i + 1));
            }
            lua_pop(L, 1);
            
            /* Convert to host byte order uint32 */
            addrs[i] = ((uint32_t)addr_bytes[0] << 24) | ((uint32_t)addr_bytes[1] << 16) |
                       ((uint32_t)addr_bytes[2] << 8) | (uint32_t)addr_bytes[3];
        }
        
        /* Batch lookup */
        lpm_lookup_batch_ipv4(t->trie, addrs, next_hops, count);
        
        free(addrs);
    }
    
    /* Build result table */
    for (lua_Integer i = 0; i < count; i++) {
        if (next_hops[i] == LPM_INVALID_NEXT_HOP) {
            /* Leave as nil (don't set anything) */
        } else {
            lua_pushinteger(L, next_hops[i]);
            lua_rawseti(L, -2, i + 1);
        }
    }
    
    free(next_hops);
    return 1;
}

/**
 * table:close() -> void
 *
 * Explicitly close and release resources of the LPM table.
 * Safe to call multiple times.
 */
static int l_close(lua_State *L) {
    lua_lpm_table_t *t = get_lpm_table(L, 1);
    
    if (!t->closed && t->trie != NULL) {
        lpm_destroy(t->trie);
        t->trie = NULL;
        t->closed = true;
    }
    
    return 0;
}

/**
 * table:is_closed() -> boolean
 *
 * Check if the table has been closed.
 *
 * @return true if closed, false otherwise
 */
static int l_is_closed(lua_State *L) {
    lua_lpm_table_t *t = get_lpm_table(L, 1);
    lua_pushboolean(L, t->closed);
    return 1;
}

/**
 * table:is_ipv6() -> boolean
 *
 * Check if the table is for IPv6 addresses.
 *
 * @return true if IPv6, false if IPv4
 */
static int l_is_ipv6(lua_State *L) {
    lua_lpm_table_t *t = get_lpm_table(L, 1);
    lua_pushboolean(L, t->is_ipv6);
    return 1;
}

/**
 * __gc metamethod - garbage collection finalizer
 */
static int l_gc(lua_State *L) {
    lua_lpm_table_t *t = get_lpm_table(L, 1);
    
    if (!t->closed && t->trie != NULL) {
        lpm_destroy(t->trie);
        t->trie = NULL;
        t->closed = true;
    }
    
    return 0;
}

/**
 * __tostring metamethod
 */
static int l_tostring(lua_State *L) {
    lua_lpm_table_t *t = get_lpm_table(L, 1);
    
    if (t->closed) {
        lua_pushstring(L, "lpm.table (closed)");
    } else {
        lua_pushfstring(L, "lpm.table (%s)", t->is_ipv6 ? "IPv6" : "IPv4");
    }
    
    return 1;
}

/* ============================================================================
 * Module Functions (functional style wrappers)
 * ============================================================================ */

/**
 * lpm.insert(table, ...) -> boolean, [error]
 *
 * Functional-style wrapper for table:insert()
 */
static int l_mod_insert(lua_State *L) {
    return l_insert(L);
}

/**
 * lpm.delete(table, ...) -> boolean, [error]
 *
 * Functional-style wrapper for table:delete()
 */
static int l_mod_delete(lua_State *L) {
    return l_delete(L);
}

/**
 * lpm.lookup(table, address) -> next_hop or nil
 *
 * Functional-style wrapper for table:lookup()
 */
static int l_mod_lookup(lua_State *L) {
    return l_lookup(L);
}

/**
 * lpm.lookup_batch(table, addresses) -> results
 *
 * Functional-style wrapper for table:lookup_batch()
 */
static int l_mod_lookup_batch(lua_State *L) {
    return l_lookup_batch(L);
}

/**
 * lpm.close(table)
 *
 * Functional-style wrapper for table:close()
 */
static int l_mod_close(lua_State *L) {
    return l_close(L);
}

/**
 * lpm.is_closed(table) -> boolean
 *
 * Functional-style wrapper for table:is_closed()
 */
static int l_mod_is_closed(lua_State *L) {
    return l_is_closed(L);
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * lpm.version() -> string
 *
 * Get the library version string.
 *
 * @return Version string
 */
static int l_version(lua_State *L) {
    lua_pushstring(L, lpm_get_version());
    return 1;
}

/**
 * lpm.lua_version() -> string
 *
 * Get the Lua binding version string.
 *
 * @return Lua binding version string
 */
static int l_lua_version(lua_State *L) {
    lua_pushstring(L, LPM_LUA_VERSION);
    return 1;
}

/* ============================================================================
 * Module Registration
 * ============================================================================ */

/* Metatable methods for lpm.table */
static const luaL_Reg lpm_table_methods[] = {
    {"insert", l_insert},
    {"delete", l_delete},
    {"lookup", l_lookup},
    {"lookup_batch", l_lookup_batch},
    {"close", l_close},
    {"is_closed", l_is_closed},
    {"is_ipv6", l_is_ipv6},
    {NULL, NULL}
};

/* Module functions */
static const luaL_Reg lpm_module_funcs[] = {
    /* Table creation */
    {"new_ipv4", l_new_ipv4},
    {"new_ipv6", l_new_ipv6},
    
    /* Functional-style wrappers */
    {"insert", l_mod_insert},
    {"delete", l_mod_delete},
    {"lookup", l_mod_lookup},
    {"lookup_batch", l_mod_lookup_batch},
    {"close", l_mod_close},
    {"is_closed", l_mod_is_closed},
    
    /* Utility functions */
    {"version", l_version},
    {"lua_version", l_lua_version},
    
    {NULL, NULL}
};

/**
 * Module initialization function.
 *
 * Called when the module is loaded via require("liblpm").
 */
int luaopen_liblpm(lua_State *L) {
    /* Create metatable for lpm.table userdata */
    luaL_newmetatable(L, LPM_TABLE_MT);
    
    /* Set __index to self for method access */
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    
    /* Set __gc for garbage collection */
    lua_pushcfunction(L, l_gc);
    lua_setfield(L, -2, "__gc");
    
    /* Set __tostring */
    lua_pushcfunction(L, l_tostring);
    lua_setfield(L, -2, "__tostring");
    
    /* Register methods */
    luaL_setfuncs(L, lpm_table_methods, 0);
    
    lua_pop(L, 1); /* Pop metatable */
    
    /* Create module table */
    luaL_newlib(L, lpm_module_funcs);
    
    /* Add constants */
    lua_pushinteger(L, LPM_INVALID_NEXT_HOP);
    lua_setfield(L, -2, "INVALID_NEXT_HOP");
    
    lua_pushinteger(L, LPM_IPV4_MAX_DEPTH);
    lua_setfield(L, -2, "IPV4_MAX_DEPTH");
    
    lua_pushinteger(L, LPM_IPV6_MAX_DEPTH);
    lua_setfield(L, -2, "IPV6_MAX_DEPTH");
    
    /* Add version string */
    lua_pushstring(L, LPM_LUA_VERSION);
    lua_setfield(L, -2, "_VERSION");
    
    return 1;
}
