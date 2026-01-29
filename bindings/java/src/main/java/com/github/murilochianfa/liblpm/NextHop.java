/*
 * Copyright (c) 2024 Murilo Chianfa
 * 
 * Licensed under the MIT License.
 */
package com.github.murilochianfa.liblpm;

/**
 * Constants and utilities for next hop values.
 * <p>
 * In liblpm, next hop values are 32-bit unsigned integers. However, note that
 * the DIR-24-8 algorithm only supports 30-bit next hop values (0 to 0x3FFFFFFF)
 * because the upper 2 bits are reserved for internal flags.
 * 
 * <h2>Invalid Next Hop</h2>
 * The value {@link #INVALID} (0xFFFFFFFF or -1 when interpreted as signed)
 * indicates that no matching prefix was found during lookup.
 * 
 * <h2>Example Usage</h2>
 * <pre>{@code
 * int result = table.lookup(address);
 * if (result == NextHop.INVALID) {
 *     System.out.println("No route found");
 * } else {
 *     System.out.println("Next hop: " + NextHop.toUnsigned(result));
 * }
 * }</pre>
 * 
 * @author Murilo Chianfa
 * @since 1.0.0
 */
public final class NextHop {
    
    /**
     * Invalid next hop value, returned when no matching prefix is found.
     * <p>
     * This is equivalent to {@code 0xFFFFFFFF} (unsigned) or {@code -1} (signed).
     */
    public static final int INVALID = -1; // 0xFFFFFFFF as unsigned
    
    /**
     * Maximum valid next hop value for DIR-24-8 algorithm.
     * <p>
     * DIR-24-8 reserves the upper 2 bits, limiting next hop values to 30 bits.
     */
    public static final int MAX_DIR24 = 0x3FFFFFFF;
    
    /**
     * Maximum valid next hop value for stride-based algorithms.
     * <p>
     * Stride algorithms can use the full 32-bit range except for INVALID.
     */
    public static final int MAX_STRIDE = 0x7FFFFFFF; // Leave room for INVALID
    
    /** Private constructor to prevent instantiation */
    private NextHop() {
        throw new AssertionError("NextHop cannot be instantiated");
    }
    
    /**
     * Checks if the given next hop value indicates no route was found.
     * 
     * @param nextHop the next hop value from a lookup
     * @return {@code true} if no route was found
     */
    public static boolean isInvalid(int nextHop) {
        return nextHop == INVALID;
    }
    
    /**
     * Checks if the given next hop value is valid (a route was found).
     * 
     * @param nextHop the next hop value from a lookup
     * @return {@code true} if a route was found
     */
    public static boolean isValid(int nextHop) {
        return nextHop != INVALID;
    }
    
    /**
     * Converts a signed int next hop value to its unsigned long representation.
     * <p>
     * Java's {@code int} is signed, but next hop values are unsigned 32-bit.
     * Use this method when you need the actual unsigned value.
     * 
     * @param nextHop the signed int next hop value
     * @return the unsigned value as a long
     */
    public static long toUnsigned(int nextHop) {
        return Integer.toUnsignedLong(nextHop);
    }
    
    /**
     * Validates a next hop value for DIR-24-8 algorithm.
     * 
     * @param nextHop the next hop value
     * @throws IllegalArgumentException if the value exceeds 30 bits
     */
    public static void validateForDir24(int nextHop) {
        if ((nextHop & 0xC0000000) != 0 && nextHop != INVALID) {
            throw new IllegalArgumentException(
                "Next hop value " + toUnsigned(nextHop) + " exceeds DIR-24-8 limit of " + MAX_DIR24);
        }
    }
}
