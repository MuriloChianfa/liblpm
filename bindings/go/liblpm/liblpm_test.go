package liblpm

import (
	"net/netip"
	"testing"
)

// TestSimpleIPv4InsertLookup tests basic IPv4 functionality.
func TestSimpleIPv4InsertLookup(t *testing.T) {
	table, err := NewTableIPv4()
	if err != nil {
		t.Fatalf("Failed to create IPv4 table: %v", err)
	}
	defer table.Close()

	// Insert route
	prefix := netip.MustParsePrefix("192.168.0.0/16")
	err = table.Insert(prefix, 100)
	if err != nil {
		t.Fatalf("Failed to insert: %v", err)
	}

	// Lookup should find it
	addr := netip.MustParseAddr("192.168.1.1")
	nh, found := table.Lookup(addr)
	if !found || nh != 100 {
		t.Errorf("Expected nh=100, got nh=%d, found=%v", nh, found)
	}

	// Lookup outside range should not find it
	addr2 := netip.MustParseAddr("10.0.0.1")
	_, found2 := table.Lookup(addr2)
	if found2 {
		t.Error("Should not find route outside range")
	}
}

// TestIPv4Batch tests batch lookups.
func TestIPv4Batch(t *testing.T) {
	table, err := NewTableIPv4()
	if err != nil {
		t.Fatalf("Failed to create table: %v", err)
	}
	defer table.Close()

	// Insert routes
	table.Insert(netip.MustParsePrefix("10.0.0.0/8"), 100)
	table.Insert(netip.MustParsePrefix("192.168.0.0/16"), 200)

	// Batch lookup
	addrs := []netip.Addr{
		netip.MustParseAddr("10.1.1.1"),
		netip.MustParseAddr("192.168.1.1"),
		netip.MustParseAddr("1.1.1.1"),
	}

	results, err := table.LookupBatch(addrs)
	if err != nil {
		t.Fatalf("Batch lookup failed: %v", err)
	}

	expected := []NextHop{100, 200, InvalidNextHop}
	for i, exp := range expected {
		if results[i] != exp {
			t.Errorf("Address %s: expected %d, got %d", addrs[i], exp, results[i])
		}
	}
}

// TestMultiplePrefixes tests multiple overlapping prefixes.
func TestMultiplePrefixes(t *testing.T) {
	table, err := NewTableIPv4()
	if err != nil {
		t.Fatalf("Failed to create table: %v", err)
	}
	defer table.Close()

	// Insert overlapping prefixes
	table.Insert(netip.MustParsePrefix("10.0.0.0/8"), 100)
	table.Insert(netip.MustParsePrefix("10.1.0.0/16"), 200)
	table.Insert(netip.MustParsePrefix("10.1.1.0/24"), 300)

	// Test longest prefix match
	tests := []struct {
		addr string
		want NextHop
	}{
		{"10.1.1.1", 300},
		{"10.1.2.1", 200},
		{"10.2.1.1", 100},
	}

	for _, tt := range tests {
		addr := netip.MustParseAddr(tt.addr)
		nh, found := table.Lookup(addr)
		if !found || nh != tt.want {
			t.Errorf("Lookup %s: want %d, got %d (found=%v)", tt.addr, tt.want, nh, found)
		}
	}
}

// TestNewTableIPv4 tests IPv4 table creation.
func TestNewTableIPv4(t *testing.T) {
	table, err := NewTableIPv4()
	if err != nil {
		t.Fatalf("Failed to create IPv4 table: %v", err)
	}
	defer table.Close()

	if table == nil {
		t.Fatal("Table is nil")
	}
	if !table.isIPv4 {
		t.Error("Table should be marked as IPv4")
	}
	if table.closed {
		t.Error("Table should not be closed")
	}
}

// TestNewTableIPv6 tests IPv6 table creation.
func TestNewTableIPv6(t *testing.T) {
	table, err := NewTableIPv6()
	if err != nil {
		t.Fatalf("Failed to create IPv6 table: %v", err)
	}
	defer table.Close()

	if table == nil {
		t.Fatal("Table is nil")
	}
	if table.isIPv4 {
		t.Error("Table should be marked as IPv6")
	}
	if table.closed {
		t.Error("Table should not be closed")
	}
}

