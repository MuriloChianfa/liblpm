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

import java.net.Inet4Address;
import java.net.InetAddress;

import static org.junit.jupiter.api.Assertions.*;

/**
 * Unit tests for LpmTableIPv4.
 */
@DisplayName("LpmTableIPv4")
class LpmTableIPv4Test {
    
    private LpmTableIPv4 table;
    
    @BeforeEach
    void setUp() {
        table = LpmTableIPv4.create();
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
        @DisplayName("creates with default algorithm (DIR24)")
        void createsWithDefaultAlgorithm() {
            try (LpmTableIPv4 t = LpmTableIPv4.create()) {
                assertNotNull(t);
                assertEquals(Algorithm.DIR24, t.getAlgorithm());
                assertFalse(t.isIPv6());
                assertFalse(t.isClosed());
            }
        }
        
        @Test
        @DisplayName("creates with STRIDE8 algorithm")
        void createsWithStride8() {
            try (LpmTableIPv4 t = LpmTableIPv4.create(Algorithm.STRIDE8)) {
                assertNotNull(t);
                assertEquals(Algorithm.STRIDE8, t.getAlgorithm());
            }
        }
        
        @Test
        @DisplayName("rejects WIDE16 algorithm for IPv4")
        void rejectsWide16() {
            assertThrows(IllegalArgumentException.class, () -> {
                LpmTableIPv4.create(Algorithm.WIDE16);
            });
        }
        
        @Test
        @DisplayName("rejects null algorithm")
        void rejectsNullAlgorithm() {
            assertThrows(NullPointerException.class, () -> {
                LpmTableIPv4.create(null);
            });
        }
    }
    
    @Nested
    @DisplayName("Insert operations")
    class InsertTests {
        
        @Test
        @DisplayName("inserts prefix using byte array")
        void insertsByteArray() {
            byte[] prefix = new byte[] {(byte)192, (byte)168, 0, 0};
            assertDoesNotThrow(() -> table.insert(prefix, 16, 100));
        }
        
        @Test
        @DisplayName("inserts prefix using CIDR string")
        void insertsCidrString() {
            assertDoesNotThrow(() -> table.insert("192.168.0.0/16", 100));
            assertDoesNotThrow(() -> table.insert("10.0.0.0/8", 200));
            assertDoesNotThrow(() -> table.insert("0.0.0.0/0", 1));
        }
        
        @Test
        @DisplayName("inserts prefix using InetAddress")
        void insertsInetAddress() throws Exception {
            InetAddress addr = InetAddress.getByName("192.168.0.0");
            assertDoesNotThrow(() -> table.insert(addr, 16, 100));
        }
        
        @Test
        @DisplayName("inserts /32 host route")
        void insertsHostRoute() {
            assertDoesNotThrow(() -> table.insert("192.168.1.1/32", 999));
            assertEquals(999, table.lookup("192.168.1.1"));
        }
        
        @Test
        @DisplayName("inserts default route")
        void insertsDefaultRoute() {
            table.insert("0.0.0.0/0", 1);
            assertEquals(1, table.lookup("8.8.8.8"));
        }
        
        @Test
        @DisplayName("rejects invalid prefix length")
        void rejectsInvalidPrefixLength() {
            byte[] prefix = new byte[] {(byte)192, (byte)168, 0, 0};
            
            assertThrows(InvalidPrefixException.class, () -> table.insert(prefix, -1, 100));
            assertThrows(InvalidPrefixException.class, () -> table.insert(prefix, 33, 100));
        }
        
        @Test
        @DisplayName("rejects invalid byte array length")
        void rejectsInvalidByteArrayLength() {
            byte[] tooShort = new byte[] {(byte)192, (byte)168};
            byte[] tooLong = new byte[16];
            
            assertThrows(InvalidPrefixException.class, () -> table.insert(tooShort, 16, 100));
            assertThrows(InvalidPrefixException.class, () -> table.insert(tooLong, 16, 100));
        }
        
        @Test
        @DisplayName("rejects null prefix")
        void rejectsNullPrefix() {
            assertThrows(NullPointerException.class, () -> table.insert((String)null, 100));
            assertThrows(NullPointerException.class, () -> table.insert((InetAddress)null, 16, 100));
            assertThrows(InvalidPrefixException.class, () -> table.insert((byte[])null, 16, 100));
        }
        
