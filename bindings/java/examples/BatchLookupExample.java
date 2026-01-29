/*
 * Copyright (c) 2024 Murilo Chianfa
 * 
 * Batch Lookup Example - liblpm Java Bindings
 * 
 * Demonstrates high-performance batch lookups for processing large numbers
 * of addresses efficiently.
 */
package com.github.murilochianfa.liblpm.examples;

import com.github.murilochianfa.liblpm.LpmTableIPv4;
import com.github.murilochianfa.liblpm.LpmTableIPv6;
import com.github.murilochianfa.liblpm.NextHop;

import java.util.Random;

/**
 * Batch lookup example demonstrating high-performance operations.
 */
public class BatchLookupExample {
    
    private static final int NUM_ROUTES = 1000;
    private static final int NUM_LOOKUPS = 100_000;
    
    public static void main(String[] args) {
        System.out.println("liblpm Java Bindings - Batch Lookup Example");
        System.out.println();
        
        // IPv4 batch lookup
        ipv4BatchExample();
        
        System.out.println();
        
        // IPv6 batch lookup
        ipv6BatchExample();
    }
    
    private static void ipv4BatchExample() {
        System.out.println("=== IPv4 Batch Lookup Example ===");
        
        try (LpmTableIPv4 table = LpmTableIPv4.create()) {
            Random random = new Random(42);  // Fixed seed for reproducibility
            
            // Insert random routes
            System.out.printf("Inserting %d random routes...%n", NUM_ROUTES);
            long insertStart = System.nanoTime();
            
            for (int i = 0; i < NUM_ROUTES; i++) {
                byte[] prefix = new byte[] {
                    (byte) random.nextInt(256),
                    (byte) random.nextInt(256),
                    0, 0
                };
                int prefixLen = 8 + random.nextInt(17);  // /8 to /24
                table.insert(prefix, prefixLen, i);
            }
            
            // Add default route
            table.insert("0.0.0.0/0", -2);
            
            long insertTime = System.nanoTime() - insertStart;
            System.out.printf("Insert time: %.2f ms%n", insertTime / 1_000_000.0);
            System.out.println();
            
            // Generate random addresses for lookup
            int[] addresses = new int[NUM_LOOKUPS];
            for (int i = 0; i < NUM_LOOKUPS; i++) {
                addresses[i] = random.nextInt();
            }
            
            // Method 1: Single lookups (baseline)
            System.out.printf("Performing %,d single lookups...%n", NUM_LOOKUPS);
            long singleStart = System.nanoTime();
            
            int matches = 0;
            for (int addr : addresses) {
                int result = table.lookup(addr);
                if (NextHop.isValid(result)) {
                    matches++;
                }
            }
            
            long singleTime = System.nanoTime() - singleStart;
            double singleRate = NUM_LOOKUPS / (singleTime / 1_000_000_000.0);
            System.out.printf("Single lookup: %.2f ms (%.2f M lookups/sec), %d matches%n",
                singleTime / 1_000_000.0, singleRate / 1_000_000.0, matches);
            
            // Method 2: Batch lookup (optimized)
            System.out.printf("Performing %,d batch lookups...%n", NUM_LOOKUPS);
            int[] results = new int[NUM_LOOKUPS];
            
            long batchStart = System.nanoTime();
            table.lookupBatchFast(addresses, results);
            long batchTime = System.nanoTime() - batchStart;
            
            // Count matches
            matches = 0;
            for (int result : results) {
                if (NextHop.isValid(result)) {
                    matches++;
                }
            }
            
            double batchRate = NUM_LOOKUPS / (batchTime / 1_000_000_000.0);
            System.out.printf("Batch lookup:  %.2f ms (%.2f M lookups/sec), %d matches%n",
                batchTime / 1_000_000.0, batchRate / 1_000_000.0, matches);
            
            double speedup = (double) singleTime / batchTime;
            System.out.printf("Batch speedup: %.2fx%n", speedup);
        }
    }
    
    private static void ipv6BatchExample() {
        System.out.println("=== IPv6 Batch Lookup Example ===");
        
        try (LpmTableIPv6 table = LpmTableIPv6.create()) {
            Random random = new Random(42);
            
            // Insert random routes
            System.out.printf("Inserting %d random routes...%n", NUM_ROUTES);
            long insertStart = System.nanoTime();
            
            for (int i = 0; i < NUM_ROUTES; i++) {
                byte[] prefix = new byte[16];
                // Generate random prefix in 2001:db8::/32 range
                prefix[0] = 0x20;
                prefix[1] = 0x01;
                prefix[2] = 0x0d;
                prefix[3] = (byte) 0xb8;
                prefix[4] = (byte) random.nextInt(256);
                prefix[5] = (byte) random.nextInt(256);
                
                int prefixLen = 32 + random.nextInt(17);  // /32 to /48
                table.insert(prefix, prefixLen, i);
            }
            
            // Add default route
            table.insert("::/0", -2);
            
            long insertTime = System.nanoTime() - insertStart;
            System.out.printf("Insert time: %.2f ms%n", insertTime / 1_000_000.0);
            System.out.println();
            
            // Generate random addresses for lookup
            int numLookups = NUM_LOOKUPS / 10;  // Fewer for IPv6 (slower)
            byte[][] addresses = new byte[numLookups][16];
            for (int i = 0; i < numLookups; i++) {
                addresses[i][0] = 0x20;
                addresses[i][1] = 0x01;
                addresses[i][2] = 0x0d;
                addresses[i][3] = (byte) 0xb8;
                for (int j = 4; j < 16; j++) {
                    addresses[i][j] = (byte) random.nextInt(256);
                }
            }
            
            // Batch lookup
            System.out.printf("Performing %,d batch lookups...%n", numLookups);
            
            long batchStart = System.nanoTime();
            int[] results = table.lookupBatch(addresses);
            long batchTime = System.nanoTime() - batchStart;
            
            // Count matches
            int matches = 0;
            for (int result : results) {
                if (NextHop.isValid(result)) {
                    matches++;
                }
            }
            
            double batchRate = numLookups / (batchTime / 1_000_000_000.0);
            System.out.printf("Batch lookup: %.2f ms (%.2f M lookups/sec), %d matches%n",
                batchTime / 1_000_000.0, batchRate / 1_000_000.0, matches);
        }
    }
}
