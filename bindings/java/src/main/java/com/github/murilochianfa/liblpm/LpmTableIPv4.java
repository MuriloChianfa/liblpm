/*
 * Copyright (c) 2024 Murilo Chianfa
 * 
 * Licensed under the MIT License.
 */
package com.github.murilochianfa.liblpm;

import java.net.Inet4Address;
import java.net.InetAddress;
import java.net.UnknownHostException;
import java.util.Objects;

/**
 * IPv4 Longest Prefix Match (LPM) routing table.
 * <p>
 * This class provides high-performance IPv4 routing table lookups using
 * liblpm's optimized algorithms.
 * 
 * <h2>Quick Start</h2>
 * <pre>{@code
 * try (LpmTableIPv4 table = LpmTableIPv4.create()) {
 *     // Insert routes
 *     table.insert("192.168.0.0/16", 100);
 *     table.insert("10.0.0.0/8", 200);
 *     table.insert("0.0.0.0/0", 1);  // Default route
 *     
 *     // Lookup
 *     int nh = table.lookup("192.168.1.1");  // Returns 100
 *     if (NextHop.isValid(nh)) {
 *         System.out.println("Next hop: " + nh);
 *     }
 * }
 * }</pre>
 * 
 * <h2>Algorithm Selection</h2>
 * Two algorithms are available for IPv4:
 * <ul>
 *   <li>{@link Algorithm#DIR24} (default): DIR-24-8 algorithm, optimal for
 *       typical BGP routing tables. Uses ~64MB memory but provides 1-2 memory
 *       accesses per lookup.</li>
 *   <li>{@link Algorithm#STRIDE8}: 8-bit stride trie, more memory-efficient
 *       for sparse tables but may require more memory accesses.</li>
 * </ul>
 * 
 * <h2>Performance Tips</h2>
 * <ul>
 *   <li>Use {@link #lookup(byte[])} for maximum performance</li>
 *   <li>Use {@link #lookupBatch(byte[][])} for bulk lookups</li>
 *   <li>Use {@link #lookupBatchFast(int[], int[])} for zero-copy batch lookups</li>
 * </ul>
 * 
 * @author Murilo Chianfa
 * @since 1.0.0
 * @see LpmTableIPv6
 * @see Algorithm
 */
public class LpmTableIPv4 extends LpmTable {
    
    /** IPv4 address length in bytes */
    private static final int IPV4_ADDR_LEN = 4;
    
    /** Maximum IPv4 prefix length */
    private static final int IPV4_MAX_PREFIX_LEN = 32;
    
    /**
     * Private constructor - use factory methods.
     */
    private LpmTableIPv4(long nativeHandle, Algorithm algorithm) {
        super(nativeHandle, algorithm, false);
    }
    
    /**
     * Creates a new IPv4 routing table with the default algorithm (DIR24).
     * 
     * @return a new IPv4 table
     * @throws NativeLibraryException if the native library cannot be loaded
     */
    public static LpmTableIPv4 create() {
        return create(Algorithm.DIR24);
    }
    
    /**
     * Creates a new IPv4 routing table with the specified algorithm.
     * 
     * @param algorithm the algorithm to use (DIR24 or STRIDE8)
     * @return a new IPv4 table
     * @throws IllegalArgumentException if the algorithm doesn't support IPv4
     * @throws NativeLibraryException if the native library cannot be loaded
     */
    public static LpmTableIPv4 create(Algorithm algorithm) {
        Objects.requireNonNull(algorithm, "Algorithm cannot be null");
        algorithm.validateIPv4();
        
        long handle = nativeCreateIPv4(algorithm.getNativeCode());
        return new LpmTableIPv4(handle, algorithm);
    }
    
    // ========================================================================
    // Insert operations
    // ========================================================================
    
    @Override
    public void insert(byte[] prefix, int prefixLen, int nextHop) {
        ensureOpen();
        validatePrefix(prefix, prefixLen);
        
        int result = nativeAdd(nativeHandle, prefix, prefixLen, nextHop);
        if (result != 0) {
            throw new LpmException("Failed to insert prefix");
        }
    }
    
    @Override
    public void insert(InetAddress prefix, int prefixLen, int nextHop) {
        Objects.requireNonNull(prefix, "Prefix cannot be null");
        
        if (!(prefix instanceof Inet4Address)) {
            throw new InvalidPrefixException(
                "Expected IPv4 address (Inet4Address), got: " + prefix.getClass().getSimpleName());
        }
        
        insert(prefix.getAddress(), prefixLen, nextHop);
    }
    
