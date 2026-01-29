/*
 * Copyright (c) 2024 Murilo Chianfa
 * 
 * Licensed under the MIT License.
 */
package com.github.murilochianfa.liblpm;

/**
 * Exception thrown when an invalid IP prefix is provided.
 * <p>
 * This exception is thrown when:
 * <ul>
 *   <li>The prefix byte array has incorrect length (not 4 for IPv4, not 16 for IPv6)</li>
 *   <li>The prefix length is out of range (0-32 for IPv4, 0-128 for IPv6)</li>
 *   <li>The prefix string cannot be parsed</li>
 *   <li>The InetAddress type doesn't match the table type (e.g., IPv6 address for IPv4 table)</li>
 * </ul>
 * 
 * @author Murilo Chianfa
 * @since 1.0.0
 */
public class InvalidPrefixException extends LpmException {
    
    private static final long serialVersionUID = 1L;
    
    /**
     * Constructs a new InvalidPrefixException with the specified detail message.
     * 
     * @param message the detail message
     */
    public InvalidPrefixException(String message) {
        super(message);
    }
    
    /**
     * Constructs a new InvalidPrefixException with the specified detail message and cause.
     * 
     * @param message the detail message
     * @param cause the cause
     */
    public InvalidPrefixException(String message, Throwable cause) {
        super(message, cause);
    }
}
