package benchmarks

import (
	"encoding/binary"
	"testing"

	"github.com/MuriloChianfa/liblpm/go/liblpm"
)

// BenchmarkBatchLookupOptimized_100 - Small batch (100 lookups)
func BenchmarkBatchLookupOptimized_100(b *testing.B) {
	table, _ := liblpm.NewBatchTableIPv4()
	defer table.Close()

	// Insert 10K routes
	prefixes := generateRandomIPv4Prefixes(10000)
	for i, prefix := range prefixes {
		table.Insert(prefix, liblpm.NextHop(i))
	}

	// Prepare 100 lookup addresses
	addrs := generateRandomIPv4Addrs(100)

	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		table.LookupBatch(addrs)
	}

	// Report per-lookup metric
	b.ReportMetric(float64(b.Elapsed().Nanoseconds())/float64(b.N*100), "ns/lookup")
}

// BenchmarkBatchLookupOptimized_1000 - Medium batch (1000 lookups)
func BenchmarkBatchLookupOptimized_1000(b *testing.B) {
	table, _ := liblpm.NewBatchTableIPv4()
	defer table.Close()

	// Insert 10K routes
	prefixes := generateRandomIPv4Prefixes(10000)
	for i, prefix := range prefixes {
		table.Insert(prefix, liblpm.NextHop(i))
	}

	// Prepare 1000 lookup addresses
	addrs := generateRandomIPv4Addrs(1000)

	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		table.LookupBatch(addrs)
	}

	b.ReportMetric(float64(b.Elapsed().Nanoseconds())/float64(b.N*1000), "ns/lookup")
}

// BenchmarkBatchLookupOptimized_10000 - Large batch (10000 lookups)
func BenchmarkBatchLookupOptimized_10000(b *testing.B) {
	table, _ := liblpm.NewBatchTableIPv4()
	defer table.Close()

	// Insert 10K routes
	prefixes := generateRandomIPv4Prefixes(10000)
	for i, prefix := range prefixes {
		table.Insert(prefix, liblpm.NextHop(i))
	}

	// Prepare 10000 lookup addresses
	addrs := generateRandomIPv4Addrs(10000)

	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		table.LookupBatch(addrs)
	}

	b.ReportMetric(float64(b.Elapsed().Nanoseconds())/float64(b.N*10000), "ns/lookup")
}

// BenchmarkBatchLookupRaw_1000 - Ultra-optimized with pre-converted addresses
func BenchmarkBatchLookupRaw_1000(b *testing.B) {
	table, _ := liblpm.NewBatchTableIPv4()
	defer table.Close()

	// Insert 10K routes
	prefixes := generateRandomIPv4Prefixes(10000)
	for i, prefix := range prefixes {
		table.Insert(prefix, liblpm.NextHop(i))
	}

	// Pre-convert addresses to uint32 (network byte order)
	addrs := generateRandomIPv4Addrs(1000)
	addrsU32 := make([]uint32, 1000)
	for i, addr := range addrs {
		addr4 := addr.As4()
		addrsU32[i] = binary.BigEndian.Uint32(addr4[:])
	}

	results := make([]uint32, 1000)

	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		table.LookupBatchRaw(addrsU32, results)
	}

	b.ReportMetric(float64(b.Elapsed().Nanoseconds())/float64(b.N*1000), "ns/lookup")
}

// BenchmarkBatchLookupPreallocated_1000 - Zero-allocation batch lookup
func BenchmarkBatchLookupPreallocated_1000(b *testing.B) {
	table, _ := liblpm.NewBatchTableIPv4()
	defer table.Close()

	// Insert 10K routes
	prefixes := generateRandomIPv4Prefixes(10000)
	for i, prefix := range prefixes {
		table.Insert(prefix, liblpm.NextHop(i))
	}

	// Prepare addresses
	addrs := generateRandomIPv4Addrs(1000)

	// Pre-allocate buffers (reused across iterations)
	addrsU32 := make([]uint32, 1000)
	results := make([]uint32, 1000)

	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		table.PreallocatedBatchLookup(addrs, addrsU32, results)
	}

	b.ReportMetric(float64(b.Elapsed().Nanoseconds())/float64(b.N*1000), "ns/lookup")
}

// BenchmarkBatchInsert_1000 - Batch insert performance
func BenchmarkBatchInsert_1000(b *testing.B) {
	prefixes := generateRandomIPv4Prefixes(1000)
	nextHops := make([]liblpm.NextHop, 1000)
	for i := range nextHops {
		nextHops[i] = liblpm.NextHop(i)
	}

	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		b.StopTimer()
		table, _ := liblpm.NewBatchTableIPv4()
		b.StartTimer()

		table.BatchInsert(prefixes, nextHops)

		b.StopTimer()
		table.Close()
		b.StartTimer()
	}

	b.ReportMetric(float64(b.Elapsed().Nanoseconds())/float64(b.N*1000), "ns/route")
}

