--TEST--
Default route (0.0.0.0/0 and ::/0)
--SKIPIF--
<?php if (!extension_loaded('liblpm')) die('skip liblpm extension not loaded'); ?>
--FILE--
<?php
// Test IPv4 default route
echo "=== IPv4 Default Route ===\n";
$table = new LpmTableIPv4();

// Insert default route and a specific route
$table->insert("0.0.0.0/0", 1);       // Default route
$table->insert("192.168.0.0/16", 100); // Specific route

// Specific route should match
var_dump($table->lookup("192.168.1.1") === 100);

// Default route should match anything else
var_dump($table->lookup("8.8.8.8") === 1);
var_dump($table->lookup("1.1.1.1") === 1);
var_dump($table->lookup("10.0.0.1") === 1);

// Delete default route
$table->delete("0.0.0.0/0");
var_dump($table->lookup("8.8.8.8") === false);

$table->close();
echo "IPv4 default route tests passed\n";

// Test IPv6 default route
echo "=== IPv6 Default Route ===\n";
$table6 = new LpmTableIPv6();

// Insert default route and a specific route
$table6->insert("::/0", 1);            // Default route
$table6->insert("2001:db8::/32", 1000); // Specific route

// Specific route should match
var_dump($table6->lookup("2001:db8::1") === 1000);

// Default route should match anything else
var_dump($table6->lookup("2001:db9::1") === 1);
var_dump($table6->lookup("fe80::1") === 1);

// Delete default route
$table6->delete("::/0");
var_dump($table6->lookup("fe80::1") === false);

$table6->close();
echo "IPv6 default route tests passed\n";
?>
--EXPECT--
=== IPv4 Default Route ===
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
IPv4 default route tests passed
=== IPv6 Default Route ===
bool(true)
bool(true)
bool(true)
bool(true)
IPv6 default route tests passed
