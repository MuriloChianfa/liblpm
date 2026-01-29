// LpmTrieIPv4.cs - IPv4-specific LPM trie wrapper
// Provides optimized IPv4 operations and convenience methods

using System;
using System.Buffers.Binary;
using System.Net;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace LibLpm
{
    /// <summary>
    /// IPv4-specific LPM (Longest Prefix Match) trie.
    /// Provides optimized operations for IPv4 address lookups.
    /// </summary>
    /// <remarks>
    /// This class is not thread-safe. For concurrent access, use external synchronization.
    /// Implements IDisposable for deterministic resource cleanup.
    /// </remarks>
    public sealed class LpmTrieIPv4 : LpmTrie
    {
        private readonly LpmAlgorithm _algorithm;

        /// <summary>
        /// Initializes a new instance of the <see cref="LpmTrieIPv4"/> class.
        /// </summary>
        /// <param name="handle">The native handle to wrap.</param>
        /// <param name="algorithm">The algorithm used for this trie.</param>
        private LpmTrieIPv4(SafeLpmHandle handle, LpmAlgorithm algorithm) : base(handle)
        {
            _algorithm = algorithm;
        }

        /// <inheritdoc/>
        public override bool IsIPv6 => false;

        /// <summary>
        /// Gets the algorithm used for this trie.
        /// </summary>
        public LpmAlgorithm Algorithm => _algorithm;

        // ============================================================================
        // Factory Methods
        // ============================================================================

        /// <summary>
        /// Creates a new IPv4 LPM trie using the default algorithm (DIR-24-8).
        /// </summary>
        /// <returns>A new IPv4 LPM trie.</returns>
        /// <exception cref="LpmCreationException">Thrown if the trie creation fails.</exception>
        public static LpmTrieIPv4 CreateDefault()
        {
            NativeLibraryLoader.Initialize();
            var ptr = NativeMethods.lpm_create_ipv4();
            if (ptr == IntPtr.Zero)
            {
                throw new LpmCreationException(isIPv6: false, LpmAlgorithm.Default);
            }
            return new LpmTrieIPv4(new SafeLpmHandle(ptr), LpmAlgorithm.Default);
        }

        /// <summary>
        /// Creates a new IPv4 LPM trie using DIR-24-8 algorithm explicitly.
        /// </summary>
        /// <returns>A new IPv4 LPM trie with DIR-24-8 algorithm.</returns>
        /// <exception cref="LpmCreationException">Thrown if the trie creation fails.</exception>
        public static LpmTrieIPv4 CreateDir24()
        {
            NativeLibraryLoader.Initialize();
            var ptr = NativeMethods.lpm_create_ipv4_dir24();
            if (ptr == IntPtr.Zero)
            {
                throw new LpmCreationException(isIPv6: false, LpmAlgorithm.Dir24);
            }
            return new LpmTrieIPv4(new SafeLpmHandle(ptr), LpmAlgorithm.Dir24);
        }

        /// <summary>
        /// Creates a new IPv4 LPM trie using 8-bit stride algorithm.
        /// </summary>
        /// <returns>A new IPv4 LPM trie with 8-bit stride algorithm.</returns>
        /// <exception cref="LpmCreationException">Thrown if the trie creation fails.</exception>
        public static LpmTrieIPv4 CreateStride8()
        {
            NativeLibraryLoader.Initialize();
            var ptr = NativeMethods.lpm_create_ipv4_8stride();
            if (ptr == IntPtr.Zero)
            {
                throw new LpmCreationException(isIPv6: false, LpmAlgorithm.Stride8);
            }
            return new LpmTrieIPv4(new SafeLpmHandle(ptr), LpmAlgorithm.Stride8);
        }

        /// <summary>
        /// Creates a new IPv4 LPM trie with the specified algorithm.
        /// </summary>
        /// <param name="algorithm">The algorithm to use.</param>
        /// <returns>A new IPv4 LPM trie.</returns>
        /// <exception cref="LpmCreationException">Thrown if the trie creation fails.</exception>
        /// <exception cref="ArgumentException">Thrown if an IPv6-only algorithm is specified.</exception>
        public static LpmTrieIPv4 Create(LpmAlgorithm algorithm)
        {
            return algorithm switch
            {
                LpmAlgorithm.Default => CreateDefault(),
                LpmAlgorithm.Dir24 => CreateDir24(),
                LpmAlgorithm.Stride8 => CreateStride8(),
                LpmAlgorithm.Wide16 => throw new ArgumentException(
                    "Wide16 algorithm is only valid for IPv6 tries.", nameof(algorithm)),
                _ => throw new ArgumentOutOfRangeException(nameof(algorithm))
            };
        }

        // ============================================================================
        // Lookup Operations
        // ============================================================================

        /// <inheritdoc/>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public override uint? Lookup(byte[] address)
        {
            ThrowIfDisposed();
            ValidateAddress(address);

            // Convert bytes to uint32 in network byte order (big-endian)
            uint addr32 = BytesToUInt32(address);
            uint result = NativeMethods.lpm_lookup_ipv4(_handle.DangerousGetHandle(), addr32);
            return result == LpmConstants.InvalidNextHop ? null : result;
        }

        /// <inheritdoc/>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public override unsafe uint? Lookup(ReadOnlySpan<byte> address)
        {
            ThrowIfDisposed();
            if (address.Length != 4)
            {
                throw new ArgumentException("IPv4 address must be 4 bytes.", nameof(address));
            }

            // Convert bytes to uint32 in network byte order (big-endian)
            uint addr32 = (uint)(address[0] << 24 | address[1] << 16 | address[2] << 8 | address[3]);
            uint result = NativeMethods.lpm_lookup_ipv4(_handle.DangerousGetHandle(), addr32);
            return result == LpmConstants.InvalidNextHop ? null : result;
        }

        /// <summary>
        /// Performs a lookup for the given IPv4 address as a 32-bit integer.
        /// </summary>
        /// <param name="address">The IPv4 address in network byte order (big-endian).</param>
        /// <returns>The next hop value if found, or null if no match.</returns>
        /// <exception cref="ObjectDisposedException">Thrown if the trie has been disposed.</exception>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public uint? Lookup(uint address)
        {
            ThrowIfDisposed();
            uint result = NativeMethods.lpm_lookup_ipv4(_handle.DangerousGetHandle(), address);
            return result == LpmConstants.InvalidNextHop ? null : result;
        }

        /// <summary>
        /// Performs a raw lookup returning the raw next hop value (including InvalidNextHop).
        /// This is the fastest lookup method with no null checking overhead.
        /// </summary>
        /// <param name="address">The IPv4 address in network byte order.</param>
        /// <returns>The raw next hop value, or LpmConstants.InvalidNextHop if no match.</returns>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public uint LookupRaw(uint address)
        {
            ThrowIfDisposed();
            return NativeMethods.lpm_lookup_ipv4(_handle.DangerousGetHandle(), address);
        }

        // ============================================================================
        // Batch Operations
        // ============================================================================

        /// <summary>
        /// Performs batch lookups for multiple IPv4 addresses.
        /// </summary>
        /// <param name="addresses">Array of IPv4 addresses (network byte order).</param>
        /// <param name="nextHops">Output array for next hop values.</param>
        /// <exception cref="ArgumentNullException">Thrown if addresses or nextHops is null.</exception>
        /// <exception cref="ArgumentException">Thrown if array lengths don't match.</exception>
        /// <exception cref="ObjectDisposedException">Thrown if the trie has been disposed.</exception>
        public void LookupBatch(uint[] addresses, uint[] nextHops)
        {
            ThrowIfDisposed();
            if (addresses == null) throw new ArgumentNullException(nameof(addresses));
            if (nextHops == null) throw new ArgumentNullException(nameof(nextHops));
            if (addresses.Length > nextHops.Length)
            {
                throw new ArgumentException("Output array must be at least as large as input array.");
            }

            NativeMethods.lpm_lookup_batch_ipv4(
                _handle.DangerousGetHandle(),
                addresses,
                nextHops,
                (UIntPtr)addresses.Length);
        }

        /// <summary>
        /// Performs batch lookups for multiple IPv4 addresses (Span version for zero-copy).
        /// </summary>
        /// <param name="addresses">Span of IPv4 addresses (network byte order).</param>
        /// <param name="nextHops">Output span for next hop values.</param>
        /// <exception cref="ArgumentException">Thrown if output span is too small.</exception>
        /// <exception cref="ObjectDisposedException">Thrown if the trie has been disposed.</exception>
        public unsafe void LookupBatch(ReadOnlySpan<uint> addresses, Span<uint> nextHops)
        {
            ThrowIfDisposed();
            if (addresses.Length > nextHops.Length)
            {
                throw new ArgumentException("Output span must be at least as large as input span.");
            }

            fixed (uint* addrPtr = addresses)
            fixed (uint* nhPtr = nextHops)
            {
                NativeMethods.lpm_lookup_batch_ipv4(
                    _handle.DangerousGetHandle(),
                    addrPtr,
                    nhPtr,
                    (UIntPtr)addresses.Length);
            }
        }

        /// <summary>
        /// Performs batch lookups for multiple IPv4 addresses as byte arrays.
        /// </summary>
        /// <param name="addresses">Array of IPv4 addresses (each 4 bytes, contiguous).</param>
        /// <param name="nextHops">Output array for next hop values.</param>
        /// <exception cref="ArgumentException">Thrown if array sizes are invalid.</exception>
        /// <exception cref="ObjectDisposedException">Thrown if the trie has been disposed.</exception>
        public void LookupBatch(byte[] addresses, uint[] nextHops)
        {
            ThrowIfDisposed();
            if (addresses == null) throw new ArgumentNullException(nameof(addresses));
            if (nextHops == null) throw new ArgumentNullException(nameof(nextHops));
            if (addresses.Length % 4 != 0)
            {
                throw new ArgumentException("Addresses array length must be a multiple of 4.", nameof(addresses));
            }

            int count = addresses.Length / 4;
            if (count > nextHops.Length)
            {
                throw new ArgumentException("Output array must be at least as large as address count.");
            }

            // Convert byte array to uint array in network byte order
            var uint32Addrs = new uint[count];
            for (int i = 0; i < count; i++)
            {
                int offset = i * 4;
                uint32Addrs[i] = (uint)(addresses[offset] << 24 | addresses[offset + 1] << 16 | 
                                        addresses[offset + 2] << 8 | addresses[offset + 3]);
            }

            LookupBatch(uint32Addrs, nextHops);
        }

        // ============================================================================
        // Convenience Methods
        // ============================================================================

        /// <summary>
        /// Converts an IPv4 address string to network byte order uint.
        /// </summary>
        /// <param name="address">The IPv4 address string.</param>
        /// <returns>The address in network byte order.</returns>
        public static uint ParseIPv4Address(string address)
        {
            if (!IPAddress.TryParse(address, out var ip) ||
                ip.AddressFamily != System.Net.Sockets.AddressFamily.InterNetwork)
            {
                throw new ArgumentException($"Invalid IPv4 address: {address}", nameof(address));
            }

            var bytes = ip.GetAddressBytes();
            return BinaryPrimitives.ReadUInt32BigEndian(bytes);
        }

        /// <summary>
        /// Converts an IPv4 address from byte array to network byte order uint.
        /// </summary>
        /// <param name="bytes">The IPv4 address bytes.</param>
        /// <returns>The address in network byte order.</returns>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static uint BytesToUInt32(byte[] bytes)
        {
            if (bytes == null || bytes.Length != 4)
            {
                throw new ArgumentException("IPv4 address must be 4 bytes.", nameof(bytes));
            }
            return BinaryPrimitives.ReadUInt32BigEndian(bytes);
        }

        /// <summary>
        /// Converts a network byte order uint to IPv4 address bytes.
        /// </summary>
        /// <param name="address">The address in network byte order.</param>
        /// <returns>The IPv4 address bytes.</returns>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static byte[] UInt32ToBytes(uint address)
        {
            var bytes = new byte[4];
            BinaryPrimitives.WriteUInt32BigEndian(bytes, address);
            return bytes;
        }

        /// <summary>
        /// Converts a network byte order uint to IPv4 address bytes (Span version).
        /// </summary>
        /// <param name="address">The address in network byte order.</param>
        /// <param name="destination">The destination span.</param>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static void UInt32ToBytes(uint address, Span<byte> destination)
        {
            if (destination.Length < 4)
            {
                throw new ArgumentException("Destination must be at least 4 bytes.", nameof(destination));
            }
            BinaryPrimitives.WriteUInt32BigEndian(destination, address);
        }
    }
}