// TestInsertAndLookupIPv4 tests basic insert and lookup for IPv4.
func TestInsertAndLookupIPv4(t *testing.T) {
	table, err := NewTableIPv4()
	if err != nil {
		t.Fatalf("Failed to create IPv4 table: %v", err)
	}
	defer table.Close()

	// Insert 192.168.0.0/16 -> next hop 100
	prefix := netip.MustParsePrefix("192.168.0.0/16")
	err = table.Insert(prefix, 100)
	if err != nil {
		t.Fatalf("Failed to insert prefix: %v", err)
	}

	// Lookup address in the prefix
	addr := netip.MustParseAddr("192.168.1.1")
	nh, found := table.Lookup(addr)
	if !found {
		t.Error("Expected to find route")
	}
	if nh != 100 {
		t.Errorf("Expected next hop 100, got %d", nh)
	}

	// Lookup address not in the prefix
	addr2 := netip.MustParseAddr("10.0.0.1")
	nh2, found2 := table.Lookup(addr2)
	if found2 {
		t.Errorf("Should not find route for 10.0.0.1, but got next hop %d", nh2)
	}
}

// TestInsertAndLookupIPv6 tests basic insert and lookup for IPv6.
func TestInsertAndLookupIPv6(t *testing.T) {
	table, err := NewTableIPv6()
	if err != nil {
		t.Fatalf("Failed to create IPv6 table: %v", err)
	}
	defer table.Close()

	// Insert 2001:db8::/32 -> next hop 200
	prefix := netip.MustParsePrefix("2001:db8::/32")
	err = table.Insert(prefix, 200)
	if err != nil {
		t.Fatalf("Failed to insert prefix: %v", err)
	}

	// Lookup address in the prefix
	addr := netip.MustParseAddr("2001:db8::1")
	nh, found := table.Lookup(addr)
	if !found {
		t.Error("Expected to find route")
	}
	if nh != 200 {
		t.Errorf("Expected next hop 200, got %d", nh)
	}

	// Lookup address not in the prefix
	addr2 := netip.MustParseAddr("2001:db9::1")
	nh2, found2 := table.Lookup(addr2)
	if found2 {
		t.Errorf("Should not find route, but got next hop %d", nh2)
	}
}

// TestOverlappingPrefixes tests longest prefix match with overlapping prefixes.
func TestOverlappingPrefixes(t *testing.T) {
	table, err := NewTableIPv4()
	if err != nil {
		t.Fatalf("Failed to create IPv4 table: %v", err)
	}
	defer table.Close()

	// Insert overlapping prefixes
	prefixes := []struct {
		prefix  string
		nextHop NextHop
	}{
		{"10.0.0.0/8", 100},
		{"10.1.0.0/16", 200},
		{"10.1.1.0/24", 300},
	}

	for _, p := range prefixes {
		prefix := netip.MustParsePrefix(p.prefix)
		err := table.Insert(prefix, p.nextHop)
		if err != nil {
			t.Fatalf("Failed to insert prefix %s: %v", p.prefix, err)
		}
	}

	// Test lookups - should match most specific prefix
	tests := []struct {
		addr           string
		expectedNextHop NextHop
	}{
		{"10.1.1.1", 300},   // Matches /24
		{"10.1.2.1", 200},   // Matches /16
		{"10.2.0.1", 100},   // Matches /8
		{"11.0.0.1", InvalidNextHop}, // No match
	}

	for _, tc := range tests {
		addr := netip.MustParseAddr(tc.addr)
		nh, found := table.Lookup(addr)
		
		if tc.expectedNextHop == InvalidNextHop {
			if found {
				t.Errorf("Address %s should not match, but got next hop %d", tc.addr, nh)
			}
		} else {
			if !found {
				t.Errorf("Address %s should match, but no route found", tc.addr)
			} else if nh != tc.expectedNextHop {
				t.Errorf("Address %s: expected next hop %d, got %d", tc.addr, tc.expectedNextHop, nh)
			}
		}
	}
}

