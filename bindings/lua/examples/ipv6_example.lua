#!/usr/bin/env lua
-- ============================================================================
-- ipv6_example.lua - IPv6 usage example for liblpm Lua bindings
-- ============================================================================
--
-- This example demonstrates:
-- - Creating IPv6 routing tables
-- - Inserting IPv6 routes with various notation styles
-- - IPv6 address lookups
-- - Algorithm selection for IPv6
--
-- Run: lua examples/ipv6_example.lua
--
-- ============================================================================

local lpm = require("liblpm")

-- Print header
print("=" .. string.rep("=", 60))
print("liblpm Lua bindings - IPv6 Example")
print("=" .. string.rep("=", 60))
print("")

-- ============================================================================
-- Example 1: Basic IPv6 Routing
-- ============================================================================

print("Example 1: Basic IPv6 Routing")
print("-" .. string.rep("-", 40))

-- Create IPv6 table with default (wide16) algorithm
local ipv6_table = lpm.new_ipv6()

-- Common IPv6 prefixes and their uses
local routes = {
    -- Documentation prefix (RFC 3849)
    {"2001:db8::/32",       100, "Documentation prefix"},
    {"2001:db8:1::/48",     101, "Documentation /48 allocation"},
    {"2001:db8:1:2::/64",   102, "Documentation /64 subnet"},
    
    -- Link-local (fe80::/10)
    {"fe80::/10",           200, "Link-local addresses"},
    
    -- Loopback (::1/128)
    {"::1/128",             300, "Loopback address"},
    
    -- IPv4-mapped addresses (::ffff:0:0/96)
    {"::ffff:0:0/96",       400, "IPv4-mapped addresses"},
    
    -- Unique Local Addresses (fc00::/7)
    {"fc00::/7",            500, "Unique Local Addresses"},
    
    -- Default route
    {"::/0",                999, "Default route"},
}

print("\nInserting IPv6 routes:")
for _, route in ipairs(routes) do
    local prefix, next_hop, name = route[1], route[2], route[3]
    local ok, err = ipv6_table:insert(prefix, next_hop)
    if ok then
        print(string.format("  %-24s -> %3d  (%s)", prefix, next_hop, name))
    else
        print(string.format("  %-24s FAILED: %s", prefix, err))
    end
end

-- Lookup test addresses
print("\nLooking up IPv6 addresses:")
local test_addrs = {
    "2001:db8::1",          -- Matches /32 -> 100
    "2001:db8:1::1",        -- Matches /48 -> 101
    "2001:db8:1:2::1",      -- Matches /64 -> 102
    "fe80::1",              -- Matches link-local -> 200
    "::1",                  -- Matches loopback -> 300
    "::ffff:192.168.1.1",   -- Matches IPv4-mapped -> 400
    "fd00::1",              -- Matches ULA -> 500
    "2607:f8b0:4004::1",    -- Matches default -> 999 (Google)
}

for _, addr in ipairs(test_addrs) do
    local next_hop = ipv6_table:lookup(addr)
    if next_hop then
        print(string.format("  %-24s -> %d", addr, next_hop))
    else
        print(string.format("  %-24s -> no match", addr))
    end
end

ipv6_table:close()

-- ============================================================================
-- Example 2: IPv6 Address Formats
-- ============================================================================

print("\n" .. "=" .. string.rep("=", 60))
print("Example 2: IPv6 Address Formats")
print("-" .. string.rep("-", 40))

local table2 = lpm.new_ipv6()

-- Full expanded notation
print("\nFull expanded notation:")
table2:insert("2001:0db8:0000:0000:0000:0000:0000:0000/32", 1)
print("  Inserted: 2001:0db8:0000:0000:0000:0000:0000:0000/32")

-- Compressed notation with ::
print("\nCompressed notation:")
table2:insert("2001:db8:1::/48", 2)
print("  Inserted: 2001:db8:1::/48")

-- Leading zeros omitted
table2:insert("2001:db8:a:b::/64", 3)
print("  Inserted: 2001:db8:a:b::/64")

-- Verify lookups
print("\nVerification:")
print(string.format("  2001:db8::1        -> %d", table2:lookup("2001:db8::1")))
print(string.format("  2001:db8:1::1      -> %d", table2:lookup("2001:db8:1::1")))
print(string.format("  2001:db8:a:b::1    -> %d", table2:lookup("2001:db8:a:b::1")))

