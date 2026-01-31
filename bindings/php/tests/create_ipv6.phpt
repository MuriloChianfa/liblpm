--TEST--
Create IPv6 routing table
--SKIPIF--
<?php if (!extension_loaded('liblpm')) die('skip liblpm extension not loaded'); ?>
--FILE--
<?php
// Test basic IPv6 table creation
$table = new LpmTableIPv6();
var_dump($table instanceof LpmTableIPv6);

// Test table is usable
var_dump($table->size() === 0);

// Test close
$table->close();
echo "OK\n";

// Test creation with Wide-16 algorithm
$table2 = new LpmTableIPv6(LpmTableIPv6::ALGO_WIDE16);
var_dump($table2 instanceof LpmTableIPv6);
$table2->close();

// Test creation with 8-stride algorithm
$table3 = new LpmTableIPv6(LpmTableIPv6::ALGO_8STRIDE);
var_dump($table3 instanceof LpmTableIPv6);
$table3->close();

echo "All IPv6 table creation tests passed\n";
?>
--EXPECT--
bool(true)
bool(true)
OK
bool(true)
bool(true)
All IPv6 table creation tests passed