        @Test
        @DisplayName("rejects invalid CIDR format")
        void rejectsInvalidCidr() {
            assertThrows(InvalidPrefixException.class, () -> table.insert("192.168.0.0", 100));
            assertThrows(InvalidPrefixException.class, () -> table.insert("192.168.0.0/abc", 100));
        }
    }
    
    @Nested
    @DisplayName("Lookup operations")
    class LookupTests {
        
        @BeforeEach
        void insertRoutes() {
            table.insert("192.168.0.0/16", 100);
            table.insert("192.168.1.0/24", 101);
            table.insert("10.0.0.0/8", 200);
            table.insert("0.0.0.0/0", 1);
        }
        
        @Test
        @DisplayName("performs longest prefix match")
        void longestPrefixMatch() {
            // Most specific wins
            assertEquals(101, table.lookup("192.168.1.1"));
            assertEquals(100, table.lookup("192.168.2.1"));
            assertEquals(200, table.lookup("10.1.2.3"));
            assertEquals(1, table.lookup("8.8.8.8"));
        }
        
        @Test
        @DisplayName("lookup using byte array")
        void lookupByteArray() {
            byte[] addr = new byte[] {(byte)192, (byte)168, 1, 1};
            assertEquals(101, table.lookup(addr));
        }
        
        @Test
        @DisplayName("lookup using InetAddress")
        void lookupInetAddress() throws Exception {
            InetAddress addr = InetAddress.getByName("192.168.1.1");
            assertEquals(101, table.lookup(addr));
        }
        
        @Test
        @DisplayName("lookup using int representation")
        void lookupInt() {
            // 192.168.1.1 = 0xC0A80101
            int addr = (192 << 24) | (168 << 16) | (1 << 8) | 1;
            assertEquals(101, table.lookup(addr));
        }
        
        @Test
        @DisplayName("returns INVALID for no match (without default route)")
        void returnsInvalidForNoMatch() {
            try (LpmTableIPv4 emptyTable = LpmTableIPv4.create()) {
                int result = emptyTable.lookup("192.168.1.1");
                assertEquals(NextHop.INVALID, result);
                assertTrue(NextHop.isInvalid(result));
            }
        }
        
        @Test
        @DisplayName("rejects invalid address")
        void rejectsInvalidAddress() {
            assertThrows(InvalidPrefixException.class, () -> table.lookup(new byte[3]));
            assertThrows(InvalidPrefixException.class, () -> table.lookup(new byte[16]));
        }
    }
    
    @Nested
    @DisplayName("Delete operations")
    class DeleteTests {
        
        @BeforeEach
        void insertRoutes() {
            table.insert("192.168.0.0/16", 100);
            table.insert("10.0.0.0/8", 200);
        }
        
        @Test
        @DisplayName("deletes existing prefix")
        void deletesExisting() {
            assertTrue(table.delete("192.168.0.0/16"));
            assertEquals(NextHop.INVALID, table.lookup("192.168.1.1"));
        }
        
        @Test
        @DisplayName("returns false for non-existent prefix")
        void returnsFalseForNonExistent() {
            assertFalse(table.delete("172.16.0.0/12"));
        }
        
        @Test
        @DisplayName("deletes using byte array")
        void deletesByteArray() {
            byte[] prefix = new byte[] {(byte)192, (byte)168, 0, 0};
            assertTrue(table.delete(prefix, 16));
        }
        
        @Test
        @DisplayName("deletes using InetAddress")
        void deletesInetAddress() throws Exception {
            InetAddress addr = InetAddress.getByName("192.168.0.0");
            assertTrue(table.delete(addr, 16));
        }
    }
    
    @Nested
    @DisplayName("Batch operations")
    class BatchTests {
        
        @BeforeEach
        void insertRoutes() {
            table.insert("192.168.0.0/16", 100);
            table.insert("10.0.0.0/8", 200);
            table.insert("0.0.0.0/0", 1);
        }
        
        @Test
        @DisplayName("batch lookup with byte arrays")
        void batchLookupByteArrays() {
            byte[][] addresses = {
                new byte[] {(byte)192, (byte)168, 1, 1},
                new byte[] {10, 1, 2, 3},
                new byte[] {8, 8, 8, 8}
            };
            
            int[] results = table.lookupBatch(addresses);
            
            assertEquals(3, results.length);
            assertEquals(100, results[0]);
            assertEquals(200, results[1]);
            assertEquals(1, results[2]);
        }
        