// TestDefaultRoute tests default route (0.0.0.0/0).
func TestDefaultRoute(t *testing.T) {
	table, err := NewTableIPv4()
	if err != nil {
		t.Fatalf("Failed to create IPv4 table: %v", err)
	}
	defer table.Close()

	// Insert default route
	defaultPrefix := netip.MustParsePrefix("0.0.0.0/0")
	err = table.Insert(defaultPrefix, 999)
	if err != nil {
		t.Fatalf("Failed to insert default route: %v", err)
	}

	// Insert specific prefix
	specificPrefix := netip.MustParsePrefix("10.0.0.0/8")
	err = table.Insert(specificPrefix, 100)
	if err != nil {
		t.Fatalf("Failed to insert specific prefix: %v", err)
	}

	// Test that specific prefix takes precedence
	addr1 := netip.MustParseAddr("10.1.1.1")
	nh1, found1 := table.Lookup(addr1)
	if !found1 || nh1 != 100 {
		t.Errorf("Expected next hop 100 for 10.1.1.1, got %d", nh1)
	}

	// Test that default route catches everything else
	addr2 := netip.MustParseAddr("8.8.8.8")
	nh2, found2 := table.Lookup(addr2)
	if !found2 || nh2 != 999 {
		t.Errorf("Expected next hop 999 for 8.8.8.8 (default route), got %d", nh2)
	}
}

// TestDelete tests prefix deletion.
func TestDelete(t *testing.T) {
	table, err := NewTableIPv4()
	if err != nil {
		t.Fatalf("Failed to create IPv4 table: %v", err)
	}
	defer table.Close()

	// Insert and verify
	prefix := netip.MustParsePrefix("192.168.0.0/16")
	err = table.Insert(prefix, 100)
	if err != nil {
		t.Fatalf("Failed to insert prefix: %v", err)
	}

	addr := netip.MustParseAddr("192.168.1.1")
	nh, found := table.Lookup(addr)
	if !found || nh != 100 {
		t.Fatal("Route should exist after insert")
	}

	// Delete and verify
	err = table.Delete(prefix)
	if err != nil {
		t.Fatalf("Failed to delete prefix: %v", err)
	}

	_, found = table.Lookup(addr)
	if found {
		t.Error("Route should not exist after delete")
	}
}

// TestLookupBatchIPv4 tests batch lookup for IPv4.
func TestLookupBatchIPv4(t *testing.T) {
	table, err := NewTableIPv4()
	if err != nil {
		t.Fatalf("Failed to create IPv4 table: %v", err)
	}
	defer table.Close()

	// Insert some prefixes
	prefixes := []struct {
		prefix  string
		nextHop NextHop
	}{
		{"10.0.0.0/8", 100},
		{"192.168.0.0/16", 200},
		{"172.16.0.0/12", 300},
	}

	for _, p := range prefixes {
		prefix := netip.MustParsePrefix(p.prefix)
		err := table.Insert(prefix, p.nextHop)
		if err != nil {
			t.Fatalf("Failed to insert prefix %s: %v", p.prefix, err)
		}
	}

	// Batch lookup
	addrs := []netip.Addr{
		netip.MustParseAddr("10.1.1.1"),
		netip.MustParseAddr("192.168.1.1"),
		netip.MustParseAddr("172.16.1.1"),
		netip.MustParseAddr("8.8.8.8"), // No match
	}

	results, err := table.LookupBatch(addrs)
	if err != nil {
		t.Fatalf("Batch lookup failed: %v", err)
	}

	expectedResults := []NextHop{100, 200, 300, InvalidNextHop}
	for i, expected := range expectedResults {
		if results[i] != expected {
			t.Errorf("Address %s: expected next hop %d, got %d", addrs[i], expected, results[i])
		}
	}
}

