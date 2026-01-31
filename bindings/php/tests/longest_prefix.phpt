--TEST--
Longest prefix match with overlapping routes
--SKIPIF--
<?php if (!extension_loaded('liblpm')) die('skip liblpm extension not loaded'); ?>
--FILE--
<?php
// Test IPv4 longest prefix match
echo "=== IPv4 Longest Prefix Match ===\n";
$table = new LpmTableIPv4();

// Insert overlapping prefixes
$table->insert("10.0.0.0/8", 100);     // /8 - broadest
$table->insert("10.1.0.0/16", 200);    // /16 - more specific
$table->insert("10.1.1.0/24", 300);    // /24 - most specific

// Test that most specific match wins
var_dump($table->lookup("10.1.1.1") === 300);   // /24 match
var_dump($table->lookup("10.1.2.1") === 200);   // /16 match
var_dump($table->lookup("10.2.1.1") === 100);   // /8 match
var_dump($table->lookup("11.0.0.1") === false); // No match

// Add even more specific route
$table->insert("10.1.1.128/25", 400);
var_dump($table->lookup("10.1.1.129") === 400); // /25 match
var_dump($table->lookup("10.1.1.1") === 300);   // /24 match (below /25 range)

$table->close();
echo "IPv4 longest prefix match tests passed\n";

// Test IPv6 longest prefix match
echo "=== IPv6 Longest Prefix Match ===\n";
$table6 = new LpmTableIPv6();

// Insert overlapping prefixes
$table6->insert("2001:db8::/32", 1000);
$table6->insert("2001:db8:1::/48", 2000);
$table6->insert("2001:db8:1:2::/64", 3000);

// Test that most specific match wins
var_dump($table6->lookup("2001:db8:1:2::1") === 3000);  // /64 match
var_dump($table6->lookup("2001:db8:1:3::1") === 2000);  // /48 match
var_dump($table6->lookup("2001:db8:2::1") === 1000);    // /32 match
var_dump($table6->lookup("2001:db9::1") === false);     // No match

$table6->close();
echo "IPv6 longest prefix match tests passed\n";
?>
--EXPECT--
=== IPv4 Longest Prefix Match ===
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
IPv4 longest prefix match tests passed
=== IPv6 Longest Prefix Match ===
bool(true)
bool(true)
bool(true)
bool(true)
IPv6 longest prefix match tests passed
