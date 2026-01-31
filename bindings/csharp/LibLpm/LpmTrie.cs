// LpmTrie.cs - Base wrapper class for LPM trie
// Provides common functionality for IPv4 and IPv6 tries

using System;
using System.Net;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Text.RegularExpressions;

namespace LibLpm
{
    /// <summary>
    /// Base class for LPM (Longest Prefix Match) trie.
    /// Provides common functionality for IPv4 and IPv6 tries.
    /// </summary>
    /// <remarks>
    /// This class is not thread-safe. For concurrent access, use external synchronization.
    /// Implements IDisposable for deterministic resource cleanup.
    /// </remarks>
    public abstract class LpmTrie : IDisposable
    {
        /// <summary>
        /// The safe handle to the native trie.
        /// </summary>
        protected SafeLpmHandle _handle;

        /// <summary>
        /// Whether the trie has been disposed.
        /// </summary>
        private bool _disposed = false;

        /// <summary>
        /// Static constructor to initialize native library resolver.
        /// </summary>
        static LpmTrie()
        {
            NativeLibraryLoader.Initialize();
        }

        /// <summary>
        /// Initializes a new instance of the <see cref="LpmTrie"/> class.
        /// </summary>
        /// <param name="handle">The native handle to wrap.</param>
        protected LpmTrie(SafeLpmHandle handle)
        {
            _handle = handle ?? throw new ArgumentNullException(nameof(handle));
            if (_handle.IsInvalid)
            {
                throw new LpmCreationException("Invalid handle provided.");
            }
        }

        /// <summary>
        /// Gets whether this is an IPv6 trie.
        /// </summary>
        public abstract bool IsIPv6 { get; }

        /// <summary>
        /// Gets the maximum prefix length for this trie type.
        /// </summary>
        public byte MaxPrefixLength => IsIPv6 ? LpmConstants.IPv6MaxDepth : LpmConstants.IPv4MaxDepth;

        /// <summary>
        /// Gets the expected prefix byte length for this trie type.
        /// </summary>
        public int PrefixByteLength => IsIPv6 ? 16 : 4;

        /// <summary>
        /// Gets whether the trie has been disposed.
        /// </summary>
        public bool IsDisposed => _disposed;

        /// <summary>
        /// Gets the native handle. For advanced users only.
        /// </summary>
        public IntPtr NativeHandle
        {
            get
            {
                ThrowIfDisposed();
                return _handle.DangerousGetHandle();
            }
        }

        /// <summary>
        /// Adds a prefix to the trie with the specified next hop.
        /// </summary>
        /// <param name="prefix">The prefix bytes (4 for IPv4, 16 for IPv6).</param>
        /// <param name="prefixLen">The prefix length in bits.</param>
        /// <param name="nextHop">The next hop value to associate with the prefix.</param>
        /// <exception cref="ArgumentNullException">Thrown if prefix is null.</exception>
        /// <exception cref="LpmInvalidPrefixException">Thrown if prefix length is invalid.</exception>
        /// <exception cref="LpmOperationException">Thrown if the add operation fails.</exception>
        /// <exception cref="ObjectDisposedException">Thrown if the trie has been disposed.</exception>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public void Add(byte[] prefix, byte prefixLen, uint nextHop)
        {
            ThrowIfDisposed();
            ValidatePrefix(prefix, prefixLen);

            int result = NativeMethods.lpm_add(_handle.DangerousGetHandle(), prefix, prefixLen, nextHop);
            if (result != 0)
            {
                throw new LpmOperationException("add", "Failed to add prefix to trie.");
            }
        }

        /// <summary>
        /// Adds a prefix to the trie with the specified next hop (Span version for zero-copy).
        /// </summary>
        /// <param name="prefix">The prefix bytes (4 for IPv4, 16 for IPv6).</param>
        /// <param name="prefixLen">The prefix length in bits.</param>
        /// <param name="nextHop">The next hop value to associate with the prefix.</param>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public unsafe void Add(ReadOnlySpan<byte> prefix, byte prefixLen, uint nextHop)
        {
            ThrowIfDisposed();
            if (prefix.Length != PrefixByteLength)
            {
                throw new LpmInvalidPrefixException($"Prefix must be {PrefixByteLength} bytes for {(IsIPv6 ? "IPv6" : "IPv4")}.");
            }
            if (prefixLen > MaxPrefixLength)
            {
                throw new LpmInvalidPrefixException(prefixLen, MaxPrefixLength);
            }

            fixed (byte* ptr = prefix)
            {
                int result = NativeMethods.lpm_add(_handle.DangerousGetHandle(), ptr, prefixLen, nextHop);
                if (result != 0)
                {
                    throw new LpmOperationException("add", "Failed to add prefix to trie.");
                }
            }
        }

