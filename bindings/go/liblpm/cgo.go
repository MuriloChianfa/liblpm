package liblpm

/*
#cgo CFLAGS: -I/usr/local/include -I../../include
#cgo LDFLAGS: -L/usr/local/lib -L../../build -llpm -Wl,-rpath,/usr/local/lib -Wl,-rpath,${SRCDIR}/../../build

#include <lpm.h>
#include <stdlib.h>
#include <string.h>

// Helper function to copy Go byte slice to C array
static void copy_bytes(uint8_t *dst, const void *src, size_t n) {
    memcpy(dst, src, n);
}
*/
import "C"
import (
	"encoding/binary"
	"errors"
	"runtime"
	"unsafe"
)

// cCreateIPv4 creates an IPv4 LPM trie using the default algorithm (DIR-24-8).
func cCreateIPv4() (uintptr, error) {
	trie := C.lpm_create_ipv4()
	if trie == nil {
		return 0, errors.New("failed to create IPv4 trie")
	}
	return uintptr(unsafe.Pointer(trie)), nil
}

// cCreateIPv4Dir24 creates an IPv4 LPM trie using DIR-24-8 optimization explicitly.
func cCreateIPv4Dir24() (uintptr, error) {
	trie := C.lpm_create_ipv4_dir24()
	if trie == nil {
		return 0, errors.New("failed to create IPv4 DIR-24-8 trie")
	}
	return uintptr(unsafe.Pointer(trie)), nil
}

// cCreateIPv4Stride8 creates an IPv4 LPM trie using 8-bit stride explicitly.
func cCreateIPv4Stride8() (uintptr, error) {
	trie := C.lpm_create_ipv4_8stride()
	if trie == nil {
		return 0, errors.New("failed to create IPv4 8-stride trie")
	}
	return uintptr(unsafe.Pointer(trie)), nil
}

// cCreateIPv6 creates an IPv6 LPM trie using the default algorithm (wide16).
func cCreateIPv6() (uintptr, error) {
	trie := C.lpm_create_ipv6()
	if trie == nil {
		return 0, errors.New("failed to create IPv6 trie")
	}
	return uintptr(unsafe.Pointer(trie)), nil
}

// cCreateIPv6Wide16 creates an IPv6 LPM trie using wide 16-bit stride explicitly.
func cCreateIPv6Wide16() (uintptr, error) {
	trie := C.lpm_create_ipv6_wide16()
	if trie == nil {
		return 0, errors.New("failed to create IPv6 wide16 trie")
	}
	return uintptr(unsafe.Pointer(trie)), nil
}

// cCreateIPv6Stride8 creates an IPv6 LPM trie using 8-bit stride explicitly.
func cCreateIPv6Stride8() (uintptr, error) {
	trie := C.lpm_create_ipv6_8stride()
	if trie == nil {
		return 0, errors.New("failed to create IPv6 8-stride trie")
	}
	return uintptr(unsafe.Pointer(trie)), nil
}

// cDestroy destroys an LPM trie and frees all resources.
func cDestroy(triePtr uintptr) {
	if triePtr == 0 {
		return
	}
	trie := (*C.lpm_trie_t)(unsafe.Pointer(triePtr))
	C.lpm_destroy(trie)
}

// cAdd adds a prefix to the trie.
// For single operations, we use memcpy for simplicity and safety.
// Batch operations use zero-copy for performance.
func cAdd(triePtr uintptr, prefix []byte, prefixLen uint8, nextHop uint32) error {
	if triePtr == 0 {
		return ErrTableClosed
	}
	if len(prefix) == 0 {
		return ErrInvalidPrefix
	}

	trie := (*C.lpm_trie_t)(unsafe.Pointer(triePtr))
	
	// Allocate C memory for the prefix
	cPrefix := (*C.uint8_t)(C.malloc(C.size_t(len(prefix))))
	if cPrefix == nil {
		return errors.New("failed to allocate memory")
	}
	defer C.free(unsafe.Pointer(cPrefix))

	// Copy prefix bytes to C memory
	C.copy_bytes(cPrefix, unsafe.Pointer(&prefix[0]), C.size_t(len(prefix)))

	// Call C function
	result := C.lpm_add(trie, cPrefix, C.uint8_t(prefixLen), C.uint32_t(nextHop))
	if result != 0 {
		return ErrInsertFailed
	}

	return nil
}