// TestLookupBatchIPv6 tests batch lookup for IPv6.
func TestLookupBatchIPv6(t *testing.T) {
	table, err := NewTableIPv6()
	if err != nil {
		t.Fatalf("Failed to create IPv6 table: %v", err)
	}
	defer table.Close()

	// Insert some prefixes
	prefixes := []struct {
		prefix  string
		nextHop NextHop
	}{
		{"2001:db8::/32", 100},
		{"2001:db9::/32", 200},
	}

	for _, p := range prefixes {
		prefix := netip.MustParsePrefix(p.prefix)
		err := table.Insert(prefix, p.nextHop)
		if err != nil {
			t.Fatalf("Failed to insert prefix %s: %v", p.prefix, err)
		}
	}

	// Batch lookup
	addrs := []netip.Addr{
		netip.MustParseAddr("2001:db8::1"),
		netip.MustParseAddr("2001:db9::1"),
		netip.MustParseAddr("2001:dba::1"), // No match
	}

	results, err := table.LookupBatch(addrs)
	if err != nil {
		t.Fatalf("Batch lookup failed: %v", err)
	}

	expectedResults := []NextHop{100, 200, InvalidNextHop}
	for i, expected := range expectedResults {
		if results[i] != expected {
			t.Errorf("Address %s: expected next hop %d, got %d", addrs[i], expected, results[i])
		}
	}
}

// TestClose tests that operations fail after Close().
func TestClose(t *testing.T) {
	table, err := NewTableIPv4()
	if err != nil {
		t.Fatalf("Failed to create IPv4 table: %v", err)
	}

	err = table.Close()
	if err != nil {
		t.Fatalf("Failed to close table: %v", err)
	}

	// Operations should fail after close
	prefix := netip.MustParsePrefix("192.168.0.0/16")
	err = table.Insert(prefix, 100)
	if err != ErrTableClosed {
		t.Errorf("Expected ErrTableClosed, got %v", err)
	}

	addr := netip.MustParseAddr("192.168.1.1")
	_, found := table.Lookup(addr)
	if found {
		t.Error("Lookup should fail on closed table")
	}

	// Close should be idempotent
	err = table.Close()
	if err != nil {
		t.Errorf("Second Close() should not error: %v", err)
	}
}

// TestMultipleCreateDestroy tests creating and destroying multiple tables.
func TestMultipleCreateDestroy(t *testing.T) {
	for i := 0; i < 10; i++ {
		table, err := NewTableIPv4()
		if err != nil {
			t.Fatalf("Failed to create table %d: %v", i, err)
		}

		// Do some operations
		prefix := netip.MustParsePrefix("10.0.0.0/8")
		err = table.Insert(prefix, NextHop(i))
		if err != nil {
			t.Fatalf("Failed to insert in table %d: %v", i, err)
		}

		err = table.Close()
		if err != nil {
			t.Fatalf("Failed to close table %d: %v", i, err)
		}
	}
}

// TestInvalidPrefixes tests error handling for invalid prefixes.
func TestInvalidPrefixes(t *testing.T) {
	table, err := NewTableIPv4()
	if err != nil {
		t.Fatalf("Failed to create IPv4 table: %v", err)
	}
	defer table.Close()

	// Try to insert IPv6 prefix into IPv4 table
	ipv6Prefix := netip.MustParsePrefix("2001:db8::/32")
	err = table.Insert(ipv6Prefix, 100)
	if err != ErrInvalidPrefix {
		t.Errorf("Expected ErrInvalidPrefix for IPv6 in IPv4 table, got %v", err)
	}

	// Try to lookup IPv6 address in IPv4 table
	ipv6Addr := netip.MustParseAddr("2001:db8::1")
	_, found := table.Lookup(ipv6Addr)
	if found {
		t.Error("Should not find IPv6 address in IPv4 table")
	}
}

// TestSafeTableConcurrency tests thread-safe operations.
func TestSafeTableConcurrency(t *testing.T) {
	table, err := NewSafeTableIPv4()
	if err != nil {
		t.Fatalf("Failed to create safe table: %v", err)
	}
	defer table.Close()

	// Insert initial prefix
	prefix := netip.MustParsePrefix("10.0.0.0/8")
	err = table.Insert(prefix, 100)
	if err != nil {
		t.Fatalf("Failed to insert prefix: %v", err)
	}

	// Run concurrent lookups
	done := make(chan bool)
	for i := 0; i < 10; i++ {
		go func() {
			addr := netip.MustParseAddr("10.1.1.1")
			for j := 0; j < 100; j++ {
				nh, found := table.Lookup(addr)
				if !found || nh != 100 {
					t.Errorf("Concurrent lookup failed: found=%v, nh=%d", found, nh)
				}
			}
			done <- true
		}()
	}

	// Wait for all goroutines
	for i := 0; i < 10; i++ {
		<-done
	}
}

