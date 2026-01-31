#!/usr/bin/env lua
-- ============================================================================
-- basic_example.lua - Basic usage example for liblpm Lua bindings
-- ============================================================================
--
-- This example demonstrates:
-- - Creating IPv4 routing tables
-- - Inserting routes using different formats
-- - Looking up addresses
-- - Proper resource cleanup
--
-- Run: lua examples/basic_example.lua
-- Or via CMake: make lua_example
--
-- ============================================================================

local lpm = require("liblpm")

-- Print header
print("=" .. string.rep("=", 60))
print("liblpm Lua bindings - Basic Example")
print("=" .. string.rep("=", 60))
print(string.format("Library version: %s", lpm.version()))
print(string.format("Lua binding version: %s", lpm._VERSION))
print("")

-- ============================================================================
-- Example 1: Basic IPv4 Routing Table
-- ============================================================================

print("Example 1: Basic IPv4 Routing Table")
print("-" .. string.rep("-", 40))

-- Create an IPv4 routing table using the default (dir24) algorithm
local ipv4_table = lpm.new_ipv4()

-- Define routes with descriptive names
local routes = {
    -- CIDR notation: prefix/length, next_hop
    {"10.0.0.0/8",        100, "Private Class A"},
    {"172.16.0.0/12",     200, "Private Class B"},
    {"192.168.0.0/16",    300, "Private Class C"},
    {"192.168.1.0/24",    301, "LAN Subnet 1"},
    {"192.168.2.0/24",    302, "LAN Subnet 2"},
    {"0.0.0.0/0",         999, "Default Route"},
}

-- Insert routes
print("\nInserting routes:")
for _, route in ipairs(routes) do
    local prefix, next_hop, name = route[1], route[2], route[3]
    local ok, err = ipv4_table:insert(prefix, next_hop)
    if ok then
        print(string.format("  %-20s -> %3d  (%s)", prefix, next_hop, name))
    else
        print(string.format("  %-20s FAILED: %s", prefix, err))
    end
end

-- Lookup some addresses
print("\nLooking up addresses:")
local test_addrs = {
    "10.1.2.3",      -- Should match 10.0.0.0/8 -> 100
    "172.20.1.1",    -- Should match 172.16.0.0/12 -> 200
    "192.168.1.100", -- Should match 192.168.1.0/24 -> 301 (most specific)
    "192.168.2.50",  -- Should match 192.168.2.0/24 -> 302
    "192.168.3.1",   -- Should match 192.168.0.0/16 -> 300
    "8.8.8.8",       -- Should match 0.0.0.0/0 -> 999 (default)
}

for _, addr in ipairs(test_addrs) do
    local next_hop = ipv4_table:lookup(addr)
    if next_hop then
        print(string.format("  %-16s -> next_hop = %d", addr, next_hop))
    else
        print(string.format("  %-16s -> no match", addr))
    end
end

-- Clean up
ipv4_table:close()
print("\nTable closed.")

-- ============================================================================
-- Example 2: Different Input Formats
-- ============================================================================

print("\n" .. "=" .. string.rep("=", 60))
print("Example 2: Different Input Formats")
print("-" .. string.rep("-", 40))

local table2 = lpm.new_ipv4()

-- Format 1: CIDR string
print("\nFormat 1: CIDR string")
table2:insert("10.0.0.0/8", 1)
print("  table2:insert(\"10.0.0.0/8\", 1)")

-- Format 2: Address string + prefix length
print("\nFormat 2: Address + prefix length")
table2:insert("172.16.0.0", 12, 2)
print("  table2:insert(\"172.16.0.0\", 12, 2)")

-- Format 3: Byte table + prefix length
print("\nFormat 3: Byte table + prefix length")
table2:insert({192, 168, 0, 0}, 16, 3)
print("  table2:insert({192, 168, 0, 0}, 16, 3)")

