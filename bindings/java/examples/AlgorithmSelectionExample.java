/*
 * Copyright (c) 2024 Murilo Chianfa
 * 
 * Algorithm Selection Example - liblpm Java Bindings
 * 
 * Demonstrates different LPM algorithms and their characteristics.
 */
package com.github.murilochianfa.liblpm.examples;

import com.github.murilochianfa.liblpm.Algorithm;
import com.github.murilochianfa.liblpm.LpmTableIPv4;
import com.github.murilochianfa.liblpm.LpmTableIPv6;
import com.github.murilochianfa.liblpm.LpmTable;

import java.util.Random;

/**
 * Example demonstrating algorithm selection for different use cases.
 */
public class AlgorithmSelectionExample {
    
    private static final int NUM_ROUTES = 10_000;
    private static final int NUM_LOOKUPS = 100_000;
    
    public static void main(String[] args) {
        System.out.println("liblpm Java Bindings - Algorithm Selection Example");
        System.out.println("Version: " + LpmTable.getVersion());
        System.out.println();
        
        // Compare IPv4 algorithms
        compareIPv4Algorithms();
        
        System.out.println();
        
        // Compare IPv6 algorithms
        compareIPv6Algorithms();
        
        System.out.println();
        
        // Algorithm recommendations
        printRecommendations();
    }
    
    private static void compareIPv4Algorithms() {
        System.out.println("=== IPv4 Algorithm Comparison ===");
        System.out.println();
        System.out.println("Available IPv4 algorithms:");
        System.out.println("  - DIR24:   DIR-24-8 (default, fastest for typical routing tables)");
        System.out.println("  - STRIDE8: 8-bit stride trie (more memory-efficient for sparse tables)");
        System.out.println();
        
        // Benchmark DIR24
        System.out.println("Benchmarking DIR24 algorithm...");
        benchmarkIPv4(Algorithm.DIR24);
        
        System.out.println();
        
        // Benchmark STRIDE8
        System.out.println("Benchmarking STRIDE8 algorithm...");
        benchmarkIPv4(Algorithm.STRIDE8);
    }
    
    private static void benchmarkIPv4(Algorithm algorithm) {
        try (LpmTableIPv4 table = LpmTableIPv4.create(algorithm)) {
            Random random = new Random(42);
            
            // Insert routes
            long insertStart = System.nanoTime();
            for (int i = 0; i < NUM_ROUTES; i++) {
                byte[] prefix = new byte[] {
                    (byte) random.nextInt(256),
                    (byte) random.nextInt(256),
                    0, 0
                };
                int prefixLen = 8 + random.nextInt(17);
                table.insert(prefix, prefixLen, i);
            }
            table.insert("0.0.0.0/0", -2);
            long insertTime = System.nanoTime() - insertStart;
            
            // Generate lookup addresses
            int[] addresses = new int[NUM_LOOKUPS];
            for (int i = 0; i < NUM_LOOKUPS; i++) {
                addresses[i] = random.nextInt();
            }
            int[] results = new int[NUM_LOOKUPS];
            
            // Benchmark lookups
            long lookupStart = System.nanoTime();
            table.lookupBatchFast(addresses, results);
            long lookupTime = System.nanoTime() - lookupStart;
            
            double insertRate = NUM_ROUTES / (insertTime / 1_000_000_000.0);
            double lookupRate = NUM_LOOKUPS / (lookupTime / 1_000_000_000.0);
            
            System.out.printf("  Algorithm: %s%n", algorithm);
            System.out.printf("  Insert:    %,d routes in %.2f ms (%.2f K inserts/sec)%n",
                NUM_ROUTES, insertTime / 1_000_000.0, insertRate / 1000.0);
            System.out.printf("  Lookup:    %,d lookups in %.2f ms (%.2f M lookups/sec)%n",
                NUM_LOOKUPS, lookupTime / 1_000_000.0, lookupRate / 1_000_000.0);
        }
    }
    
