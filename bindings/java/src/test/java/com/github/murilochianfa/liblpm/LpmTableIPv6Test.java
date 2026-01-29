/*
 * Copyright (c) 2024 Murilo Chianfa
 * 
 * Licensed under the MIT License.
 */
package com.github.murilochianfa.liblpm;

import org.junit.jupiter.api.AfterEach;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.DisplayName;
import org.junit.jupiter.api.Nested;
import org.junit.jupiter.api.Test;

import java.net.Inet6Address;
import java.net.InetAddress;

import static org.junit.jupiter.api.Assertions.*;

/**
 * Unit tests for LpmTableIPv6.
 */
@DisplayName("LpmTableIPv6")
class LpmTableIPv6Test {
    
    private LpmTableIPv6 table;
    
    @BeforeEach
    void setUp() {
        table = LpmTableIPv6.create();
    }
    
    @AfterEach
    void tearDown() {
        if (table != null && !table.isClosed()) {
            table.close();
        }
    }
    
    @Nested
    @DisplayName("Creation")
    class CreationTests {
        
        @Test
        @DisplayName("creates with default algorithm (WIDE16)")
        void createsWithDefaultAlgorithm() {
            try (LpmTableIPv6 t = LpmTableIPv6.create()) {
                assertNotNull(t);
                assertEquals(Algorithm.WIDE16, t.getAlgorithm());
                assertTrue(t.isIPv6());
                assertFalse(t.isClosed());
            }
        }
        
        @Test
        @DisplayName("creates with STRIDE8 algorithm")
        void createsWithStride8() {
            try (LpmTableIPv6 t = LpmTableIPv6.create(Algorithm.STRIDE8)) {
                assertNotNull(t);
                assertEquals(Algorithm.STRIDE8, t.getAlgorithm());
            }
        }
        
        @Test
        @DisplayName("rejects DIR24 algorithm for IPv6")
        void rejectsDir24() {
            assertThrows(IllegalArgumentException.class, () -> {
                LpmTableIPv6.create(Algorithm.DIR24);
            });
        }
        
        @Test
        @DisplayName("rejects null algorithm")
        void rejectsNullAlgorithm() {
            assertThrows(NullPointerException.class, () -> {
                LpmTableIPv6.create(null);
            });
        }
    }
    
    @Nested
    @DisplayName("Insert operations")
    class InsertTests {
        
        @Test
        @DisplayName("inserts prefix using byte array")
        void insertsByteArray() {
            byte[] prefix = new byte[16];
            prefix[0] = 0x20;
            prefix[1] = 0x01;
            prefix[2] = 0x0d;
            prefix[3] = (byte)0xb8;
            assertDoesNotThrow(() -> table.insert(prefix, 32, 100));
        }
        
        @Test
        @DisplayName("inserts prefix using CIDR string")
        void insertsCidrString() {
            assertDoesNotThrow(() -> table.insert("2001:db8::/32", 100));
            assertDoesNotThrow(() -> table.insert("fc00::/7", 200));
            assertDoesNotThrow(() -> table.insert("::/0", 1));
        }
        
        @Test
        @DisplayName("inserts prefix using InetAddress")
        void insertsInetAddress() throws Exception {
            InetAddress addr = InetAddress.getByName("2001:db8::");
            assertDoesNotThrow(() -> table.insert(addr, 32, 100));
        }
        
        @Test
        @DisplayName("inserts /128 host route")
        void insertsHostRoute() {
            assertDoesNotThrow(() -> table.insert("2001:db8::1/128", 999));
            assertEquals(999, table.lookup("2001:db8::1"));
        }
        
        @Test
        @DisplayName("inserts default route")
        void insertsDefaultRoute() {
            table.insert("::/0", 1);
            assertEquals(1, table.lookup("2607:f8b0:4004:800::200e"));
        }
        
        @Test
        @DisplayName("rejects invalid prefix length")
        void rejectsInvalidPrefixLength() {
            byte[] prefix = new byte[16];
            
            assertThrows(InvalidPrefixException.class, () -> table.insert(prefix, -1, 100));
            assertThrows(InvalidPrefixException.class, () -> table.insert(prefix, 129, 100));
        }
        
