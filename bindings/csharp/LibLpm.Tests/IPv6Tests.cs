// IPv6Tests.cs - Unit tests for IPv6 LPM operations

using System;
using System.Net;
using Xunit;

namespace LibLpm.Tests
{
    /// <summary>
    /// Tests for IPv6 LPM trie operations.
    /// </summary>
    public class IPv6Tests
    {
        [Fact]
        public void CreateDefault_ReturnsValidTrie()
        {
            using var trie = LpmTrieIPv6.CreateDefault();
            Assert.NotNull(trie);
            Assert.True(trie.IsIPv6);
            Assert.False(trie.IsDisposed);
        }

        [Fact]
        public void CreateWide16_ReturnsValidTrie()
        {
            using var trie = LpmTrieIPv6.CreateWide16();
            Assert.NotNull(trie);
            Assert.Equal(LpmAlgorithm.Wide16, trie.Algorithm);
        }

        [Fact]
        public void CreateStride8_ReturnsValidTrie()
        {
            using var trie = LpmTrieIPv6.CreateStride8();
            Assert.NotNull(trie);
            Assert.Equal(LpmAlgorithm.Stride8, trie.Algorithm);
        }

        [Fact]
        public void Create_WithDir24_ThrowsArgumentException()
        {
            Assert.Throws<ArgumentException>(() => LpmTrieIPv6.Create(LpmAlgorithm.Dir24));
        }

