--TEST--
iterate thought db
--SKIPIF--
--FILE--
<?php
namespace WiredTiger;

include 'wt.inc';
\cleanup_wiredtiger_on_shutdown();

$wt_home = dirname(__FILE__) . '/wt-iterator.test-db';
$wt_uri = 'table:itetator';

$db = new Db($wt_home);
$db->create($wt_uri);
$cursor = $db->open($wt_uri);

/* Add test data, and the data will be be sorted */
$data = array(
    'First', 'Second', 'Third', 10, '', 'Last'
);

foreach ($data as $item) {
    $cursor->set($item, $item);
}

echo "*** Loop through ***\n";
for ($cursor->rewind(); $cursor->valid(); $cursor->next()) {
    echo $cursor->key() . ' => ' . $cursor->current() . "\n";
}

echo "\n*** Reset to last ***\n";
var_dump($cursor->last());
var_dump($cursor->key() . ' => ' . $cursor->current());

echo "\n*** Last->next will be invalid ***\n";
var_dump($cursor->next());
var_dump($cursor->valid());
var_dump($cursor->key());

echo "\n*** Seek to give key ***\n";
$cursor->seek('Second');
var_dump($cursor->current());

echo "\n*** Seek to a non-exist key ***\n";
$cursor->seek('11');
var_dump($cursor->current());

echo "\n*** Seek to a non-exist key will point to nearest next key ***\n";
$cursor->seek('11', true);
var_dump($cursor->current());

echo "\n*** Bound checking ***\n";
$cursor->rewind();
$cursor->prev();
$cursor->prev();
var_dump($cursor->current());

$cursor->next();
$cursor->next();
$cursor->next();
$cursor->next();
$cursor->next();
$cursor->next();
$cursor->next();
$cursor->next();
$cursor->next();
$cursor->next();
var_dump($cursor->current());

?>
==DONE==
--EXPECTF--
*** Loop through ***
 => 
10 => 10
First => First
Last => Last
Second => Second
Third => Third

*** Reset to last ***
bool(true)
string(14) "Third => Third"

*** Last->next will be invalid ***
bool(false)
bool(false)
bool(false)

*** Seek to give key ***
string(6) "Second"

*** Seek to a non-exist key ***
bool(false)

*** Seek to a non-exist key will point to nearest next key ***
string(5) "First"

*** Bound checking ***
bool(false)
bool(false)
==DONE==