table2:close()

-- ============================================================================
-- Example 3: IPv6 Longest Prefix Matching
-- ============================================================================

print("\n" .. "=" .. string.rep("=", 60))
print("Example 3: IPv6 Longest Prefix Matching")
print("-" .. string.rep("-", 40))

local table3 = lpm.new_ipv6()

-- Create overlapping prefixes
print("\nInserting overlapping prefixes:")
local prefixes = {
    {"2001:db8::/32", 32},
    {"2001:db8:abcd::/48", 48},
    {"2001:db8:abcd:ef01::/64", 64},
    {"2001:db8:abcd:ef01:2345::/80", 80},
}

for _, p in ipairs(prefixes) do
    table3:insert(p[1], p[2])
    print(string.format("  %s -> %d", p[1], p[2]))
end

-- Test longest prefix matching
print("\nLongest prefix match tests:")
local tests = {
    {"2001:db8::1", 32, "/32"},
    {"2001:db8:abcd::1", 48, "/48"},
    {"2001:db8:abcd:ef01::1", 64, "/64"},
    {"2001:db8:abcd:ef01:2345::1", 80, "/80"},
}

for _, test in ipairs(tests) do
    local addr, expected, desc = test[1], test[2], test[3]
    local result = table3:lookup(addr)
    local status = (result == expected) and "OK" or "MISMATCH"
    local result_str = result and tostring(result) or "nil"
    print(string.format("  %-32s -> %s (expected %s) [%s]", 
        addr, result_str, desc, status))
end

table3:close()

-- ============================================================================
-- Example 4: Algorithm Comparison
-- ============================================================================

print("\n" .. "=" .. string.rep("=", 60))
print("Example 4: IPv6 Algorithm Comparison")
print("-" .. string.rep("-", 40))

-- Wide 16-bit stride (default, good for common /48 allocations)
print("\nWide 16-bit stride algorithm (default):")
local wide16 = lpm.new_ipv6("wide16")
wide16:insert("2001:db8::/32", 100)
wide16:insert("fe80::/10", 200)
print(string.format("  2001:db8::1  -> %d", wide16:lookup("2001:db8::1")))
print(string.format("  fe80::1      -> %d", wide16:lookup("fe80::1")))
wide16:close()

-- 8-bit stride algorithm
print("\n8-bit stride algorithm:")
local stride8 = lpm.new_ipv6("stride8")
stride8:insert("2001:db8::/32", 100)
stride8:insert("fe80::/10", 200)
print(string.format("  2001:db8::1  -> %d", stride8:lookup("2001:db8::1")))
print(string.format("  fe80::1      -> %d", stride8:lookup("fe80::1")))
stride8:close()

print("\nBoth algorithms produce the same results.")
print("wide16 is optimized for common /48 allocations (ISP assignments).")
print("stride8 is simpler and may use less memory for sparse tables.")

-- ============================================================================
-- Example 5: IPv6 Host Routes (/128)
-- ============================================================================

print("\n" .. "=" .. string.rep("=", 60))
print("Example 5: IPv6 Host Routes (/128)")
print("-" .. string.rep("-", 40))

local table5 = lpm.new_ipv6()

-- Insert host-specific routes
print("\nInserting host routes:")
table5:insert("2001:db8::/32", 1)  -- Network
table5:insert("2001:db8::1/128", 100)  -- Specific host
table5:insert("2001:db8::2/128", 200)  -- Another host

print("  2001:db8::/32     -> 1   (network)")
print("  2001:db8::1/128   -> 100 (host 1)")
print("  2001:db8::2/128   -> 200 (host 2)")

print("\nLookups:")
print(string.format("  2001:db8::1    -> %d (exact match)", table5:lookup("2001:db8::1")))
print(string.format("  2001:db8::2    -> %d (exact match)", table5:lookup("2001:db8::2")))
print(string.format("  2001:db8::3    -> %d (falls back to /32)", table5:lookup("2001:db8::3")))

table5:close()

-- ============================================================================
-- Summary
-- ============================================================================

print("\n" .. "=" .. string.rep("=", 60))
print("IPv6 Example complete!")
print("=" .. string.rep("=", 60))
