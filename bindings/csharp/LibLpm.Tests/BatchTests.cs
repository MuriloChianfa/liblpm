// BatchTests.cs - Unit tests for batch LPM operations

using System;
using System.Buffers.Binary;
using Xunit;

namespace LibLpm.Tests
{
    /// <summary>
    /// Tests for batch lookup operations.
    /// </summary>
    public class BatchTests
    {
        [Fact]
        public void IPv4_LookupBatch_UInt32Array_Success()
        {
            using var trie = LpmTrieIPv4.CreateDefault();
            
            trie.Add("192.168.0.0/16", 100);
            trie.Add("10.0.0.0/8", 200);

            uint[] addresses = new uint[]
            {
                0xC0A80101, // 192.168.1.1
                0xC0A80202, // 192.168.2.2
                0x0A010101, // 10.1.1.1
                0x08080808, // 8.8.8.8 (no match)
            };
            uint[] results = new uint[4];

            trie.LookupBatch(addresses, results);

            Assert.Equal(100u, results[0]);
            Assert.Equal(100u, results[1]);
            Assert.Equal(200u, results[2]);
            Assert.Equal(LpmConstants.InvalidNextHop, results[3]);
        }

        [Fact]
        public void IPv4_LookupBatch_Span_Success()
        {
            using var trie = LpmTrieIPv4.CreateDefault();
            
            trie.Add("192.168.0.0/16", 100);

            Span<uint> addresses = stackalloc uint[]
            {
                0xC0A80101,
                0xC0A80202,
            };
            Span<uint> results = stackalloc uint[2];

            trie.LookupBatch(addresses, results);

            Assert.Equal(100u, results[0]);
            Assert.Equal(100u, results[1]);
        }

        [Fact]
        public void IPv4_LookupBatch_ByteArray_Success()
        {
            using var trie = LpmTrieIPv4.CreateDefault();
            
            trie.Add("192.168.0.0/16", 100);

            // 2 addresses, 4 bytes each = 8 bytes total
            byte[] addresses = new byte[]
            {
                192, 168, 1, 1, // Address 1
                192, 168, 2, 2, // Address 2
            };
            uint[] results = new uint[2];

            trie.LookupBatch(addresses, results);

            Assert.Equal(100u, results[0]);
            Assert.Equal(100u, results[1]);
        }

        [Fact]
        public void IPv4_LookupBatch_EmptyArray_Success()
        {
            using var trie = LpmTrieIPv4.CreateDefault();
            
            uint[] addresses = Array.Empty<uint>();
            uint[] results = Array.Empty<uint>();

            trie.LookupBatch(addresses, results);
            // Should complete without error
        }

        [Fact]
        public void IPv4_LookupBatch_LargeArray_Success()
        {
            using var trie = LpmTrieIPv4.CreateDefault();
            
            trie.Add("0.0.0.0/0", 999);

            const int count = 10000;
            uint[] addresses = new uint[count];
            uint[] results = new uint[count];

            // Fill with sequential addresses
            for (int i = 0; i < count; i++)
            {
                addresses[i] = (uint)i;
            }

            trie.LookupBatch(addresses, results);

            // All should match the default route
            Assert.All(results, r => Assert.Equal(999u, r));
        }

        [Fact]
        public void IPv4_LookupBatch_OutputTooSmall_ThrowsException()
        {
            using var trie = LpmTrieIPv4.CreateDefault();
            
            uint[] addresses = new uint[10];
            uint[] results = new uint[5]; // Too small

            Assert.Throws<ArgumentException>(() => trie.LookupBatch(addresses, results));
        }

        [Fact]
        public void IPv4_LookupBatch_InvalidByteArrayLength_ThrowsException()
        {
            using var trie = LpmTrieIPv4.CreateDefault();
            
            byte[] addresses = new byte[7]; // Not a multiple of 4
            uint[] results = new uint[2];

            Assert.Throws<ArgumentException>(() => trie.LookupBatch(addresses, results));
        }

