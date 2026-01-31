<?php
/**
 * procedural_example.php - Procedural API usage
 *
 * This example demonstrates the procedural (function-based) API
 * which is an alternative to the object-oriented API.
 *
 * Usage:
 *   php -d extension=liblpm.so procedural_example.php
 */

echo "=== liblpm PHP Extension - Procedural API Example ===\n\n";

if (!extension_loaded('liblpm')) {
    die("Error: liblpm extension is not loaded.\n");
}

// ============================================================================
// Get library version
// ============================================================================
echo "--- Library Info ---\n";
echo "liblpm version: " . lpm_version() . "\n\n";

// ============================================================================
// Create IPv4 table
// ============================================================================
echo "--- IPv4 Procedural API ---\n";

// Create table with default algorithm (DIR-24-8)
$table = lpm_create_ipv4();
if ($table === false) {
    die("Failed to create IPv4 table\n");
}
echo "IPv4 table created\n";

// Create table with specific algorithm
$table2 = lpm_create_ipv4(LPM_ALGO_IPV4_8STRIDE);
if ($table2 === false) {
    die("Failed to create IPv4 8-stride table\n");
}
echo "IPv4 8-stride table created\n";

// Insert routes
if (lpm_insert($table, "192.168.0.0/16", 100)) {
    echo "Inserted: 192.168.0.0/16 -> 100\n";
}
if (lpm_insert($table, "10.0.0.0/8", 200)) {
    echo "Inserted: 10.0.0.0/8 -> 200\n";
}
if (lpm_insert($table, "0.0.0.0/0", 1)) {
    echo "Inserted: 0.0.0.0/0 -> 1 (default)\n";
}

// Lookup
echo "\nLookups:\n";
$nh = lpm_lookup($table, "192.168.1.1");
echo "  192.168.1.1 -> " . ($nh === false ? "NO MATCH" : $nh) . "\n";

$nh = lpm_lookup($table, "10.0.0.1");
echo "  10.0.0.1 -> " . ($nh === false ? "NO MATCH" : $nh) . "\n";

$nh = lpm_lookup($table, "8.8.8.8");
echo "  8.8.8.8 -> " . ($nh === false ? "NO MATCH" : $nh) . "\n";

// Batch lookup
echo "\nBatch lookup:\n";
$results = lpm_lookup_batch($table, ["192.168.1.1", "10.0.0.1", "8.8.8.8"]);
print_r($results);

// Get size
echo "Table size: " . lpm_size($table) . "\n";

// Delete
if (lpm_delete($table, "192.168.0.0/16")) {
    echo "Deleted: 192.168.0.0/16\n";
}

// Verify deletion
$nh = lpm_lookup($table, "192.168.1.1");
echo "After delete: 192.168.1.1 -> " . ($nh === false ? "NO MATCH" : $nh) . "\n";

// Close tables
lpm_close($table);
lpm_close($table2);
echo "Tables closed\n\n";

// ============================================================================
// Create IPv6 table
// ============================================================================
echo "--- IPv6 Procedural API ---\n";

$table6 = lpm_create_ipv6();
if ($table6 === false) {
    die("Failed to create IPv6 table\n");
}
echo "IPv6 table created\n";

// Insert routes
if (lpm_insert($table6, "2001:db8::/32", 1000)) {
    echo "Inserted: 2001:db8::/32 -> 1000\n";
}
if (lpm_insert($table6, "fd00::/8", 2000)) {
    echo "Inserted: fd00::/8 -> 2000\n";
}
if (lpm_insert($table6, "::/0", 1)) {
    echo "Inserted: ::/0 -> 1 (default)\n";
}

// Lookup
echo "\nLookups:\n";
$nh = lpm_lookup($table6, "2001:db8::1");
echo "  2001:db8::1 -> " . ($nh === false ? "NO MATCH" : $nh) . "\n";

$nh = lpm_lookup($table6, "fd00::1234");
echo "  fd00::1234 -> " . ($nh === false ? "NO MATCH" : $nh) . "\n";

$nh = lpm_lookup($table6, "2607:f8b0:4000::1");
echo "  2607:f8b0:4000::1 -> " . ($nh === false ? "NO MATCH" : $nh) . "\n";

// Close
lpm_close($table6);
echo "IPv6 table closed\n\n";

// ============================================================================
// Algorithm constants
// ============================================================================
echo "--- Algorithm Constants ---\n";
echo "LPM_ALGO_IPV4_DIR24 = " . LPM_ALGO_IPV4_DIR24 . "\n";
echo "LPM_ALGO_IPV4_8STRIDE = " . LPM_ALGO_IPV4_8STRIDE . "\n";
echo "LPM_ALGO_IPV6_WIDE16 = " . LPM_ALGO_IPV6_WIDE16 . "\n";
echo "LPM_ALGO_IPV6_8STRIDE = " . LPM_ALGO_IPV6_8STRIDE . "\n\n";

echo "=== Example Complete ===\n";
