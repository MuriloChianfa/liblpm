#!/usr/bin/env lua
-- ============================================================================
-- test_lpm.lua - Comprehensive test suite for liblpm Lua bindings
-- ============================================================================
--
-- Run: lua tests/test_lpm.lua
-- Or via CMake: make lua_test
--
-- ============================================================================

local lpm = require("liblpm")

-- ============================================================================
-- Test Framework
-- ============================================================================

local tests = {}
local passed = 0
local failed = 0
local current_test = nil

local function test(name, fn)
    tests[#tests + 1] = {name = name, fn = fn}
end

local function assert_eq(actual, expected, msg)
    if actual ~= expected then
        error(string.format("%s: expected %s, got %s",
            msg or "assertion failed",
            tostring(expected),
            tostring(actual)), 2)
    end
end

local function assert_nil(value, msg)
    if value ~= nil then
        error(string.format("%s: expected nil, got %s",
            msg or "assertion failed",
            tostring(value)), 2)
    end
end

local function assert_not_nil(value, msg)
    if value == nil then
        error((msg or "assertion failed") .. ": expected non-nil value", 2)
    end
end

local function assert_true(value, msg)
    if not value then
        error((msg or "assertion failed") .. ": expected true", 2)
    end
end

local function assert_false(value, msg)
    if value then
        error((msg or "assertion failed") .. ": expected false", 2)
    end
end

local function assert_error(fn, msg)
    local ok, err = pcall(fn)
    if ok then
        error((msg or "assertion failed") .. ": expected error", 2)
    end
    return err
end

local function run_tests()
    print(string.format("Running %d tests...\n", #tests))
    
    for _, t in ipairs(tests) do
        current_test = t.name
        io.write(string.format("  %-50s ", t.name))
        io.flush()
        
        local ok, err = pcall(t.fn)
        if ok then
            passed = passed + 1
            print("[PASS]")
        else
            failed = failed + 1
            print("[FAIL]")
            print(string.format("    Error: %s", tostring(err)))
        end
    end
    
    print(string.format("\n%d passed, %d failed, %d total",
        passed, failed, passed + failed))
    
    if failed > 0 then
        os.exit(1)
    end
end

-- ============================================================================
-- Module Tests
-- ============================================================================

test("module loads correctly", function()
    assert_not_nil(lpm, "lpm module should load")
    assert_not_nil(lpm.new_ipv4, "new_ipv4 function should exist")
    assert_not_nil(lpm.new_ipv6, "new_ipv6 function should exist")
    assert_not_nil(lpm.version, "version function should exist")
end)

test("version returns string", function()
    local ver = lpm.version()
    assert_not_nil(ver, "version should not be nil")
    assert_eq(type(ver), "string", "version should be a string")
    assert_true(#ver > 0, "version should not be empty")
end)

test("constants are defined", function()
    assert_not_nil(lpm.INVALID_NEXT_HOP, "INVALID_NEXT_HOP should be defined")
    assert_eq(lpm.INVALID_NEXT_HOP, 0xFFFFFFFF, "INVALID_NEXT_HOP value")
    assert_eq(lpm.IPV4_MAX_DEPTH, 32, "IPV4_MAX_DEPTH value")
    assert_eq(lpm.IPV6_MAX_DEPTH, 128, "IPV6_MAX_DEPTH value")
end)

-- ============================================================================
-- IPv4 Table Creation Tests
-- ============================================================================

test("create IPv4 table (default algorithm)", function()
    local t = lpm.new_ipv4()
    assert_not_nil(t, "table should be created")
    assert_false(t:is_closed(), "table should not be closed")
    assert_false(t:is_ipv6(), "table should be IPv4")
    t:close()
end)

test("create IPv4 table (dir24 algorithm)", function()
    local t = lpm.new_ipv4("dir24")
    assert_not_nil(t, "table should be created")
    t:close()
end)

test("create IPv4 table (stride8 algorithm)", function()
    local t = lpm.new_ipv4("stride8")
    assert_not_nil(t, "table should be created")
    t:close()
end)

test("create IPv4 table with invalid algorithm fails", function()
    assert_error(function()
        lpm.new_ipv4("invalid")
    end, "invalid algorithm should error")
end)

-- ============================================================================
-- IPv6 Table Creation Tests
-- ============================================================================

test("create IPv6 table (default algorithm)", function()
    local t = lpm.new_ipv6()
    assert_not_nil(t, "table should be created")
    assert_false(t:is_closed(), "table should not be closed")
    assert_true(t:is_ipv6(), "table should be IPv6")
    t:close()
end)

test("create IPv6 table (wide16 algorithm)", function()
    local t = lpm.new_ipv6("wide16")
    assert_not_nil(t, "table should be created")
    t:close()
end)

test("create IPv6 table (stride8 algorithm)", function()
    local t = lpm.new_ipv6("stride8")
    assert_not_nil(t, "table should be created")
    t:close()
end)

-- ============================================================================
-- Table Lifecycle Tests
-- ============================================================================

test("close table", function()
    local t = lpm.new_ipv4()
    assert_false(t:is_closed(), "table should not be closed initially")
    t:close()
    assert_true(t:is_closed(), "table should be closed after close()")
end)

test("double close is safe", function()
    local t = lpm.new_ipv4()
    t:close()
    t:close()  -- Should not error
    assert_true(t:is_closed(), "table should remain closed")
end)

test("operations on closed table fail", function()
    local t = lpm.new_ipv4()
    t:close()
    
    assert_error(function()
        t:insert("192.168.0.0/16", 100)
    end, "insert on closed table should fail")
    
    assert_error(function()
        t:lookup("192.168.1.1")
    end, "lookup on closed table should fail")
end)

test("tostring works", function()
    local t = lpm.new_ipv4()
    local s = tostring(t)
    assert_true(s:find("IPv4") ~= nil, "tostring should mention IPv4")
    t:close()
    s = tostring(t)
    assert_true(s:find("closed") ~= nil, "tostring should mention closed")
end)

-- ============================================================================
-- IPv4 Insert Tests
-- ============================================================================

test("insert IPv4 with CIDR string", function()
    local t = lpm.new_ipv4()
    local ok, err = t:insert("192.168.0.0/16", 100)
    assert_true(ok, "insert should succeed")
    t:close()
end)

test("insert IPv4 with address + prefix_len", function()
    local t = lpm.new_ipv4()
    local ok, err = t:insert("10.0.0.0", 8, 200)
    assert_true(ok, "insert should succeed")
    t:close()
end)

test("insert IPv4 with byte table", function()
    local t = lpm.new_ipv4()
    local ok, err = t:insert({172, 16, 0, 0}, 12, 300)
    assert_true(ok, "insert should succeed")
    t:close()
end)

test("insert IPv4 default route", function()
    local t = lpm.new_ipv4()
    local ok, err = t:insert("0.0.0.0/0", 999)
    assert_true(ok, "insert should succeed")
    t:close()
end)

test("insert IPv4 host route (/32)", function()
    local t = lpm.new_ipv4()
    local ok, err = t:insert("192.168.1.100/32", 500)
    assert_true(ok, "insert should succeed")
    t:close()
end)

-- ============================================================================
-- IPv4 Lookup Tests
-- ============================================================================

test("lookup IPv4 with string address", function()
    local t = lpm.new_ipv4()
    t:insert("192.168.0.0/16", 100)
    
    local nh = t:lookup("192.168.1.1")
    assert_eq(nh, 100, "should find next hop")
    t:close()
end)

test("lookup IPv4 with byte table", function()
    local t = lpm.new_ipv4()
    t:insert("10.0.0.0/8", 200)
    
    local nh = t:lookup({10, 1, 2, 3})
    assert_eq(nh, 200, "should find next hop")
    t:close()
end)

test("lookup IPv4 no match returns nil", function()
    local t = lpm.new_ipv4()
    t:insert("192.168.0.0/16", 100)
    
    local nh = t:lookup("10.0.0.1")
    assert_nil(nh, "should return nil for no match")
    t:close()
end)

test("lookup IPv4 longest prefix match", function()
    local t = lpm.new_ipv4()
    t:insert("10.0.0.0/8", 100)
    t:insert("10.1.0.0/16", 200)
    t:insert("10.1.2.0/24", 300)
    
    assert_eq(t:lookup("10.0.0.1"), 100, "should match /8")
    assert_eq(t:lookup("10.1.0.1"), 200, "should match /16")
    assert_eq(t:lookup("10.1.2.1"), 300, "should match /24")
    t:close()
end)

test("lookup IPv4 default route fallback", function()
    local t = lpm.new_ipv4()
    t:insert("0.0.0.0/0", 999)
    t:insert("192.168.0.0/16", 100)
    
    assert_eq(t:lookup("192.168.1.1"), 100, "should match specific")
    assert_eq(t:lookup("8.8.8.8"), 999, "should match default")
    t:close()
end)

-- ============================================================================
-- IPv4 Delete Tests
-- ============================================================================

test("delete IPv4 with CIDR string", function()
    local t = lpm.new_ipv4()
    t:insert("192.168.0.0/16", 100)
    
    local ok, err = t:delete("192.168.0.0/16")
    assert_true(ok, "delete should succeed")
    
    local nh = t:lookup("192.168.1.1")
    assert_nil(nh, "should not find deleted route")
    t:close()
end)

test("delete IPv4 non-existent route is idempotent", function()
    local t = lpm.new_ipv4()
    
    -- Delete is idempotent - deleting a non-existent route succeeds
    local ok, err = t:delete("192.168.0.0/16")
    assert_true(ok, "delete of non-existent route should succeed (idempotent)")
    t:close()
end)

test("delete IPv4 preserves other routes", function()
    local t = lpm.new_ipv4()
    t:insert("10.0.0.0/8", 100)
    t:insert("192.168.0.0/16", 200)
    
    t:delete("10.0.0.0/8")
    
    assert_nil(t:lookup("10.1.1.1"), "deleted route should not match")
    assert_eq(t:lookup("192.168.1.1"), 200, "other route should remain")
    t:close()
end)

-- ============================================================================
-- IPv6 Insert and Lookup Tests
-- ============================================================================

test("insert and lookup IPv6 CIDR", function()
    local t = lpm.new_ipv6()
    local ok = t:insert("2001:db8::/32", 100)
    assert_true(ok, "insert should succeed")
    
    local nh = t:lookup("2001:db8::1")
    assert_eq(nh, 100, "should find next hop")
    t:close()
end)

test("insert and lookup IPv6 with address + prefix_len", function()
    local t = lpm.new_ipv6()
    local ok = t:insert("fe80::", 10, 200)
    assert_true(ok, "insert should succeed")
    
    local nh = t:lookup("fe80::1")
    assert_eq(nh, 200, "should find next hop")
    t:close()
end)

test("IPv6 longest prefix match", function()
    local t = lpm.new_ipv6()
    t:insert("2001:db8::/32", 100)
    t:insert("2001:db8:1::/48", 200)
    t:insert("2001:db8:1:2::/64", 300)
    
    assert_eq(t:lookup("2001:db8::1"), 100, "should match /32")
    assert_eq(t:lookup("2001:db8:1::1"), 200, "should match /48")
    assert_eq(t:lookup("2001:db8:1:2::1"), 300, "should match /64")
    t:close()
end)

test("IPv6 no match returns nil", function()
    local t = lpm.new_ipv6()
    t:insert("2001:db8::/32", 100)
    
    local nh = t:lookup("2001:db9::1")
    assert_nil(nh, "should return nil for no match")
    t:close()
end)

test("IPv6 default route", function()
    local t = lpm.new_ipv6()
    t:insert("::/0", 999)
    
    local nh = t:lookup("2001:db8::1")
    assert_eq(nh, 999, "should match default route")
    t:close()
end)

test("IPv6 host route (/128)", function()
    local t = lpm.new_ipv6()
    t:insert("2001:db8::1/128", 500)
    
    assert_eq(t:lookup("2001:db8::1"), 500, "should match exact host")
    assert_nil(t:lookup("2001:db8::2"), "should not match different host")
    t:close()
end)

-- ============================================================================
-- Batch Lookup Tests
-- ============================================================================

test("batch lookup IPv4", function()
    local t = lpm.new_ipv4()
    t:insert("10.0.0.0/8", 100)
    t:insert("192.168.0.0/16", 200)
    
    local addrs = {"10.1.1.1", "192.168.1.1", "8.8.8.8"}
    local results = t:lookup_batch(addrs)
    
    assert_eq(results[1], 100, "first should match /8")
    assert_eq(results[2], 200, "second should match /16")
    assert_nil(results[3], "third should not match")
    t:close()
end)

test("batch lookup IPv4 with byte tables", function()
    local t = lpm.new_ipv4()
    t:insert("10.0.0.0/8", 100)
    
    local addrs = {{10, 1, 1, 1}, {10, 2, 2, 2}}
    local results = t:lookup_batch(addrs)
    
    assert_eq(results[1], 100, "first should match")
    assert_eq(results[2], 100, "second should match")
    t:close()
end)

test("batch lookup IPv6", function()
    local t = lpm.new_ipv6()
    t:insert("2001:db8::/32", 100)
    t:insert("fe80::/10", 200)
    
    local addrs = {"2001:db8::1", "fe80::1", "::1"}
    local results = t:lookup_batch(addrs)
    
    assert_eq(results[1], 100, "first should match")
    assert_eq(results[2], 200, "second should match")
    assert_nil(results[3], "third should not match")
    t:close()
end)

test("batch lookup empty table", function()
    local t = lpm.new_ipv4()
    local results = t:lookup_batch({})
    assert_eq(#results, 0, "empty batch should return empty table")
    t:close()
end)

test("batch lookup large batch", function()
    local t = lpm.new_ipv4()
    t:insert("0.0.0.0/0", 100)
    
    -- Create 1000 addresses
    local addrs = {}
    for i = 1, 1000 do
        addrs[i] = string.format("%d.%d.%d.%d", 
            i % 256, (i // 256) % 256, (i // 65536) % 256, 0)
    end
    
    local results = t:lookup_batch(addrs)
    assert_eq(#results, 1000, "should return 1000 results")
    
    for i = 1, 1000 do
        assert_eq(results[i], 100, string.format("result %d should match", i))
    end
    t:close()
end)

-- ============================================================================
-- Functional API Tests
-- ============================================================================

test("functional API insert", function()
    local t = lpm.new_ipv4()
    local ok = lpm.insert(t, "192.168.0.0/16", 100)
    assert_true(ok, "functional insert should work")
    t:close()
end)

test("functional API lookup", function()
    local t = lpm.new_ipv4()
    lpm.insert(t, "192.168.0.0/16", 100)
    local nh = lpm.lookup(t, "192.168.1.1")
    assert_eq(nh, 100, "functional lookup should work")
    t:close()
end)

test("functional API delete", function()
    local t = lpm.new_ipv4()
    lpm.insert(t, "192.168.0.0/16", 100)
    local ok = lpm.delete(t, "192.168.0.0/16")
    assert_true(ok, "functional delete should work")
    t:close()
end)

test("functional API close and is_closed", function()
    local t = lpm.new_ipv4()
    assert_false(lpm.is_closed(t), "should not be closed")
    lpm.close(t)
    assert_true(lpm.is_closed(t), "should be closed")
end)

-- ============================================================================
-- Error Handling Tests
-- ============================================================================

test("insert with invalid CIDR fails", function()
    local t = lpm.new_ipv4()
    local ok, err = t:insert("invalid/cidr", 100)
    assert_nil(ok, "should fail")
    assert_not_nil(err, "should have error message")
    t:close()
end)

test("insert with invalid prefix length fails", function()
    local t = lpm.new_ipv4()
    local ok, err = t:insert("192.168.0.0/33", 100)
    assert_nil(ok, "should fail for /33")
    t:close()
end)

test("insert with negative prefix length fails", function()
    local t = lpm.new_ipv4()
    local ok, err = t:insert("192.168.0.0", -1, 100)
    assert_nil(ok, "should fail for negative prefix")
    t:close()
end)

test("insert with invalid next_hop fails", function()
    local t = lpm.new_ipv4()
    local ok, err = t:insert("192.168.0.0/16", -1)
    assert_nil(ok, "should fail for negative next_hop")
    t:close()
end)

test("lookup with invalid address type fails", function()
    local t = lpm.new_ipv4()
    assert_error(function()
        t:lookup(12345)  -- number instead of string
    end, "should fail for invalid type")
    t:close()
end)

test("IPv4 lookup with IPv6 address fails", function()
    local t = lpm.new_ipv4()
    t:insert("192.168.0.0/16", 100)
    assert_error(function()
        t:lookup("2001:db8::1")
    end, "should fail for IPv6 address in IPv4 table")
    t:close()
end)

-- ============================================================================
-- Edge Cases
-- ============================================================================

test("overlapping prefixes", function()
    local t = lpm.new_ipv4()
    t:insert("10.0.0.0/8", 1)
    t:insert("10.0.0.0/16", 2)
    t:insert("10.0.0.0/24", 3)
    
    assert_eq(t:lookup("10.0.0.1"), 3, "most specific should win")
    assert_eq(t:lookup("10.0.1.1"), 2, "next specific should win")
    assert_eq(t:lookup("10.1.0.1"), 1, "least specific should win")
    t:close()
end)

test("update route (re-insert)", function()
    local t = lpm.new_ipv4()
    t:insert("192.168.0.0/16", 100)
    assert_eq(t:lookup("192.168.1.1"), 100)
    
    t:insert("192.168.0.0/16", 200)  -- Update
    assert_eq(t:lookup("192.168.1.1"), 200, "should have updated value")
    t:close()
end)

test("many routes", function()
    local t = lpm.new_ipv4()
    
    -- Insert 1000 routes
    for i = 0, 999 do
        local a = (i // 256) % 256
        local b = i % 256
        t:insert(string.format("10.%d.%d.0/24", a, b), i)
    end
    
    -- Verify some
    assert_eq(t:lookup("10.0.0.1"), 0)
    assert_eq(t:lookup("10.0.255.1"), 255)
    assert_eq(t:lookup("10.3.231.1"), 999)
    t:close()
end)

test("binary string address", function()
    local t = lpm.new_ipv4()
    t:insert("192.168.0.0/16", 100)
    
    -- 4-byte binary string for 192.168.1.1
    local addr = string.char(192, 168, 1, 1)
    local nh = t:lookup(addr)
    assert_eq(nh, 100, "binary string lookup should work")
    t:close()
end)

-- ============================================================================
-- Algorithm Comparison Tests
-- ============================================================================

test("dir24 and stride8 give same results", function()
    local t1 = lpm.new_ipv4("dir24")
    local t2 = lpm.new_ipv4("stride8")
    
    -- Same routes
    local routes = {
        {"10.0.0.0/8", 100},
        {"192.168.0.0/16", 200},
        {"172.16.0.0/12", 300},
    }
    
    for _, r in ipairs(routes) do
        t1:insert(r[1], r[2])
        t2:insert(r[1], r[2])
    end
    
    -- Same lookups
    local addrs = {"10.1.1.1", "192.168.1.1", "172.17.1.1", "8.8.8.8"}
    for _, addr in ipairs(addrs) do
        assert_eq(t1:lookup(addr), t2:lookup(addr),
            string.format("results should match for %s", addr))
    end
    
    t1:close()
    t2:close()
end)

test("wide16 and stride8 give same results for IPv6", function()
    local t1 = lpm.new_ipv6("wide16")
    local t2 = lpm.new_ipv6("stride8")
    
    local routes = {
        {"2001:db8::/32", 100},
        {"fe80::/10", 200},
    }
    
    for _, r in ipairs(routes) do
        t1:insert(r[1], r[2])
        t2:insert(r[1], r[2])
    end
    
    local addrs = {"2001:db8::1", "fe80::1", "::1"}
    for _, addr in ipairs(addrs) do
        assert_eq(t1:lookup(addr), t2:lookup(addr),
            string.format("results should match for %s", addr))
    end
    
    t1:close()
    t2:close()
end)

-- ============================================================================
-- Run All Tests
-- ============================================================================

print("liblpm Lua bindings test suite")
print("Version: " .. (lpm._VERSION or "unknown"))
print("Library: " .. (lpm.version() or "unknown"))
print("")

run_tests()
