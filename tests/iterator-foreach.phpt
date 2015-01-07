--TEST--
iterate thought db by foreach
--SKIPIF--
--FILE--
<?php
namespace WiredTiger;

include 'wt.inc';
\cleanup_wiredtiger_on_shutdown();

$wt_home = dirname(__FILE__) . '/wt-iterator-foreach.test-db';
$wt_uri = 'table:itetator-foreach';

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

echo "*** Loop through in foreach style ***\n";
foreach ($cursor as $key => $value) {
    echo "{$key} => {$value}\n";
}
?>
==DONE==
--EXPECTF--
*** Loop through in foreach style ***
 => 
10 => 10
First => First
Last => Last
Second => Second
Third => Third
==DONE==