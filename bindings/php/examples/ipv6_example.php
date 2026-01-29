<?php
/**
 * ipv6_example.php - IPv6 routing with liblpm
 *
 * This example demonstrates IPv6 routing table operations including
 * various address formats and common ISP/enterprise prefix sizes.
 *
 * Usage:
 *   php -d extension=liblpm.so ipv6_example.php
 */

echo "=== liblpm PHP Extension - IPv6 Example ===\n\n";

if (!extension_loaded('liblpm')) {
    die("Error: liblpm extension is not loaded.\n");
}

// ============================================================================
// Create IPv6 table
// ============================================================================
echo "--- Creating IPv6 Routing Table ---\n";
$table = new LpmTableIPv6();
echo "Table created with Wide-16 algorithm (optimized for /48 allocations)\n\n";

// ============================================================================
// Insert common IPv6 prefix types
// ============================================================================
echo "--- Inserting Routes ---\n";

$routes = [
    // Default route
    ["::/0", 1, "Default route"],
    
    // Global unicast (2000::/3)
    ["2000::/3", 100, "Global unicast aggregate"],
    
    // Documentation prefix (RFC 3849)
    ["2001:db8::/32", 200, "Documentation prefix"],
    
    // ISP allocation examples
    ["2001:db8:1::/48", 210, "ISP allocation /48"],
    ["2001:db8:2::/48", 220, "Another ISP /48"],
    
    // Customer site allocations
    ["2001:db8:1:1::/64", 211, "Customer subnet /64"],
    ["2001:db8:1:2::/64", 212, "Another customer subnet"],
    
    // Unique Local Addresses (RFC 4193)
    ["fc00::/7", 300, "Unique local addresses"],
    ["fd00::/8", 310, "Locally assigned ULA"],
    
    // Link-local (fe80::/10)
    ["fe80::/10", 400, "Link-local addresses"],
    
    // Multicast (ff00::/8)
    ["ff00::/8", 500, "Multicast addresses"],
    ["ff02::/16", 510, "Link-local multicast"],
];

foreach ($routes as [$prefix, $nextHop, $description]) {
    if ($table->insert($prefix, $nextHop)) {
        printf("  %-25s -> %3d (%s)\n", $prefix, $nextHop, $description);
    }
}
echo "\n";

// ============================================================================
// Test lookups with various address formats
// ============================================================================
echo "--- Address Format Tests ---\n";

$testAddresses = [
    // Full address format
    ["2001:0db8:0001:0001:0000:0000:0000:0001", "Full format"],
    
    // Compressed format
    ["2001:db8:1:1::1", "Compressed"],
    
    // Leading zeros omitted
    ["2001:db8:1:2::1", "Different subnet"],
    
    // Link-local
    ["fe80::1", "Link-local"],
    ["fe80::1234:5678:abcd:ef01", "Link-local with interface ID"],
    
    // Multicast
    ["ff02::1", "All-nodes multicast"],
    ["ff02::fb", "mDNS multicast"],
    
    // ULA
    ["fd12:3456:789a::1", "ULA address"],
    
    // Global unicast
    ["2607:f8b0:4000::1", "Google IPv6"],
    ["2606:4700:4700::1111", "Cloudflare DNS"],
    
    // Loopback
    ["::1", "Loopback"],
];

foreach ($testAddresses as [$addr, $description]) {
    $result = $table->lookup($addr);
    if ($result === false) {
        printf("  %-40s -> NO MATCH (%s)\n", $addr, $description);
    } else {
        printf("  %-40s -> %3d (%s)\n", $addr, $result, $description);
    }
}
echo "\n";

// ============================================================================
// Longest prefix match demonstration
// ============================================================================
echo "--- Longest Prefix Match ---\n";
echo "Testing address 2001:db8:1:1::cafe\n";

$prefixes = [
    "2000::/3" => "Global unicast",
    "2001:db8::/32" => "Documentation",
    "2001:db8:1::/48" => "ISP allocation",
    "2001:db8:1:1::/64" => "Customer subnet",
];

$testAddr = "2001:db8:1:1::cafe";
$result = $table->lookup($testAddr);

echo "  Matching prefixes (most to least specific):\n";
foreach (array_reverse($prefixes, true) as $prefix => $desc) {
    printf("    %-25s (%s)\n", $prefix, $desc);
}
echo "  Result: next hop $result (matches /64 - most specific)\n\n";

// ============================================================================
// Batch IPv6 lookups
// ============================================================================
echo "--- Batch IPv6 Lookups ---\n";

$batchAddresses = [
    "2001:db8::1",
    "2001:db8:1::1",
    "2001:db8:1:1::1",
    "2001:db8:2::1",
    "fd00::1",
    "fe80::1",
    "ff02::1",
    "2607:f8b0::1",
];

$results = $table->lookupBatch($batchAddresses);

foreach ($batchAddresses as $i => $addr) {
    $nh = $results[$i];
    printf("  %-25s -> %s\n", $addr, $nh === false ? "NO MATCH" : $nh);
}
echo "\n";

// ============================================================================
// Algorithm comparison
// ============================================================================
echo "--- Algorithm Comparison ---\n";

// Wide-16 (default)
$tableWide = new LpmTableIPv6(LpmTableIPv6::ALGO_WIDE16);
$tableWide->insert("2001:db8::/32", 100);
$tableWide->insert("2001:db8:1::/48", 200);
echo "Wide-16 lookup: " . $tableWide->lookup("2001:db8:1::1") . "\n";
$tableWide->close();

// 8-stride
$table8 = new LpmTableIPv6(LpmTableIPv6::ALGO_8STRIDE);
$table8->insert("2001:db8::/32", 100);
$table8->insert("2001:db8:1::/48", 200);
echo "8-stride lookup: " . $table8->lookup("2001:db8:1::1") . "\n";
$table8->close();

echo "\nBoth algorithms produce the same result, but:\n";
echo "  - Wide-16: Optimized for /48 allocations (fewer memory accesses)\n";
echo "  - 8-stride: More memory efficient for sparse tables\n\n";

// ============================================================================
// Clean up
// ============================================================================
$table->close();
echo "=== Example Complete ===\n";
