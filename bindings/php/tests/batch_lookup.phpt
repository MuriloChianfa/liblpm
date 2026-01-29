--TEST--
Batch lookup operations
--SKIPIF--
<?php if (!extension_loaded('liblpm')) die('skip liblpm extension not loaded'); ?>
--FILE--
<?php
// Test IPv4 batch lookup
echo "=== IPv4 Batch Lookup ===\n";
$table = new LpmTableIPv4();

// Insert routes
$table->insert("10.0.0.0/8", 100);
$table->insert("192.168.0.0/16", 200);
$table->insert("172.16.0.0/12", 300);

// Batch lookup
$addresses = [
    "10.1.1.1",
    "192.168.1.1",
    "172.20.5.5",
    "8.8.8.8"  // Not found
];

$results = $table->lookupBatch($addresses);
var_dump(count($results) === 4);
var_dump($results[0] === 100);
var_dump($results[1] === 200);
var_dump($results[2] === 300);
var_dump($results[3] === false);

// Empty batch
$results = $table->lookupBatch([]);
var_dump(count($results) === 0);

$table->close();
echo "IPv4 batch tests passed\n";

// Test IPv6 batch lookup
echo "=== IPv6 Batch Lookup ===\n";
$table6 = new LpmTableIPv6();

// Insert routes
$table6->insert("2001:db8::/32", 1000);
$table6->insert("fd00::/8", 2000);

// Batch lookup
$addresses6 = [
    "2001:db8::1",
    "fd00::1234",
    "2001:db9::1"  // Not found
];

$results6 = $table6->lookupBatch($addresses6);
var_dump(count($results6) === 3);
var_dump($results6[0] === 1000);
var_dump($results6[1] === 2000);
var_dump($results6[2] === false);

$table6->close();
echo "IPv6 batch tests passed\n";
?>
--EXPECT--
=== IPv4 Batch Lookup ===
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
IPv4 batch tests passed
=== IPv6 Batch Lookup ===
bool(true)
bool(true)
bool(true)
bool(true)
IPv6 batch tests passed