    private static void compareIPv6Algorithms() {
        System.out.println("=== IPv6 Algorithm Comparison ===");
        System.out.println();
        System.out.println("Available IPv6 algorithms:");
        System.out.println("  - WIDE16:  16-bit first stride (default, optimized for /48 allocations)");
        System.out.println("  - STRIDE8: 8-bit stride trie (simpler, good for diverse prefixes)");
        System.out.println();
        
        // Benchmark WIDE16
        System.out.println("Benchmarking WIDE16 algorithm...");
        benchmarkIPv6(Algorithm.WIDE16);
        
        System.out.println();
        
        // Benchmark STRIDE8
        System.out.println("Benchmarking STRIDE8 algorithm...");
        benchmarkIPv6(Algorithm.STRIDE8);
    }
    
    private static void benchmarkIPv6(Algorithm algorithm) {
        try (LpmTableIPv6 table = LpmTableIPv6.create(algorithm)) {
            Random random = new Random(42);
            int numRoutes = NUM_ROUTES / 10;  // Fewer routes for IPv6
            int numLookups = NUM_LOOKUPS / 10;
            
            // Insert routes
            long insertStart = System.nanoTime();
            for (int i = 0; i < numRoutes; i++) {
                byte[] prefix = new byte[16];
                prefix[0] = 0x20;
                prefix[1] = 0x01;
                prefix[2] = 0x0d;
                prefix[3] = (byte) 0xb8;
                prefix[4] = (byte) random.nextInt(256);
                prefix[5] = (byte) random.nextInt(256);
                
                int prefixLen = 32 + random.nextInt(17);
                table.insert(prefix, prefixLen, i);
            }
            table.insert("::/0", -2);
            long insertTime = System.nanoTime() - insertStart;
            
            // Generate lookup addresses
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
            
            // Benchmark lookups
            long lookupStart = System.nanoTime();
            int[] results = table.lookupBatch(addresses);
            long lookupTime = System.nanoTime() - lookupStart;
            
            double insertRate = numRoutes / (insertTime / 1_000_000_000.0);
            double lookupRate = numLookups / (lookupTime / 1_000_000_000.0);
            
            System.out.printf("  Algorithm: %s%n", algorithm);
            System.out.printf("  Insert:    %,d routes in %.2f ms (%.2f K inserts/sec)%n",
                numRoutes, insertTime / 1_000_000.0, insertRate / 1000.0);
            System.out.printf("  Lookup:    %,d lookups in %.2f ms (%.2f M lookups/sec)%n",
                numLookups, lookupTime / 1_000_000.0, lookupRate / 1_000_000.0);
        }
    }
    
    private static void printRecommendations() {
        System.out.println("=== Algorithm Recommendations ===");
        System.out.println();
        System.out.println("IPv4:");
        System.out.println("  - Use DIR24 (default) for:");
        System.out.println("    * BGP routing tables with many prefixes");
        System.out.println("    * High-throughput packet forwarding");
        System.out.println("    * When memory (~64MB) is not a constraint");
        System.out.println();
        System.out.println("  - Use STRIDE8 for:");
        System.out.println("    * Sparse routing tables with few prefixes");
        System.out.println("    * Memory-constrained environments");
        System.out.println("    * Embedded systems");
        System.out.println();
        System.out.println("IPv6:");
        System.out.println("  - Use WIDE16 (default) for:");
        System.out.println("    * Standard IPv6 deployments");
        System.out.println("    * ISP routing tables with /48 allocations");
        System.out.println("    * Optimized performance");
        System.out.println();
        System.out.println("  - Use STRIDE8 for:");
        System.out.println("    * Memory-constrained environments");
        System.out.println("    * Unusual prefix distributions");
        System.out.println();
        System.out.println("Example usage:");
        System.out.println("  // IPv4 with default (DIR24)");
        System.out.println("  LpmTableIPv4 table = LpmTableIPv4.create();");
        System.out.println();
        System.out.println("  // IPv4 with explicit algorithm");
        System.out.println("  LpmTableIPv4 table = LpmTableIPv4.create(Algorithm.STRIDE8);");
        System.out.println();
        System.out.println("  // IPv6 with default (WIDE16)");
        System.out.println("  LpmTableIPv6 table = LpmTableIPv6.create();");
        System.out.println();
        System.out.println("  // IPv6 with explicit algorithm");
        System.out.println("  LpmTableIPv6 table = LpmTableIPv6.create(Algorithm.STRIDE8);");
    }
}
