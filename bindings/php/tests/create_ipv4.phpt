--TEST--
Create IPv4 routing table
--SKIPIF--
<?php if (!extension_loaded('liblpm')) die('skip liblpm extension not loaded'); ?>
--FILE--
<?php
// Test basic IPv4 table creation
$table = new LpmTableIPv4();
var_dump($table instanceof LpmTableIPv4);

// Test table is usable
var_dump($table->size() === 0);

// Test close
$table->close();
echo "OK\n";

// Test creation with DIR-24-8 algorithm
$table2 = new LpmTableIPv4(LpmTableIPv4::ALGO_DIR24);
var_dump($table2 instanceof LpmTableIPv4);
$table2->close();

// Test creation with 8-stride algorithm
$table3 = new LpmTableIPv4(LpmTableIPv4::ALGO_8STRIDE);
var_dump($table3 instanceof LpmTableIPv4);
$table3->close();

echo "All IPv4 table creation tests passed\n";
?>
--EXPECT--
bool(true)
bool(true)
OK
bool(true)
bool(true)
All IPv4 table creation tests passed
