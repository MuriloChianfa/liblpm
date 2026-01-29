/*
 * Copyright (c) 2024 Murilo Chianfa
 * 
 * Licensed under the MIT License.
 */
package com.github.murilochianfa.liblpm;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Native library loader for liblpm JNI bindings.
 * <p>
 * This class handles loading the native JNI library from either:
 * <ol>
 *   <li>Bundled resources within the JAR (auto-extracted to temp directory)</li>
 *   <li>System library path (fallback via {@code java.library.path})</li>
 * </ol>
 * <p>
 * Supported platforms:
 * <ul>
 *   <li>Linux x86_64</li>
 *   <li>Linux aarch64</li>
 *   <li>macOS x86_64</li>
 *   <li>macOS aarch64 (Apple Silicon)</li>
 *   <li>Windows x86_64</li>
 * </ul>
 * 
 * <p>Thread Safety: This class is thread-safe. The native library is loaded
 * exactly once during class initialization.
 * 
 * @author Murilo Chianfa
 * @since 1.0.0
 */
public final class NativeLibraryLoader {
    
    /** Library name without prefix/suffix */
    private static final String LIBRARY_NAME = "lpmjni";
    
    /** Resource path prefix for bundled natives */
    private static final String NATIVE_RESOURCE_PREFIX = "/native/";
    
    /** Tracks whether the library has been loaded */
    private static final AtomicBoolean loaded = new AtomicBoolean(false);
    
    /** Error encountered during loading, if any */
    private static volatile Throwable loadError = null;
    
    /** Temporary file for extracted native library */
    private static volatile Path extractedLibrary = null;
    
    // Static initializer - loads the library when class is first accessed
    static {
        loadNativeLibrary();
    }
    
    /** Private constructor to prevent instantiation */
    private NativeLibraryLoader() {
        throw new AssertionError("NativeLibraryLoader cannot be instantiated");
    }
    
    /**
     * Ensures the native library is loaded.
     * <p>
     * This method is idempotent and thread-safe. If the library has already
     * been loaded, this method returns immediately. If loading failed during
     * class initialization, this method throws the original error.
     * 
     * @throws NativeLibraryException if the native library cannot be loaded
     */
    public static void ensureLoaded() throws NativeLibraryException {
        if (loadError != null) {
            throw new NativeLibraryException("Failed to load native library", loadError);
        }
        if (!loaded.get()) {
            throw new NativeLibraryException("Native library not loaded");
        }
    }
    
    /**
     * Returns whether the native library has been successfully loaded.
     * 
     * @return {@code true} if the library is loaded, {@code false} otherwise
     */
    public static boolean isLoaded() {
        return loaded.get() && loadError == null;
    }
    
    /**
     * Gets the platform identifier for the current system.
     * <p>
     * Format: {@code os-arch} (e.g., "linux-x86_64", "darwin-aarch64")
     * 
     * @return the platform identifier string
     */
    public static String getPlatform() {
        return detectOS() + "-" + detectArch();
    }
    
    /**
     * Main library loading logic.
     */
    private static void loadNativeLibrary() {
        if (!loaded.compareAndSet(false, true)) {
            return; // Already loaded or loading
        }
        
        try {
            // First, try to load from bundled resources
            if (loadFromResources()) {
                return;
            }
            
            // Fallback: try system library path
            loadFromSystemPath();
            
        } catch (Throwable t) {
            loadError = t;
            loaded.set(false);
        }
    }
    
    /**
     * Attempts to load the native library from bundled JAR resources.
     * 
     * @return {@code true} if successfully loaded, {@code false} if resource not found
     * @throws NativeLibraryException if extraction or loading fails
     */
    private static boolean loadFromResources() throws NativeLibraryException {
        String platform = getPlatform();
        String libraryFileName = getLibraryFileName();
        String resourcePath = NATIVE_RESOURCE_PREFIX + platform + "/" + libraryFileName;
        
        try (InputStream in = NativeLibraryLoader.class.getResourceAsStream(resourcePath)) {
            if (in == null) {
                // Resource not found - not an error, will try system path
                return false;
            }
            
            // Extract to temporary file
            Path tempDir = Files.createTempDirectory("liblpm-native-");
            Path tempLib = tempDir.resolve(libraryFileName);
            
            try (OutputStream out = Files.newOutputStream(tempLib)) {
                byte[] buffer = new byte[8192];
                int bytesRead;
                while ((bytesRead = in.read(buffer)) != -1) {
                    out.write(buffer, 0, bytesRead);
                }
            }
            
            // Make executable on Unix systems
            File libFile = tempLib.toFile();
            if (!libFile.setExecutable(true)) {
                // Ignore - not all systems require this
            }
            
            // Register cleanup on JVM shutdown
            extractedLibrary = tempLib;
            Runtime.getRuntime().addShutdownHook(new Thread(() -> {
                try {
                    Files.deleteIfExists(tempLib);
                    Files.deleteIfExists(tempDir);
                } catch (IOException e) {
                    // Ignore cleanup errors
                }
            }, "liblpm-cleanup"));
            
            // Load the extracted library
            System.load(tempLib.toAbsolutePath().toString());
            return true;
            
        } catch (IOException e) {
            throw new NativeLibraryException(
                "Failed to extract native library from resources: " + resourcePath, e);
        } catch (UnsatisfiedLinkError e) {
            throw new NativeLibraryException(
                "Failed to load extracted native library", e);
        }
    }
    
    /**
     * Attempts to load the native library from the system library path.
     * 
     * @throws NativeLibraryException if loading fails
     */
    private static void loadFromSystemPath() throws NativeLibraryException {
        try {
            System.loadLibrary(LIBRARY_NAME);
        } catch (UnsatisfiedLinkError e) {
            throw new NativeLibraryException(
                "Failed to load native library '" + LIBRARY_NAME + "' from system path. " +
                "Ensure the library is installed or set java.library.path. " +
                "Platform: " + getPlatform(), e);
        }
    }
    
    /**
     * Detects the operating system.
     * 
     * @return normalized OS name (linux, darwin, windows, or the raw name)
     */
    private static String detectOS() {
        String osName = System.getProperty("os.name", "").toLowerCase();
        
        if (osName.contains("linux")) {
            return "linux";
        } else if (osName.contains("mac") || osName.contains("darwin")) {
            return "darwin";
        } else if (osName.contains("windows")) {
            return "windows";
        }
        
        // Return the raw name for unknown OS
        return osName.replaceAll("\\s+", "-").toLowerCase();
    }
    
    /**
     * Detects the CPU architecture.
     * 
     * @return normalized architecture name (x86_64, aarch64, or the raw name)
     */
    private static String detectArch() {
        String osArch = System.getProperty("os.arch", "").toLowerCase();
        
        if (osArch.equals("amd64") || osArch.equals("x86_64")) {
            return "x86_64";
        } else if (osArch.equals("aarch64") || osArch.equals("arm64")) {
            return "aarch64";
        }
        
        return osArch;
    }
    
    /**
     * Gets the platform-specific library file name.
     * 
     * @return library file name (e.g., "liblpmjni.so", "lpmjni.dll")
     */
    private static String getLibraryFileName() {
        String os = detectOS();
        
        if (os.equals("windows")) {
            return LIBRARY_NAME + ".dll";
        } else if (os.equals("darwin")) {
            return "lib" + LIBRARY_NAME + ".dylib";
        } else {
            // Linux and others
            return "lib" + LIBRARY_NAME + ".so";
        }
    }
}
