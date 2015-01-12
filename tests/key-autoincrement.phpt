--TEST--
set key autoincrement
--SKIPIF--
--FILE--
<?php
namespace WiredTiger;

include 'wt.inc';
\cleanup_wiredtiger_on_shutdown();

$wt_home = dirname(__FILE__) . '/wt-key-autoincrement.test-db';
$wt_uri = 'table:key-autoincrement';

$db = new Db($wt_home, 'create');
var_dump($db->create($wt_uri, 'key_format=r,value_format=S'));

$cursor = $db->open($wt_uri, 'append');

$values = array('a', 'b', 'c', 'd');

foreach ($values as $val) {
    $cursor->set(0, $val);
}

foreach($cursor as $key => $val) {
    echo "key: {$key}, value: {$val}\n";
}
?>
==DONE==
--EXPECTF--
bool(true)
key: 1, value: a
key: 2, value: b
key: 3, value: c
key: 4, value: d
==DONE==
