/*
 * Copyright (c) 2024 Murilo Chianfa
 * 
 * Licensed under the MIT License.
 */
package com.github.murilochianfa.liblpm;

import java.net.InetAddress;
import java.util.Objects;

/**
 * Abstract base class for LPM (Longest Prefix Match) routing tables.
 * <p>
 * This class provides the common interface and implementation for both
 * IPv4 ({@link LpmTableIPv4}) and IPv6 ({@link LpmTableIPv6}) routing tables.
 * 
 * <h2>Resource Management</h2>
 * LpmTable implements {@link AutoCloseable} and should be used with
 * try-with-resources or explicitly closed when no longer needed:
 * <pre>{@code
 * try (LpmTableIPv4 table = LpmTableIPv4.create()) {
 *     table.insert("192.168.0.0/16", 100);
 *     int nh = table.lookup("192.168.1.1");
 * } // Automatically closed
 * }</pre>
 * 
 * <h2>Thread Safety</h2>
 * <ul>
 *   <li><strong>Read operations</strong> ({@code lookup}, {@code lookupBatch}):
 *       Thread-safe. Multiple threads can perform lookups concurrently.</li>
 *   <li><strong>Write operations</strong> ({@code insert}, {@code delete}):
 *       NOT thread-safe. Concurrent writes require external synchronization.</li>
 *   <li><strong>Mixed operations</strong>: NOT thread-safe. Do not perform
 *       lookups while modifying the table without synchronization.</li>
 * </ul>
 * 
 * <h2>Performance</h2>
 * For best performance:
 * <ul>
 *   <li>Use byte array APIs ({@code lookup(byte[])}) instead of string/InetAddress</li>
 *   <li>Use batch operations ({@code lookupBatch}) for multiple lookups</li>
 *   <li>Pre-allocate arrays for batch operations</li>
 * </ul>
 * 
 * @author Murilo Chianfa
 * @since 1.0.0
 * @see LpmTableIPv4
 * @see LpmTableIPv6
 */
public abstract class LpmTable implements AutoCloseable {
    
    // Load native library on first access
    static {
        NativeLibraryLoader.ensureLoaded();
    }
    
    /** Native trie handle (pointer as long) */
    protected volatile long nativeHandle;
    
    /** The algorithm used by this table */
    protected final Algorithm algorithm;
    
    /** Whether this table is for IPv6 */
    protected final boolean isIPv6;
    
    /** Lock object for close operations */
    private final Object closeLock = new Object();
    
    /**
     * Protected constructor for subclasses.
     * 
     * @param nativeHandle the native trie handle
     * @param algorithm the algorithm used
     * @param isIPv6 whether this is an IPv6 table
     */
    protected LpmTable(long nativeHandle, Algorithm algorithm, boolean isIPv6) {
        if (nativeHandle == 0) {
            throw new NativeLibraryException("Failed to create native trie");
        }
        this.nativeHandle = nativeHandle;
        this.algorithm = algorithm;
        this.isIPv6 = isIPv6;
    }
    
    /**
     * Returns the algorithm used by this table.
     * 
     * @return the algorithm
     */
    public Algorithm getAlgorithm() {
        return algorithm;
    }
    
    /**
     * Returns whether this is an IPv6 table.
     * 
     * @return {@code true} for IPv6, {@code false} for IPv4
     */
    public boolean isIPv6() {
        return isIPv6;
    }
    
    /**
     * Returns whether this table has been closed.
     * 
     * @return {@code true} if closed
     */
    public boolean isClosed() {
        return nativeHandle == 0;
    }
    
    /**
     * Ensures the table is not closed.
     * 
     * @throws IllegalStateException if the table is closed
     */
    protected void ensureOpen() {
        if (nativeHandle == 0) {
            throw new IllegalStateException("LpmTable has been closed");
        }
    }
    
    /**
     * Returns the expected byte array length for addresses.
     * 
     * @return 4 for IPv4, 16 for IPv6
     */
    protected int getAddressLength() {
        return isIPv6 ? 16 : 4;
    }
    
    /**
     * Returns the maximum prefix length.
     * 
     * @return 32 for IPv4, 128 for IPv6
     */
    protected int getMaxPrefixLength() {
        return isIPv6 ? 128 : 32;
    }
    
    // ========================================================================
    // Abstract methods to be implemented by subclasses
    // ========================================================================
    
    /**
     * Inserts a prefix with the specified next hop value.
     * 
     * @param prefix the prefix bytes (4 for IPv4, 16 for IPv6)
     * @param prefixLen the prefix length (0-32 for IPv4, 0-128 for IPv6)
     * @param nextHop the next hop value
     * @throws InvalidPrefixException if the prefix or length is invalid
     * @throws IllegalStateException if the table is closed
     */
    public abstract void insert(byte[] prefix, int prefixLen, int nextHop);
    
    /**
     * Inserts a prefix using an InetAddress.
     * 
     * @param prefix the prefix address
     * @param prefixLen the prefix length
     * @param nextHop the next hop value
     * @throws InvalidPrefixException if the prefix or length is invalid
     * @throws IllegalStateException if the table is closed
     */
    public abstract void insert(InetAddress prefix, int prefixLen, int nextHop);
    
