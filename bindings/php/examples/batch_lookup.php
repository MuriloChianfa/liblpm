<?php
/**
 * batch_lookup.php - Batch lookup operations with liblpm
 *
 * This example demonstrates efficient batch lookups for high-throughput
 * scenarios where you need to look up many addresses at once.
 *
 * Usage:
 *   php -d extension=liblpm.so batch_lookup.php
 */

echo "=== liblpm PHP Extension - Batch Lookup Example ===\n\n";

if (!extension_loaded('liblpm')) {
    die("Error: liblpm extension is not loaded.\n");
}

// ============================================================================
// Create and populate table
// ============================================================================
echo "--- Setting up routing table ---\n";
$table = new LpmTableIPv4();

// Insert a variety of routes
$routes = [
    "0.0.0.0/0" => 1,       // Default
    "10.0.0.0/8" => 100,
    "10.1.0.0/16" => 110,
    "10.2.0.0/16" => 120,
    "172.16.0.0/12" => 200,
    "192.168.0.0/16" => 300,
    "192.168.1.0/24" => 310,
    "192.168.2.0/24" => 320,
];

foreach ($routes as $prefix => $nextHop) {
    $table->insert($prefix, $nextHop);
}
echo "Inserted " . count($routes) . " routes\n\n";

// ============================================================================
// Prepare batch of addresses
// ============================================================================
echo "--- Preparing batch of addresses ---\n";

$addresses = [
    // 10.x.x.x range
    "10.0.0.1",
    "10.1.1.1",
    "10.2.2.2",
    "10.3.3.3",
    
    // 172.x.x.x range
    "172.16.1.1",
    "172.20.5.10",
    "172.31.255.255",
    
    // 192.168.x.x range
    "192.168.0.1",
    "192.168.1.100",
    "192.168.2.50",
    "192.168.3.1",
    
    // Public addresses (default route)
    "8.8.8.8",
    "1.1.1.1",
    "208.67.222.222",
];

echo "Batch size: " . count($addresses) . " addresses\n\n";

// ============================================================================
// Perform batch lookup
// ============================================================================
echo "--- Batch lookup ---\n";

// Time the batch lookup
$startTime = microtime(true);
$results = $table->lookupBatch($addresses);
$endTime = microtime(true);

// Display results
foreach ($addresses as $i => $addr) {
    $nh = $results[$i];
    if ($nh === false) {
        printf("  %-20s -> NO MATCH\n", $addr);
    } else {
        printf("  %-20s -> %d\n", $addr, $nh);
    }
}

$elapsed = ($endTime - $startTime) * 1000000; // microseconds
printf("\nBatch lookup completed in %.2f microseconds\n", $elapsed);
printf("Average: %.2f microseconds per lookup\n\n", $elapsed / count($addresses));

// ============================================================================
// Compare with individual lookups
// ============================================================================
echo "--- Comparison: Individual lookups ---\n";

$startTime = microtime(true);
$individualResults = [];
foreach ($addresses as $addr) {
    $individualResults[] = $table->lookup($addr);
}
$endTime = microtime(true);

$elapsed = ($endTime - $startTime) * 1000000;
printf("Individual lookups completed in %.2f microseconds\n", $elapsed);
printf("Average: %.2f microseconds per lookup\n\n", $elapsed / count($addresses));

// Verify results match
$match = $results === $individualResults;
echo "Results match: " . ($match ? "YES" : "NO") . "\n\n";

// ============================================================================
// Large batch example
// ============================================================================
echo "--- Large batch example ---\n";

// Generate many random addresses
$largeAddresses = [];
for ($i = 0; $i < 1000; $i++) {
    $largeAddresses[] = sprintf("%d.%d.%d.%d",
        rand(0, 255), rand(0, 255), rand(0, 255), rand(0, 255));
}

echo "Generated " . count($largeAddresses) . " random addresses\n";

$startTime = microtime(true);
$largeResults = $table->lookupBatch($largeAddresses);
$endTime = microtime(true);

$elapsed = ($endTime - $startTime) * 1000000;
printf("Large batch completed in %.2f microseconds\n", $elapsed);
printf("Average: %.3f microseconds per lookup\n", $elapsed / count($largeAddresses));

// Count results
$found = count(array_filter($largeResults, fn($r) => $r !== false));
$notFound = count($largeResults) - $found;
echo "Results: $found found, $notFound not found\n\n";

// ============================================================================
// Clean up
// ============================================================================
$table->close();
echo "=== Example Complete ===\n";
