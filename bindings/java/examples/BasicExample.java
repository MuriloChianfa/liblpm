/*
 * Copyright (c) 2024 Murilo Chianfa
 * 
 * Basic Example - liblpm Java Bindings
 * 
 * Demonstrates basic usage of the LpmTableIPv4 class for IP routing lookups.
 */
package com.github.murilochianfa.liblpm.examples;

import com.github.murilochianfa.liblpm.LpmTableIPv4;
import com.github.murilochianfa.liblpm.LpmTableIPv6;
import com.github.murilochianfa.liblpm.LpmTable;
import com.github.murilochianfa.liblpm.NextHop;

/**
 * Basic example demonstrating liblpm Java bindings.
 */
public class BasicExample {
    
    public static void main(String[] args) {
        System.out.println("liblpm Java Bindings - Basic Example");
        System.out.println("Version: " + LpmTable.getVersion());
        System.out.println();
        
        // IPv4 example
        ipv4Example();
        
        System.out.println();
        
        // IPv6 example
        ipv6Example();
    }
    
    private static void ipv4Example() {
        System.out.println("=== IPv4 Routing Table Example ===");
        
        // Create an IPv4 routing table using try-with-resources
        // The table is automatically closed when the block exits
        try (LpmTableIPv4 table = LpmTableIPv4.create()) {
            
            // Insert routes using CIDR notation (convenient API)
            table.insert("0.0.0.0/0", 1);        // Default route
            table.insert("10.0.0.0/8", 100);     // Private class A
            table.insert("192.168.0.0/16", 200); // Private class C
            table.insert("192.168.1.0/24", 201); // More specific
            table.insert("8.8.8.0/24", 300);     // Google DNS network
            
            System.out.println("Inserted 5 routes");
            System.out.println();
            
            // Perform lookups - longest prefix match (LPM)
            String[] testAddresses = {
                "192.168.1.100",  // Matches /24 -> 201
                "192.168.2.100",  // Matches /16 -> 200
                "10.1.2.3",       // Matches /8 -> 100
                "8.8.8.8",        // Matches /24 -> 300
                "1.2.3.4"         // Matches default -> 1
            };
            
            System.out.println("Lookup results:");
            for (String addr : testAddresses) {
                int nextHop = table.lookup(addr);
                
                if (NextHop.isValid(nextHop)) {
                    System.out.printf("  %-16s -> next hop: %d%n", addr, nextHop);
                } else {
                    System.out.printf("  %-16s -> no route%n", addr);
                }
            }
            
            System.out.println();
            
            // Delete a route
            boolean deleted = table.delete("192.168.1.0/24");
            System.out.println("Deleted 192.168.1.0/24: " + deleted);
            
            // Now 192.168.1.100 falls back to /16
            int result = table.lookup("192.168.1.100");
            System.out.printf("192.168.1.100 now -> next hop: %d (was 201)%n", result);
            
        } // Table automatically closed here
    }
    
    private static void ipv6Example() {
        System.out.println("=== IPv6 Routing Table Example ===");
        
        try (LpmTableIPv6 table = LpmTableIPv6.create()) {
            
            // Insert routes
            table.insert("::/0", 1);                    // Default route
            table.insert("2001:db8::/32", 100);         // Documentation prefix
            table.insert("2001:db8:1234::/48", 101);    // More specific
            table.insert("fc00::/7", 200);              // Unique local addresses
            
            System.out.println("Inserted 4 routes");
            System.out.println();
            
            // Perform lookups
            String[] testAddresses = {
                "2001:db8:1234::1",    // Matches /48 -> 101
                "2001:db8:5678::1",    // Matches /32 -> 100
                "fd12:3456:7890::1",   // Matches fc00::/7 -> 200
                "2607:f8b0:4004::1"    // Matches default -> 1
            };
            
            System.out.println("Lookup results:");
            for (String addr : testAddresses) {
                int nextHop = table.lookup(addr);
                
                if (NextHop.isValid(nextHop)) {
                    System.out.printf("  %-25s -> next hop: %d%n", addr, nextHop);
                } else {
                    System.out.printf("  %-25s -> no route%n", addr);
                }
            }
        }
    }
}
