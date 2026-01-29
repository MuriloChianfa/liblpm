--TEST--
Procedural API functions
--SKIPIF--
<?php if (!extension_loaded('liblpm')) die('skip liblpm extension not loaded'); ?>
--FILE--
<?php
// Test lpm_version
echo "=== Version ===\n";
$version = lpm_version();
var_dump(is_string($version));
var_dump(strlen($version) > 0);

// Test IPv4 procedural API
echo "=== IPv4 Procedural API ===\n";
$table = lpm_create_ipv4();
var_dump(is_resource($table) || is_object($table));

// Insert
var_dump(lpm_insert($table, "192.168.0.0/16", 100) === true);
var_dump(lpm_insert($table, "10.0.0.0/8", 200) === true);

// Lookup
var_dump(lpm_lookup($table, "192.168.1.1") === 100);
var_dump(lpm_lookup($table, "10.1.1.1") === 200);
var_dump(lpm_lookup($table, "8.8.8.8") === false);

// Size
var_dump(lpm_size($table) >= 0);

// Batch lookup
$results = lpm_lookup_batch($table, ["192.168.1.1", "10.1.1.1", "8.8.8.8"]);
var_dump(count($results) === 3);
var_dump($results[0] === 100);
var_dump($results[1] === 200);
var_dump($results[2] === false);

// Delete
var_dump(lpm_delete($table, "192.168.0.0/16") === true);
var_dump(lpm_lookup($table, "192.168.1.1") === false);

// Close
lpm_close($table);
echo "IPv4 procedural API tests passed\n";

// Test IPv6 procedural API
echo "=== IPv6 Procedural API ===\n";
$table6 = lpm_create_ipv6();
var_dump(is_resource($table6) || is_object($table6));

// Insert
var_dump(lpm_insert($table6, "2001:db8::/32", 1000) === true);

// Lookup
var_dump(lpm_lookup($table6, "2001:db8::1") === 1000);
var_dump(lpm_lookup($table6, "2001:db9::1") === false);

// Delete
var_dump(lpm_delete($table6, "2001:db8::/32") === true);
var_dump(lpm_lookup($table6, "2001:db8::1") === false);

// Close
lpm_close($table6);
echo "IPv6 procedural API tests passed\n";

// Test algorithm constants
echo "=== Algorithm Constants ===\n";
var_dump(defined('LPM_ALGO_IPV4_DIR24'));
var_dump(defined('LPM_ALGO_IPV4_8STRIDE'));
var_dump(defined('LPM_ALGO_IPV6_WIDE16'));
var_dump(defined('LPM_ALGO_IPV6_8STRIDE'));

// Test with algorithm selection
$table_dir24 = lpm_create_ipv4(LPM_ALGO_IPV4_DIR24);
var_dump(is_resource($table_dir24) || is_object($table_dir24));
lpm_close($table_dir24);

$table_stride = lpm_create_ipv4(LPM_ALGO_IPV4_8STRIDE);
var_dump(is_resource($table_stride) || is_object($table_stride));
lpm_close($table_stride);

echo "All procedural API tests passed\n";
?>
--EXPECT--
=== Version ===
bool(true)
bool(true)
=== IPv4 Procedural API ===
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
IPv4 procedural API tests passed
=== IPv6 Procedural API ===
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
IPv6 procedural API tests passed
=== Algorithm Constants ===
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
All procedural API tests passed