        /// <summary>
        /// Adds a prefix to the trie using CIDR notation (e.g., "192.168.0.0/16").
        /// </summary>
        /// <param name="cidr">The prefix in CIDR notation.</param>
        /// <param name="nextHop">The next hop value to associate with the prefix.</param>
        /// <exception cref="ArgumentNullException">Thrown if cidr is null.</exception>
        /// <exception cref="LpmInvalidPrefixException">Thrown if the CIDR notation is invalid.</exception>
        /// <exception cref="LpmOperationException">Thrown if the add operation fails.</exception>
        /// <exception cref="ObjectDisposedException">Thrown if the trie has been disposed.</exception>
        public void Add(string cidr, uint nextHop)
        {
            if (string.IsNullOrEmpty(cidr))
            {
                throw new ArgumentNullException(nameof(cidr));
            }

            var (prefix, prefixLen) = ParseCidr(cidr);
            Add(prefix, prefixLen, nextHop);
        }

        /// <summary>
        /// Tries to add a prefix to the trie. Returns false if the operation fails.
        /// </summary>
        /// <param name="prefix">The prefix bytes.</param>
        /// <param name="prefixLen">The prefix length in bits.</param>
        /// <param name="nextHop">The next hop value.</param>
        /// <returns>True if successful, false otherwise.</returns>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public bool TryAdd(byte[] prefix, byte prefixLen, uint nextHop)
        {
            if (_disposed || prefix == null || prefix.Length != PrefixByteLength || prefixLen > MaxPrefixLength)
            {
                return false;
            }

            return NativeMethods.lpm_add(_handle.DangerousGetHandle(), prefix, prefixLen, nextHop) == 0;
        }

        /// <summary>
        /// Deletes a prefix from the trie.
        /// </summary>
        /// <param name="prefix">The prefix bytes (4 for IPv4, 16 for IPv6).</param>
        /// <param name="prefixLen">The prefix length in bits.</param>
        /// <returns>True if the prefix was deleted, false if it was not found.</returns>
        /// <exception cref="ArgumentNullException">Thrown if prefix is null.</exception>
        /// <exception cref="LpmInvalidPrefixException">Thrown if prefix length is invalid.</exception>
        /// <exception cref="ObjectDisposedException">Thrown if the trie has been disposed.</exception>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public bool Delete(byte[] prefix, byte prefixLen)
        {
            ThrowIfDisposed();
            ValidatePrefix(prefix, prefixLen);

            return NativeMethods.lpm_delete(_handle.DangerousGetHandle(), prefix, prefixLen) == 0;
        }

        /// <summary>
        /// Deletes a prefix from the trie (Span version for zero-copy).
        /// </summary>
        /// <param name="prefix">The prefix bytes.</param>
        /// <param name="prefixLen">The prefix length in bits.</param>
        /// <returns>True if the prefix was deleted, false if it was not found.</returns>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public unsafe bool Delete(ReadOnlySpan<byte> prefix, byte prefixLen)
        {
            ThrowIfDisposed();
            if (prefix.Length != PrefixByteLength)
            {
                throw new LpmInvalidPrefixException($"Prefix must be {PrefixByteLength} bytes for {(IsIPv6 ? "IPv6" : "IPv4")}.");
            }
            if (prefixLen > MaxPrefixLength)
            {
                throw new LpmInvalidPrefixException(prefixLen, MaxPrefixLength);
            }

            fixed (byte* ptr = prefix)
            {
                return NativeMethods.lpm_delete(_handle.DangerousGetHandle(), ptr, prefixLen) == 0;
            }
        }

        /// <summary>
        /// Deletes a prefix from the trie using CIDR notation.
        /// </summary>
        /// <param name="cidr">The prefix in CIDR notation.</param>
        /// <returns>True if the prefix was deleted, false if it was not found.</returns>
        public bool Delete(string cidr)
        {
            if (string.IsNullOrEmpty(cidr))
            {
                throw new ArgumentNullException(nameof(cidr));
            }

            var (prefix, prefixLen) = ParseCidr(cidr);
            return Delete(prefix, prefixLen);
        }

        /// <summary>
        /// Performs a lookup in the trie for the given address.
        /// </summary>
        /// <param name="address">The address bytes (4 for IPv4, 16 for IPv6).</param>
        /// <returns>The next hop value if found, or null if no match.</returns>
        /// <exception cref="ArgumentNullException">Thrown if address is null.</exception>
        /// <exception cref="ArgumentException">Thrown if address length is invalid.</exception>
        /// <exception cref="ObjectDisposedException">Thrown if the trie has been disposed.</exception>
        public abstract uint? Lookup(byte[] address);

        /// <summary>
        /// Performs a lookup in the trie for the given address (Span version).
        /// </summary>
        /// <param name="address">The address bytes.</param>
        /// <returns>The next hop value if found, or null if no match.</returns>
        public abstract uint? Lookup(ReadOnlySpan<byte> address);

