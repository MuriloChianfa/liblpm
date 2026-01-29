// NativeMethods.cs - P/Invoke declarations for liblpm C library
// This file contains all native function declarations and constants

using System;
using System.Runtime.InteropServices;

namespace LibLpm
{
    /// <summary>
    /// Constants from lpm.h
    /// </summary>
    public static class LpmConstants
    {
        /// <summary>
        /// Invalid next hop value returned when no match is found.
        /// </summary>
        public const uint InvalidNextHop = 0xFFFFFFFF;

        /// <summary>
        /// Invalid index value.
        /// </summary>
        public const uint InvalidIndex = 0;

        /// <summary>
        /// Maximum prefix depth for IPv4 addresses.
        /// </summary>
        public const byte IPv4MaxDepth = 32;

        /// <summary>
        /// Maximum prefix depth for IPv6 addresses.
        /// </summary>
        public const byte IPv6MaxDepth = 128;

        /// <summary>
        /// Cache line size for alignment.
        /// </summary>
        public const int CacheLineSize = 64;

        /// <summary>
        /// 8-bit stride size (256 entries per node).
        /// </summary>
        public const int StrideBits8 = 8;
        public const int StrideSize8 = 256;

        /// <summary>
        /// 16-bit stride size (65536 entries per node).
        /// </summary>
        public const int StrideBits16 = 16;
        public const int StrideSize16 = 65536;

        /// <summary>
        /// IPv4 DIR-24-8 configuration.
        /// </summary>
        public const int IPv4Dir24Bits = 24;
        public const int IPv4Dir24Size = 16777216; // 1 << 24
    }

    /// <summary>
    /// Native P/Invoke methods for liblpm.
    /// </summary>
    internal static class NativeMethods
    {
        /// <summary>
        /// Library name for P/Invoke. Platform-specific resolution is handled by NativeLibraryLoader.
        /// </summary>
        public const string LibraryName = "lpm";

        // ============================================================================
        // GENERIC API (dispatches to compile-time selected algorithm)
        // ============================================================================

        /// <summary>
        /// Creates a new IPv4 LPM trie using the default algorithm (DIR-24-8).
        /// </summary>
        /// <returns>Pointer to the created trie, or IntPtr.Zero on failure.</returns>
        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr lpm_create_ipv4();

        /// <summary>
        /// Creates a new IPv6 LPM trie using the default algorithm (Wide16).
        /// </summary>
        /// <returns>Pointer to the created trie, or IntPtr.Zero on failure.</returns>
        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr lpm_create_ipv6();

        /// <summary>
        /// Destroys an LPM trie and frees all associated memory.
        /// </summary>
        /// <param name="trie">Pointer to the trie to destroy. Safe to call with IntPtr.Zero.</param>
        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void lpm_destroy(IntPtr trie);

        /// <summary>
        /// Adds a prefix to the LPM trie.
        /// </summary>
        /// <param name="trie">Pointer to the trie.</param>
        /// <param name="prefix">Prefix bytes (4 for IPv4, 16 for IPv6).</param>
        /// <param name="prefix_len">Prefix length in bits.</param>
        /// <param name="next_hop">Next hop value to associate with the prefix.</param>
        /// <returns>0 on success, -1 on failure.</returns>
        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int lpm_add(IntPtr trie, byte[] prefix, byte prefix_len, uint next_hop);

        /// <summary>
        /// Adds a prefix to the LPM trie (unsafe version for Span support).
        /// </summary>
        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        public static extern unsafe int lpm_add(IntPtr trie, byte* prefix, byte prefix_len, uint next_hop);

        /// <summary>
        /// Deletes a prefix from the LPM trie.
        /// </summary>
        /// <param name="trie">Pointer to the trie.</param>
        /// <param name="prefix">Prefix bytes (4 for IPv4, 16 for IPv6).</param>
        /// <param name="prefix_len">Prefix length in bits.</param>
        /// <returns>0 on success, -1 on failure (including prefix not found).</returns>
        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int lpm_delete(IntPtr trie, byte[] prefix, byte prefix_len);

        /// <summary>
        /// Deletes a prefix from the LPM trie (unsafe version for Span support).
        /// </summary>
        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        public static extern unsafe int lpm_delete(IntPtr trie, byte* prefix, byte prefix_len);