        @Test
        @DisplayName("batch lookup with int arrays")
        void batchLookupIntArrays() {
            int[] addresses = {
                (192 << 24) | (168 << 16) | (1 << 8) | 1,  // 192.168.1.1
                (10 << 24) | (1 << 16) | (2 << 8) | 3,     // 10.1.2.3
                (8 << 24) | (8 << 16) | (8 << 8) | 8       // 8.8.8.8
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
                InetAddress.getByName("192.168.1.1"),
                InetAddress.getByName("10.1.2.3"),
                InetAddress.getByName("8.8.8.8")
            };
            
            int[] results = table.lookupBatch(addresses);
            
            assertEquals(3, results.length);
            assertEquals(100, results[0]);
            assertEquals(200, results[1]);
            assertEquals(1, results[2]);
        }
        
        @Test
        @DisplayName("batch lookup fast with pre-allocated array")
        void batchLookupFast() {
            int[] addresses = {
                (192 << 24) | (168 << 16) | (1 << 8) | 1,
                (10 << 24) | (1 << 16) | (2 << 8) | 3
            };
            int[] results = new int[2];
            
            table.lookupBatchFast(addresses, results);
            
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
        void rejectsUndersizedResults() {
            int[] addresses = {1, 2, 3};
            int[] results = new int[2];
            
            assertThrows(IllegalArgumentException.class, () -> 
                table.lookupBatchFast(addresses, results));
        }
    }
    
    @Nested
    @DisplayName("Resource management")
    class ResourceTests {
        
        @Test
        @DisplayName("close releases resources")
        void closeReleasesResources() {
            LpmTableIPv4 t = LpmTableIPv4.create();
            assertFalse(t.isClosed());
            
            t.close();
            
            assertTrue(t.isClosed());
        }
        
        @Test
        @DisplayName("double close is safe")
        void doubleCloseIsSafe() {
            LpmTableIPv4 t = LpmTableIPv4.create();
            t.close();
            assertDoesNotThrow(t::close);
        }
        
        @Test
        @DisplayName("operations throw after close")
        void operationsThrowAfterClose() {
            table.close();
            
            assertThrows(IllegalStateException.class, () -> 
                table.insert("192.168.0.0/16", 100));
            assertThrows(IllegalStateException.class, () -> 
                table.lookup("192.168.1.1"));
            assertThrows(IllegalStateException.class, () -> 
                table.delete("192.168.0.0/16"));
        }
        
        @Test
        @DisplayName("works with try-with-resources")
        void worksWithTryWithResources() {
            LpmTableIPv4 outerRef;
            try (LpmTableIPv4 t = LpmTableIPv4.create()) {
                outerRef = t;
                t.insert("192.168.0.0/16", 100);
                assertEquals(100, t.lookup("192.168.1.1"));
            }
            assertTrue(outerRef.isClosed());
        }
    }
    
    @Nested
    @DisplayName("Utility methods")
    class UtilityTests {
        
        @Test
        @DisplayName("bytesToInt converts correctly")
        void bytesToIntConverts() {
            byte[] addr = new byte[] {(byte)192, (byte)168, 1, 1};
            int expected = (192 << 24) | (168 << 16) | (1 << 8) | 1;
            assertEquals(expected, LpmTableIPv4.bytesToInt(addr));
        }
        
        @Test
        @DisplayName("intToBytes converts correctly")
        void intToBytesConverts() {
            int addr = (192 << 24) | (168 << 16) | (1 << 8) | 1;
            byte[] expected = new byte[] {(byte)192, (byte)168, 1, 1};
            assertArrayEquals(expected, LpmTableIPv4.intToBytes(addr));
        }
        
        @Test
        @DisplayName("getVersion returns non-null")
        void getVersionReturnsNonNull() {
            String version = LpmTable.getVersion();
            assertNotNull(version);
            assertFalse(version.isEmpty());
        }
        
        @Test
        @DisplayName("toString includes useful info")
        void toStringIsUseful() {
            String str = table.toString();
            assertTrue(str.contains("LpmTableIPv4"));
            assertTrue(str.contains("algorithm"));
        }
    }
}
