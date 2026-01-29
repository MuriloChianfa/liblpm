// LpmTrieIPv6.cs - IPv6-specific LPM trie wrapper
// Provides optimized IPv6 operations and convenience methods

using System;
using System.Net;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace LibLpm
{
    /// <summary>
    /// IPv6-specific LPM (Longest Prefix Match) trie.
    /// Provides optimized operations for IPv6 address lookups.
    /// </summary>
    /// <remarks>
    /// This class is not thread-safe. For concurrent access, use external synchronization.
    /// Implements IDisposable for deterministic resource cleanup.
    /// </remarks>
    public sealed class LpmTrieIPv6 : LpmTrie
    {
        private readonly LpmAlgorithm _algorithm;

        /// <summary>
        /// Initializes a new instance of the <see cref="LpmTrieIPv6"/> class.
        /// </summary>
        /// <param name="handle">The native handle to wrap.</param>
        /// <param name="algorithm">The algorithm used for this trie.</param>
        private LpmTrieIPv6(SafeLpmHandle handle, LpmAlgorithm algorithm) : base(handle)
        {
            _algorithm = algorithm;
        }

        /// <inheritdoc/>
        public override bool IsIPv6 => true;

        /// <summary>
        /// Gets the algorithm used for this trie.
        /// </summary>
        public LpmAlgorithm Algorithm => _algorithm;

        // ============================================================================
        // Factory Methods
        // ============================================================================

        /// <summary>
        /// Creates a new IPv6 LPM trie using the default algorithm (Wide16).
        /// </summary>
        /// <returns>A new IPv6 LPM trie.</returns>
        /// <exception cref="LpmCreationException">Thrown if the trie creation fails.</exception>
        public static LpmTrieIPv6 CreateDefault()
        {
            NativeLibraryLoader.Initialize();
            var ptr = NativeMethods.lpm_create_ipv6();
            if (ptr == IntPtr.Zero)
            {
                throw new LpmCreationException(isIPv6: true, LpmAlgorithm.Default);
            }
            return new LpmTrieIPv6(new SafeLpmHandle(ptr), LpmAlgorithm.Default);
        }

        /// <summary>
        /// Creates a new IPv6 LPM trie using Wide 16-bit stride algorithm explicitly.
        /// </summary>
        /// <returns>A new IPv6 LPM trie with Wide16 algorithm.</returns>
        /// <exception cref="LpmCreationException">Thrown if the trie creation fails.</exception>
        public static LpmTrieIPv6 CreateWide16()
        {
            NativeLibraryLoader.Initialize();
            var ptr = NativeMethods.lpm_create_ipv6_wide16();
            if (ptr == IntPtr.Zero)
            {
                throw new LpmCreationException(isIPv6: true, LpmAlgorithm.Wide16);
            }
            return new LpmTrieIPv6(new SafeLpmHandle(ptr), LpmAlgorithm.Wide16);
        }

        /// <summary>
        /// Creates a new IPv6 LPM trie using 8-bit stride algorithm.
        /// </summary>
        /// <returns>A new IPv6 LPM trie with 8-bit stride algorithm.</returns>
        /// <exception cref="LpmCreationException">Thrown if the trie creation fails.</exception>
        public static LpmTrieIPv6 CreateStride8()
        {
            NativeLibraryLoader.Initialize();
            var ptr = NativeMethods.lpm_create_ipv6_8stride();
            if (ptr == IntPtr.Zero)
            {
                throw new LpmCreationException(isIPv6: true, LpmAlgorithm.Stride8);
            }
            return new LpmTrieIPv6(new SafeLpmHandle(ptr), LpmAlgorithm.Stride8);
        }

        /// <summary>
        /// Creates a new IPv6 LPM trie with the specified algorithm.
        /// </summary>
        /// <param name="algorithm">The algorithm to use.</param>
        /// <returns>A new IPv6 LPM trie.</returns>
        /// <exception cref="LpmCreationException">Thrown if the trie creation fails.</exception>
        /// <exception cref="ArgumentException">Thrown if an IPv4-only algorithm is specified.</exception>
        public static LpmTrieIPv6 Create(LpmAlgorithm algorithm)
        {
            return algorithm switch
            {
                LpmAlgorithm.Default => CreateDefault(),
                LpmAlgorithm.Wide16 => CreateWide16(),
                LpmAlgorithm.Stride8 => CreateStride8(),
                LpmAlgorithm.Dir24 => throw new ArgumentException(
                    "Dir24 algorithm is only valid for IPv4 tries.", nameof(algorithm)),
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

            uint result = NativeMethods.lpm_lookup_ipv6(_handle.DangerousGetHandle(), address);
            return result == LpmConstants.InvalidNextHop ? null : result;
        }

        /// <inheritdoc/>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public override unsafe uint? Lookup(ReadOnlySpan<byte> address)
        {
            ThrowIfDisposed();
            if (address.Length != 16)
            {
                throw new ArgumentException("IPv6 address must be 16 bytes.", nameof(address));
            }

            fixed (byte* ptr = address)
            {
                uint result = NativeMethods.lpm_lookup_ipv6(_handle.DangerousGetHandle(), ptr);
                return result == LpmConstants.InvalidNextHop ? null : result;
            }
        }

        /// <summary>
        /// Performs a raw lookup returning the raw next hop value (including InvalidNextHop).
        /// This is the fastest lookup method with no null checking overhead.
        /// </summary>
        /// <param name="address">The IPv6 address (16 bytes).</param>
        /// <returns>The raw next hop value, or LpmConstants.InvalidNextHop if no match.</returns>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public uint LookupRaw(byte[] address)
        {
            ThrowIfDisposed();
            ValidateAddress(address);
            return NativeMethods.lpm_lookup_ipv6(_handle.DangerousGetHandle(), address);
        }

        /// <summary>
        /// Performs a raw lookup returning the raw next hop value (Span version).
        /// </summary>
        /// <param name="address">The IPv6 address (16 bytes).</param>
        /// <returns>The raw next hop value, or LpmConstants.InvalidNextHop if no match.</returns>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public unsafe uint LookupRaw(ReadOnlySpan<byte> address)
        {
            ThrowIfDisposed();
            if (address.Length != 16)
            {
                throw new ArgumentException("IPv6 address must be 16 bytes.", nameof(address));
            }

            fixed (byte* ptr = address)
            {
                return NativeMethods.lpm_lookup_ipv6(_handle.DangerousGetHandle(), ptr);
            }
        }

        // ============================================================================
        // Batch Operations
        // ============================================================================

        /// <summary>
        /// Performs batch lookups for multiple IPv6 addresses.
        /// </summary>
        /// <param name="addresses">Array of IPv6 addresses (each 16 bytes, contiguous).</param>
        /// <param name="nextHops">Output array for next hop values.</param>
        /// <exception cref="ArgumentNullException">Thrown if addresses or nextHops is null.</exception>
        /// <exception cref="ArgumentException">Thrown if array sizes are invalid.</exception>
        /// <exception cref="ObjectDisposedException">Thrown if the trie has been disposed.</exception>
        public unsafe void LookupBatch(byte[] addresses, uint[] nextHops)
        {
            ThrowIfDisposed();
            if (addresses == null) throw new ArgumentNullException(nameof(addresses));
            if (nextHops == null) throw new ArgumentNullException(nameof(nextHops));
            if (addresses.Length % 16 != 0)
            {
                throw new ArgumentException("Addresses array length must be a multiple of 16.", nameof(addresses));
            }

            int count = addresses.Length / 16;
            if (count > nextHops.Length)
            {
                throw new ArgumentException("Output array must be at least as large as address count.");
            }

            fixed (byte* addrPtr = addresses)
            fixed (uint* nhPtr = nextHops)
            {
                NativeMethods.lpm_lookup_batch_ipv6(
                    _handle.DangerousGetHandle(),
                    addrPtr,
                    nhPtr,
                    (UIntPtr)count);
            }
        }

        /// <summary>
        /// Performs batch lookups for multiple IPv6 addresses (Span version).
        /// </summary>
        /// <param name="addresses">Span of IPv6 addresses (each 16 bytes, contiguous).</param>
        /// <param name="nextHops">Output span for next hop values.</param>
        /// <exception cref="ArgumentException">Thrown if sizes are invalid.</exception>
        /// <exception cref="ObjectDisposedException">Thrown if the trie has been disposed.</exception>
        public unsafe void LookupBatch(ReadOnlySpan<byte> addresses, Span<uint> nextHops)
        {
            ThrowIfDisposed();
            if (addresses.Length % 16 != 0)
            {
                throw new ArgumentException("Addresses span length must be a multiple of 16.", nameof(addresses));
            }

            int count = addresses.Length / 16;
            if (count > nextHops.Length)
            {
                throw new ArgumentException("Output span must be at least as large as address count.");
            }

            fixed (byte* addrPtr = addresses)
            fixed (uint* nhPtr = nextHops)
            {
                NativeMethods.lpm_lookup_batch_ipv6(
                    _handle.DangerousGetHandle(),
                    addrPtr,
                    nhPtr,
                    (UIntPtr)count);
            }
        }

        // ============================================================================
        // Convenience Methods
        // ============================================================================

        /// <summary>
        /// Parses an IPv6 address string to bytes.
        /// </summary>
        /// <param name="address">The IPv6 address string.</param>
        /// <returns>The address as 16 bytes.</returns>
        public static byte[] ParseIPv6Address(string address)
        {
            if (!IPAddress.TryParse(address, out var ip) ||
                ip.AddressFamily != System.Net.Sockets.AddressFamily.InterNetworkV6)
            {
                throw new ArgumentException($"Invalid IPv6 address: {address}", nameof(address));
            }

            return ip.GetAddressBytes();
        }

        /// <summary>
        /// Formats an IPv6 address byte array as a string.
        /// </summary>
        /// <param name="bytes">The IPv6 address bytes (16 bytes).</param>
        /// <returns>The formatted IPv6 address string.</returns>
        public static string FormatIPv6Address(byte[] bytes)
        {
            if (bytes == null || bytes.Length != 16)
            {
                throw new ArgumentException("IPv6 address must be 16 bytes.", nameof(bytes));
            }

            return new IPAddress(bytes).ToString();
        }

        /// <summary>
        /// Formats an IPv6 address from a span as a string.
        /// </summary>
        /// <param name="bytes">The IPv6 address bytes (16 bytes).</param>
        /// <returns>The formatted IPv6 address string.</returns>
        public static string FormatIPv6Address(ReadOnlySpan<byte> bytes)
        {
            if (bytes.Length != 16)
            {
                throw new ArgumentException("IPv6 address must be 16 bytes.", nameof(bytes));
            }

            return new IPAddress(bytes.ToArray()).ToString();
        }

        /// <summary>
        /// Creates a zero-filled IPv6 address buffer.
        /// </summary>
        /// <returns>A new 16-byte array filled with zeros.</returns>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static byte[] CreateAddressBuffer()
        {
            return new byte[16];
        }
    }
}
