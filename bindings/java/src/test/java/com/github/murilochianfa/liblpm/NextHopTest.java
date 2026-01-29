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
 * Unit tests for NextHop utility class.
 */
@DisplayName("NextHop")
class NextHopTest {
    
    @Test
    @DisplayName("INVALID constant is -1 (0xFFFFFFFF)")
    void invalidConstant() {
        assertEquals(-1, NextHop.INVALID);
        assertEquals(0xFFFFFFFFL, NextHop.toUnsigned(NextHop.INVALID));
    }
    
    @Test
    @DisplayName("isInvalid returns true for INVALID")
    void isInvalidForInvalid() {
        assertTrue(NextHop.isInvalid(NextHop.INVALID));
        assertTrue(NextHop.isInvalid(-1));
    }
    
    @Test
    @DisplayName("isInvalid returns false for valid values")
    void isInvalidForValid() {
        assertFalse(NextHop.isInvalid(0));
        assertFalse(NextHop.isInvalid(1));
        assertFalse(NextHop.isInvalid(100));
        assertFalse(NextHop.isInvalid(Integer.MAX_VALUE));
    }
    
    @Test
    @DisplayName("isValid returns true for valid values")
    void isValidForValid() {
        assertTrue(NextHop.isValid(0));
        assertTrue(NextHop.isValid(1));
        assertTrue(NextHop.isValid(100));
        assertTrue(NextHop.isValid(Integer.MAX_VALUE));
    }
    
    @Test
    @DisplayName("isValid returns false for INVALID")
    void isValidForInvalid() {
        assertFalse(NextHop.isValid(NextHop.INVALID));
        assertFalse(NextHop.isValid(-1));
    }
    
    @Test
    @DisplayName("toUnsigned converts correctly")
    void toUnsignedConverts() {
        assertEquals(0L, NextHop.toUnsigned(0));
        assertEquals(1L, NextHop.toUnsigned(1));
        assertEquals(100L, NextHop.toUnsigned(100));
        assertEquals(Integer.MAX_VALUE, NextHop.toUnsigned(Integer.MAX_VALUE));
        assertEquals(0xFFFFFFFFL, NextHop.toUnsigned(-1));
        assertEquals(0x80000000L, NextHop.toUnsigned(Integer.MIN_VALUE));
    }
    
    @Test
    @DisplayName("MAX_DIR24 is 30-bit max")
    void maxDir24Value() {
        assertEquals(0x3FFFFFFF, NextHop.MAX_DIR24);
    }
    
    @Test
    @DisplayName("validateForDir24 accepts valid values")
    void validateForDir24AcceptsValid() {
        assertDoesNotThrow(() -> NextHop.validateForDir24(0));
        assertDoesNotThrow(() -> NextHop.validateForDir24(1));
        assertDoesNotThrow(() -> NextHop.validateForDir24(100));
        assertDoesNotThrow(() -> NextHop.validateForDir24(NextHop.MAX_DIR24));
        assertDoesNotThrow(() -> NextHop.validateForDir24(NextHop.INVALID)); // INVALID is special
    }
    
    @Test
    @DisplayName("validateForDir24 rejects values exceeding 30 bits")
    void validateForDir24RejectsLarge() {
        assertThrows(IllegalArgumentException.class, () -> 
            NextHop.validateForDir24(0x40000000));
        assertThrows(IllegalArgumentException.class, () -> 
            NextHop.validateForDir24(0x7FFFFFFF)); // MAX_INT uses bit 30
    }
}
