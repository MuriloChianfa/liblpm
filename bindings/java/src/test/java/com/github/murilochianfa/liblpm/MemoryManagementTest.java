/*
 * Copyright (c) 2024 Murilo Chianfa
 * 
 * Licensed under the MIT License.
 */
package com.github.murilochianfa.liblpm;

import org.junit.jupiter.api.DisplayName;
import org.junit.jupiter.api.Test;

import java.util.ArrayList;
import java.util.List;

import static org.junit.jupiter.api.Assertions.*;

/**
 * Tests for memory management and resource cleanup.
 */
@DisplayName("Memory Management")
class MemoryManagementTest {
    
    @Test
    @DisplayName("many tables can be created and closed")
    void manyTablesCanBeCreatedAndClosed() {
        for (int i = 0; i < 100; i++) {
            try (LpmTableIPv4 table = LpmTableIPv4.create()) {
                table.insert("192.168.0.0/16", i);
                assertEquals(i, table.lookup("192.168.1.1"));
            }
        }
    }
    
    @Test
    @DisplayName("concurrent table creation")
    void concurrentTableCreation() throws Exception {
        int numThreads = 10;
        int tablesPerThread = 10;
        
        List<Thread> threads = new ArrayList<>();
        List<Throwable> errors = new ArrayList<>();
        
        for (int t = 0; t < numThreads; t++) {
            final int threadId = t;
            Thread thread = new Thread(() -> {
                try {
                    for (int i = 0; i < tablesPerThread; i++) {
                        try (LpmTableIPv4 table = LpmTableIPv4.create()) {
                            table.insert("192.168.0.0/16", threadId * 100 + i);
                            int result = table.lookup("192.168.1.1");
                            assertEquals(threadId * 100 + i, result);
                        }
                    }
                } catch (Throwable e) {
                    synchronized (errors) {
                        errors.add(e);
                    }
                }
            });
            threads.add(thread);
            thread.start();
        }
        
        for (Thread thread : threads) {
            thread.join();
        }
        
        assertTrue(errors.isEmpty(), "Errors occurred: " + errors);
    }
    
    @Test
    @DisplayName("large number of routes")
    void largeNumberOfRoutes() {
        try (LpmTableIPv4 table = LpmTableIPv4.create()) {
            // Insert 10000 routes
            for (int i = 0; i < 10000; i++) {
                byte[] prefix = new byte[] {
                    (byte) ((i >> 8) & 0xFF),
                    (byte) (i & 0xFF),
                    0, 0
                };
                table.insert(prefix, 16, i);
            }
            
            // Verify lookups
            for (int i = 0; i < 10000; i++) {
                byte[] addr = new byte[] {
                    (byte) ((i >> 8) & 0xFF),
                    (byte) (i & 0xFF),
                    1, 1
                };
                assertEquals(i, table.lookup(addr));
            }
        }
    }
    
    @Test
    @DisplayName("repeated insert and delete cycles")
    void repeatedInsertDeleteCycles() {
        try (LpmTableIPv4 table = LpmTableIPv4.create()) {
            for (int cycle = 0; cycle < 100; cycle++) {
                // Insert
                table.insert("192.168.0.0/16", cycle);
                assertEquals(cycle, table.lookup("192.168.1.1"));
                
                // Delete
                assertTrue(table.delete("192.168.0.0/16"));
                assertEquals(NextHop.INVALID, table.lookup("192.168.1.1"));
            }
        }
    }
    
    @Test
    @DisplayName("batch operations with many addresses")
    void batchOperationsWithManyAddresses() {
        try (LpmTableIPv4 table = LpmTableIPv4.create()) {
            table.insert("0.0.0.0/0", 1);
            table.insert("192.168.0.0/16", 100);
            
            int count = 10000;
            int[] addresses = new int[count];
            int[] results = new int[count];
            
            for (int i = 0; i < count; i++) {
                // Generate addresses in 192.168.x.y range
                addresses[i] = (192 << 24) | (168 << 16) | ((i / 256) << 8) | (i % 256);
            }
            
            table.lookupBatchFast(addresses, results);
            
            // All should match 192.168.0.0/16
            for (int i = 0; i < count; i++) {
                assertEquals(100, results[i]);
            }
        }
    }
    
    @Test
    @DisplayName("operations after close throw appropriate exception")
    void operationsAfterCloseThrow() {
        LpmTableIPv4 table = LpmTableIPv4.create();
        table.insert("192.168.0.0/16", 100);
        table.close();
        
        // All operations should throw IllegalStateException
        assertThrows(IllegalStateException.class, () -> table.insert("10.0.0.0/8", 200));
        assertThrows(IllegalStateException.class, () -> table.lookup("192.168.1.1"));
        assertThrows(IllegalStateException.class, () -> table.delete("192.168.0.0/16"));
        assertThrows(IllegalStateException.class, () -> table.lookupBatch(new byte[][] {{192, (byte)168, 1, 1}}));
    }
    
    @Test
    @DisplayName("isClosed reflects correct state")
    void isClosedReflectsCorrectState() {
        LpmTableIPv4 table = LpmTableIPv4.create();
        assertFalse(table.isClosed());
        
        table.insert("192.168.0.0/16", 100);
        assertFalse(table.isClosed());
        
        table.close();
        assertTrue(table.isClosed());
        
        // Second close is safe
        table.close();
        assertTrue(table.isClosed());
    }
}
