<?php
/**
 * basic_example.php - Basic usage of the liblpm PHP extension
 *
 * This example demonstrates the object-oriented API for IPv4 routing tables.
 *
 * Usage:
 *   php -d extension=liblpm.so basic_example.php
 */

echo "=== liblpm PHP Extension - Basic Example ===\n\n";

// Verify extension is loaded
if (!extension_loaded('liblpm')) {
    die("Error: liblpm extension is not loaded.\n" .
        "Make sure to enable it with: php -d extension=liblpm.so\n");
}

echo "liblpm version: " . lpm_version() . "\n\n";

// ============================================================================
// Create an IPv4 routing table
// ============================================================================
echo "--- Creating IPv4 Routing Table ---\n";
$table = new LpmTableIPv4();
echo "Table created successfully\n\n";

// ============================================================================
// Insert some routes
// ============================================================================
echo "--- Inserting Routes ---\n";

$routes = [
    ["0.0.0.0/0", 1],         // Default route
    ["10.0.0.0/8", 100],      // Private network
    ["10.1.0.0/16", 101],     // More specific
    ["10.1.1.0/24", 102],     // Even more specific
    ["192.168.0.0/16", 200],  // Another private network
    ["172.16.0.0/12", 300],   // Yet another private network
];

foreach ($routes as [$prefix, $nextHop]) {
    if ($table->insert($prefix, $nextHop)) {
        echo "  Inserted: $prefix -> $nextHop\n";
    } else {
        echo "  FAILED: $prefix\n";
    }
}
echo "\n";

// ============================================================================
// Perform lookups
// ============================================================================
echo "--- Performing Lookups ---\n";

$addresses = [
    "10.1.1.1",      // Should match /24 -> 102
    "10.1.2.1",      // Should match /16 -> 101
    "10.2.0.1",      // Should match /8 -> 100
    "192.168.1.1",   // Should match 192.168.0.0/16 -> 200
    "172.20.5.10",   // Should match 172.16.0.0/12 -> 300
    "8.8.8.8",       // Should match default -> 1
    "1.1.1.1",       // Should match default -> 1
];

foreach ($addresses as $addr) {
    $result = $table->lookup($addr);
    if ($result === false) {
        echo "  $addr -> NO MATCH\n";
    } else {
        echo "  $addr -> $result\n";
    }
}
echo "\n";

// ============================================================================
// Delete a route
// ============================================================================
echo "--- Deleting Route ---\n";
echo "Before delete: 10.1.1.1 -> " . $table->lookup("10.1.1.1") . "\n";
$table->delete("10.1.1.0/24");
echo "After delete:  10.1.1.1 -> " . $table->lookup("10.1.1.1") . "\n\n";

// ============================================================================
// Error handling
// ============================================================================
echo "--- Error Handling ---\n";
try {
    $table->insert("invalid-prefix", 999);
} catch (LpmInvalidPrefixException $e) {
    echo "Caught LpmInvalidPrefixException: " . $e->getMessage() . "\n";
}

try {
    $table->lookup("not-an-ip");
} catch (LpmInvalidPrefixException $e) {
    echo "Caught LpmInvalidPrefixException: " . $e->getMessage() . "\n";
}
echo "\n";

// ============================================================================
// Clean up
// ============================================================================
echo "--- Cleanup ---\n";
$table->close();
echo "Table closed\n\n";

echo "=== Example Complete ===\n";
