// BasicExample.cs - Basic usage example for liblpm C# bindings
// Demonstrates IPv4 and IPv6 LPM operations

using System;
using System.Net;
using LibLpm;

namespace LibLpm.Examples
{
    /// <summary>
    /// Basic example demonstrating liblpm C# bindings.
    /// </summary>
    public static class BasicExample
    {
        public static void Main(string[] args)
        {
            Console.WriteLine("liblpm C# Bindings - Basic Example");
            Console.WriteLine("===================================");
            Console.WriteLine();

            // Print library version
            var version = LpmTrie.GetVersion();
            Console.WriteLine($"Library Version: {version}");
            Console.WriteLine();

            // Run IPv4 examples
            IPv4Example();
            Console.WriteLine();

            // Run IPv6 examples
            IPv6Example();
            Console.WriteLine();

            // Run fast path example
            FastPathExample();
            Console.WriteLine();

            Console.WriteLine(new string('=', 60));
            Console.WriteLine();

            // Run batch example
            BatchExample.Run();
            Console.WriteLine();

            Console.WriteLine("All examples completed successfully!");
        }

        /// <summary>
        /// Demonstrates basic IPv4 operations.
        /// </summary>
        private static void IPv4Example()
        {
            Console.WriteLine("=== IPv4 Example ===");

            // Create an IPv4 trie using the default algorithm (DIR-24-8)
            using var trie = LpmTrieIPv4.CreateDefault();

            // Add routes using different APIs
            
            // 1. String-based (convenient)
            trie.Add("0.0.0.0/0", 1);         // Default route
            trie.Add("192.168.0.0/16", 100);   // /16 prefix
            trie.Add("192.168.1.0/24", 200);   // /24 prefix
            trie.Add("10.0.0.0/8", 300);       // /8 prefix
            trie.Add("172.16.0.0/12", 400);    // /12 prefix

            // 2. Byte array (fast path, no parsing)
            byte[] prefix = { 8, 8, 8, 0 };
            trie.Add(prefix, 24, 500);

            Console.WriteLine("Added routes:");
            Console.WriteLine("  0.0.0.0/0 -> 1 (default)");
            Console.WriteLine("  192.168.0.0/16 -> 100");
            Console.WriteLine("  192.168.1.0/24 -> 200");
            Console.WriteLine("  10.0.0.0/8 -> 300");
            Console.WriteLine("  172.16.0.0/12 -> 400");
            Console.WriteLine("  8.8.8.0/24 -> 500");
            Console.WriteLine();

            // Perform lookups
            Console.WriteLine("Lookups:");
            
            // String-based lookup
            var result1 = trie.Lookup("192.168.1.100");
            Console.WriteLine($"  192.168.1.100 -> {result1} (matches /24)");
            
            var result2 = trie.Lookup("192.168.2.50");
            Console.WriteLine($"  192.168.2.50 -> {result2} (matches /16)");
            
            // IPAddress lookup
            var addr = IPAddress.Parse("10.255.255.255");
            var result3 = trie.Lookup(addr);
            Console.WriteLine($"  10.255.255.255 -> {result3} (matches /8)");
            
            // Byte array lookup (fastest)
            byte[] lookupAddr = { 8, 8, 8, 8 };
            var result4 = trie.Lookup(lookupAddr);
            Console.WriteLine($"  8.8.8.8 -> {result4} (matches /24)");
            
            // Address with no specific route (falls back to default)
            var result5 = trie.Lookup("1.2.3.4");
            Console.WriteLine($"  1.2.3.4 -> {result5} (matches default)");
            Console.WriteLine();

            // Delete a route
            Console.WriteLine("Deleting 192.168.1.0/24...");
            bool deleted = trie.Delete("192.168.1.0/24");
            Console.WriteLine($"  Deleted: {deleted}");
            
            var result6 = trie.Lookup("192.168.1.100");
            Console.WriteLine($"  192.168.1.100 -> {result6} (now matches /16)");
        }

