--TEST--
Algorithm selection via constructor
--SKIPIF--
<?php if (!extension_loaded('liblpm')) die('skip liblpm extension not loaded'); ?>
--FILE--
<?php
// Test IPv4 DIR-24-8 algorithm
echo "=== IPv4 DIR-24-8 ===\n";
$table = new LpmTableIPv4(LpmTableIPv4::ALGO_DIR24);
var_dump($table instanceof LpmTableIPv4);
$table->insert("192.168.0.0/16", 100);
var_dump($table->lookup("192.168.1.1") === 100);
$table->close();
echo "DIR-24-8 OK\n";

// Test IPv4 8-stride algorithm
echo "=== IPv4 8-stride ===\n";
$table = new LpmTableIPv4(LpmTableIPv4::ALGO_8STRIDE);
var_dump($table instanceof LpmTableIPv4);
$table->insert("192.168.0.0/16", 200);
var_dump($table->lookup("192.168.1.1") === 200);
$table->close();
echo "8-stride OK\n";

// Test IPv6 Wide-16 algorithm
echo "=== IPv6 Wide-16 ===\n";
$table = new LpmTableIPv6(LpmTableIPv6::ALGO_WIDE16);
var_dump($table instanceof LpmTableIPv6);
$table->insert("2001:db8::/32", 1000);
var_dump($table->lookup("2001:db8::1") === 1000);
$table->close();
echo "Wide-16 OK\n";

// Test IPv6 8-stride algorithm
echo "=== IPv6 8-stride ===\n";
$table = new LpmTableIPv6(LpmTableIPv6::ALGO_8STRIDE);
var_dump($table instanceof LpmTableIPv6);
$table->insert("2001:db8::/32", 2000);
var_dump($table->lookup("2001:db8::1") === 2000);
$table->close();
echo "8-stride OK\n";

// Test class constants exist
echo "=== Class Constants ===\n";
var_dump(LpmTableIPv4::ALGO_DIR24 === 0);
var_dump(LpmTableIPv4::ALGO_8STRIDE === 1);
var_dump(LpmTableIPv6::ALGO_WIDE16 === 0);
var_dump(LpmTableIPv6::ALGO_8STRIDE === 1);

echo "All algorithm selection tests passed\n";
?>
--EXPECT--
=== IPv4 DIR-24-8 ===
bool(true)
bool(true)
DIR-24-8 OK
=== IPv4 8-stride ===
bool(true)
bool(true)
8-stride OK
=== IPv6 Wide-16 ===
bool(true)
bool(true)
Wide-16 OK
=== IPv6 8-stride ===
bool(true)
bool(true)
8-stride OK
=== Class Constants ===
bool(true)
bool(true)
bool(true)
bool(true)
All algorithm selection tests passed
