--TEST--
Memory cleanup and resource management
--SKIPIF--
<?php if (!extension_loaded('liblpm')) die('skip liblpm extension not loaded'); ?>
--FILE--
<?php
// Test explicit close
echo "=== Explicit Close ===\n";
$table = new LpmTableIPv4();
$table->insert("192.168.0.0/16", 100);
$table->close();
echo "Explicit close OK\n";

// Test double close (should not crash)
$table->close();
echo "Double close OK\n";

// Test destructor cleanup (object goes out of scope)
echo "=== Destructor Cleanup ===\n";
function createAndForget() {
    $table = new LpmTableIPv4();
    $table->insert("10.0.0.0/8", 100);
    // Table goes out of scope here
}
createAndForget();
echo "Destructor cleanup OK\n";

// Test multiple tables
echo "=== Multiple Tables ===\n";
$tables = [];
for ($i = 0; $i < 5; $i++) {
    $tables[] = new LpmTableIPv4();
    $tables[$i]->insert("10.$i.0.0/16", $i * 100);
}

// Verify all tables work
for ($i = 0; $i < 5; $i++) {
    var_dump($tables[$i]->lookup("10.$i.1.1") === $i * 100);
}

// Close all tables
foreach ($tables as $table) {
    $table->close();
}
echo "Multiple tables OK\n";

// Test procedural API resource cleanup
echo "=== Procedural Resource Cleanup ===\n";
$res = lpm_create_ipv4();
lpm_insert($res, "192.168.0.0/16", 100);
lpm_close($res);
echo "Procedural cleanup OK\n";

// Test closing already closed resource (should not crash)
lpm_close($res);
echo "Double procedural close OK\n";

// Test mixed OOP and procedural
echo "=== Mixed API ===\n";
$oop = new LpmTableIPv4();
$proc = lpm_create_ipv4();

$oop->insert("10.0.0.0/8", 100);
lpm_insert($proc, "192.168.0.0/16", 200);

var_dump($oop->lookup("10.1.1.1") === 100);
var_dump(lpm_lookup($proc, "192.168.1.1") === 200);

$oop->close();
lpm_close($proc);
echo "Mixed API OK\n";

// Test IPv6 cleanup
echo "=== IPv6 Cleanup ===\n";
$table6 = new LpmTableIPv6();
$table6->insert("2001:db8::/32", 1000);
$table6->close();
$table6->close(); // Double close
echo "IPv6 cleanup OK\n";

echo "All memory cleanup tests passed\n";
?>
--EXPECT--
=== Explicit Close ===
Explicit close OK
Double close OK
=== Destructor Cleanup ===
Destructor cleanup OK
=== Multiple Tables ===
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
Multiple tables OK
=== Procedural Resource Cleanup ===
Procedural cleanup OK
Double procedural close OK
=== Mixed API ===
bool(true)
bool(true)
Mixed API OK
=== IPv6 Cleanup ===
IPv6 cleanup OK
All memory cleanup tests passed
