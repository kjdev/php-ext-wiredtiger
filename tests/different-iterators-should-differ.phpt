--TEST--
different iterators should not affect each other
--SKIPIF--
--FILE--
<?php
namespace WiredTiger;

include 'wt.inc';
\cleanup_wiredtiger_on_shutdown();

$wt_home = dirname(__FILE__) . '/wt-iterator-shoud-not-affect.test-db';
$wt_uri = 'table:itetator-affect';

$db = new Db($wt_home);
$db->create($wt_uri);
$cursor = $db->open($wt_uri);

foreach(array(1, 2, 3, 4) as $k) {
    $cursor->set($k, $k);
}

$cursor1 = $db->open($wt_uri);
$cursor2 = $db->open($wt_uri);

foreach($cursor1 as $k => $v) {
    echo "$k => $v\n";
}

$cursor1->rewind();
var_dump($cursor1->next());
var_dump($cursor1->next());
var_dump($cursor1->current());
var_dump($cursor2->current());

?>
==DONE==
--EXPECTF--
1 => 1
2 => 2
3 => 3
4 => 4
bool(true)
bool(true)
string(1) "3"
string(1) "1"
==DONE==
