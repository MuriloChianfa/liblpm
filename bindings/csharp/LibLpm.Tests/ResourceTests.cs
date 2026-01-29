// ResourceTests.cs - Unit tests for resource management and lifecycle

using System;
using Xunit;

namespace LibLpm.Tests
{
    /// <summary>
    /// Tests for resource management and object lifecycle.
    /// </summary>
    public class ResourceTests
    {
        [Fact]
        public void Dispose_SetsIsDisposed()
        {
            var trie = LpmTrieIPv4.CreateDefault();
            Assert.False(trie.IsDisposed);
            
            trie.Dispose();
            
            Assert.True(trie.IsDisposed);
        }

        [Fact]
        public void Dispose_MultipleCalls_Safe()
        {
            var trie = LpmTrieIPv4.CreateDefault();
            
            trie.Dispose();
            trie.Dispose(); // Should not throw
            trie.Dispose();
            
            Assert.True(trie.IsDisposed);
        }

        [Fact]
        public void Add_AfterDispose_ThrowsObjectDisposedException()
        {
            var trie = LpmTrieIPv4.CreateDefault();
            trie.Dispose();

            Assert.Throws<ObjectDisposedException>(() => trie.Add("192.168.0.0/16", 100));
        }

        [Fact]
        public void Lookup_AfterDispose_ThrowsObjectDisposedException()
        {
            var trie = LpmTrieIPv4.CreateDefault();
            trie.Dispose();

            Assert.Throws<ObjectDisposedException>(() => trie.Lookup("192.168.1.1"));
        }

        [Fact]
        public void Delete_AfterDispose_ThrowsObjectDisposedException()
        {
            var trie = LpmTrieIPv4.CreateDefault();
            trie.Dispose();

            Assert.Throws<ObjectDisposedException>(() => trie.Delete("192.168.0.0/16"));
        }

        [Fact]
        public void NativeHandle_AfterDispose_ThrowsObjectDisposedException()
        {
            var trie = LpmTrieIPv4.CreateDefault();
            trie.Dispose();

            Assert.Throws<ObjectDisposedException>(() => _ = trie.NativeHandle);
        }

        [Fact]
        public void Using_DisposesAutomatically()
        {
            LpmTrieIPv4 trie;
            
            using (trie = LpmTrieIPv4.CreateDefault())
            {
                Assert.False(trie.IsDisposed);
            }
            
            Assert.True(trie.IsDisposed);
        }

        [Fact]
        public void TryAdd_AfterDispose_ReturnsFalse()
        {
            var trie = LpmTrieIPv4.CreateDefault();
            trie.Dispose();

            byte[] prefix = { 192, 168, 0, 0 };
            bool result = trie.TryAdd(prefix, 16, 100);

            Assert.False(result);
        }

        [Fact]
        public void IPv6_Dispose_Works()
        {
            var trie = LpmTrieIPv6.CreateDefault();
            Assert.False(trie.IsDisposed);
            
            trie.Dispose();
            
            Assert.True(trie.IsDisposed);
        }

        [Fact]
        public void IPv6_Add_AfterDispose_ThrowsObjectDisposedException()
        {
            var trie = LpmTrieIPv6.CreateDefault();
            trie.Dispose();

            Assert.Throws<ObjectDisposedException>(() => trie.Add("2001:db8::/32", 100));
        }

        [Fact]
        public void GetVersion_ReturnsString()
        {
            var version = LpmTrie.GetVersion();
            
            Assert.NotNull(version);
            Assert.NotEmpty(version);
        }

        [Fact]
        public void NativeLibraryLoader_FindLibraryPath_ReturnsPathOrNull()
        {
            var path = NativeLibraryLoader.FindLibraryPath();
            
            // Path may or may not be found depending on environment
            // Just ensure no exception is thrown
        }

        [Fact]
        public void NativeLibraryLoader_RuntimeIdentifier_ReturnsValidRid()
        {
            var rid = NativeLibraryLoader.RuntimeIdentifier;
            
            Assert.NotNull(rid);
            Assert.NotEmpty(rid);
            Assert.Contains("-", rid); // e.g., linux-x64, win-x64
        }

        [Fact]
        public void NativeLibraryLoader_NativeLibraryName_ReturnsValidName()
        {
            var name = NativeLibraryLoader.NativeLibraryName;
            
            Assert.NotNull(name);
            Assert.NotEmpty(name);
            Assert.True(
                name.EndsWith(".so") || 
                name.EndsWith(".dll") || 
                name.EndsWith(".dylib"));
        }

        [Fact]
        public void SafeLpmHandle_Invalid_IsInvalid()
        {
            var handle = SafeLpmHandle.Invalid;
            
            Assert.True(handle.IsInvalid);
        }

        [Fact]
        public void PrintStats_AfterDispose_ThrowsObjectDisposedException()
        {
            var trie = LpmTrieIPv4.CreateDefault();
            trie.Dispose();

            Assert.Throws<ObjectDisposedException>(() => trie.PrintStats());
        }

        [Fact]
        public void MaxPrefixLength_IPv4_Returns32()
        {
            using var trie = LpmTrieIPv4.CreateDefault();
            Assert.Equal(32, trie.MaxPrefixLength);
        }

        [Fact]
        public void MaxPrefixLength_IPv6_Returns128()
        {
            using var trie = LpmTrieIPv6.CreateDefault();
            Assert.Equal(128, trie.MaxPrefixLength);
        }

        [Fact]
        public void PrefixByteLength_IPv4_Returns4()
        {
            using var trie = LpmTrieIPv4.CreateDefault();
            Assert.Equal(4, trie.PrefixByteLength);
        }

        [Fact]
        public void PrefixByteLength_IPv6_Returns16()
        {
            using var trie = LpmTrieIPv6.CreateDefault();
            Assert.Equal(16, trie.PrefixByteLength);
        }

        [Fact]
        public void LpmConstants_InvalidNextHop_IsMaxUInt()
        {
            Assert.Equal(uint.MaxValue, LpmConstants.InvalidNextHop);
        }

        [Fact]
        public void LpmConstants_IPv4MaxDepth_Is32()
        {
            Assert.Equal(32, LpmConstants.IPv4MaxDepth);
        }

        [Fact]
        public void LpmConstants_IPv6MaxDepth_Is128()
        {
            Assert.Equal(128, LpmConstants.IPv6MaxDepth);
        }

        [Fact]
        public void MultipleTriesCanCoexist()
        {
            using var trie1 = LpmTrieIPv4.CreateDefault();
            using var trie2 = LpmTrieIPv4.CreateDir24();
            using var trie3 = LpmTrieIPv6.CreateDefault();

            trie1.Add("192.168.0.0/16", 1);
            trie2.Add("10.0.0.0/8", 2);
            trie3.Add("2001:db8::/32", 3);

            Assert.Equal(1u, trie1.Lookup("192.168.1.1")!.Value);
            Assert.Equal(2u, trie2.Lookup("10.1.1.1")!.Value);
            Assert.Equal(3u, trie3.Lookup("2001:db8::1")!.Value);

            // Cross-check: trie1 shouldn't have trie2's data
            Assert.Null(trie1.Lookup("10.1.1.1"));
        }
    }
}
