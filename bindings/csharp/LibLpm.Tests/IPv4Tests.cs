// IPv4Tests.cs - Unit tests for IPv4 LPM operations

using System;
using System.Net;
using Xunit;

namespace LibLpm.Tests
{
    /// <summary>
    /// Tests for IPv4 LPM trie operations.
    /// </summary>
    public class IPv4Tests
    {
        [Fact]
        public void CreateDefault_ReturnsValidTrie()
        {
            using var trie = LpmTrieIPv4.CreateDefault();
            Assert.NotNull(trie);
            Assert.False(trie.IsIPv6);
            Assert.False(trie.IsDisposed);
        }

        [Fact]
        public void CreateDir24_ReturnsValidTrie()
        {
            using var trie = LpmTrieIPv4.CreateDir24();
            Assert.NotNull(trie);
            Assert.Equal(LpmAlgorithm.Dir24, trie.Algorithm);
        }

        [Fact]
        public void CreateStride8_ReturnsValidTrie()
        {
            using var trie = LpmTrieIPv4.CreateStride8();
            Assert.NotNull(trie);
            Assert.Equal(LpmAlgorithm.Stride8, trie.Algorithm);
        }

        [Fact]
        public void Create_WithWide16_ThrowsArgumentException()
        {
            Assert.Throws<ArgumentException>(() => LpmTrieIPv4.Create(LpmAlgorithm.Wide16));
        }

        [Fact]
        public void AddAndLookup_ByteArray_Success()
        {
            using var trie = LpmTrieIPv4.CreateDefault();
            
            byte[] prefix = { 192, 168, 0, 0 };
            trie.Add(prefix, 16, 100);

            byte[] addr = { 192, 168, 1, 1 };
            var result = trie.Lookup(addr);

            Assert.NotNull(result);
            Assert.Equal(100u, result.Value);
        }

        [Fact]
        public void AddAndLookup_String_Success()
        {
            using var trie = LpmTrieIPv4.CreateDefault();
            
            trie.Add("10.0.0.0/8", 200);
            
            var result = trie.Lookup("10.255.255.255");

            Assert.NotNull(result);
            Assert.Equal(200u, result.Value);
        }

        [Fact]
        public void AddAndLookup_IPAddress_Success()
        {
            using var trie = LpmTrieIPv4.CreateDefault();
            
            trie.Add("172.16.0.0/12", 300);
            
            var addr = IPAddress.Parse("172.31.255.255");
            var result = trie.Lookup(addr);

            Assert.NotNull(result);
            Assert.Equal(300u, result.Value);
        }

        [Fact]
        public void AddAndLookup_UInt32_Success()
        {
            using var trie = LpmTrieIPv4.CreateDefault();
            
            byte[] prefix = { 192, 168, 0, 0 };
            trie.Add(prefix, 16, 100);

            // 192.168.1.1 in network byte order
            uint addr = 0xC0A80101;
            var result = trie.Lookup(addr);

            Assert.NotNull(result);
            Assert.Equal(100u, result.Value);
        }

        [Fact]
        public void Lookup_NoMatch_ReturnsNull()
        {
            using var trie = LpmTrieIPv4.CreateDefault();
            
            trie.Add("192.168.0.0/16", 100);

            var result = trie.Lookup("10.0.0.1");

            Assert.Null(result);
        }

        [Fact]
        public void LookupRaw_NoMatch_ReturnsInvalidNextHop()
        {
            using var trie = LpmTrieIPv4.CreateDefault();

            uint result = trie.LookupRaw(0x0A000001); // 10.0.0.1

            Assert.Equal(LpmConstants.InvalidNextHop, result);
        }

        [Fact]
        public void Delete_ExistingPrefix_ReturnsTrue()
        {
            using var trie = LpmTrieIPv4.CreateDefault();
            
            trie.Add("192.168.0.0/16", 100);
            
            bool deleted = trie.Delete("192.168.0.0/16");

            Assert.True(deleted);
            Assert.Null(trie.Lookup("192.168.1.1"));
        }

        [Fact]
        public void Delete_NonExistingPrefix_ReturnsFalse()
        {
            using var trie = LpmTrieIPv4.CreateDefault();
            
            bool deleted = trie.Delete("192.168.0.0/16");

            Assert.False(deleted);
        }

        [Fact]
        public void LongestPrefixMatch_SelectsMostSpecific()
        {
            using var trie = LpmTrieIPv4.CreateDefault();
            
            trie.Add("0.0.0.0/0", 1);       // Default route
            trie.Add("192.168.0.0/16", 2);   // /16
            trie.Add("192.168.1.0/24", 3);   // /24
            trie.Add("192.168.1.128/25", 4); // /25

            Assert.Equal(4u, trie.Lookup("192.168.1.200")!.Value); // Matches /25
            Assert.Equal(3u, trie.Lookup("192.168.1.100")!.Value); // Matches /24
            Assert.Equal(2u, trie.Lookup("192.168.2.1")!.Value);   // Matches /16
            Assert.Equal(1u, trie.Lookup("10.0.0.1")!.Value);      // Matches default
        }

