--TEST--
Delete routes from routing table
--SKIPIF--
<?php if (!extension_loaded('liblpm')) die('skip liblpm extension not loaded'); ?>
--FILE--
<?php
// Test IPv4 delete
echo "=== IPv4 Delete Tests ===\n";
$table = new LpmTableIPv4();

// Insert and verify
$table->insert("192.168.0.0/16", 100);
var_dump($table->lookup("192.168.1.1") === 100);

// Delete the route
$result = $table->delete("192.168.0.0/16");
var_dump($result === true);

// Verify it's gone
var_dump($table->lookup("192.168.1.1") === false);

// Delete non-existent route (should still succeed or fail gracefully)
// Note: behavior may vary by algorithm
$result = $table->delete("10.0.0.0/8");
echo "Delete non-existent: " . ($result ? "success" : "failed") . "\n";

$table->close();
echo "IPv4 delete tests passed\n";

// Test IPv6 delete
echo "=== IPv6 Delete Tests ===\n";
$table6 = new LpmTableIPv6();

// Insert and verify
$table6->insert("2001:db8::/32", 1000);
var_dump($table6->lookup("2001:db8::1") === 1000);

// Delete the route
$result = $table6->delete("2001:db8::/32");
var_dump($result === true);

// Verify it's gone
var_dump($table6->lookup("2001:db8::1") === false);

$table6->close();
echo "IPv6 delete tests passed\n";
?>
--EXPECTF--
=== IPv4 Delete Tests ===
bool(true)
bool(true)
bool(true)
Delete non-existent: %s
IPv4 delete tests passed
=== IPv6 Delete Tests ===
bool(true)
bool(true)
bool(true)
IPv6 delete tests passed
