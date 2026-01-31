// BatchExample.cs - Batch operations example for liblpm C# bindings
// Demonstrates high-performance batch lookups

using System;
using System.Buffers.Binary;
using System.Diagnostics;
using LibLpm;

namespace LibLpm.Examples
{
    /// <summary>
    /// Example demonstrating batch lookup operations for high throughput.
    /// </summary>
    public static class BatchExample
    {
        /// <summary>
        /// Runs the batch example.
        /// </summary>
        public static void Run()
        {
            Console.WriteLine("liblpm C# Bindings - Batch Operations Example");
            Console.WriteLine("=============================================");
            Console.WriteLine();

            IPv4BatchExample();
            Console.WriteLine();

            IPv6BatchExample();
            Console.WriteLine();

            PerformanceBenchmark();
        }

        /// <summary>
        /// Demonstrates IPv4 batch lookups.
        /// </summary>
        private static void IPv4BatchExample()
        {
            Console.WriteLine("=== IPv4 Batch Lookup ===");

            using var trie = LpmTrieIPv4.CreateDefault();

            // Add some routes
            trie.Add("192.168.0.0/16", 100);
            trie.Add("10.0.0.0/8", 200);
            trie.Add("172.16.0.0/12", 300);
            trie.Add("0.0.0.0/0", 1); // Default route

            Console.WriteLine("Added routes: 192.168.0.0/16, 10.0.0.0/8, 172.16.0.0/12, 0.0.0.0/0");
            Console.WriteLine();

            // Batch lookup with uint array
            Console.WriteLine("Batch lookup with uint[] (network byte order):");
            uint[] addresses = new uint[]
            {
                0xC0A80101, // 192.168.1.1
                0xC0A80202, // 192.168.2.2
                0x0A010101, // 10.1.1.1
                0xAC101010, // 172.16.16.16
                0x08080808, // 8.8.8.8 (no specific route)
            };
            uint[] results = new uint[5];

            trie.LookupBatch(addresses, results);

            for (int i = 0; i < addresses.Length; i++)
            {
                var addrStr = FormatIPv4(addresses[i]);
                var resultStr = results[i] == LpmConstants.InvalidNextHop 
                    ? "no match (default)" 
                    : results[i].ToString();
                Console.WriteLine($"  {addrStr} -> {resultStr}");
            }
            Console.WriteLine();

            // Batch lookup with byte array
            Console.WriteLine("Batch lookup with byte[] (contiguous addresses):");
            byte[] byteAddresses = new byte[]
            {
                192, 168, 1, 1,   // Address 1
                10, 255, 255, 1,  // Address 2
            };
            uint[] byteResults = new uint[2];

            trie.LookupBatch(byteAddresses, byteResults);

            Console.WriteLine($"  192.168.1.1 -> {byteResults[0]}");
            Console.WriteLine($"  10.255.255.1 -> {byteResults[1]}");
            Console.WriteLine();

            // Batch lookup with Span<T> (zero allocation)
            Console.WriteLine("Batch lookup with Span<T> (stack allocated):");
            Span<uint> spanAddresses = stackalloc uint[]
            {
                0xC0A80303, // 192.168.3.3
                0xC0A80404, // 192.168.4.4
            };
            Span<uint> spanResults = stackalloc uint[2];

            trie.LookupBatch(spanAddresses, spanResults);

            Console.WriteLine($"  192.168.3.3 -> {spanResults[0]}");
            Console.WriteLine($"  192.168.4.4 -> {spanResults[1]}");
        }

