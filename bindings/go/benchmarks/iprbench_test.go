package benchmarks

import (
	"math/rand"
	"net/netip"
	"testing"

	"github.com/MuriloChianfa/liblpm/go/liblpm"
)

// generateRandomIPv4Prefixes generates random IPv4 prefixes for testing.
func generateRandomIPv4Prefixes(count int) []netip.Prefix {
	prefixes := make([]netip.Prefix, count)
	rng := rand.New(rand.NewSource(42)) // Fixed seed for reproducibility

	for i := 0; i < count; i++ {
		// Generate random IPv4 address
		b := make([]byte, 4)
		rng.Read(b)
		addr := netip.AddrFrom4([4]byte{b[0], b[1], b[2], b[3]})
		
		// Random prefix length between 8 and 32
		prefixLen := 8 + rng.Intn(25)
		
		prefix, err := addr.Prefix(prefixLen)
		if err != nil {
			// Fallback to /24 if prefix creation fails
			prefix = netip.MustParsePrefix("10.0.0.0/24")
		}
		prefixes[i] = prefix
	}

	return prefixes
}

// generateRandomIPv6Prefixes generates random IPv6 prefixes for testing.
func generateRandomIPv6Prefixes(count int) []netip.Prefix {
	prefixes := make([]netip.Prefix, count)
	rng := rand.New(rand.NewSource(42)) // Fixed seed for reproducibility

	for i := 0; i < count; i++ {
		// Generate random IPv6 address
		b := make([]byte, 16)
		rng.Read(b)
		var addr16 [16]byte
		copy(addr16[:], b)
		addr := netip.AddrFrom16(addr16)
		
		// Random prefix length between 16 and 64
		prefixLen := 16 + rng.Intn(49)
		
		prefix, err := addr.Prefix(prefixLen)
		if err != nil {
			// Fallback to /48 if prefix creation fails
			prefix = netip.MustParsePrefix("2001:db8::/48")
		}
		prefixes[i] = prefix
	}

	return prefixes
}

// generateRandomIPv4Addrs generates random IPv4 addresses for lookup testing.
func generateRandomIPv4Addrs(count int) []netip.Addr {
	addrs := make([]netip.Addr, count)
	rng := rand.New(rand.NewSource(123)) // Different seed from prefixes

	for i := 0; i < count; i++ {
		b := make([]byte, 4)
		rng.Read(b)
		addrs[i] = netip.AddrFrom4([4]byte{b[0], b[1], b[2], b[3]})
	}

	return addrs
}

// generateRandomIPv6Addrs generates random IPv6 addresses for lookup testing.
func generateRandomIPv6Addrs(count int) []netip.Addr {
	addrs := make([]netip.Addr, count)
	rng := rand.New(rand.NewSource(123)) // Different seed from prefixes

	for i := 0; i < count; i++ {
		b := make([]byte, 16)
		rng.Read(b)
		var addr16 [16]byte
		copy(addr16[:], b)
		addrs[i] = netip.AddrFrom16(addr16)
	}

	return addrs
}

// BenchmarkInsertRandomPfxsIPv4_1_000 benchmarks inserting 1,000 random IPv4 prefixes.
func BenchmarkInsertRandomPfxsIPv4_1_000(b *testing.B) {
	prefixes := generateRandomIPv4Prefixes(1000)
	
	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		b.StopTimer()
		table, _ := liblpm.NewTableIPv4()
		b.StartTimer()
		
		for j, prefix := range prefixes {
			table.Insert(prefix, liblpm.NextHop(j))
		}
		
		b.StopTimer()
		table.Close()
		b.StartTimer()
	}
	
	// Report per-route metric to match iprbench format
	b.ReportMetric(float64(b.Elapsed().Nanoseconds())/float64(b.N*len(prefixes)), "ns/route")
}

// BenchmarkInsertRandomPfxsIPv4_10_000 benchmarks inserting 10,000 random IPv4 prefixes.
func BenchmarkInsertRandomPfxsIPv4_10_000(b *testing.B) {
	prefixes := generateRandomIPv4Prefixes(10000)
	
	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		b.StopTimer()
		table, _ := liblpm.NewTableIPv4()
		b.StartTimer()
		
		for j, prefix := range prefixes {
			table.Insert(prefix, liblpm.NextHop(j))
		}
		
		b.StopTimer()
		table.Close()
		b.StartTimer()
	}
	
	b.ReportMetric(float64(b.Elapsed().Nanoseconds())/float64(b.N*len(prefixes)), "ns/route")
}

