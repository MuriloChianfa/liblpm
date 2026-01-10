// basic_example.cpp - Demonstrates C++ wrapper usage
// 
// This example shows both the fast byte-array API and the convenient
// string-based API for the liblpm C++ wrapper.

#include <liblpm>  // Clean include!
#include <iostream>
#include <iomanip>

void print_separator(const char* title) {
    std::cout << "\n========== " << title << " ==========\n";
}

int main() {
    print_separator("Basic IPv4 Example");
    
    // Create IPv4 routing table
    liblpm::LpmTableIPv4 table;
    
    // Insert routes using string API (convenient but slightly slower)
    table.insert("192.168.0.0/16", 100);
    table.insert("10.0.0.0/8", 200);
    table.insert("172.16.0.0/12", 300);
    table.insert("0.0.0.0/0", 1);  // Default route
    
    std::cout << "Inserted " << table.size() << " routes\n";
    
    // Lookup with string API
    print_separator("String-based Lookups");
    
    const char* test_addrs[] = {
        "192.168.1.1",
        "10.5.5.5",
        "172.16.100.1",
        "8.8.8.8",
    };
    
    for (const char* addr : test_addrs) {
        uint32_t nh = table.lookup(addr);
        if (nh != LPM_INVALID_NEXT_HOP) {
            std::cout << "  " << std::setw(15) << addr 
                      << " -> next_hop: " << nh << "\n";
        } else {
            std::cout << "  " << std::setw(15) << addr 
                      << " -> NOT FOUND\n";
        }
    }
    
    // FAST PATH: Direct byte array access (zero parsing overhead)
    print_separator("Fast Path: Byte Array API");
    
    uint8_t addr1[4] = {192, 168, 1, 1};
    uint8_t addr2[4] = {10, 1, 2, 3};
    
    uint32_t nh1 = table.lookup(addr1);  // Fully inlined!
    uint32_t nh2 = table.lookup(addr2);
    
    std::cout << "  192.168.1.1 -> next_hop: " << nh1 << "\n";
    std::cout << "  10.1.2.3    -> next_hop: " << nh2 << "\n";
    
    // Batch lookups (zero-copy with spans)
    print_separator("Batch Lookups (Zero-Copy)");
    
    uint8_t batch_addr1[4] = {192, 168, 1, 1};
    uint8_t batch_addr2[4] = {10, 1, 2, 3};
    uint8_t batch_addr3[4] = {172, 16, 1, 1};
    uint8_t batch_addr4[4] = {8, 8, 8, 8};
    
    const uint8_t* addrs[] = {
        batch_addr1,
        batch_addr2,
        batch_addr3,
        batch_addr4
    };
    uint32_t results[4];
    
    table.lookup_batch(liblpm::span<const uint8_t* const>(addrs, 4), 
                       liblpm::span<uint32_t>(results, 4));
    
    const char* addr_strs[] = {
        "192.168.1.1",
        "10.1.2.3",
        "172.16.1.1",
        "8.8.8.8"
    };
    
    for (size_t i = 0; i < 4; ++i) {
        if (results[i] != LPM_INVALID_NEXT_HOP) {
            std::cout << "  " << std::setw(15) << addr_strs[i] 
                      << " -> next_hop: " << results[i] << "\n";
        }
    }
    
    // Delete a route
    print_separator("Route Deletion");
    
    std::cout << "Deleting 192.168.0.0/16...\n";
    table.remove("192.168.0.0/16");
    
    // Lookup again
    uint32_t nh_after = table.lookup("192.168.1.1");
    std::cout << "192.168.1.1 now -> next_hop: " << nh_after 
              << " (should be default route: 1)\n";
    
    // IPv6 Example
    print_separator("IPv6 Example");
    
    liblpm::LpmTableIPv6 table_v6;
    
    table_v6.insert("2001:db8::/32", 1000);
    table_v6.insert("2001:db8:1::/48", 2000);
    table_v6.insert("::/0", 1);  // Default route
    
    std::cout << "Inserted " << table_v6.size() << " IPv6 routes\n";
    
    const char* test_v6_addrs[] = {
        "2001:db8::1",
        "2001:db8:1::1",
        "2001:db8:2::1",
        "2606:4700:4700::1111",
    };
    
    for (const char* addr : test_v6_addrs) {
        uint32_t nh = table_v6.lookup(addr);
        if (nh != LPM_INVALID_NEXT_HOP) {
            std::cout << "  " << std::setw(25) << addr 
                      << " -> next_hop: " << nh << "\n";
        }
    }
    
    print_separator("Done");
    std::cout << "C++ wrapper demo completed successfully!\n";
    std::cout << "Performance: Byte array lookups are ~13-18ns (same as C)\n";
    std::cout << "             String lookups are ~18-28ns (+5-10ns parsing)\n";
    
    return 0;
}

