/*
 * Copyright (c) 2024 Murilo Chianfa
 * 
 * Licensed under the MIT License.
 */
package com.github.murilochianfa.liblpm;

import java.net.Inet6Address;
import java.net.InetAddress;
import java.net.UnknownHostException;
import java.util.Objects;

/**
 * IPv6 Longest Prefix Match (LPM) routing table.
 * <p>
 * This class provides high-performance IPv6 routing table lookups using
 * liblpm's optimized algorithms.
 * 
 * <h2>Quick Start</h2>
 * <pre>{@code
 * try (LpmTableIPv6 table = LpmTableIPv6.create()) {
 *     // Insert routes
 *     table.insert("2001:db8::/32", 100);
 *     table.insert("::ffff:0:0/96", 200);  // IPv4-mapped IPv6
 *     table.insert("::/0", 1);  // Default route
 *     
 *     // Lookup
 *     int nh = table.lookup("2001:db8::1");  // Returns 100
 *     if (NextHop.isValid(nh)) {
 *         System.out.println("Next hop: " + nh);
 *     }
 * }
 * }</pre>
 * 
 * <h2>Algorithm Selection</h2>
 * Two algorithms are available for IPv6:
 * <ul>
 *   <li>{@link Algorithm#WIDE16} (default): 16-bit stride for first level,
 *       8-bit for remaining. Optimized for common /48 allocations.</li>
 *   <li>{@link Algorithm#STRIDE8}: 8-bit stride trie with up to 16 levels.
 *       More memory-efficient for sparse tables.</li>
 * </ul>
 * 
 * <h2>Performance Tips</h2>
 * <ul>
 *   <li>Use {@link #lookup(byte[])} for maximum performance</li>
 *   <li>Use {@link #lookupBatch(byte[][])} for bulk lookups</li>
 *   <li>IPv6 lookups are inherently slower than IPv4 due to longer addresses</li>
 * </ul>
 * 
 * @author Murilo Chianfa
 * @since 1.0.0
 * @see LpmTableIPv4
 * @see Algorithm
 */
public class LpmTableIPv6 extends LpmTable {
    
    /** IPv6 address length in bytes */
    private static final int IPV6_ADDR_LEN = 16;
    
    /** Maximum IPv6 prefix length */
    private static final int IPV6_MAX_PREFIX_LEN = 128;
    
    /**
     * Private constructor - use factory methods.
     */
    private LpmTableIPv6(long nativeHandle, Algorithm algorithm) {
        super(nativeHandle, algorithm, true);
    }
    
    /**
     * Creates a new IPv6 routing table with the default algorithm (WIDE16).
     * 
     * @return a new IPv6 table
     * @throws NativeLibraryException if the native library cannot be loaded
     */
    public static LpmTableIPv6 create() {
        return create(Algorithm.WIDE16);
    }
    
    /**
     * Creates a new IPv6 routing table with the specified algorithm.
     * 
     * @param algorithm the algorithm to use (WIDE16 or STRIDE8)
     * @return a new IPv6 table
     * @throws IllegalArgumentException if the algorithm doesn't support IPv6
     * @throws NativeLibraryException if the native library cannot be loaded
     */
    public static LpmTableIPv6 create(Algorithm algorithm) {
        Objects.requireNonNull(algorithm, "Algorithm cannot be null");
        algorithm.validateIPv6();
        
        long handle = nativeCreateIPv6(algorithm.getNativeCode());
        return new LpmTableIPv6(handle, algorithm);
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
        
        if (!(prefix instanceof Inet6Address)) {
            throw new InvalidPrefixException(
                "Expected IPv6 address (Inet6Address), got: " + prefix.getClass().getSimpleName());
        }
        
        insert(prefix.getAddress(), prefixLen, nextHop);
    }
    
    @Override
    public void insert(String cidr, int nextHop) {
        Objects.requireNonNull(cidr, "CIDR cannot be null");
        
        int slashIdx = cidr.lastIndexOf('/');  // Use lastIndexOf for IPv6 (contains colons)
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
        
        byte[] prefix = parseIPv6Address(addrPart);
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
        
        if (!(prefix instanceof Inet6Address)) {
            throw new InvalidPrefixException(
                "Expected IPv6 address (Inet6Address), got: " + prefix.getClass().getSimpleName());
        }
        
        return delete(prefix.getAddress(), prefixLen);
    }
    