// BenchmarkInsertRandomPfxsIPv4_100_000 benchmarks inserting 100,000 random IPv4 prefixes.
func BenchmarkInsertRandomPfxsIPv4_100_000(b *testing.B) {
	prefixes := generateRandomIPv4Prefixes(100000)
	
	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		b.StopTimer()
		table, _ := liblpm.NewTableIPv4()
		b.StartTimer()
		
		for j, prefix := range prefixes {
			table.Insert(prefix, liblpm.NextHop(j))
		}
		
		b.StopTimer()
		table.Close()
		b.StartTimer()
	}
	
	b.ReportMetric(float64(b.Elapsed().Nanoseconds())/float64(b.N*len(prefixes)), "ns/route")
}

// BenchmarkInsertRandomPfxsIPv4_200_000 benchmarks inserting 200,000 random IPv4 prefixes.
func BenchmarkInsertRandomPfxsIPv4_200_000(b *testing.B) {
	prefixes := generateRandomIPv4Prefixes(200000)
	
	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		b.StopTimer()
		table, _ := liblpm.NewTableIPv4()
		b.StartTimer()
		
		for j, prefix := range prefixes {
			table.Insert(prefix, liblpm.NextHop(j))
		}
		
		b.StopTimer()
		table.Close()
		b.StartTimer()
	}
	
	b.ReportMetric(float64(b.Elapsed().Nanoseconds())/float64(b.N*len(prefixes)), "ns/route")
}

// BenchmarkDeleteRandomPfxsIPv4_1_000 benchmarks deleting 1,000 random IPv4 prefixes.
func BenchmarkDeleteRandomPfxsIPv4_1_000(b *testing.B) {
	prefixes := generateRandomIPv4Prefixes(1000)
	
	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		b.StopTimer()
		table, _ := liblpm.NewTableIPv4()
		for j, prefix := range prefixes {
			table.Insert(prefix, liblpm.NextHop(j))
		}
		b.StartTimer()
		
		for _, prefix := range prefixes {
			table.Delete(prefix)
		}
		
		b.StopTimer()
		table.Close()
		b.StartTimer()
	}
	
	b.ReportMetric(float64(b.Elapsed().Nanoseconds())/float64(b.N*len(prefixes)), "ns/route")
}

// BenchmarkDeleteRandomPfxsIPv4_10_000 benchmarks deleting 10,000 random IPv4 prefixes.
func BenchmarkDeleteRandomPfxsIPv4_10_000(b *testing.B) {
	prefixes := generateRandomIPv4Prefixes(10000)
	
	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		b.StopTimer()
		table, _ := liblpm.NewTableIPv4()
		for j, prefix := range prefixes {
			table.Insert(prefix, liblpm.NextHop(j))
		}
		b.StartTimer()
		
		for _, prefix := range prefixes {
			table.Delete(prefix)
		}
		
		b.StopTimer()
		table.Close()
		b.StartTimer()
	}
	
	b.ReportMetric(float64(b.Elapsed().Nanoseconds())/float64(b.N*len(prefixes)), "ns/route")
}

// BenchmarkDeleteRandomPfxsIPv4_100_000 benchmarks deleting 100,000 random IPv4 prefixes.
func BenchmarkDeleteRandomPfxsIPv4_100_000(b *testing.B) {
	prefixes := generateRandomIPv4Prefixes(100000)
	
	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		b.StopTimer()
		table, _ := liblpm.NewTableIPv4()
		for j, prefix := range prefixes {
			table.Insert(prefix, liblpm.NextHop(j))
		}
		b.StartTimer()
		
		for _, prefix := range prefixes {
			table.Delete(prefix)
		}
		
		b.StopTimer()
		table.Close()
		b.StartTimer()
	}
	
	b.ReportMetric(float64(b.Elapsed().Nanoseconds())/float64(b.N*len(prefixes)), "ns/route")
}

// BenchmarkDeleteRandomPfxsIPv4_200_000 benchmarks deleting 200,000 random IPv4 prefixes.
func BenchmarkDeleteRandomPfxsIPv4_200_000(b *testing.B) {
	prefixes := generateRandomIPv4Prefixes(200000)
	
	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		b.StopTimer()
		table, _ := liblpm.NewTableIPv4()
		for j, prefix := range prefixes {
			table.Insert(prefix, liblpm.NextHop(j))
		}
		b.StartTimer()
		
		for _, prefix := range prefixes {
			table.Delete(prefix)
		}
		
		b.StopTimer()
		table.Close()
		b.StartTimer()
	}
	
	b.ReportMetric(float64(b.Elapsed().Nanoseconds())/float64(b.N*len(prefixes)), "ns/route")
}