        @Test
        @DisplayName("rejects invalid byte array length")
        void rejectsInvalidByteArrayLength() {
            byte[] tooShort = new byte[4];
            byte[] tooLong = new byte[32];
            
            assertThrows(InvalidPrefixException.class, () -> table.insert(tooShort, 32, 100));
            assertThrows(InvalidPrefixException.class, () -> table.insert(tooLong, 32, 100));
        }
        
        @Test
        @DisplayName("rejects null prefix")
        void rejectsNullPrefix() {
            assertThrows(NullPointerException.class, () -> table.insert((String)null, 100));
            assertThrows(NullPointerException.class, () -> table.insert((InetAddress)null, 32, 100));
            assertThrows(InvalidPrefixException.class, () -> table.insert((byte[])null, 32, 100));
        }
        
        @Test
        @DisplayName("rejects invalid CIDR format")
        void rejectsInvalidCidr() {
            assertThrows(InvalidPrefixException.class, () -> table.insert("2001:db8::", 100));
            assertThrows(InvalidPrefixException.class, () -> table.insert("2001:db8::/abc", 100));
        }
    }
    
    @Nested
    @DisplayName("Lookup operations")
    class LookupTests {
        
        @BeforeEach
        void insertRoutes() {
            table.insert("2001:db8::/32", 100);
            table.insert("2001:db8:1234::/48", 101);
            table.insert("fc00::/7", 200);
            table.insert("::/0", 1);
        }
        
        @Test
        @DisplayName("performs longest prefix match")
        void longestPrefixMatch() {
            // Most specific wins
            assertEquals(101, table.lookup("2001:db8:1234::1"));
            assertEquals(100, table.lookup("2001:db8:5678::1"));
            assertEquals(200, table.lookup("fd12:3456:7890::1"));
            assertEquals(1, table.lookup("2607:f8b0:4004:800::200e"));
        }
        
        @Test
        @DisplayName("lookup using byte array")
        void lookupByteArray() throws Exception {
            InetAddress addr = InetAddress.getByName("2001:db8:1234::1");
            assertEquals(101, table.lookup(addr.getAddress()));
        }
        
        @Test
        @DisplayName("lookup using InetAddress")
        void lookupInetAddress() throws Exception {
            InetAddress addr = InetAddress.getByName("2001:db8:1234::1");
            assertEquals(101, table.lookup(addr));
        }
        
        @Test
        @DisplayName("returns INVALID for no match (without default route)")
        void returnsInvalidForNoMatch() {
            try (LpmTableIPv6 emptyTable = LpmTableIPv6.create()) {
                int result = emptyTable.lookup("2001:db8::1");
                assertEquals(NextHop.INVALID, result);
                assertTrue(NextHop.isInvalid(result));
            }
        }
        
        @Test
        @DisplayName("rejects invalid address")
        void rejectsInvalidAddress() {
            assertThrows(InvalidPrefixException.class, () -> table.lookup(new byte[4]));
            assertThrows(InvalidPrefixException.class, () -> table.lookup(new byte[32]));
        }
    }
    
    @Nested
    @DisplayName("Delete operations")
    class DeleteTests {
        
        @BeforeEach
        void insertRoutes() {
            table.insert("2001:db8::/32", 100);
            table.insert("fc00::/7", 200);
        }
        
        @Test
        @DisplayName("deletes existing prefix")
        void deletesExisting() {
            assertTrue(table.delete("2001:db8::/32"));
            assertEquals(NextHop.INVALID, table.lookup("2001:db8::1"));
        }
        
        @Test
        @DisplayName("returns false for non-existent prefix")
        void returnsFalseForNonExistent() {
            assertFalse(table.delete("2001:db9::/32"));
        }
        
        @Test
        @DisplayName("deletes using byte array")
        void deletesByteArray() throws Exception {
            InetAddress addr = InetAddress.getByName("2001:db8::");
            assertTrue(table.delete(addr.getAddress(), 32));
        }
        
        @Test
        @DisplayName("deletes using InetAddress")
        void deletesInetAddress() throws Exception {
            InetAddress addr = InetAddress.getByName("2001:db8::");
            assertTrue(table.delete(addr, 32));
        }
    }
    
    @Nested
    @DisplayName("Batch operations")
    class BatchTests {
        
        @BeforeEach
        void insertRoutes() {
            table.insert("2001:db8::/32", 100);
            table.insert("fc00::/7", 200);
            table.insert("::/0", 1);
        }
        