        /// <summary>
        /// Demonstrates IPv6 batch lookups.
        /// </summary>
        private static void IPv6BatchExample()
        {
            Console.WriteLine("=== IPv6 Batch Lookup ===");

            using var trie = LpmTrieIPv6.CreateDefault();

            // Add some routes
            trie.Add("2001:db8::/32", 100);
            trie.Add("2001:db8:1::/48", 200);
            trie.Add("fe80::/10", 300);
            trie.Add("::/0", 1); // Default route

            Console.WriteLine("Added routes: 2001:db8::/32, 2001:db8:1::/48, fe80::/10, ::/0");
            Console.WriteLine();

            // Batch lookup with byte array
            Console.WriteLine("Batch lookup with byte[] (3 addresses):");
            byte[] addresses = new byte[48]; // 3 addresses * 16 bytes each

            // Address 1: 2001:db8::1
            addresses[0] = 0x20;
            addresses[1] = 0x01;
            addresses[2] = 0x0d;
            addresses[3] = 0xb8;
            addresses[15] = 0x01;

            // Address 2: 2001:db8:1::1
            addresses[16] = 0x20;
            addresses[17] = 0x01;
            addresses[18] = 0x0d;
            addresses[19] = 0xb8;
            addresses[20] = 0x00;
            addresses[21] = 0x01;
            addresses[31] = 0x01;

            // Address 3: fe80::1
            addresses[32] = 0xfe;
            addresses[33] = 0x80;
            addresses[47] = 0x01;

            uint[] results = new uint[3];

            trie.LookupBatch(addresses, results);

            Console.WriteLine($"  2001:db8::1 -> {results[0]} (matches /32)");
            Console.WriteLine($"  2001:db8:1::1 -> {results[1]} (matches /48)");
            Console.WriteLine($"  fe80::1 -> {results[2]} (link-local)");
        }

        /// <summary>
        /// Demonstrates performance of batch vs single lookups.
        /// </summary>
        private static void PerformanceBenchmark()
        {
            Console.WriteLine("=== Performance Comparison ===");
            Console.WriteLine();

            using var trie = LpmTrieIPv4.CreateDefault();

            // Add a default route
            trie.Add("0.0.0.0/0", 1);

            const int count = 100000;
            uint[] addresses = new uint[count];
            uint[] results = new uint[count];

            // Generate random addresses
            var random = new Random(42);
            for (int i = 0; i < count; i++)
            {
                addresses[i] = (uint)random.Next();
            }

            // Warm up
            trie.LookupBatch(addresses, results);
            for (int i = 0; i < 1000; i++)
            {
                trie.Lookup(addresses[i]);
            }

            // Benchmark batch lookup
            var sw = Stopwatch.StartNew();
            for (int iter = 0; iter < 10; iter++)
            {
                trie.LookupBatch(addresses, results);
            }
            sw.Stop();
            double batchTime = sw.Elapsed.TotalMilliseconds / 10;
            double batchRate = count / (batchTime / 1000.0);

            Console.WriteLine($"Batch lookup ({count:N0} addresses):");
            Console.WriteLine($"  Time: {batchTime:F2} ms");
            Console.WriteLine($"  Rate: {batchRate:N0} lookups/sec");
            Console.WriteLine();

            // Benchmark single lookups
            sw.Restart();
            for (int iter = 0; iter < 10; iter++)
            {
                for (int i = 0; i < count; i++)
                {
                    trie.Lookup(addresses[i]);
                }
            }
            sw.Stop();
            double singleTime = sw.Elapsed.TotalMilliseconds / 10;
            double singleRate = count / (singleTime / 1000.0);

            Console.WriteLine($"Single lookups ({count:N0} addresses):");
            Console.WriteLine($"  Time: {singleTime:F2} ms");
            Console.WriteLine($"  Rate: {singleRate:N0} lookups/sec");
            Console.WriteLine();

            double speedup = singleTime / batchTime;
            Console.WriteLine($"Batch speedup: {speedup:F2}x");
        }

        /// <summary>
        /// Formats a uint IPv4 address as a string.
        /// </summary>
        private static string FormatIPv4(uint addr)
        {
            return $"{(addr >> 24) & 0xFF}.{(addr >> 16) & 0xFF}.{(addr >> 8) & 0xFF}.{addr & 0xFF}";
        }
    }
}
