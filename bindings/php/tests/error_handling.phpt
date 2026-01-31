--TEST--
Error handling and exceptions
--SKIPIF--
<?php if (!extension_loaded('liblpm')) die('skip liblpm extension not loaded'); ?>
--FILE--
<?php
// Test invalid IPv4 prefix format
echo "=== Invalid Prefix Format ===\n";
$table = new LpmTableIPv4();

try {
    $table->insert("invalid", 100);
    echo "ERROR: Should have thrown exception\n";
} catch (LpmInvalidPrefixException $e) {
    echo "Caught LpmInvalidPrefixException: OK\n";
} catch (Exception $e) {
    echo "Caught Exception: " . get_class($e) . "\n";
}

try {
    $table->insert("192.168.0.0", 100); // Missing prefix length
    echo "ERROR: Should have thrown exception\n";
} catch (LpmInvalidPrefixException $e) {
    echo "Missing prefix length caught: OK\n";
} catch (Exception $e) {
    echo "Caught: " . get_class($e) . "\n";
}

try {
    $table->insert("192.168.0.0/33", 100); // Invalid prefix length
    echo "ERROR: Should have thrown exception\n";
} catch (LpmInvalidPrefixException $e) {
    echo "Invalid prefix length caught: OK\n";
} catch (Exception $e) {
    echo "Caught: " . get_class($e) . "\n";
}

$table->close();

// Test invalid IPv4 address format
echo "=== Invalid Address Format ===\n";
$table = new LpmTableIPv4();
$table->insert("192.168.0.0/16", 100);

try {
    $table->lookup("invalid");
    echo "ERROR: Should have thrown exception\n";
} catch (LpmInvalidPrefixException $e) {
    echo "Invalid address caught: OK\n";
} catch (Exception $e) {
    echo "Caught: " . get_class($e) . "\n";
}

$table->close();

// Test operation on closed table
echo "=== Operation on Closed Table ===\n";
$table = new LpmTableIPv4();
$table->close();

try {
    $table->insert("192.168.0.0/16", 100);
    echo "ERROR: Should have thrown exception\n";
} catch (LpmOperationException $e) {
    echo "Closed table operation caught: OK\n";
} catch (Exception $e) {
    echo "Caught: " . get_class($e) . "\n";
}

// Test invalid IPv6 prefix
echo "=== Invalid IPv6 Format ===\n";
$table6 = new LpmTableIPv6();

try {
    $table6->insert("invalid", 100);
    echo "ERROR: Should have thrown exception\n";
} catch (LpmInvalidPrefixException $e) {
    echo "Invalid IPv6 caught: OK\n";
} catch (Exception $e) {
    echo "Caught: " . get_class($e) . "\n";
}

try {
    $table6->insert("2001:db8::/129", 100); // Invalid prefix length (max 128)
    echo "ERROR: Should have thrown exception\n";
} catch (LpmInvalidPrefixException $e) {
    echo "Invalid IPv6 prefix length caught: OK\n";
} catch (Exception $e) {
    echo "Caught: " . get_class($e) . "\n";
}

$table6->close();

// Test exception hierarchy
echo "=== Exception Hierarchy ===\n";
var_dump(is_subclass_of('LpmInvalidPrefixException', 'LpmException'));
var_dump(is_subclass_of('LpmOperationException', 'LpmException'));
var_dump(is_subclass_of('LpmException', 'Exception'));

echo "All error handling tests passed\n";
?>
--EXPECT--
=== Invalid Prefix Format ===
Caught LpmInvalidPrefixException: OK
Missing prefix length caught: OK
Invalid prefix length caught: OK
=== Invalid Address Format ===
Invalid address caught: OK
=== Operation on Closed Table ===
Closed table operation caught: OK
=== Invalid IPv6 Format ===
Invalid IPv6 caught: OK
Invalid IPv6 prefix length caught: OK
=== Exception Hierarchy ===
bool(true)
bool(true)
bool(true)
All error handling tests passed