        @Test
        @DisplayName("batch lookup with byte arrays")
        void batchLookupByteArrays() throws Exception {
            byte[][] addresses = {
                InetAddress.getByName("2001:db8::1").getAddress(),
                InetAddress.getByName("fd00::1").getAddress(),
                InetAddress.getByName("2607:f8b0::1").getAddress()
            };
            
            int[] results = table.lookupBatch(addresses);
            
            assertEquals(3, results.length);
            assertEquals(100, results[0]);
            assertEquals(200, results[1]);
            assertEquals(1, results[2]);
        }
        
        @Test
        @DisplayName("batch lookup with InetAddress arrays")
        void batchLookupInetAddresses() throws Exception {
            InetAddress[] addresses = {
                InetAddress.getByName("2001:db8::1"),
                InetAddress.getByName("fd00::1"),
                InetAddress.getByName("2607:f8b0::1")
            };
            
            int[] results = table.lookupBatch(addresses);
            
            assertEquals(3, results.length);
            assertEquals(100, results[0]);
            assertEquals(200, results[1]);
            assertEquals(1, results[2]);
        }
        
        @Test
        @DisplayName("batch lookup into pre-allocated array")
        void batchLookupInto() throws Exception {
            byte[][] addresses = {
                InetAddress.getByName("2001:db8::1").getAddress(),
                InetAddress.getByName("fd00::1").getAddress()
            };
            int[] results = new int[2];
            
            table.lookupBatchInto(addresses, results);
            
            assertEquals(100, results[0]);
            assertEquals(200, results[1]);
        }
        
        @Test
        @DisplayName("handles empty batch")
        void handlesEmptyBatch() {
            int[] results = table.lookupBatch(new byte[0][]);
            assertEquals(0, results.length);
        }
        
        @Test
        @DisplayName("rejects undersized results array")
        void rejectsUndersizedResults() throws Exception {
            byte[][] addresses = {
                InetAddress.getByName("2001:db8::1").getAddress(),
                InetAddress.getByName("fd00::1").getAddress(),
                InetAddress.getByName("2607:f8b0::1").getAddress()
            };
            int[] results = new int[2];
            
            assertThrows(IllegalArgumentException.class, () -> 
                table.lookupBatchInto(addresses, results));
        }
    }
    
    @Nested
    @DisplayName("Resource management")
    class ResourceTests {
        
        @Test
        @DisplayName("close releases resources")
        void closeReleasesResources() {
            LpmTableIPv6 t = LpmTableIPv6.create();
            assertFalse(t.isClosed());
            
            t.close();
            
            assertTrue(t.isClosed());
        }
        
        @Test
        @DisplayName("double close is safe")
        void doubleCloseIsSafe() {
            LpmTableIPv6 t = LpmTableIPv6.create();
            t.close();
            assertDoesNotThrow(t::close);
        }
        
        @Test
        @DisplayName("operations throw after close")
        void operationsThrowAfterClose() {
            table.close();
            
            assertThrows(IllegalStateException.class, () -> 
                table.insert("2001:db8::/32", 100));
            assertThrows(IllegalStateException.class, () -> 
                table.lookup("2001:db8::1"));
            assertThrows(IllegalStateException.class, () -> 
                table.delete("2001:db8::/32"));
        }
        
        @Test
        @DisplayName("works with try-with-resources")
        void worksWithTryWithResources() {
            LpmTableIPv6 outerRef;
            try (LpmTableIPv6 t = LpmTableIPv6.create()) {
                outerRef = t;
                t.insert("2001:db8::/32", 100);
                assertEquals(100, t.lookup("2001:db8::1"));
            }
            assertTrue(outerRef.isClosed());
        }
    }
    
    @Nested
    @DisplayName("Utility methods")
    class UtilityTests {
        
        @Test
        @DisplayName("formatAddress formats correctly")
        void formatAddressFormats() throws Exception {
            byte[] addr = InetAddress.getByName("2001:db8::1").getAddress();
            String formatted = LpmTableIPv6.formatAddress(addr);
            assertNotNull(formatted);
            assertTrue(formatted.contains("2001"));
            assertTrue(formatted.contains("db8"));
        }
        
        @Test
        @DisplayName("toString includes useful info")
        void toStringIsUseful() {
            String str = table.toString();
            assertTrue(str.contains("LpmTableIPv6"));
            assertTrue(str.contains("algorithm"));
        }
    }
}
