// Package liblpm provides Go bindings for the high-performance liblpm C library.
// It supports both IPv4 and IPv6 longest prefix match (LPM) routing table operations.
package liblpm

import (
	"encoding/binary"
	"net/netip"
	"runtime"
	"sync"
)

// NewTableIPv4 creates a new IPv4 routing table using the default algorithm (DIR-24-8).
// This provides optimal performance for IPv4 lookups with ~64MB of memory.
// The table must be closed with Close() when no longer needed to free resources.
func NewTableIPv4() (*Table, error) {
	triePtr, err := cCreateIPv4()
	if err != nil {
		return nil, err
	}

	t := &Table{
		cTrie:  triePtr,
		closed: false,
		isIPv4: true,
	}

	// Set finalizer to ensure cleanup even if Close() is not called
	runtime.SetFinalizer(t, (*Table).finalize)

	return t, nil
}

// NewTableIPv4Dir24 creates an IPv4 routing table using DIR-24-8 algorithm explicitly.
// This is the recommended algorithm for IPv4 with 1-2 memory accesses per lookup.
func NewTableIPv4Dir24() (*Table, error) {
	triePtr, err := cCreateIPv4Dir24()
	if err != nil {
		return nil, err
	}

	t := &Table{
		cTrie:  triePtr,
		closed: false,
		isIPv4: true,
	}

	runtime.SetFinalizer(t, (*Table).finalize)
	return t, nil
}

// NewTableIPv4Stride8 creates an IPv4 routing table using 8-bit stride algorithm.
// This is memory-efficient for diverse prefix distributions.
func NewTableIPv4Stride8() (*Table, error) {
	triePtr, err := cCreateIPv4Stride8()
	if err != nil {
		return nil, err
	}

	t := &Table{
		cTrie:  triePtr,
		closed: false,
		isIPv4: true,
	}

	runtime.SetFinalizer(t, (*Table).finalize)
	return t, nil
}

// NewTableIPv6 creates a new IPv6 routing table using the default algorithm (wide16).
// This uses a 16-bit stride for the first level and 8-bit strides for remaining levels.
// The table must be closed with Close() when no longer needed to free resources.
func NewTableIPv6() (*Table, error) {
	triePtr, err := cCreateIPv6()
	if err != nil {
		return nil, err
	}

	t := &Table{
		cTrie:  triePtr,
		closed: false,
		isIPv4: false,
	}

	// Set finalizer to ensure cleanup even if Close() is not called
	runtime.SetFinalizer(t, (*Table).finalize)

	return t, nil
}

// NewTableIPv6Wide16 creates an IPv6 routing table using wide 16-bit stride explicitly.
// Optimal for IPv6 with common /48 allocations.
func NewTableIPv6Wide16() (*Table, error) {
	triePtr, err := cCreateIPv6Wide16()
	if err != nil {
		return nil, err
	}

	t := &Table{
		cTrie:  triePtr,
		closed: false,
		isIPv4: false,
	}

	runtime.SetFinalizer(t, (*Table).finalize)
	return t, nil
}

// NewTableIPv6Stride8 creates an IPv6 routing table using 8-bit stride algorithm.
// Simple and memory-efficient for sparse prefix sets.
func NewTableIPv6Stride8() (*Table, error) {
	triePtr, err := cCreateIPv6Stride8()
	if err != nil {
		return nil, err
	}

	t := &Table{
		cTrie:  triePtr,
		closed: false,
		isIPv4: false,
	}

	runtime.SetFinalizer(t, (*Table).finalize)
	return t, nil
}

// finalize is called by the garbage collector to clean up C resources.
func (t *Table) finalize() {
	if !t.closed && t.cTrie != 0 {
		cDestroy(t.cTrie)
		t.cTrie = 0
		t.closed = true
	}
}

// Close releases all resources associated with the table.
// After calling Close(), the table cannot be used anymore.
// It is safe to call Close() multiple times.
func (t *Table) Close() error {
	if t.closed {
		return nil
	}

	if t.cTrie != 0 {
		cDestroy(t.cTrie)
		t.cTrie = 0
	}
	t.closed = true

	// Remove finalizer since we manually cleaned up
	runtime.SetFinalizer(t, nil)

	return nil
}

// Insert adds a prefix route to the table with the specified next hop.
// The prefix must match the table's IP version (IPv4 for IPv4 table, IPv6 for IPv6 table).
// Returns an error if the prefix is invalid or insertion fails.
func (t *Table) Insert(prefix netip.Prefix, nextHop NextHop) error {
	if t.closed {
		return ErrTableClosed
	}

	// Validate prefix matches table type
	if t.isIPv4 && !prefix.Addr().Is4() {
		return ErrInvalidPrefix
	}
	if !t.isIPv4 && !prefix.Addr().Is6() {
		return ErrInvalidPrefix
	}

	prefixBytes, prefixLen, err := prefixToBytes(prefix)
	if err != nil {
		return err
	}

	return cAdd(t.cTrie, prefixBytes, prefixLen, uint32(nextHop))
}

// Delete removes a prefix route from the table.
// Returns an error if the prefix doesn't exist or deletion fails.
func (t *Table) Delete(prefix netip.Prefix) error {
	if t.closed {
		return ErrTableClosed
	}

	// Validate prefix matches table type
	if t.isIPv4 && !prefix.Addr().Is4() {
		return ErrInvalidPrefix
	}
	if !t.isIPv4 && !prefix.Addr().Is6() {
		return ErrInvalidPrefix
	}

	prefixBytes, prefixLen, err := prefixToBytes(prefix)
	if err != nil {
		return err
	}

	return cDelete(t.cTrie, prefixBytes, prefixLen)
}