    @Override
    public void insert(String cidr, int nextHop) {
        Objects.requireNonNull(cidr, "CIDR cannot be null");
        
        int slashIdx = cidr.indexOf('/');
        if (slashIdx < 0) {
            throw new InvalidPrefixException("Invalid CIDR notation, missing '/': " + cidr);
        }
        
        String addrPart = cidr.substring(0, slashIdx);
        String lenPart = cidr.substring(slashIdx + 1);
        
        int prefixLen;
        try {
            prefixLen = Integer.parseInt(lenPart);
        } catch (NumberFormatException e) {
            throw new InvalidPrefixException("Invalid prefix length: " + lenPart, e);
        }
        
        byte[] prefix = parseIPv4Address(addrPart);
        insert(prefix, prefixLen, nextHop);
    }
    
    // ========================================================================
    // Delete operations
    // ========================================================================
    
    @Override
    public boolean delete(byte[] prefix, int prefixLen) {
        ensureOpen();
        validatePrefix(prefix, prefixLen);
        
        int result = nativeDelete(nativeHandle, prefix, prefixLen);
        return result == 0;
    }
    
    @Override
    public boolean delete(InetAddress prefix, int prefixLen) {
        Objects.requireNonNull(prefix, "Prefix cannot be null");
        
        if (!(prefix instanceof Inet4Address)) {
            throw new InvalidPrefixException(
                "Expected IPv4 address (Inet4Address), got: " + prefix.getClass().getSimpleName());
        }
        
        return delete(prefix.getAddress(), prefixLen);
    }
    
    /**
     * Deletes a prefix using CIDR notation.
     * 
     * @param cidr the prefix in CIDR notation (e.g., "192.168.0.0/16")
     * @return {@code true} if the prefix was found and deleted
     * @throws InvalidPrefixException if the CIDR string is invalid
     * @throws IllegalStateException if the table is closed
     */
    public boolean delete(String cidr) {
        Objects.requireNonNull(cidr, "CIDR cannot be null");
        
        int slashIdx = cidr.indexOf('/');
        if (slashIdx < 0) {
            throw new InvalidPrefixException("Invalid CIDR notation, missing '/': " + cidr);
        }
        
        String addrPart = cidr.substring(0, slashIdx);
        String lenPart = cidr.substring(slashIdx + 1);
        
        int prefixLen;
        try {
            prefixLen = Integer.parseInt(lenPart);
        } catch (NumberFormatException e) {
            throw new InvalidPrefixException("Invalid prefix length: " + lenPart, e);
        }
        
        byte[] prefix = parseIPv4Address(addrPart);
        return delete(prefix, prefixLen);
    }
    
    // ========================================================================
    // Lookup operations
    // ========================================================================
    
    @Override
    public int lookup(byte[] address) {
        ensureOpen();
        validateAddress(address);
        return nativeLookup(nativeHandle, address);
    }
    
    @Override
    public int lookup(InetAddress address) {
        Objects.requireNonNull(address, "Address cannot be null");
        
        if (!(address instanceof Inet4Address)) {
            throw new InvalidPrefixException(
                "Expected IPv4 address (Inet4Address), got: " + address.getClass().getSimpleName());
        }
        
        return lookup(address.getAddress());
    }
    
    @Override
    public int lookup(String address) {
        Objects.requireNonNull(address, "Address cannot be null");
        return lookup(parseIPv4Address(address));
    }
    
    /**
     * Optimized lookup using address as a 32-bit integer.
     * <p>
     * The address should be in network byte order (big-endian):
     * {@code (a << 24) | (b << 16) | (c << 8) | d} for address a.b.c.d
     * 
     * @param addressAsInt the IPv4 address as a 32-bit integer
     * @return the next hop value, or {@link NextHop#INVALID} if no match
     * @throws IllegalStateException if the table is closed
     */
    public int lookup(int addressAsInt) {
        ensureOpen();
        return nativeLookupIPv4(nativeHandle, addressAsInt);
    }
    
    // ========================================================================
    // Batch lookup operations
    // ========================================================================
    
    @Override
    public int[] lookupBatch(byte[][] addresses) {
        ensureOpen();
        Objects.requireNonNull(addresses, "Addresses cannot be null");
        
        if (addresses.length == 0) {
            return new int[0];
        }
        
        // Validate all addresses
        for (int i = 0; i < addresses.length; i++) {
            if (addresses[i] == null || addresses[i].length != IPV4_ADDR_LEN) {
                throw new InvalidPrefixException(
                    "Invalid address at index " + i + ": expected " + IPV4_ADDR_LEN + " bytes");
            }
        }
        
        int[] results = new int[addresses.length];
        nativeLookupBatch(nativeHandle, addresses, results);
        return results;
    }
    