        /// <summary>
        /// Performs an IPv4 lookup in the trie.
        /// </summary>
        /// <param name="trie">Pointer to the trie.</param>
        /// <param name="addr">IPv4 address as a 32-bit integer (network byte order).</param>
        /// <returns>Next hop value on match, LPM_INVALID_NEXT_HOP if no match.</returns>
        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        public static extern uint lpm_lookup_ipv4(IntPtr trie, uint addr);

        /// <summary>
        /// Performs an IPv6 lookup in the trie.
        /// </summary>
        /// <param name="trie">Pointer to the trie.</param>
        /// <param name="addr">IPv6 address as 16 bytes.</param>
        /// <returns>Next hop value on match, LPM_INVALID_NEXT_HOP if no match.</returns>
        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        public static extern uint lpm_lookup_ipv6(IntPtr trie, byte[] addr);

        /// <summary>
        /// Performs an IPv6 lookup in the trie (unsafe version for Span support).
        /// </summary>
        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        public static extern unsafe uint lpm_lookup_ipv6(IntPtr trie, byte* addr);

        /// <summary>
        /// Performs a batch IPv4 lookup.
        /// </summary>
        /// <param name="trie">Pointer to the trie.</param>
        /// <param name="addrs">Array of IPv4 addresses (network byte order).</param>
        /// <param name="next_hops">Output array for next hop values.</param>
        /// <param name="count">Number of addresses to look up.</param>
        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void lpm_lookup_batch_ipv4(IntPtr trie, uint[] addrs, uint[] next_hops, UIntPtr count);

        /// <summary>
        /// Performs a batch IPv4 lookup (unsafe version).
        /// </summary>
        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        public static extern unsafe void lpm_lookup_batch_ipv4(IntPtr trie, uint* addrs, uint* next_hops, UIntPtr count);

        /// <summary>
        /// Performs a batch IPv6 lookup.
        /// </summary>
        /// <param name="trie">Pointer to the trie.</param>
        /// <param name="addrs">Array of IPv6 addresses (each 16 bytes, contiguous).</param>
        /// <param name="next_hops">Output array for next hop values.</param>
        /// <param name="count">Number of addresses to look up.</param>
        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        public static extern unsafe void lpm_lookup_batch_ipv6(IntPtr trie, byte* addrs, uint* next_hops, UIntPtr count);

        // ============================================================================
        // ALGORITHM-SPECIFIC API: IPv4 DIR-24-8
        // ============================================================================

        /// <summary>
        /// Creates a new IPv4 LPM trie using DIR-24-8 algorithm explicitly.
        /// </summary>
        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr lpm_create_ipv4_dir24();

        /// <summary>
        /// Adds a prefix using DIR-24-8 algorithm.
        /// </summary>
        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int lpm_add_ipv4_dir24(IntPtr trie, byte[] prefix, byte prefix_len, uint next_hop);

        /// <summary>
        /// Adds a prefix using DIR-24-8 algorithm (unsafe version).
        /// </summary>
        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        public static extern unsafe int lpm_add_ipv4_dir24(IntPtr trie, byte* prefix, byte prefix_len, uint next_hop);

        /// <summary>
        /// Deletes a prefix using DIR-24-8 algorithm.
        /// </summary>
        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int lpm_delete_ipv4_dir24(IntPtr trie, byte[] prefix, byte prefix_len);

        /// <summary>
        /// Deletes a prefix using DIR-24-8 algorithm (unsafe version).
        /// </summary>
        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        public static extern unsafe int lpm_delete_ipv4_dir24(IntPtr trie, byte* prefix, byte prefix_len);

        /// <summary>
        /// Performs an IPv4 lookup using DIR-24-8 algorithm.
        /// </summary>
        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        public static extern uint lpm_lookup_ipv4_dir24(IntPtr trie, uint addr);

        /// <summary>
        /// Performs an IPv4 lookup using DIR-24-8 algorithm (byte array version).
        /// </summary>
        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        public static extern uint lpm_lookup_ipv4_dir24_bytes(IntPtr trie, byte[] addr);

        /// <summary>
        /// Performs an IPv4 lookup using DIR-24-8 algorithm (byte array version, unsafe).
        /// </summary>
        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        public static extern unsafe uint lpm_lookup_ipv4_dir24_bytes(IntPtr trie, byte* addr);