        [Fact]
        public void IPv6_LookupBatch_ByteArray_Success()
        {
            using var trie = LpmTrieIPv6.CreateDefault();
            
            trie.Add("2001:db8::/32", 100);

            // 2 addresses, 16 bytes each = 32 bytes total
            byte[] addresses = new byte[32];
            
            // Address 1: 2001:db8::1
            addresses[0] = 0x20;
            addresses[1] = 0x01;
            addresses[2] = 0x0d;
            addresses[3] = 0xb8;
            addresses[15] = 0x01;
            
            // Address 2: 2001:db8::2
            addresses[16] = 0x20;
            addresses[17] = 0x01;
            addresses[18] = 0x0d;
            addresses[19] = 0xb8;
            addresses[31] = 0x02;

            uint[] results = new uint[2];

            trie.LookupBatch(addresses, results);

            Assert.Equal(100u, results[0]);
            Assert.Equal(100u, results[1]);
        }

        [Fact]
        public void IPv6_LookupBatch_Span_Success()
        {
            using var trie = LpmTrieIPv6.CreateDefault();
            
            trie.Add("2001:db8::/32", 100);

            Span<byte> addresses = stackalloc byte[32];
            
            // Address 1
            addresses[0] = 0x20;
            addresses[1] = 0x01;
            addresses[2] = 0x0d;
            addresses[3] = 0xb8;
            addresses[15] = 0x01;
            
            // Address 2
            addresses[16] = 0x20;
            addresses[17] = 0x01;
            addresses[18] = 0x0d;
            addresses[19] = 0xb8;
            addresses[31] = 0x02;

            Span<uint> results = stackalloc uint[2];

            trie.LookupBatch(addresses, results);

            Assert.Equal(100u, results[0]);
            Assert.Equal(100u, results[1]);
        }

        [Fact]
        public void IPv6_LookupBatch_EmptyArray_Success()
        {
            using var trie = LpmTrieIPv6.CreateDefault();
            
            byte[] addresses = Array.Empty<byte>();
            uint[] results = Array.Empty<uint>();

            trie.LookupBatch(addresses, results);
            // Should complete without error
        }

        [Fact]
        public void IPv6_LookupBatch_InvalidByteArrayLength_ThrowsException()
        {
            using var trie = LpmTrieIPv6.CreateDefault();
            
            byte[] addresses = new byte[20]; // Not a multiple of 16
            uint[] results = new uint[2];

            Assert.Throws<ArgumentException>(() => trie.LookupBatch(addresses, results));
        }

        [Fact]
        public void IPv6_LookupBatch_OutputTooSmall_ThrowsException()
        {
            using var trie = LpmTrieIPv6.CreateDefault();
            
            byte[] addresses = new byte[32]; // 2 addresses
            uint[] results = new uint[1]; // Too small

            Assert.Throws<ArgumentException>(() => trie.LookupBatch(addresses, results));
        }

        [Fact]
        public void IPv6_LookupBatch_MixedMatches_Success()
        {
            using var trie = LpmTrieIPv6.CreateDefault();
            
            trie.Add("2001:db8::/32", 100);
            trie.Add("fc00::/7", 200);

            byte[] addresses = new byte[48]; // 3 addresses
            
            // Address 1: 2001:db8::1 (should match 100)
            addresses[0] = 0x20;
            addresses[1] = 0x01;
            addresses[2] = 0x0d;
            addresses[3] = 0xb8;
            addresses[15] = 0x01;
            
            // Address 2: fd00::1 (should match 200)
            addresses[16] = 0xfd;
            addresses[31] = 0x01;
            
            // Address 3: 2001:db9::1 (no match)
            addresses[32] = 0x20;
            addresses[33] = 0x01;
            addresses[34] = 0x0d;
            addresses[35] = 0xb9;
            addresses[47] = 0x01;

            uint[] results = new uint[3];

            trie.LookupBatch(addresses, results);

            Assert.Equal(100u, results[0]);
            Assert.Equal(200u, results[1]);
            Assert.Equal(LpmConstants.InvalidNextHop, results[2]);
        }

        [Fact]
        public void IPv4_LookupBatch_OutputLargerThanInput_Success()
        {
            using var trie = LpmTrieIPv4.CreateDefault();
            
            trie.Add("0.0.0.0/0", 100);

            uint[] addresses = new uint[] { 0x01020304 };
            uint[] results = new uint[10]; // Larger than needed

            trie.LookupBatch(addresses, results);

            Assert.Equal(100u, results[0]);
            // Rest of results array is unspecified
        }
    }
}