// cDelete removes a prefix from the trie.
func cDelete(triePtr uintptr, prefix []byte, prefixLen uint8) error {
	if triePtr == 0 {
		return ErrTableClosed
	}
	if len(prefix) == 0 {
		return ErrInvalidPrefix
	}

	trie := (*C.lpm_trie_t)(unsafe.Pointer(triePtr))
	
	// Allocate C memory for the prefix
	cPrefix := (*C.uint8_t)(C.malloc(C.size_t(len(prefix))))
	if cPrefix == nil {
		return errors.New("failed to allocate memory")
	}
	defer C.free(unsafe.Pointer(cPrefix))

	// Copy prefix bytes to C memory
	C.copy_bytes(cPrefix, unsafe.Pointer(&prefix[0]), C.size_t(len(prefix)))

	// Call C function
	result := C.lpm_delete(trie, cPrefix, C.uint8_t(prefixLen))
	if result != 0 {
		return ErrDeleteFailed
	}

	return nil
}

// cLookup performs a single address lookup.
func cLookup(triePtr uintptr, addr []byte) uint32 {
	if triePtr == 0 {
		return uint32(InvalidNextHop)
	}
	if len(addr) == 0 {
		return uint32(InvalidNextHop)
	}

	trie := (*C.lpm_trie_t)(unsafe.Pointer(triePtr))
	
	// Allocate C memory for the address
	cAddr := (*C.uint8_t)(C.malloc(C.size_t(len(addr))))
	if cAddr == nil {
		return uint32(InvalidNextHop)
	}
	defer C.free(unsafe.Pointer(cAddr))

	// Copy address bytes to C memory
	C.copy_bytes(cAddr, unsafe.Pointer(&addr[0]), C.size_t(len(addr)))

	// Call C function
	result := C.lpm_lookup(trie, cAddr)
	return uint32(result)
}

// cLookupIPv4 performs an IPv4 lookup using the optimized function.
func cLookupIPv4(triePtr uintptr, addr uint32) uint32 {
	if triePtr == 0 {
		return uint32(InvalidNextHop)
	}

	trie := (*C.lpm_trie_t)(unsafe.Pointer(triePtr))
	result := C.lpm_lookup_ipv4(trie, C.uint32_t(addr))
	return uint32(result)
}

// cLookupIPv6 performs an IPv6 lookup.
func cLookupIPv6(triePtr uintptr, addr []byte) uint32 {
	if triePtr == 0 || len(addr) != 16 {
		return uint32(InvalidNextHop)
	}

	trie := (*C.lpm_trie_t)(unsafe.Pointer(triePtr))
	
	// Allocate C memory for the address
	cAddr := (*C.uint8_t)(C.malloc(16))
	if cAddr == nil {
		return uint32(InvalidNextHop)
	}
	defer C.free(unsafe.Pointer(cAddr))

	// Copy address bytes to C memory
	C.copy_bytes(cAddr, unsafe.Pointer(&addr[0]), 16)

	// Call C function
	result := C.lpm_lookup_ipv6(trie, cAddr)
	return uint32(result)
}

// cLookupBatch performs batch lookup for multiple addresses.
func cLookupBatch(triePtr uintptr, addrs [][]byte, results []uint32) error {
	if triePtr == 0 {
		return ErrTableClosed
	}
	if len(addrs) == 0 {
		return nil
	}
	if len(results) < len(addrs) {
		return errors.New("results slice too small")
	}

	trie := (*C.lpm_trie_t)(unsafe.Pointer(triePtr))
	count := len(addrs)

	// Allocate array of pointers for addresses
	cAddrs := (**C.uint8_t)(C.malloc(C.size_t(count) * C.size_t(unsafe.Sizeof(uintptr(0)))))
	if cAddrs == nil {
		return errors.New("failed to allocate memory")
	}
	defer C.free(unsafe.Pointer(cAddrs))

	// Allocate and copy each address
	addrPointers := make([]unsafe.Pointer, count)
	for i, addr := range addrs {
		if len(addr) == 0 {
			continue
		}
		cAddr := C.malloc(C.size_t(len(addr)))
		if cAddr == nil {
			// Clean up previously allocated addresses
			for j := 0; j < i; j++ {
				if addrPointers[j] != nil {
					C.free(addrPointers[j])
				}
			}
			return errors.New("failed to allocate memory")
		}
		addrPointers[i] = cAddr
		C.copy_bytes((*C.uint8_t)(cAddr), unsafe.Pointer(&addr[0]), C.size_t(len(addr)))
		
		// Set pointer in array
		*(**C.uint8_t)(unsafe.Pointer(uintptr(unsafe.Pointer(cAddrs)) + uintptr(i)*unsafe.Sizeof(uintptr(0)))) = (*C.uint8_t)(cAddr)
	}

	// Clean up allocated addresses
	defer func() {
		for _, ptr := range addrPointers {
			if ptr != nil {
				C.free(ptr)
			}
		}
	}()

	// Allocate results array
	cResults := (*C.uint32_t)(C.malloc(C.size_t(count) * 4))
	if cResults == nil {
		return errors.New("failed to allocate memory")
	}
	defer C.free(unsafe.Pointer(cResults))

	// Call batch lookup
	C.lpm_lookup_batch(trie, (**C.uint8_t)(unsafe.Pointer(cAddrs)), cResults, C.size_t(count))

	// Copy results back to Go slice
	for i := 0; i < count; i++ {
		results[i] = *(*uint32)(unsafe.Pointer(uintptr(unsafe.Pointer(cResults)) + uintptr(i)*4))
	}

	return nil
}