        [Fact]
        public void AddAndLookup_ByteArray_Success()
        {
            using var trie = LpmTrieIPv6.CreateDefault();
            
            // 2001:db8::
            byte[] prefix = new byte[] { 0x20, 0x01, 0x0d, 0xb8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
            trie.Add(prefix, 32, 100);

            // 2001:db8::1
            byte[] addr = new byte[] { 0x20, 0x01, 0x0d, 0xb8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 };
            var result = trie.Lookup(addr);

            Assert.NotNull(result);
            Assert.Equal(100u, result.Value);
        }

        [Fact]
        public void AddAndLookup_String_Success()
        {
            using var trie = LpmTrieIPv6.CreateDefault();
            
            trie.Add("2001:db8::/32", 200);
            
            var result = trie.Lookup("2001:db8::ffff");

            Assert.NotNull(result);
            Assert.Equal(200u, result.Value);
        }

        [Fact]
        public void AddAndLookup_IPAddress_Success()
        {
            using var trie = LpmTrieIPv6.CreateDefault();
            
            trie.Add("fc00::/7", 300);
            
            var addr = IPAddress.Parse("fd00::1");
            var result = trie.Lookup(addr);

            Assert.NotNull(result);
            Assert.Equal(300u, result.Value);
        }

        [Fact]
        public void Lookup_NoMatch_ReturnsNull()
        {
            using var trie = LpmTrieIPv6.CreateDefault();
            
            trie.Add("2001:db8::/32", 100);

            var result = trie.Lookup("2001:db9::1");

            Assert.Null(result);
        }

        [Fact]
        public void Delete_ExistingPrefix_ReturnsTrue()
        {
            using var trie = LpmTrieIPv6.CreateDefault();
            
            trie.Add("2001:db8::/32", 100);
            
            bool deleted = trie.Delete("2001:db8::/32");

            Assert.True(deleted);
            Assert.Null(trie.Lookup("2001:db8::1"));
        }

        [Fact]
        public void Delete_NonExistingPrefix_ReturnsFalse()
        {
            using var trie = LpmTrieIPv6.CreateDefault();
            
            bool deleted = trie.Delete("2001:db8::/32");

            Assert.False(deleted);
        }

        [Fact]
        public void LongestPrefixMatch_SelectsMostSpecific()
        {
            using var trie = LpmTrieIPv6.CreateDefault();
            
            trie.Add("::/0", 1);              // Default route
            trie.Add("2001:db8::/32", 2);     // /32
            trie.Add("2001:db8:1::/48", 3);   // /48
            trie.Add("2001:db8:1:1::/64", 4); // /64

            Assert.Equal(4u, trie.Lookup("2001:db8:1:1::1")!.Value); // Matches /64
            Assert.Equal(3u, trie.Lookup("2001:db8:1:2::1")!.Value); // Matches /48
            Assert.Equal(2u, trie.Lookup("2001:db8:2::1")!.Value);   // Matches /32
            Assert.Equal(1u, trie.Lookup("2001:db9::1")!.Value);     // Matches default
        }

        [Fact]
        public void DefaultRoute_MatchesEverything()
        {
            using var trie = LpmTrieIPv6.CreateDefault();
            
            trie.Add("::/0", 999);

            Assert.Equal(999u, trie.Lookup("::1")!.Value);
            Assert.Equal(999u, trie.Lookup("ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff")!.Value);
        }

        [Fact]
        public void HostRoute_ExactMatch()
        {
            using var trie = LpmTrieIPv6.CreateDefault();
            
            trie.Add("2001:db8::1/128", 100);

            Assert.Equal(100u, trie.Lookup("2001:db8::1")!.Value);
            Assert.Null(trie.Lookup("2001:db8::2"));
        }

        [Fact]
        public void Add_InvalidPrefixLength_ThrowsException()
        {
            using var trie = LpmTrieIPv6.CreateDefault();
            
            byte[] prefix = new byte[16];

            Assert.Throws<LpmInvalidPrefixException>(() => trie.Add(prefix, 129, 100));
        }

        [Fact]
        public void Add_WrongAddressSize_ThrowsException()
        {
            using var trie = LpmTrieIPv6.CreateDefault();
            
            byte[] prefix = new byte[4]; // Only 4 bytes

            Assert.Throws<LpmInvalidPrefixException>(() => trie.Add(prefix, 32, 100));
        }

        [Fact]
        public void Add_IPv4Address_ThrowsException()
        {
            using var trie = LpmTrieIPv6.CreateDefault();
            
            Assert.Throws<LpmInvalidPrefixException>(() => trie.Add("192.168.0.0/16", 100));
        }

        [Fact]
        public void Lookup_IPv4Address_ThrowsException()
        {
            using var trie = LpmTrieIPv6.CreateDefault();
            
            var addr = IPAddress.Parse("192.168.1.1");

            Assert.Throws<ArgumentException>(() => trie.Lookup(addr));
        }

        [Fact]
        public void ParseIPv6Address_ValidAddress_Success()
        {
            byte[] result = LpmTrieIPv6.ParseIPv6Address("2001:db8::1");

            Assert.Equal(16, result.Length);
            Assert.Equal(0x20, result[0]);
            Assert.Equal(0x01, result[1]);
            Assert.Equal(0x0d, result[2]);
            Assert.Equal(0xb8, result[3]);
            Assert.Equal(0x01, result[15]);
        }

        [Fact]
        public void ParseIPv6Address_InvalidAddress_ThrowsException()
        {
            Assert.Throws<ArgumentException>(() => LpmTrieIPv6.ParseIPv6Address("invalid"));
        }

        [Fact]
        public void FormatIPv6Address_ValidBytes_Success()
        {
            byte[] bytes = new byte[] { 0x20, 0x01, 0x0d, 0xb8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 };
            string result = LpmTrieIPv6.FormatIPv6Address(bytes);

            Assert.Contains("2001:db8", result.ToLower());
        }

        [Fact]
        public void CreateAddressBuffer_Returns16Bytes()
        {
            byte[] buffer = LpmTrieIPv6.CreateAddressBuffer();

            Assert.Equal(16, buffer.Length);
            Assert.All(buffer, b => Assert.Equal(0, b));
        }

        [Fact]
        public void Span_AddAndLookup_Success()
        {
            using var trie = LpmTrieIPv6.CreateDefault();
            
            Span<byte> prefix = stackalloc byte[16];
            prefix[0] = 0x20;
            prefix[1] = 0x01;
            prefix[2] = 0x0d;
            prefix[3] = 0xb8;
            trie.Add(prefix, 32, 100);

            Span<byte> addr = stackalloc byte[16];
            prefix.CopyTo(addr);
            addr[15] = 1;
            var result = trie.Lookup(addr);

            Assert.Equal(100u, result!.Value);
        }

        [Fact]
        public void OverwritePrefix_Success()
        {
            using var trie = LpmTrieIPv6.CreateDefault();
            
            trie.Add("2001:db8::/32", 100);
            trie.Add("2001:db8::/32", 200); // Overwrite

            Assert.Equal(200u, trie.Lookup("2001:db8::1")!.Value);
        }

        [Fact]
        public void CommonIPv6Prefixes_Success()
        {
            using var trie = LpmTrieIPv6.CreateDefault();
            
            // Common IPv6 prefixes
            trie.Add("::1/128", 1);           // Loopback
            trie.Add("fe80::/10", 2);         // Link-local
            trie.Add("fc00::/7", 3);          // Unique local
            trie.Add("2000::/3", 4);          // Global unicast
            trie.Add("ff00::/8", 5);          // Multicast

            Assert.Equal(1u, trie.Lookup("::1")!.Value);
            Assert.Equal(2u, trie.Lookup("fe80::1")!.Value);
            Assert.Equal(3u, trie.Lookup("fd00::1")!.Value);
            Assert.Equal(4u, trie.Lookup("2001:db8::1")!.Value);
            Assert.Equal(5u, trie.Lookup("ff02::1")!.Value);
        }
    }
}
