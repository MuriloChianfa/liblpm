package liblpm

import (
	"errors"
	"net/netip"
)

// Common errors
var (
	ErrInvalidPrefix  = errors.New("invalid prefix")
	ErrInvalidAddress = errors.New("invalid address")
	ErrTableClosed    = errors.New("table is closed")
	ErrInsertFailed   = errors.New("insert operation failed")
	ErrDeleteFailed   = errors.New("delete operation failed")
	ErrNotFound       = errors.New("route not found")
)

// Table represents an LPM routing table.
// It wraps the C lpm_trie_t structure with proper memory management.
type Table struct {
	cTrie  uintptr // Pointer to C lpm_trie_t (stored as uintptr to avoid cgo in types.go)
	closed bool
	isIPv4 bool
}

// NextHop represents a routing next hop identifier.
type NextHop uint32

// InvalidNextHop is returned when no route is found.
const InvalidNextHop NextHop = 0xFFFFFFFF

// IsValid returns true if the next hop is valid (not InvalidNextHop).
func (nh NextHop) IsValid() bool {
	return nh != InvalidNextHop
}

// prefixToBytes converts a netip.Prefix to byte slice and prefix length.
// Returns nil if the prefix is invalid.
func prefixToBytes(prefix netip.Prefix) ([]byte, uint8, error) {
	if !prefix.IsValid() {
		return nil, 0, ErrInvalidPrefix
	}

	addr := prefix.Addr()
	bits := prefix.Bits()

	if addr.Is4() {
		// IPv4: convert to 4-byte representation
		addr4 := addr.As4()
		return addr4[:], uint8(bits), nil
	} else if addr.Is6() {
		// IPv6: use 16-byte representation
		addr6 := addr.As16()
		return addr6[:], uint8(bits), nil
	}

	return nil, 0, ErrInvalidPrefix
}

// addrToBytes converts a netip.Addr to byte slice.
func addrToBytes(addr netip.Addr) ([]byte, error) {
	if !addr.IsValid() {
		return nil, ErrInvalidAddress
	}

	if addr.Is4() {
		addr4 := addr.As4()
		return addr4[:], nil
	} else if addr.Is6() {
		addr6 := addr.As16()
		return addr6[:], nil
	}

	return nil, ErrInvalidAddress
}