        /// <summary>
        /// Demonstrates basic IPv6 operations.
        /// </summary>
        private static void IPv6Example()
        {
            Console.WriteLine("=== IPv6 Example ===");

            // Create an IPv6 trie using the default algorithm (Wide16)
            using var trie = LpmTrieIPv6.CreateDefault();

            // Add common IPv6 routes
            trie.Add("::/0", 1);              // Default route
            trie.Add("2001:db8::/32", 100);    // Documentation prefix
            trie.Add("2001:db8:1::/48", 200);  // More specific
            trie.Add("fe80::/10", 300);        // Link-local
            trie.Add("fc00::/7", 400);         // Unique local
            trie.Add("ff00::/8", 500);         // Multicast

            Console.WriteLine("Added routes:");
            Console.WriteLine("  ::/0 -> 1 (default)");
            Console.WriteLine("  2001:db8::/32 -> 100");
            Console.WriteLine("  2001:db8:1::/48 -> 200");
            Console.WriteLine("  fe80::/10 -> 300 (link-local)");
            Console.WriteLine("  fc00::/7 -> 400 (unique local)");
            Console.WriteLine("  ff00::/8 -> 500 (multicast)");
            Console.WriteLine();

            // Perform lookups
            Console.WriteLine("Lookups:");
            
            var result1 = trie.Lookup("2001:db8:1::1");
            Console.WriteLine($"  2001:db8:1::1 -> {result1} (matches /48)");
            
            var result2 = trie.Lookup("2001:db8:2::1");
            Console.WriteLine($"  2001:db8:2::1 -> {result2} (matches /32)");
            
            var result3 = trie.Lookup("fe80::1");
            Console.WriteLine($"  fe80::1 -> {result3} (link-local)");
            
            var result4 = trie.Lookup("fd00::1");
            Console.WriteLine($"  fd00::1 -> {result4} (unique local)");
            
            var result5 = trie.Lookup("ff02::1");
            Console.WriteLine($"  ff02::1 -> {result5} (multicast)");
            
            var result6 = trie.Lookup("2607:f8b0:4004:800::200e");
            Console.WriteLine($"  2607:f8b0:4004:800::200e -> {result6} (matches default)");
        }

        /// <summary>
        /// Demonstrates fast path operations with byte arrays.
        /// </summary>
        private static void FastPathExample()
        {
            Console.WriteLine("=== Fast Path Example ===");

            using var trie = LpmTrieIPv4.CreateDefault();

            // Fast path: Use byte arrays and uint directly
            // This avoids string parsing overhead
            
            // Add routes using byte arrays
            byte[] prefix1 = { 192, 168, 0, 0 };
            trie.Add(prefix1, 16, 100);

            byte[] prefix2 = { 10, 0, 0, 0 };
            trie.Add(prefix2, 8, 200);

            Console.WriteLine("Added routes using byte arrays");

            // Fast lookup using uint (network byte order)
            // 192.168.1.1 = 0xC0A80101
            uint addr1 = 0xC0A80101;
            uint? result1 = trie.Lookup(addr1);
            Console.WriteLine($"  Lookup 0xC0A80101 -> {result1}");

            // Even faster: LookupRaw returns raw value without nullable check
            uint rawResult = trie.LookupRaw(addr1);
            Console.WriteLine($"  LookupRaw 0xC0A80101 -> {rawResult}");

            // Use Span<T> for zero-copy operations
            Span<byte> spanPrefix = stackalloc byte[] { 172, 16, 0, 0 };
            trie.Add(spanPrefix, 12, 300);

            ReadOnlySpan<byte> spanAddr = stackalloc byte[] { 172, 31, 255, 255 };
            var result2 = trie.Lookup(spanAddr);
            Console.WriteLine($"  Span lookup 172.31.255.255 -> {result2}");

            // Helper methods for conversion
            uint converted = LpmTrieIPv4.ParseIPv4Address("192.168.1.1");
            Console.WriteLine($"  ParseIPv4Address(\"192.168.1.1\") = 0x{converted:X8}");

            byte[] bytes = LpmTrieIPv4.UInt32ToBytes(converted);
            Console.WriteLine($"  UInt32ToBytes(0x{converted:X8}) = [{string.Join(", ", bytes)}]");
        }
    }
}
