--TEST--
Insert and lookup operations for IPv4 and IPv6
--SKIPIF--
<?php if (!extension_loaded('liblpm')) die('skip liblpm extension not loaded'); ?>
--FILE--
<?php
// Test IPv4 insert and lookup
echo "=== IPv4 Tests ===\n";
$table = new LpmTableIPv4();

// Insert a route
$result = $table->insert("192.168.0.0/16", 100);
var_dump($result === true);

// Lookup should find it
$nh = $table->lookup("192.168.1.1");
var_dump($nh === 100);

// Lookup outside range should not find it
$nh = $table->lookup("10.0.0.1");
var_dump($nh === false);

// Insert more routes
$table->insert("10.0.0.0/8", 200);
$table->insert("172.16.0.0/12", 300);

// Verify lookups
var_dump($table->lookup("10.1.2.3") === 200);
var_dump($table->lookup("172.20.5.10") === 300);
var_dump($table->lookup("8.8.8.8") === false);

$table->close();
echo "IPv4 tests passed\n";

// Test IPv6 insert and lookup
echo "=== IPv6 Tests ===\n";
$table6 = new LpmTableIPv6();

// Insert a route
$result = $table6->insert("2001:db8::/32", 1000);
var_dump($result === true);

// Lookup should find it
$nh = $table6->lookup("2001:db8::1");
var_dump($nh === 1000);

// Lookup outside range should not find it
$nh = $table6->lookup("2001:db9::1");
var_dump($nh === false);

// Insert more routes
$table6->insert("fd00::/8", 2000);

// Verify lookups
var_dump($table6->lookup("fd00::1234") === 2000);

$table6->close();
echo "IPv6 tests passed\n";
?>
--EXPECT--
=== IPv4 Tests ===
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
IPv4 tests passed
=== IPv6 Tests ===
bool(true)
bool(true)
bool(true)
bool(true)
IPv6 tests passed