    /**
     * Deletes a prefix using CIDR notation.
     * 
     * @param cidr the prefix in CIDR notation (e.g., "2001:db8::/32")
     * @return {@code true} if the prefix was found and deleted
     * @throws InvalidPrefixException if the CIDR string is invalid
     * @throws IllegalStateException if the table is closed
     */
    public boolean delete(String cidr) {
        Objects.requireNonNull(cidr, "CIDR cannot be null");
        
        int slashIdx = cidr.lastIndexOf('/');
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
        
        byte[] prefix = parseIPv6Address(addrPart);
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
        
        if (!(address instanceof Inet6Address)) {
            throw new InvalidPrefixException(
                "Expected IPv6 address (Inet6Address), got: " + address.getClass().getSimpleName());
        }
        
        return lookup(address.getAddress());
    }
    
    @Override
    public int lookup(String address) {
        Objects.requireNonNull(address, "Address cannot be null");
        return lookup(parseIPv6Address(address));
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
            if (addresses[i] == null || addresses[i].length != IPV6_ADDR_LEN) {
                throw new InvalidPrefixException(
                    "Invalid address at index " + i + ": expected " + IPV6_ADDR_LEN + " bytes");
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
            if (!(addr instanceof Inet6Address)) {
                throw new InvalidPrefixException(
                    "Expected IPv6 address at index " + i + ", got: " + addr.getClass().getSimpleName());
            }
            byteAddresses[i] = addr.getAddress();
        }
        
        return lookupBatch(byteAddresses);
    }
    
    /**
     * Batch lookup with pre-allocated results array.
     * <p>
     * This method avoids allocating a new results array, which can be
     * useful for high-throughput applications.
     * 
     * @param addresses array of 16-byte IPv6 addresses
     * @param results pre-allocated array to receive results (must be >= addresses.length)
     * @throws IllegalArgumentException if results array is too small
     * @throws InvalidPrefixException if any address is invalid
     * @throws IllegalStateException if the table is closed
     */
    public void lookupBatchInto(byte[][] addresses, int[] results) {
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
        
        // Validate all addresses
        for (int i = 0; i < addresses.length; i++) {
            if (addresses[i] == null || addresses[i].length != IPV6_ADDR_LEN) {
                throw new InvalidPrefixException(
                    "Invalid address at index " + i + ": expected " + IPV6_ADDR_LEN + " bytes");
            }
        }
        
        nativeLookupBatch(nativeHandle, addresses, results);
    }
    
    // ========================================================================
    // Validation helpers
    // ========================================================================
    
    private void validatePrefix(byte[] prefix, int prefixLen) {
        if (prefix == null) {
            throw new InvalidPrefixException("Prefix cannot be null");
        }
        if (prefix.length != IPV6_ADDR_LEN) {
            throw new InvalidPrefixException(
                "Invalid prefix length: expected " + IPV6_ADDR_LEN + " bytes, got " + prefix.length);
        }
        if (prefixLen < 0 || prefixLen > IPV6_MAX_PREFIX_LEN) {
            throw new InvalidPrefixException(
                "Prefix length out of range: " + prefixLen + " (must be 0-" + IPV6_MAX_PREFIX_LEN + ")");
        }
    }
    
    private void validateAddress(byte[] address) {
        if (address == null) {
            throw new InvalidPrefixException("Address cannot be null");
        }
        if (address.length != IPV6_ADDR_LEN) {
            throw new InvalidPrefixException(
                "Invalid address length: expected " + IPV6_ADDR_LEN + " bytes, got " + address.length);
        }
    }
    
    // ========================================================================
    // Parsing helpers
    // ========================================================================
    
    /**
     * Parses an IPv6 address string into a byte array.
     * 
     * @param address the address string (e.g., "2001:db8::1")
     * @return the 16-byte address
     * @throws InvalidPrefixException if the address is invalid
     */
    private static byte[] parseIPv6Address(String address) {
        try {
            // Handle bracket notation if present
            String addr = address;
            if (addr.startsWith("[") && addr.endsWith("]")) {
                addr = addr.substring(1, addr.length() - 1);
            }
            
            InetAddress inetAddr = InetAddress.getByName(addr);
            if (!(inetAddr instanceof Inet6Address)) {
                throw new InvalidPrefixException("Not an IPv6 address: " + address);
            }
            return inetAddr.getAddress();
        } catch (UnknownHostException e) {
            throw new InvalidPrefixException("Invalid IPv6 address: " + address, e);
        }
    }
    
    /**
     * Formats a 16-byte IPv6 address as a string.
     * 
     * @param address the 16-byte address
     * @return the formatted address string
     */
    public static String formatAddress(byte[] address) {
        if (address == null || address.length != IPV6_ADDR_LEN) {
            throw new IllegalArgumentException("Address must be 16 bytes");
        }
        
        StringBuilder sb = new StringBuilder();
        for (int i = 0; i < 16; i += 2) {
            if (i > 0) sb.append(':');
            int value = ((address[i] & 0xFF) << 8) | (address[i + 1] & 0xFF);
            sb.append(String.format("%x", value));
        }
        return sb.toString();
    }
}