// BenchmarkBatchInsert_10000 - Larger batch insert
func BenchmarkBatchInsert_10000(b *testing.B) {
	prefixes := generateRandomIPv4Prefixes(10000)
	nextHops := make([]liblpm.NextHop, 10000)
	for i := range nextHops {
		nextHops[i] = liblpm.NextHop(i)
	}

	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		b.StopTimer()
		table, _ := liblpm.NewBatchTableIPv4()
		b.StartTimer()

		table.BatchInsert(prefixes, nextHops)

		b.StopTimer()
		table.Close()
		b.StartTimer()
	}

	b.ReportMetric(float64(b.Elapsed().Nanoseconds())/float64(b.N*10000), "ns/route")
}

// BenchmarkComparisonSingleVsBatch - Direct comparison
func BenchmarkComparisonSingleVsBatch(b *testing.B) {
	table, _ := liblpm.NewBatchTableIPv4()
	defer table.Close()

	// Insert 10K routes
	prefixes := generateRandomIPv4Prefixes(10000)
	for i, prefix := range prefixes {
		table.Insert(prefix, liblpm.NextHop(i))
	}

	addrs := generateRandomIPv4Addrs(1000)

	b.Run("SingleInserts", func(b *testing.B) {
		for i := 0; i < b.N; i++ {
			b.StopTimer()
			t, _ := liblpm.NewBatchTableIPv4()
			b.StartTimer()

			for j, prefix := range prefixes {
				t.Insert(prefix, liblpm.NextHop(j))
			}

			b.StopTimer()
			t.Close()
			b.StartTimer()
		}
		b.ReportMetric(float64(b.Elapsed().Nanoseconds())/float64(b.N*len(prefixes)), "ns/route")
	})

	b.Run("BatchInsert", func(b *testing.B) {
		nextHops := make([]liblpm.NextHop, len(prefixes))
		for i := range nextHops {
			nextHops[i] = liblpm.NextHop(i)
		}

		for i := 0; i < b.N; i++ {
			b.StopTimer()
			t, _ := liblpm.NewBatchTableIPv4()
			b.StartTimer()

			t.BatchInsert(prefixes, nextHops)

			b.StopTimer()
			t.Close()
			b.StartTimer()
		}
		b.ReportMetric(float64(b.Elapsed().Nanoseconds())/float64(b.N*len(prefixes)), "ns/route")
	})

	b.Run("BatchLookup", func(b *testing.B) {
		for i := 0; i < b.N; i++ {
			table.LookupBatch(addrs)
		}
		b.ReportMetric(float64(b.Elapsed().Nanoseconds())/float64(b.N*len(addrs)), "ns/lookup")
	})

	b.Run("BatchLookupRaw", func(b *testing.B) {
		addrsU32 := make([]uint32, len(addrs))
		for i, addr := range addrs {
			addr4 := addr.As4()
			addrsU32[i] = binary.BigEndian.Uint32(addr4[:])
		}
		results := make([]uint32, len(addrs))

		b.ResetTimer()
		for i := 0; i < b.N; i++ {
			table.LookupBatchRaw(addrsU32, results)
		}
		b.ReportMetric(float64(b.Elapsed().Nanoseconds())/float64(b.N*len(addrs)), "ns/lookup")
	})
}

// BenchmarkHighVolumeScenario - Realistic high-volume router scenario
func BenchmarkHighVolumeScenario(b *testing.B) {
	// Simulate a border router with 100K routes
	table, _ := liblpm.NewBatchTableIPv4()
	defer table.Close()

	// Build routing table
	prefixes := generateRandomIPv4Prefixes(100000)
	nextHops := make([]liblpm.NextHop, 100000)
	for i := range nextHops {
		nextHops[i] = liblpm.NextHop(i)
	}
	table.BatchInsert(prefixes, nextHops)

	// Simulate burst traffic: 10K packets at once
	addrs := generateRandomIPv4Addrs(10000)
	addrsU32 := make([]uint32, 10000)
	results := make([]uint32, 10000)

	// Pre-convert addresses
	for i, addr := range addrs {
		addr4 := addr.As4()
		addrsU32[i] = binary.BigEndian.Uint32(addr4[:])
	}

	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		// Process burst
		table.LookupBatchRaw(addrsU32, results)
	}

	// Report throughput
	lookupsPerSec := float64(b.N*10000) / b.Elapsed().Seconds()
	b.ReportMetric(lookupsPerSec/1000000, "Mlookups/sec")
	b.ReportMetric(float64(b.Elapsed().Nanoseconds())/float64(b.N*10000), "ns/lookup")
}

