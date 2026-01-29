/*
 * Copyright (c) 2024 Murilo Chianfa
 * 
 * Licensed under the MIT License.
 */
package com.github.murilochianfa.liblpm;

/**
 * Base exception class for liblpm operations.
 * <p>
 * This is the root of the liblpm exception hierarchy. All liblpm-specific
 * exceptions extend this class, allowing callers to catch all liblpm errors
 * with a single catch block if desired.
 * 
 * @author Murilo Chianfa
 * @since 1.0.0
 */
public class LpmException extends RuntimeException {
    
    private static final long serialVersionUID = 1L;
    
    /**
     * Constructs a new LpmException with the specified detail message.
     * 
     * @param message the detail message
     */
    public LpmException(String message) {
        super(message);
    }
    
    /**
     * Constructs a new LpmException with the specified detail message and cause.
     * 
     * @param message the detail message
     * @param cause the cause (which is saved for later retrieval by {@link #getCause()})
     */
    public LpmException(String message, Throwable cause) {
        super(message, cause);
    }
    
    /**
     * Constructs a new LpmException with the specified cause.
     * 
     * @param cause the cause (which is saved for later retrieval by {@link #getCause()})
     */
    public LpmException(Throwable cause) {
        super(cause);
    }
}
