--TEST--
IPv6 basic operations
--SKIPIF--
<?php if (!extension_loaded('liblpm')) die('skip liblpm extension not loaded'); ?>
--FILE--
<?php
// Test various IPv6 address formats
echo "=== IPv6 Address Formats ===\n";
$table = new LpmTableIPv6();

// Full address format
$table->insert("2001:0db8:0000:0000:0000:0000:0000:0000/32", 100);
var_dump($table->lookup("2001:db8::1") === 100);

// Compressed address format
$table->insert("fe80::/10", 200);
var_dump($table->lookup("fe80::1") === 200);
var_dump($table->lookup("fe80::1234:5678") === 200);

// Link-local addresses
var_dump($table->lookup("fe80::1:2:3:4") === 200);

// Different IPv6 ranges
$table->insert("fc00::/7", 300);  // Unique local addresses
$table->insert("ff00::/8", 400);  // Multicast

var_dump($table->lookup("fd00::1") === 300);
var_dump($table->lookup("ff02::1") === 400);

$table->close();
echo "Address format tests passed\n";

// Test edge cases
echo "=== IPv6 Edge Cases ===\n";
$table = new LpmTableIPv6();

// Very specific prefix (/128)
$table->insert("2001:db8::1/128", 100);
var_dump($table->lookup("2001:db8::1") === 100);
var_dump($table->lookup("2001:db8::2") === false);

// Very broad prefix
$table->insert("2000::/3", 200);
var_dump($table->lookup("2001:db8::999") === 100);  // More specific wins
var_dump($table->lookup("3000::1") === 200);

$table->close();
echo "Edge case tests passed\n";

// Test common network prefixes
echo "=== Common IPv6 Prefixes ===\n";
$table = new LpmTableIPv6();

// ISP allocation sizes
$table->insert("2001:db8::/32", 1000);    // /32 - typical ISP allocation
$table->insert("2001:db8:1::/48", 2000);  // /48 - typical site allocation
$table->insert("2001:db8:1:1::/64", 3000); // /64 - typical subnet

var_dump($table->lookup("2001:db8:1:1::1") === 3000);
var_dump($table->lookup("2001:db8:1:2::1") === 2000);
var_dump($table->lookup("2001:db8:2::1") === 1000);

$table->close();
echo "Common prefix tests passed\n";

echo "All IPv6 tests passed\n";
?>
--EXPECT--
=== IPv6 Address Formats ===
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
Address format tests passed
=== IPv6 Edge Cases ===
bool(true)
bool(true)
bool(true)
bool(true)
Edge case tests passed
=== Common IPv6 Prefixes ===
bool(true)
bool(true)
bool(true)
Common prefix tests passed
All IPv6 tests passed