    @Override
    public int[] lookupBatch(InetAddress[] addresses) {
        Objects.requireNonNull(addresses, "Addresses cannot be null");
        
        byte[][] byteAddresses = new byte[addresses.length][];
        for (int i = 0; i < addresses.length; i++) {
            InetAddress addr = addresses[i];
            if (addr == null) {
                throw new InvalidPrefixException("Null address at index " + i);
            }
            if (!(addr instanceof Inet4Address)) {
                throw new InvalidPrefixException(
                    "Expected IPv4 address at index " + i + ", got: " + addr.getClass().getSimpleName());
            }
            byteAddresses[i] = addr.getAddress();
        }
        
        return lookupBatch(byteAddresses);
    }
    
    /**
     * High-performance batch lookup using pre-allocated arrays.
     * <p>
     * This method provides the best performance by avoiding array allocations.
     * Addresses should be in network byte order (big-endian).
     * 
     * <pre>{@code
     * int[] addresses = new int[1000];
     * int[] results = new int[1000];
     * // ... populate addresses ...
     * table.lookupBatchFast(addresses, results);
     * }</pre>
     * 
     * @param addresses array of IPv4 addresses as 32-bit integers
     * @param results pre-allocated array to receive results (must be >= addresses.length)
     * @throws IllegalArgumentException if results array is too small
     * @throws IllegalStateException if the table is closed
     */
    public void lookupBatchFast(int[] addresses, int[] results) {
        ensureOpen();
        Objects.requireNonNull(addresses, "Addresses cannot be null");
        Objects.requireNonNull(results, "Results cannot be null");
        
        if (results.length < addresses.length) {
            throw new IllegalArgumentException(
                "Results array too small: need " + addresses.length + ", got " + results.length);
        }
        
        if (addresses.length == 0) {
            return;
        }
        
        nativeLookupBatchIPv4(nativeHandle, addresses, results);
    }
    
    /**
     * Batch lookup returning a new results array.
     * 
     * @param addresses array of IPv4 addresses as 32-bit integers
     * @return array of next hop values
     * @throws IllegalStateException if the table is closed
     */
    public int[] lookupBatch(int[] addresses) {
        int[] results = new int[addresses.length];
        lookupBatchFast(addresses, results);
        return results;
    }
    
    // ========================================================================
    // Validation helpers
    // ========================================================================
    
    private void validatePrefix(byte[] prefix, int prefixLen) {
        if (prefix == null) {
            throw new InvalidPrefixException("Prefix cannot be null");
        }
        if (prefix.length != IPV4_ADDR_LEN) {
            throw new InvalidPrefixException(
                "Invalid prefix length: expected " + IPV4_ADDR_LEN + " bytes, got " + prefix.length);
        }
        if (prefixLen < 0 || prefixLen > IPV4_MAX_PREFIX_LEN) {
            throw new InvalidPrefixException(
                "Prefix length out of range: " + prefixLen + " (must be 0-" + IPV4_MAX_PREFIX_LEN + ")");
        }
    }
    
    private void validateAddress(byte[] address) {
        if (address == null) {
            throw new InvalidPrefixException("Address cannot be null");
        }
        if (address.length != IPV4_ADDR_LEN) {
            throw new InvalidPrefixException(
                "Invalid address length: expected " + IPV4_ADDR_LEN + " bytes, got " + address.length);
        }
    }
    
    // ========================================================================
    // Parsing helpers
    // ========================================================================
    
    /**
     * Parses an IPv4 address string into a byte array.
     * 
     * @param address the address string (e.g., "192.168.1.1")
     * @return the 4-byte address
     * @throws InvalidPrefixException if the address is invalid
     */
    private static byte[] parseIPv4Address(String address) {
        try {
            InetAddress addr = InetAddress.getByName(address);
            if (!(addr instanceof Inet4Address)) {
                throw new InvalidPrefixException("Not an IPv4 address: " + address);
            }
            return addr.getAddress();
        } catch (UnknownHostException e) {
            throw new InvalidPrefixException("Invalid IPv4 address: " + address, e);
        }
    }
    
    /**
     * Converts a byte array address to a 32-bit integer (network byte order).
     * 
     * @param address the 4-byte address
     * @return the address as a 32-bit integer
     */
    public static int bytesToInt(byte[] address) {
        if (address == null || address.length != 4) {
            throw new IllegalArgumentException("Address must be 4 bytes");
        }
        return ((address[0] & 0xFF) << 24) |
               ((address[1] & 0xFF) << 16) |
               ((address[2] & 0xFF) << 8) |
               (address[3] & 0xFF);
    }
    
    /**
     * Converts a 32-bit integer to a byte array address.
     * 
     * @param addressInt the address as a 32-bit integer
     * @return the 4-byte address
     */
    public static byte[] intToBytes(int addressInt) {
        return new byte[] {
            (byte) (addressInt >> 24),
            (byte) (addressInt >> 16),
            (byte) (addressInt >> 8),
            (byte) addressInt
        };
    }
}
