/*
 * Copyright (c) 2024 Murilo Chianfa
 * 
 * Licensed under the MIT License.
 */
package com.github.murilochianfa.liblpm;

/**
 * LPM algorithm selection for routing table creation.
 * <p>
 * Different algorithms provide different performance characteristics:
 * 
 * <h2>IPv4 Algorithms</h2>
 * <dl>
 *   <dt>{@link #DIR24}</dt>
 *   <dd>DIR-24-8 algorithm. Uses a 24-bit direct table (~64MB) for the first 24 bits,
 *       with 8-bit extension tables for /25-/32 prefixes. Provides 1-2 memory accesses
 *       per lookup. <strong>Recommended for most IPv4 use cases.</strong></dd>
 *   
 *   <dt>{@link #STRIDE8}</dt>
 *   <dd>8-bit stride trie. Uses 256-entry nodes with up to 4 levels for IPv4.
 *       More memory-efficient for sparse routing tables.</dd>
 * </dl>
 * 
 * <h2>IPv6 Algorithms</h2>
 * <dl>
 *   <dt>{@link #WIDE16}</dt>
 *   <dd>Wide 16-bit stride for first level, 8-bit for remaining levels.
 *       Optimized for common /48 allocations. <strong>Recommended for IPv6.</strong></dd>
 *   
 *   <dt>{@link #STRIDE8}</dt>
 *   <dd>8-bit stride trie. Uses 256-entry nodes with up to 16 levels for IPv6.
 *       Good for diverse prefix distributions.</dd>
 * </dl>
 * 
 * <h2>Example Usage</h2>
 * <pre>{@code
 * // IPv4 with DIR-24-8 (fastest for typical BGP tables)
 * LpmTableIPv4 table = LpmTableIPv4.create(Algorithm.DIR24);
 * 
 * // IPv6 with wide 16-bit stride
 * LpmTableIPv6 table6 = LpmTableIPv6.create(Algorithm.WIDE16);
 * }</pre>
 * 
 * @author Murilo Chianfa
 * @since 1.0.0
 * @see LpmTableIPv4
 * @see LpmTableIPv6
 */
public enum Algorithm {
    
    /**
     * DIR-24-8 algorithm for IPv4.
     * <p>
     * Optimized for IPv4 routing tables with 1-2 memory accesses per lookup.
     * Uses approximately 64MB for the DIR-24 table plus extension tables as needed.
     * This is the default and recommended algorithm for IPv4.
     */
    DIR24(0, true, false),
    
    /**
     * 8-bit stride trie algorithm.
     * <p>
     * Works for both IPv4 (4 levels max) and IPv6 (16 levels max).
     * More memory-efficient for sparse routing tables but may require
     * more memory accesses for lookups.
     */
    STRIDE8(1, true, true),
    
    /**
     * Wide 16-bit stride algorithm for IPv6.
     * <p>
     * Uses 16-bit stride for the first level (~512KB), then 8-bit stride
     * for remaining levels. Optimized for IPv6 routing tables with common
     * /48 allocations. This is the default and recommended algorithm for IPv6.
     */
    WIDE16(2, false, true);
    
    private final int nativeCode;
    private final boolean supportsIPv4;
    private final boolean supportsIPv6;
    
    Algorithm(int nativeCode, boolean supportsIPv4, boolean supportsIPv6) {
        this.nativeCode = nativeCode;
        this.supportsIPv4 = supportsIPv4;
        this.supportsIPv6 = supportsIPv6;
    }
    
    /**
     * Returns the native code used by the JNI layer.
     * 
     * @return the native algorithm code
     */
    int getNativeCode() {
        return nativeCode;
    }
    
    /**
     * Returns whether this algorithm supports IPv4 tables.
     * 
     * @return {@code true} if IPv4 is supported
     */
    public boolean supportsIPv4() {
        return supportsIPv4;
    }
    
    /**
     * Returns whether this algorithm supports IPv6 tables.
     * 
     * @return {@code true} if IPv6 is supported
     */
    public boolean supportsIPv6() {
        return supportsIPv6;
    }
    
    /**
     * Validates that this algorithm supports IPv4.
     * 
     * @throws IllegalArgumentException if IPv4 is not supported
     */
    void validateIPv4() {
        if (!supportsIPv4) {
            throw new IllegalArgumentException(
                "Algorithm " + name() + " does not support IPv4. Use DIR24 or STRIDE8.");
        }
    }
    
    /**
     * Validates that this algorithm supports IPv6.
     * 
     * @throws IllegalArgumentException if IPv6 is not supported
     */
    void validateIPv6() {
        if (!supportsIPv6) {
            throw new IllegalArgumentException(
                "Algorithm " + name() + " does not support IPv6. Use WIDE16 or STRIDE8.");
        }
    }
}