-- Verify all formats work
print("\nVerification:")
print(string.format("  10.1.1.1      -> %d (expected: 1)", table2:lookup("10.1.1.1")))
print(string.format("  172.20.1.1    -> %d (expected: 2)", table2:lookup("172.20.1.1")))
print(string.format("  192.168.1.1   -> %d (expected: 3)", table2:lookup("192.168.1.1")))

-- Lookup with byte table
local nh = table2:lookup({10, 2, 3, 4})
print(string.format("  {10,2,3,4}    -> %d (expected: 1)", nh))

table2:close()

-- ============================================================================
-- Example 3: Route Deletion
-- ============================================================================

print("\n" .. "=" .. string.rep("=", 60))
print("Example 3: Route Deletion")
print("-" .. string.rep("-", 40))

local table3 = lpm.new_ipv4()

-- Insert routes
table3:insert("192.168.0.0/16", 100)
table3:insert("192.168.1.0/24", 200)

print("\nBefore deletion:")
print(string.format("  192.168.1.1 -> %d (matches /24)", table3:lookup("192.168.1.1")))
print(string.format("  192.168.2.1 -> %d (matches /16)", table3:lookup("192.168.2.1")))

-- Delete the more specific route
print("\nDeleting 192.168.1.0/24...")
table3:delete("192.168.1.0/24")

-- Note: DIR-24-8 algorithm delete clears entries without re-applying
-- shorter prefixes. For proper fallback behavior, use stride8 algorithm
-- or re-insert the shorter prefix after deletion.
print("\nAfter deletion:")
local nh1 = table3:lookup("192.168.1.1")
local nh2 = table3:lookup("192.168.2.1")
print(string.format("  192.168.1.1 -> %s", nh1 and tostring(nh1) or "nil"))
print(string.format("  192.168.2.1 -> %s (still matches /16)", nh2 and tostring(nh2) or "nil"))

table3:close()

-- ============================================================================
-- Example 4: Functional API Style
-- ============================================================================

print("\n" .. "=" .. string.rep("=", 60))
print("Example 4: Functional API Style")
print("-" .. string.rep("-", 40))

local table4 = lpm.new_ipv4()

-- Using functional style (lpm.insert instead of table:insert)
lpm.insert(table4, "10.0.0.0/8", 100)
lpm.insert(table4, "192.168.0.0/16", 200)

-- Functional lookup
local nh1 = lpm.lookup(table4, "10.1.1.1")
local nh2 = lpm.lookup(table4, "192.168.1.1")

print(string.format("\n  lpm.lookup(table, \"10.1.1.1\")     -> %d", nh1))
print(string.format("  lpm.lookup(table, \"192.168.1.1\") -> %d", nh2))

-- Functional close
print(string.format("\n  lpm.is_closed(table) -> %s", tostring(lpm.is_closed(table4))))
lpm.close(table4)
print(string.format("  lpm.is_closed(table) -> %s (after close)", tostring(lpm.is_closed(table4))))

-- ============================================================================
-- Example 5: Algorithm Selection
-- ============================================================================

print("\n" .. "=" .. string.rep("=", 60))
print("Example 5: Algorithm Selection")
print("-" .. string.rep("-", 40))

-- DIR-24-8 algorithm (default, optimal for most IPv4 use cases)
print("\nDIR-24-8 algorithm (default):")
local dir24_table = lpm.new_ipv4("dir24")
dir24_table:insert("10.0.0.0/8", 100)
print(string.format("  10.1.1.1 -> %d", dir24_table:lookup("10.1.1.1")))
dir24_table:close()

-- 8-bit stride algorithm
print("\n8-bit stride algorithm:")
local stride8_table = lpm.new_ipv4("stride8")
stride8_table:insert("10.0.0.0/8", 100)
print(string.format("  10.1.1.1 -> %d", stride8_table:lookup("10.1.1.1")))
stride8_table:close()

-- ============================================================================
-- Summary
-- ============================================================================

print("\n" .. "=" .. string.rep("=", 60))
print("Example complete!")
print("=" .. string.rep("=", 60))