    /**
     * Inserts a prefix using CIDR notation.
     * 
     * @param cidr the prefix in CIDR notation (e.g., "192.168.0.0/16")
     * @param nextHop the next hop value
     * @throws InvalidPrefixException if the CIDR string is invalid
     * @throws IllegalStateException if the table is closed
     */
    public abstract void insert(String cidr, int nextHop);
    
    /**
     * Deletes a prefix from the table.
     * 
     * @param prefix the prefix bytes
     * @param prefixLen the prefix length
     * @return {@code true} if the prefix was found and deleted
     * @throws InvalidPrefixException if the prefix or length is invalid
     * @throws IllegalStateException if the table is closed
     */
    public abstract boolean delete(byte[] prefix, int prefixLen);
    
    /**
     * Deletes a prefix using an InetAddress.
     * 
     * @param prefix the prefix address
     * @param prefixLen the prefix length
     * @return {@code true} if the prefix was found and deleted
     * @throws InvalidPrefixException if the prefix or length is invalid
     * @throws IllegalStateException if the table is closed
     */
    public abstract boolean delete(InetAddress prefix, int prefixLen);
    
    /**
     * Looks up an address and returns the matching next hop.
     * 
     * @param address the address bytes (4 for IPv4, 16 for IPv6)
     * @return the next hop value, or {@link NextHop#INVALID} if no match
     * @throws InvalidPrefixException if the address is invalid
     * @throws IllegalStateException if the table is closed
     */
    public abstract int lookup(byte[] address);
    
    /**
     * Looks up an address using an InetAddress.
     * 
     * @param address the address
     * @return the next hop value, or {@link NextHop#INVALID} if no match
     * @throws InvalidPrefixException if the address is invalid
     * @throws IllegalStateException if the table is closed
     */
    public abstract int lookup(InetAddress address);
    
    /**
     * Looks up an address using string representation.
     * 
     * @param address the address string (e.g., "192.168.1.1")
     * @return the next hop value, or {@link NextHop#INVALID} if no match
     * @throws InvalidPrefixException if the address is invalid
     * @throws IllegalStateException if the table is closed
     */
    public abstract int lookup(String address);
    
    /**
     * Performs batch lookup for multiple addresses.
     * <p>
     * This is more efficient than calling {@link #lookup(byte[])} repeatedly
     * due to reduced JNI overhead.
     * 
     * @param addresses array of address byte arrays
     * @return array of next hop values (same order as input)
     * @throws InvalidPrefixException if any address is invalid
     * @throws IllegalStateException if the table is closed
     */
    public abstract int[] lookupBatch(byte[][] addresses);
    
    /**
     * Performs batch lookup for multiple InetAddresses.
     * 
     * @param addresses array of addresses
     * @return array of next hop values (same order as input)
     * @throws InvalidPrefixException if any address is invalid
     * @throws IllegalStateException if the table is closed
     */
    public abstract int[] lookupBatch(InetAddress[] addresses);
    
    // ========================================================================
    // Resource management
    // ========================================================================
    
    /**
     * Closes this table and releases native resources.
     * <p>
     * After calling this method, all operations on this table will throw
     * {@link IllegalStateException}. This method is idempotent.
     */
    @Override
    public void close() {
        synchronized (closeLock) {
            if (nativeHandle != 0) {
                nativeDestroy(nativeHandle);
                nativeHandle = 0;
            }
        }
    }
    
    /**
     * Finalizer as a safety net for resource cleanup.
     * <p>
     * <strong>Note:</strong> Always prefer explicit {@link #close()} or
     * try-with-resources over relying on finalization.
     */
    @Override
    @SuppressWarnings("deprecation")
    protected void finalize() throws Throwable {
        try {
            close();
        } finally {
            super.finalize();
        }
    }
    
    // ========================================================================
    // Utility methods
    // ========================================================================
    
    /**
     * Returns the library version string.
     * 
     * @return the version (e.g., "2.0.0")
     */
    public static String getVersion() {
        NativeLibraryLoader.ensureLoaded();
        return nativeGetVersion();
    }
    
    @Override
    public String toString() {
        return getClass().getSimpleName() + "[" +
               "algorithm=" + algorithm +
               ", closed=" + isClosed() +
               "]";
    }
    
    // ========================================================================
    // Native method declarations
    // ========================================================================
    
    // Creation
    protected static native long nativeCreateIPv4(int algorithm);
    protected static native long nativeCreateIPv6(int algorithm);
    
    // Operations
    protected static native int nativeAdd(long handle, byte[] prefix, int prefixLen, int nextHop);
    protected static native int nativeDelete(long handle, byte[] prefix, int prefixLen);
    protected static native int nativeLookup(long handle, byte[] address);
    protected static native void nativeLookupBatch(long handle, byte[][] addresses, int[] results);
    
    // IPv4-specific optimized lookup
    protected static native int nativeLookupIPv4(long handle, int addressAsInt);
    protected static native void nativeLookupBatchIPv4(long handle, int[] addresses, int[] results);
    
    // Resource management
    protected static native void nativeDestroy(long handle);
    
    // Utilities
    protected static native String nativeGetVersion();
}
