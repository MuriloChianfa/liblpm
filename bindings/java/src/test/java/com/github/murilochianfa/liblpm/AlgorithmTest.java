/*
 * Copyright (c) 2024 Murilo Chianfa
 * 
 * Licensed under the MIT License.
 */
package com.github.murilochianfa.liblpm;

import org.junit.jupiter.api.DisplayName;
import org.junit.jupiter.api.Test;

import static org.junit.jupiter.api.Assertions.*;

/**
 * Unit tests for Algorithm enum.
 */
@DisplayName("Algorithm")
class AlgorithmTest {
    
    @Test
    @DisplayName("DIR24 supports IPv4 only")
    void dir24SupportsIPv4Only() {
        assertTrue(Algorithm.DIR24.supportsIPv4());
        assertFalse(Algorithm.DIR24.supportsIPv6());
        
        assertDoesNotThrow(() -> Algorithm.DIR24.validateIPv4());
        assertThrows(IllegalArgumentException.class, () -> Algorithm.DIR24.validateIPv6());
    }
    
    @Test
    @DisplayName("STRIDE8 supports both IPv4 and IPv6")
    void stride8SupportsBoth() {
        assertTrue(Algorithm.STRIDE8.supportsIPv4());
        assertTrue(Algorithm.STRIDE8.supportsIPv6());
        
        assertDoesNotThrow(() -> Algorithm.STRIDE8.validateIPv4());
        assertDoesNotThrow(() -> Algorithm.STRIDE8.validateIPv6());
    }
    
    @Test
    @DisplayName("WIDE16 supports IPv6 only")
    void wide16SupportsIPv6Only() {
        assertFalse(Algorithm.WIDE16.supportsIPv4());
        assertTrue(Algorithm.WIDE16.supportsIPv6());
        
        assertThrows(IllegalArgumentException.class, () -> Algorithm.WIDE16.validateIPv4());
        assertDoesNotThrow(() -> Algorithm.WIDE16.validateIPv6());
    }
    
    @Test
    @DisplayName("native codes are distinct")
    void nativeCodesAreDistinct() {
        int dir24 = Algorithm.DIR24.getNativeCode();
        int stride8 = Algorithm.STRIDE8.getNativeCode();
        int wide16 = Algorithm.WIDE16.getNativeCode();
        
        assertNotEquals(dir24, stride8);
        assertNotEquals(dir24, wide16);
        assertNotEquals(stride8, wide16);
    }
    
    @Test
    @DisplayName("all algorithms can be used")
    void allAlgorithmsWork() {
        // IPv4 with DIR24
        try (LpmTableIPv4 t1 = LpmTableIPv4.create(Algorithm.DIR24)) {
            t1.insert("192.168.0.0/16", 100);
            assertEquals(100, t1.lookup("192.168.1.1"));
        }
        
        // IPv4 with STRIDE8
        try (LpmTableIPv4 t2 = LpmTableIPv4.create(Algorithm.STRIDE8)) {
            t2.insert("192.168.0.0/16", 100);
            assertEquals(100, t2.lookup("192.168.1.1"));
        }
        
        // IPv6 with WIDE16
        try (LpmTableIPv6 t3 = LpmTableIPv6.create(Algorithm.WIDE16)) {
            t3.insert("2001:db8::/32", 100);
            assertEquals(100, t3.lookup("2001:db8::1"));
        }
        
        // IPv6 with STRIDE8
        try (LpmTableIPv6 t4 = LpmTableIPv6.create(Algorithm.STRIDE8)) {
            t4.insert("2001:db8::/32", 100);
            assertEquals(100, t4.lookup("2001:db8::1"));
        }
    }
}