        /// <summary>
        /// Performs a batch IPv4 lookup using DIR-24-8 algorithm.
        /// </summary>
        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void lpm_lookup_batch_ipv4_dir24(IntPtr trie, uint[] addrs, uint[] next_hops, UIntPtr count);

        /// <summary>
        /// Performs a batch IPv4 lookup using DIR-24-8 algorithm (unsafe version).
        /// </summary>
        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        public static extern unsafe void lpm_lookup_batch_ipv4_dir24(IntPtr trie, uint* addrs, uint* next_hops, UIntPtr count);

        /// <summary>
        /// Performs a batch IPv4 lookup using DIR-24-8 algorithm (byte array version, unsafe).
        /// </summary>
        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        public static extern unsafe void lpm_lookup_batch_ipv4_dir24_bytes(IntPtr trie, byte* addrs, uint* next_hops, UIntPtr count);

        // ============================================================================
        // ALGORITHM-SPECIFIC API: IPv4 8-bit Stride
        // ============================================================================

        /// <summary>
        /// Creates a new IPv4 LPM trie using 8-bit stride algorithm.
        /// </summary>
        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr lpm_create_ipv4_8stride();

        /// <summary>
        /// Adds a prefix using 8-bit stride algorithm.
        /// </summary>
        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int lpm_add_ipv4_8stride(IntPtr trie, byte[] prefix, byte prefix_len, uint next_hop);

        /// <summary>
        /// Adds a prefix using 8-bit stride algorithm (unsafe version).
        /// </summary>
        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        public static extern unsafe int lpm_add_ipv4_8stride(IntPtr trie, byte* prefix, byte prefix_len, uint next_hop);

        /// <summary>
        /// Deletes a prefix using 8-bit stride algorithm.
        /// </summary>
        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int lpm_delete_ipv4_8stride(IntPtr trie, byte[] prefix, byte prefix_len);

        /// <summary>
        /// Deletes a prefix using 8-bit stride algorithm (unsafe version).
        /// </summary>
        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        public static extern unsafe int lpm_delete_ipv4_8stride(IntPtr trie, byte* prefix, byte prefix_len);

        /// <summary>
        /// Performs an IPv4 lookup using 8-bit stride algorithm.
        /// </summary>
        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        public static extern uint lpm_lookup_ipv4_8stride(IntPtr trie, uint addr);

        /// <summary>
        /// Performs an IPv4 lookup using 8-bit stride algorithm (byte array version).
        /// </summary>
        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        public static extern uint lpm_lookup_ipv4_8stride_bytes(IntPtr trie, byte[] addr);

        /// <summary>
        /// Performs an IPv4 lookup using 8-bit stride algorithm (byte array version, unsafe).
        /// </summary>
        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        public static extern unsafe uint lpm_lookup_ipv4_8stride_bytes(IntPtr trie, byte* addr);

        /// <summary>
        /// Performs a batch IPv4 lookup using 8-bit stride algorithm.
        /// </summary>
        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void lpm_lookup_batch_ipv4_8stride(IntPtr trie, uint[] addrs, uint[] next_hops, UIntPtr count);

        /// <summary>
        /// Performs a batch IPv4 lookup using 8-bit stride algorithm (unsafe version).
        /// </summary>
        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        public static extern unsafe void lpm_lookup_batch_ipv4_8stride(IntPtr trie, uint* addrs, uint* next_hops, UIntPtr count);

        // ============================================================================
        // ALGORITHM-SPECIFIC API: IPv6 Wide 16-bit Stride
        // ============================================================================

        /// <summary>
        /// Creates a new IPv6 LPM trie using wide 16-bit stride algorithm.
        /// </summary>
        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr lpm_create_ipv6_wide16();

        /// <summary>
        /// Adds a prefix using wide 16-bit stride algorithm.
        /// </summary>
        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int lpm_add_ipv6_wide16(IntPtr trie, byte[] prefix, byte prefix_len, uint next_hop);

        /// <summary>
        /// Adds a prefix using wide 16-bit stride algorithm (unsafe version).
        /// </summary>
        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        public static extern unsafe int lpm_add_ipv6_wide16(IntPtr trie, byte* prefix, byte prefix_len, uint next_hop);

        /// <summary>
        /// Deletes a prefix using wide 16-bit stride algorithm.
        /// </summary>
        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int lpm_delete_ipv6_wide16(IntPtr trie, byte[] prefix, byte prefix_len);

