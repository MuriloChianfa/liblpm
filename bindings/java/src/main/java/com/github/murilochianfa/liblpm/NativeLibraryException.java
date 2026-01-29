/*
 * Copyright (c) 2024 Murilo Chianfa
 * 
 * Licensed under the MIT License.
 */
package com.github.murilochianfa.liblpm;

/**
 * Exception thrown when the native library cannot be loaded.
 * <p>
 * This exception indicates a fundamental problem with the native library:
 * <ul>
 *   <li>The library is not bundled for the current platform</li>
 *   <li>The library cannot be found in the system library path</li>
 *   <li>The library cannot be extracted from the JAR</li>
 *   <li>The library has incompatible dependencies</li>
 * </ul>
 * <p>
 * When this exception is thrown, the liblpm library cannot be used on
 * the current system. Check that:
 * <ol>
 *   <li>You're using a supported platform (Linux x86_64/aarch64, macOS, Windows)</li>
 *   <li>The native library (liblpmjni.so/dll/dylib) is available</li>
 *   <li>The liblpm C library is installed and accessible</li>
 * </ol>
 * 
 * @author Murilo Chianfa
 * @since 1.0.0
 */
public class NativeLibraryException extends LpmException {
    
    private static final long serialVersionUID = 1L;
    
    /**
     * Constructs a new NativeLibraryException with the specified detail message.
     * 
     * @param message the detail message
     */
    public NativeLibraryException(String message) {
        super(message);
    }
    
    /**
     * Constructs a new NativeLibraryException with the specified detail message and cause.
     * 
     * @param message the detail message
     * @param cause the cause
     */
    public NativeLibraryException(String message, Throwable cause) {
        super(message, cause);
    }
}