        /// <summary>
        /// Performs a lookup in the trie for the given IP address.
        /// </summary>
        /// <param name="address">The IP address.</param>
        /// <returns>The next hop value if found, or null if no match.</returns>
        public uint? Lookup(IPAddress address)
        {
            if (address == null)
            {
                throw new ArgumentNullException(nameof(address));
            }

            var bytes = address.GetAddressBytes();
            if (bytes.Length != PrefixByteLength)
            {
                throw new ArgumentException(
                    $"Address must be {(IsIPv6 ? "IPv6" : "IPv4")} for this trie.",
                    nameof(address));
            }

            return Lookup(bytes);
        }

        /// <summary>
        /// Performs a lookup in the trie for the given address string.
        /// </summary>
        /// <param name="address">The IP address as a string.</param>
        /// <returns>The next hop value if found, or null if no match.</returns>
        public uint? Lookup(string address)
        {
            if (string.IsNullOrEmpty(address))
            {
                throw new ArgumentNullException(nameof(address));
            }

            if (!IPAddress.TryParse(address, out var ipAddress))
            {
                throw new ArgumentException($"Invalid IP address: {address}", nameof(address));
            }

            return Lookup(ipAddress);
        }

        /// <summary>
        /// Prints trie statistics to stdout.
        /// </summary>
        public void PrintStats()
        {
            ThrowIfDisposed();
            NativeMethods.lpm_print_stats(_handle.DangerousGetHandle());
        }

        /// <summary>
        /// Gets the library version string.
        /// </summary>
        /// <returns>The version string.</returns>
        public static string? GetVersion()
        {
            NativeLibraryLoader.Initialize();
            return NativeLibraryLoader.GetVersion();
        }

        /// <summary>
        /// Validates the prefix and prefix length.
        /// </summary>
        protected void ValidatePrefix(byte[]? prefix, byte prefixLen)
        {
            if (prefix == null)
            {
                throw new ArgumentNullException(nameof(prefix));
            }
            if (prefix.Length != PrefixByteLength)
            {
                throw new LpmInvalidPrefixException(
                    $"Prefix must be {PrefixByteLength} bytes for {(IsIPv6 ? "IPv6" : "IPv4")}.");
            }
            if (prefixLen > MaxPrefixLength)
            {
                throw new LpmInvalidPrefixException(prefixLen, MaxPrefixLength);
            }
        }

        /// <summary>
        /// Validates the address length.
        /// </summary>
        protected void ValidateAddress(byte[]? address)
        {
            if (address == null)
            {
                throw new ArgumentNullException(nameof(address));
            }
            if (address.Length != PrefixByteLength)
            {
                throw new ArgumentException(
                    $"Address must be {PrefixByteLength} bytes for {(IsIPv6 ? "IPv6" : "IPv4")}.",
                    nameof(address));
            }
        }

        /// <summary>
        /// Parses a CIDR notation string into prefix bytes and length.
        /// </summary>
        protected (byte[] prefix, byte prefixLen) ParseCidr(string cidr)
        {
            var parts = cidr.Split('/');
            if (parts.Length != 2)
            {
                throw new LpmInvalidPrefixException(cidr, "Expected format: address/prefix_length");
            }

            if (!IPAddress.TryParse(parts[0], out var address))
            {
                throw new LpmInvalidPrefixException(cidr, "Invalid IP address");
            }

            if (!byte.TryParse(parts[1], out var prefixLen))
            {
                throw new LpmInvalidPrefixException(cidr, "Invalid prefix length");
            }

            var bytes = address.GetAddressBytes();
            
            // Validate address family matches trie type
            bool isIPv6Address = address.AddressFamily == System.Net.Sockets.AddressFamily.InterNetworkV6;
            if (isIPv6Address != IsIPv6)
            {
                throw new LpmInvalidPrefixException(cidr, 
                    $"Address family mismatch: expected {(IsIPv6 ? "IPv6" : "IPv4")}");
            }

            if (prefixLen > MaxPrefixLength)
            {
                throw new LpmInvalidPrefixException(prefixLen, MaxPrefixLength);
            }

            return (bytes, prefixLen);
        }

        /// <summary>
        /// Throws ObjectDisposedException if the trie has been disposed.
        /// </summary>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        protected void ThrowIfDisposed()
        {
            if (_disposed)
            {
                throw new ObjectDisposedException(GetType().Name);
            }
        }

        /// <summary>
        /// Disposes the trie and releases all native resources.
        /// </summary>
        public void Dispose()
        {
            Dispose(true);
            GC.SuppressFinalize(this);
        }

        /// <summary>
        /// Disposes the trie.
        /// </summary>
        /// <param name="disposing">True if called from Dispose(), false if from finalizer.</param>
        protected virtual void Dispose(bool disposing)
        {
            if (_disposed)
            {
                return;
            }

            if (disposing)
            {
                // Dispose managed resources
                _handle?.Dispose();
            }

            _disposed = true;
        }

        /// <summary>
        /// Finalizer to ensure resources are released.
        /// </summary>
        ~LpmTrie()
        {
            Dispose(false);
        }
    }
}
