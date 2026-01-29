#!/usr/bin/env lua
-- ============================================================================
-- batch_example.lua - Batch operations example for liblpm Lua bindings
-- ============================================================================
--
-- This example demonstrates:
-- - Batch lookup operations for high throughput
-- - Processing large numbers of addresses efficiently
-- - Performance comparison between single and batch lookups
--
-- Run: lua examples/batch_example.lua
--
-- ============================================================================

local lpm = require("liblpm")

-- Print header
print("=" .. string.rep("=", 60))
print("liblpm Lua bindings - Batch Operations Example")
print("=" .. string.rep("=", 60))
print("")

-- ============================================================================
-- Example 1: Basic Batch Lookup
-- ============================================================================

print("Example 1: Basic Batch Lookup")
print("-" .. string.rep("-", 40))

local table1 = lpm.new_ipv4()

-- Insert routes
table1:insert("10.0.0.0/8", 100)
table1:insert("172.16.0.0/12", 200)
table1:insert("192.168.0.0/16", 300)
table1:insert("0.0.0.0/0", 999)

print("\nRoutes inserted:")
print("  10.0.0.0/8      -> 100 (Private Class A)")
print("  172.16.0.0/12   -> 200 (Private Class B)")
print("  192.168.0.0/16  -> 300 (Private Class C)")
print("  0.0.0.0/0       -> 999 (Default)")

-- Prepare batch of addresses
local addresses = {
    "10.1.1.1",
    "10.2.2.2",
    "172.16.1.1",
    "172.20.1.1",
    "192.168.1.1",
    "192.168.100.50",
    "8.8.8.8",
    "1.1.1.1",
}