// Lookup performs a longest prefix match for the given address.
// Returns the next hop and true if a match is found, or InvalidNextHop and false otherwise.
// The address must match the table's IP version.
func (t *Table) Lookup(addr netip.Addr) (NextHop, bool) {
	if t.closed {
		return InvalidNextHop, false
	}

	// Validate address matches table type
	if t.isIPv4 && !addr.Is4() {
		return InvalidNextHop, false
	}
	if !t.isIPv4 && !addr.Is6() {
		return InvalidNextHop, false
	}

	var result uint32

	if t.isIPv4 {
		// Use general lookup (avoids potential DIR-24-8 pointer issues)
		addr4 := addr.As4()
		result = cLookup(t.cTrie, addr4[:])
	} else {
		// Use IPv6 lookup
		addr6 := addr.As16()
		result = cLookup(t.cTrie, addr6[:])
	}

	nh := NextHop(result)
	return nh, nh.IsValid()
}

// LookupBatch performs lookups for multiple addresses in a single call.
// This is more efficient than calling Lookup multiple times due to reduced cgo overhead.
// Returns a slice of next hops corresponding to each input address.
// Invalid addresses or addresses with no route will have InvalidNextHop.
func (t *Table) LookupBatch(addrs []netip.Addr) ([]NextHop, error) {
	if t.closed {
		return nil, ErrTableClosed
	}
	if len(addrs) == 0 {
		return []NextHop{}, nil
	}

	results := make([]uint32, len(addrs))

	if t.isIPv4 {
		// Use optimized IPv4 batch lookup
		addrsU32 := make([]uint32, len(addrs))
		for i, addr := range addrs {
			if !addr.Is4() {
				results[i] = uint32(InvalidNextHop)
				continue
			}
			addr4 := addr.As4()
			addrsU32[i] = binary.BigEndian.Uint32(addr4[:])
		}

		err := cLookupBatchIPv4(t.cTrie, addrsU32, results)
		if err != nil {
			return nil, err
		}
	} else {
		// Use optimized IPv6 batch lookup
		addrs16 := make([][16]byte, len(addrs))
		for i, addr := range addrs {
			if !addr.Is6() {
				results[i] = uint32(InvalidNextHop)
				continue
			}
			addrs16[i] = addr.As16()
		}

		err := cLookupBatchIPv6(t.cTrie, addrs16, results)
		if err != nil {
			return nil, err
		}
	}

	// Convert to NextHop slice
	nextHops := make([]NextHop, len(results))
	for i, r := range results {
		nextHops[i] = NextHop(r)
	}

	return nextHops, nil
}

// Stats holds statistics about the routing table.
type Stats struct {
	NumPrefixes   uint64
	NumNodes      uint64
	NumWideNodes  uint64
	CacheHits     uint64
	CacheMisses   uint64
	MemoryUsageKB uint64
}

// GetStats returns statistics about the routing table (not yet implemented).
// This would require additional C bindings to expose lpm_print_stats data.
func (t *Table) GetStats() (*Stats, error) {
	if t.closed {
		return nil, ErrTableClosed
	}

	// TODO: Implement stats retrieval from C
	return &Stats{}, nil
}

// SafeTable is a thread-safe wrapper around Table.
// It uses a mutex to synchronize all operations.
type SafeTable struct {
	table *Table
	mu    sync.RWMutex
}

// NewSafeTableIPv4 creates a new thread-safe IPv4 routing table.
func NewSafeTableIPv4() (*SafeTable, error) {
	table, err := NewTableIPv4()
	if err != nil {
		return nil, err
	}
	return &SafeTable{table: table}, nil
}

// NewSafeTableIPv6 creates a new thread-safe IPv6 routing table.
func NewSafeTableIPv6() (*SafeTable, error) {
	table, err := NewTableIPv6()
	if err != nil {
		return nil, err
	}
	return &SafeTable{table: table}, nil
}

// Close closes the underlying table.
func (st *SafeTable) Close() error {
	st.mu.Lock()
	defer st.mu.Unlock()
	return st.table.Close()
}

// Insert adds a prefix route with write lock.
func (st *SafeTable) Insert(prefix netip.Prefix, nextHop NextHop) error {
	st.mu.Lock()
	defer st.mu.Unlock()
	return st.table.Insert(prefix, nextHop)
}

// Delete removes a prefix route with write lock.
func (st *SafeTable) Delete(prefix netip.Prefix) error {
	st.mu.Lock()
	defer st.mu.Unlock()
	return st.table.Delete(prefix)
}

// Lookup performs a lookup with read lock.
func (st *SafeTable) Lookup(addr netip.Addr) (NextHop, bool) {
	st.mu.RLock()
	defer st.mu.RUnlock()
	return st.table.Lookup(addr)
}

// LookupBatch performs batch lookup with read lock.
func (st *SafeTable) LookupBatch(addrs []netip.Addr) ([]NextHop, error) {
	st.mu.RLock()
	defer st.mu.RUnlock()
	return st.table.LookupBatch(addrs)
}

// GetStats returns statistics with read lock.
func (st *SafeTable) GetStats() (*Stats, error) {
	st.mu.RLock()
	defer st.mu.RUnlock()
	return st.table.GetStats()
}