// cLookupBatchIPv4 performs zero-copy batch IPv4 lookup.
func cLookupBatchIPv4(triePtr uintptr, addrs []uint32, results []uint32) error {
	if triePtr == 0 {
		return ErrTableClosed
	}
	if len(addrs) == 0 {
		return nil
	}
	if len(results) < len(addrs) {
		return errors.New("results slice too small")
	}

	trie := (*C.lpm_trie_t)(unsafe.Pointer(triePtr))
	
	// Pin both input and output slices - ZERO COPY!
	pinner := runtime.Pinner{}
	defer pinner.Unpin()
	
	pinner.Pin(&addrs[0])
	pinner.Pin(&results[0])
	
	// Direct pointers to Go memory (no allocation, no copy!)
	cAddrs := (*C.uint32_t)(unsafe.Pointer(&addrs[0]))
	cResults := (*C.uint32_t)(unsafe.Pointer(&results[0]))
	
	// Call C function with pinned Go memory
	C.lpm_lookup_batch_ipv4(trie, cAddrs, cResults, C.size_t(len(addrs)))

	return nil
}

// cLookupBatchIPv6 performs zero-copy batch IPv6 lookup.
func cLookupBatchIPv6(triePtr uintptr, addrs [][16]byte, results []uint32) error {
	if triePtr == 0 {
		return ErrTableClosed
	}
	if len(addrs) == 0 {
		return nil
	}
	if len(results) < len(addrs) {
		return errors.New("results slice too small")
	}

	trie := (*C.lpm_trie_t)(unsafe.Pointer(triePtr))
	
	// Pin memory - zero copy!
	pinner := runtime.Pinner{}
	defer pinner.Unpin()
	
	pinner.Pin(&addrs[0])
	pinner.Pin(&results[0])
	
	// Cast to C array pointer - the slice addrs is contiguous memory of [16]byte
	// which matches C's uint8_t (*)[16]
	cAddrs := (*[16]C.uint8_t)(unsafe.Pointer(&addrs[0]))
	cResults := (*C.uint32_t)(unsafe.Pointer(&results[0]))
	
	// Call batch lookup
	C.lpm_lookup_batch_ipv6(trie, cAddrs, cResults, C.size_t(len(addrs)))

	return nil
}

// cBatchInsertIPv4 performs batch insert for IPv4.
// This amortizes cgo overhead by processing multiple inserts in one call.
func cBatchInsertIPv4(triePtr uintptr, prefixes [][]byte, prefixLens []uint8, nextHops []uint32) error {
	if triePtr == 0 {
		return ErrTableClosed
	}
	if len(prefixes) == 0 {
		return nil
	}
	if len(prefixLens) != len(prefixes) || len(nextHops) != len(prefixes) {
		return errors.New("slice length mismatch")
	}

	trie := (*C.lpm_trie_t)(unsafe.Pointer(triePtr))
	
	// Process all inserts in one cgo call
	for i := range prefixes {
		if len(prefixes[i]) == 0 {
			continue
		}
		
		// Allocate C memory for this prefix
		cPrefix := (*C.uint8_t)(C.malloc(C.size_t(len(prefixes[i]))))
		if cPrefix == nil {
			return errors.New("failed to allocate memory")
		}
		
		// Copy and insert
		C.copy_bytes(cPrefix, unsafe.Pointer(&prefixes[i][0]), C.size_t(len(prefixes[i])))
		result := C.lpm_add(trie, cPrefix, C.uint8_t(prefixLens[i]), C.uint32_t(nextHops[i]))
		
		// Free immediately after insert
		C.free(unsafe.Pointer(cPrefix))
		
		if result != 0 {
			return ErrInsertFailed
		}
	}

	return nil
}

// Helper: Convert netip addresses to uint32 array for zero-copy batch lookup
func prepareIPv4AddrsForBatch(addrs []uint32, addrBytes [][4]byte) {
	for i, addr := range addrBytes {
		addrs[i] = binary.BigEndian.Uint32(addr[:])
	}
}