// BenchmarkLookupIPv4 benchmarks single IPv4 lookups with 10,000 routes.
func BenchmarkLookupIPv4(b *testing.B) {
	prefixes := generateRandomIPv4Prefixes(10000)
	addrs := generateRandomIPv4Addrs(1000)
	
	table, _ := liblpm.NewTableIPv4()
	defer table.Close()
	
	for i, prefix := range prefixes {
		table.Insert(prefix, liblpm.NextHop(i))
	}
	
	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		table.Lookup(addrs[i%len(addrs)])
	}
}

// BenchmarkLookupBatchIPv4 benchmarks batch IPv4 lookups.
func BenchmarkLookupBatchIPv4(b *testing.B) {
	prefixes := generateRandomIPv4Prefixes(10000)
	addrs := generateRandomIPv4Addrs(1000)
	
	table, _ := liblpm.NewTableIPv4()
	defer table.Close()
	
	for i, prefix := range prefixes {
		table.Insert(prefix, liblpm.NextHop(i))
	}
	
	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		table.LookupBatch(addrs)
	}
	
	b.ReportMetric(float64(b.Elapsed().Nanoseconds())/float64(b.N*len(addrs)), "ns/lookup")
}

// IPv6 Benchmarks

// BenchmarkInsertRandomPfxsIPv6_1_000 benchmarks inserting 1,000 random IPv6 prefixes.
func BenchmarkInsertRandomPfxsIPv6_1_000(b *testing.B) {
	prefixes := generateRandomIPv6Prefixes(1000)
	
	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		b.StopTimer()
		table, _ := liblpm.NewTableIPv6()
		b.StartTimer()
		
		for j, prefix := range prefixes {
			table.Insert(prefix, liblpm.NextHop(j))
		}
		
		b.StopTimer()
		table.Close()
		b.StartTimer()
	}
	
	b.ReportMetric(float64(b.Elapsed().Nanoseconds())/float64(b.N*len(prefixes)), "ns/route")
}

// BenchmarkInsertRandomPfxsIPv6_10_000 benchmarks inserting 10,000 random IPv6 prefixes.
func BenchmarkInsertRandomPfxsIPv6_10_000(b *testing.B) {
	prefixes := generateRandomIPv6Prefixes(10000)
	
	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		b.StopTimer()
		table, _ := liblpm.NewTableIPv6()
		b.StartTimer()
		
		for j, prefix := range prefixes {
			table.Insert(prefix, liblpm.NextHop(j))
		}
		
		b.StopTimer()
		table.Close()
		b.StartTimer()
	}
	
	b.ReportMetric(float64(b.Elapsed().Nanoseconds())/float64(b.N*len(prefixes)), "ns/route")
}

// BenchmarkDeleteRandomPfxsIPv6_10_000 benchmarks deleting 10,000 random IPv6 prefixes.
func BenchmarkDeleteRandomPfxsIPv6_10_000(b *testing.B) {
	prefixes := generateRandomIPv6Prefixes(10000)
	
	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		b.StopTimer()
		table, _ := liblpm.NewTableIPv6()
		for j, prefix := range prefixes {
			table.Insert(prefix, liblpm.NextHop(j))
		}
		b.StartTimer()
		
		for _, prefix := range prefixes {
			table.Delete(prefix)
		}
		
		b.StopTimer()
		table.Close()
		b.StartTimer()
	}
	
	b.ReportMetric(float64(b.Elapsed().Nanoseconds())/float64(b.N*len(prefixes)), "ns/route")
}

// BenchmarkLookupIPv6 benchmarks single IPv6 lookups with 10,000 routes.
func BenchmarkLookupIPv6(b *testing.B) {
	prefixes := generateRandomIPv6Prefixes(10000)
	addrs := generateRandomIPv6Addrs(1000)
	
	table, _ := liblpm.NewTableIPv6()
	defer table.Close()
	
	for i, prefix := range prefixes {
		table.Insert(prefix, liblpm.NextHop(i))
	}
	
	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		table.Lookup(addrs[i%len(addrs)])
	}
}

// BenchmarkLookupBatchIPv6 benchmarks batch IPv6 lookups.
func BenchmarkLookupBatchIPv6(b *testing.B) {
	prefixes := generateRandomIPv6Prefixes(10000)
	addrs := generateRandomIPv6Addrs(1000)
	
	table, _ := liblpm.NewTableIPv6()
	defer table.Close()
	
	for i, prefix := range prefixes {
		table.Insert(prefix, liblpm.NextHop(i))
	}
	
	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		table.LookupBatch(addrs)
	}
	
	b.ReportMetric(float64(b.Elapsed().Nanoseconds())/float64(b.N*len(addrs)), "ns/lookup")
}


