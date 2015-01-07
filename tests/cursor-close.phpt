--TEST--
cursor close
--SKIPIF--
--FILE--
<?php
namespace WiredTiger;

include 'wt.inc';
\cleanup_wiredtiger_on_shutdown();

$wt_home = dirname(__FILE__) . '/wt-close.test-db';
$wt_uri = 'table:itetator-foreach';

$db = new Db($wt_home);
$db->create($wt_uri);
$cursor = $db->open($wt_uri);

$cursor->set('key', 'value');

$cursor->close();
$cursor->close();

try {
    $cursor->set('new-key', 'value');
} catch(Exception $e) {
    echo $e->getMessage() . "\n";
}

try {
    $cursor->next();
} catch(Exception $e) {
    echo $e->getMessage() . "\n";
}
?>
==DONE==
--EXPECTF--
WiredTiger\Cursor: Can not operate on closed cursor
WiredTiger\Cursor: Can not operate on closed cursor
==DONE==