        /// <summary>
        /// Deletes a prefix using wide 16-bit stride algorithm (unsafe version).
        /// </summary>
        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        public static extern unsafe int lpm_delete_ipv6_wide16(IntPtr trie, byte* prefix, byte prefix_len);

        /// <summary>
        /// Performs an IPv6 lookup using wide 16-bit stride algorithm.
        /// </summary>
        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        public static extern uint lpm_lookup_ipv6_wide16(IntPtr trie, byte[] addr);

        /// <summary>
        /// Performs an IPv6 lookup using wide 16-bit stride algorithm (unsafe version).
        /// </summary>
        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        public static extern unsafe uint lpm_lookup_ipv6_wide16(IntPtr trie, byte* addr);

        /// <summary>
        /// Performs a batch IPv6 lookup using wide 16-bit stride algorithm.
        /// </summary>
        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        public static extern unsafe void lpm_lookup_batch_ipv6_wide16(IntPtr trie, byte* addrs, uint* next_hops, UIntPtr count);

        // ============================================================================
        // ALGORITHM-SPECIFIC API: IPv6 8-bit Stride
        // ============================================================================

        /// <summary>
        /// Creates a new IPv6 LPM trie using 8-bit stride algorithm.
        /// </summary>
        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr lpm_create_ipv6_8stride();

        /// <summary>
        /// Adds a prefix using 8-bit stride algorithm.
        /// </summary>
        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int lpm_add_ipv6_8stride(IntPtr trie, byte[] prefix, byte prefix_len, uint next_hop);

        /// <summary>
        /// Adds a prefix using 8-bit stride algorithm (unsafe version).
        /// </summary>
        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        public static extern unsafe int lpm_add_ipv6_8stride(IntPtr trie, byte* prefix, byte prefix_len, uint next_hop);

        /// <summary>
        /// Deletes a prefix using 8-bit stride algorithm.
        /// </summary>
        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int lpm_delete_ipv6_8stride(IntPtr trie, byte[] prefix, byte prefix_len);

        /// <summary>
        /// Deletes a prefix using 8-bit stride algorithm (unsafe version).
        /// </summary>
        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        public static extern unsafe int lpm_delete_ipv6_8stride(IntPtr trie, byte* prefix, byte prefix_len);

        /// <summary>
        /// Performs an IPv6 lookup using 8-bit stride algorithm.
        /// </summary>
        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        public static extern uint lpm_lookup_ipv6_8stride(IntPtr trie, byte[] addr);

        /// <summary>
        /// Performs an IPv6 lookup using 8-bit stride algorithm (unsafe version).
        /// </summary>
        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        public static extern unsafe uint lpm_lookup_ipv6_8stride(IntPtr trie, byte* addr);

        /// <summary>
        /// Performs a batch IPv6 lookup using 8-bit stride algorithm.
        /// </summary>
        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        public static extern unsafe void lpm_lookup_batch_ipv6_8stride(IntPtr trie, byte* addrs, uint* next_hops, UIntPtr count);

        // ============================================================================
        // LEGACY API
        // ============================================================================

        /// <summary>
        /// Creates a new LPM trie with specified max depth (legacy API).
        /// </summary>
        /// <param name="max_depth">Maximum prefix depth (32 for IPv4, 128 for IPv6).</param>
        /// <returns>Pointer to the created trie, or IntPtr.Zero on failure.</returns>
        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr lpm_create(byte max_depth);

        /// <summary>
        /// Performs a lookup in the trie (legacy API, auto-detects based on max_depth).
        /// </summary>
        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        public static extern uint lpm_lookup(IntPtr trie, byte[] addr);

        /// <summary>
        /// Performs a lookup in the trie (legacy API, unsafe version).
        /// </summary>
        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        public static extern unsafe uint lpm_lookup(IntPtr trie, byte* addr);

        // ============================================================================
        // UTILITY FUNCTIONS
        // ============================================================================

        /// <summary>
        /// Gets the library version string.
        /// </summary>
        /// <returns>Pointer to a null-terminated version string.</returns>
        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr lpm_get_version();

        /// <summary>
        /// Prints trie statistics to stdout.
        /// </summary>
        /// <param name="trie">Pointer to the trie.</param>
        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void lpm_print_stats(IntPtr trie);
    }
}