        [Fact]
        public void DefaultRoute_MatchesEverything()
        {
            using var trie = LpmTrieIPv4.CreateDefault();
            
            trie.Add("0.0.0.0/0", 999);

            Assert.Equal(999u, trie.Lookup("1.2.3.4")!.Value);
            Assert.Equal(999u, trie.Lookup("255.255.255.255")!.Value);
        }

        [Fact]
        public void HostRoute_ExactMatch()
        {
            using var trie = LpmTrieIPv4.CreateDefault();
            
            trie.Add("192.168.1.1/32", 100);

            Assert.Equal(100u, trie.Lookup("192.168.1.1")!.Value);
            Assert.Null(trie.Lookup("192.168.1.2"));
        }

        [Fact]
        public void TryAdd_ValidPrefix_ReturnsTrue()
        {
            using var trie = LpmTrieIPv4.CreateDefault();
            
            byte[] prefix = { 192, 168, 0, 0 };
            bool result = trie.TryAdd(prefix, 16, 100);

            Assert.True(result);
        }

        [Fact]
        public void TryAdd_InvalidPrefixLength_ReturnsFalse()
        {
            using var trie = LpmTrieIPv4.CreateDefault();
            
            byte[] prefix = { 192, 168, 0, 0 };
            bool result = trie.TryAdd(prefix, 33, 100); // Invalid: > 32

            Assert.False(result);
        }

        [Fact]
        public void Add_InvalidPrefixLength_ThrowsException()
        {
            using var trie = LpmTrieIPv4.CreateDefault();
            
            byte[] prefix = { 192, 168, 0, 0 };

            Assert.Throws<LpmInvalidPrefixException>(() => trie.Add(prefix, 33, 100));
        }

        [Fact]
        public void Add_WrongAddressSize_ThrowsException()
        {
            using var trie = LpmTrieIPv4.CreateDefault();
            
            byte[] prefix = { 192, 168, 0 }; // Only 3 bytes

            Assert.Throws<LpmInvalidPrefixException>(() => trie.Add(prefix, 16, 100));
        }

        [Fact]
        public void Add_IPv6Address_ThrowsException()
        {
            using var trie = LpmTrieIPv4.CreateDefault();
            
            Assert.Throws<LpmInvalidPrefixException>(() => trie.Add("2001:db8::/32", 100));
        }

        [Fact]
        public void Lookup_IPv6Address_ThrowsException()
        {
            using var trie = LpmTrieIPv4.CreateDefault();
            
            var addr = IPAddress.Parse("2001:db8::1");

            Assert.Throws<ArgumentException>(() => trie.Lookup(addr));
        }

        [Fact]
        public void ParseIPv4Address_ValidAddress_Success()
        {
            uint result = LpmTrieIPv4.ParseIPv4Address("192.168.1.1");

            // 192.168.1.1 in network byte order = 0xC0A80101
            Assert.Equal(0xC0A80101u, result);
        }

        [Fact]
        public void ParseIPv4Address_InvalidAddress_ThrowsException()
        {
            Assert.Throws<ArgumentException>(() => LpmTrieIPv4.ParseIPv4Address("invalid"));
        }

        [Fact]
        public void BytesToUInt32_ValidBytes_Success()
        {
            byte[] bytes = { 192, 168, 1, 1 };
            uint result = LpmTrieIPv4.BytesToUInt32(bytes);

            Assert.Equal(0xC0A80101u, result);
        }

        [Fact]
        public void UInt32ToBytes_ValidAddress_Success()
        {
            byte[] result = LpmTrieIPv4.UInt32ToBytes(0xC0A80101);

            Assert.Equal(new byte[] { 192, 168, 1, 1 }, result);
        }

        [Fact]
        public void Span_AddAndLookup_Success()
        {
            using var trie = LpmTrieIPv4.CreateDefault();
            
            ReadOnlySpan<byte> prefix = stackalloc byte[] { 192, 168, 0, 0 };
            trie.Add(prefix, 16, 100);

            ReadOnlySpan<byte> addr = stackalloc byte[] { 192, 168, 1, 1 };
            var result = trie.Lookup(addr);

            Assert.Equal(100u, result!.Value);
        }

        [Fact]
        public void OverwritePrefix_Success()
        {
            using var trie = LpmTrieIPv4.CreateDefault();
            
            trie.Add("192.168.0.0/16", 100);
            trie.Add("192.168.0.0/16", 200); // Overwrite

            Assert.Equal(200u, trie.Lookup("192.168.1.1")!.Value);
        }
    }
}