print(string.format("\nBatch lookup of %d addresses:", #addresses))

-- Perform batch lookup
local results = table1:lookup_batch(addresses)

-- Display results
print(string.format("\n  %-16s | %s", "Address", "Next Hop"))
print("  " .. string.rep("-", 16) .. " | " .. string.rep("-", 10))
for i, addr in ipairs(addresses) do
    local nh = results[i]
    if nh then
        print(string.format("  %-16s | %d", addr, nh))
    else
        print(string.format("  %-16s | (no match)", addr))
    end
end

table1:close()

-- ============================================================================
-- Example 2: Large Batch Processing
-- ============================================================================

print("\n" .. "=" .. string.rep("=", 60))
print("Example 2: Large Batch Processing")
print("-" .. string.rep("-", 40))

local table2 = lpm.new_ipv4()

-- Insert a variety of routes
print("\nBuilding routing table...")
local route_count = 0

-- Add some /8 routes
for i = 1, 10 do
    table2:insert(string.format("%d.0.0.0/8", i), i)
    route_count = route_count + 1
end

-- Add some /16 routes
for i = 11, 50 do
    table2:insert(string.format("10.%d.0.0/16", i), i)
    route_count = route_count + 1
end

-- Add some /24 routes
for i = 1, 100 do
    table2:insert(string.format("192.168.%d.0/24", i), 1000 + i)
    route_count = route_count + 1
end

-- Default route
table2:insert("0.0.0.0/0", 9999)
route_count = route_count + 1

print(string.format("  Inserted %d routes", route_count))

-- Generate test addresses
local function generate_random_ip()
    return string.format("%d.%d.%d.%d",
        math.random(0, 255),
        math.random(0, 255),
        math.random(0, 255),
        math.random(0, 255))
end

-- Set random seed for reproducibility
math.randomseed(12345)

local batch_size = 10000
print(string.format("\nGenerating %d random addresses...", batch_size))

local large_batch = {}
for i = 1, batch_size do
    large_batch[i] = generate_random_ip()
end

-- Time the batch lookup
print("Performing batch lookup...")
local start_time = os.clock()
local batch_results = table2:lookup_batch(large_batch)
local batch_time = os.clock() - start_time

print(string.format("\nBatch lookup completed:"))
print(string.format("  Addresses:  %d", batch_size))
print(string.format("  Time:       %.4f seconds", batch_time))
print(string.format("  Rate:       %.0f lookups/sec", batch_size / batch_time))

-- Count matches
local match_count = 0
for i = 1, batch_size do
    if batch_results[i] then
        match_count = match_count + 1
    end
end
print(string.format("  Matches:    %d (%.1f%%)", match_count, 100 * match_count / batch_size))

table2:close()

-- ============================================================================
-- Example 3: IPv6 Batch Lookup
-- ============================================================================

print("\n" .. "=" .. string.rep("=", 60))
print("Example 3: IPv6 Batch Lookup")
print("-" .. string.rep("-", 40))

local table3 = lpm.new_ipv6()

-- Insert IPv6 routes
table3:insert("2001:db8::/32", 100)
table3:insert("2001:db8:1::/48", 200)
table3:insert("fe80::/10", 300)
table3:insert("::/0", 999)

print("\nIPv6 routes:")
print("  2001:db8::/32   -> 100")
print("  2001:db8:1::/48 -> 200")
print("  fe80::/10       -> 300")
print("  ::/0            -> 999")

-- IPv6 batch
local ipv6_batch = {
    "2001:db8::1",
    "2001:db8:1::1",
    "2001:db8:2::1",
    "fe80::1",
    "fe80::abcd:1234",
    "2607:f8b0:4004::1",  -- Google IPv6
    "::1",
}

print(string.format("\nBatch lookup of %d IPv6 addresses:", #ipv6_batch))

local ipv6_results = table3:lookup_batch(ipv6_batch)

print(string.format("\n  %-24s | %s", "Address", "Next Hop"))
print("  " .. string.rep("-", 24) .. " | " .. string.rep("-", 10))
for i, addr in ipairs(ipv6_batch) do
    local nh = ipv6_results[i]
    if nh then
        print(string.format("  %-24s | %d", addr, nh))
    else
        print(string.format("  %-24s | (no match)", addr))
    end
end

table3:close()

-- ============================================================================
-- Example 4: Batch with Mixed Formats
-- ============================================================================

print("\n" .. "=" .. string.rep("=", 60))
print("Example 4: Batch with Mixed Formats")
print("-" .. string.rep("-", 40))

local table4 = lpm.new_ipv4()
table4:insert("10.0.0.0/8", 100)
table4:insert("192.168.0.0/16", 200)

print("\nAddresses in different formats:")

-- Mix of formats in the batch
local mixed_batch = {
    "10.1.1.1",           -- String format
    "10.2.2.2",           -- String format
    {192, 168, 1, 1},     -- Byte table format
    {192, 168, 2, 2},     -- Byte table format
    string.char(10, 3, 3, 3),  -- Binary string format
}

-- Note: All formats work in the same batch
local mixed_results = table4:lookup_batch(mixed_batch)

print("  \"10.1.1.1\"              -> " .. tostring(mixed_results[1]))
print("  \"10.2.2.2\"              -> " .. tostring(mixed_results[2]))
print("  {192, 168, 1, 1}        -> " .. tostring(mixed_results[3]))
print("  {192, 168, 2, 2}        -> " .. tostring(mixed_results[4]))
print("  (binary 10.3.3.3)       -> " .. tostring(mixed_results[5]))

table4:close()

-- ============================================================================
-- Example 5: Packet Processing Simulation
-- ============================================================================

print("\n" .. "=" .. string.rep("=", 60))
print("Example 5: Packet Processing Simulation")
print("-" .. string.rep("-", 40))

local router = lpm.new_ipv4()

-- Simulate a router's routing table
print("\nSetting up router...")
router:insert("0.0.0.0/0", 1)       -- Default -> Interface 1
router:insert("10.0.0.0/8", 2)      -- Internal -> Interface 2
router:insert("172.16.0.0/12", 3)   -- VPN -> Interface 3
router:insert("192.168.1.0/24", 4)  -- LAN1 -> Interface 4
router:insert("192.168.2.0/24", 5)  -- LAN2 -> Interface 5

-- Simulate incoming packet burst
local function generate_packet_burst(count)
    local packets = {}
    for i = 1, count do
        -- Simulate realistic traffic distribution
        local r = math.random(100)
        if r <= 40 then
            -- 40% internal traffic
            packets[i] = string.format("10.%d.%d.%d", 
                math.random(0, 255), math.random(0, 255), math.random(0, 255))
        elseif r <= 60 then
            -- 20% LAN traffic
            packets[i] = string.format("192.168.%d.%d",
                math.random(1, 2), math.random(1, 254))
        elseif r <= 70 then
            -- 10% VPN traffic
            packets[i] = string.format("172.%d.%d.%d",
                math.random(16, 31), math.random(0, 255), math.random(0, 255))
        else
            -- 30% external traffic (default route)
            packets[i] = string.format("%d.%d.%d.%d",
                math.random(11, 223), math.random(0, 255), 
                math.random(0, 255), math.random(0, 255))
        end
    end
    return packets
end

-- Process multiple bursts
local total_packets = 0
local total_time = 0
local interface_counts = {0, 0, 0, 0, 0}

print("\nProcessing packet bursts:")
for burst = 1, 5 do
    local packets = generate_packet_burst(5000)
    local start = os.clock()
    local decisions = router:lookup_batch(packets)
    local elapsed = os.clock() - start
    
    total_packets = total_packets + #packets
    total_time = total_time + elapsed
    
    -- Count interface assignments
    for _, iface in ipairs(decisions) do
        if iface and iface >= 1 and iface <= 5 then
            interface_counts[iface] = interface_counts[iface] + 1
        end
    end
    
    print(string.format("  Burst %d: %d packets in %.4fs (%.0f pps)",
        burst, #packets, elapsed, #packets / elapsed))
end

print(string.format("\nTotal: %d packets in %.4fs (%.0f pps average)",
    total_packets, total_time, total_packets / total_time))

print("\nTraffic distribution by interface:")
local iface_names = {"Default", "Internal", "VPN", "LAN1", "LAN2"}
for i = 1, 5 do
    print(string.format("  Interface %d (%s): %d packets (%.1f%%)",
        i, iface_names[i], interface_counts[i], 
        100 * interface_counts[i] / total_packets))
end

router:close()

-- ============================================================================
-- Summary
-- ============================================================================

print("\n" .. "=" .. string.rep("=", 60))
print("Batch Operations Example complete!")
print("")
print("Key takeaways:")
print("  - Batch lookups are more efficient for multiple addresses")
print("  - Use lookup_batch() when processing packet bursts")
print("  - Mixed address formats work in the same batch")
print("  - IPv6 batch operations work the same way as IPv4")
print("=" .. string.rep("=", 60))
