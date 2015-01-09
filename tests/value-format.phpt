--TEST--
set value format
--SKIPIF--
--FILE--
<?php
namespace WiredTiger;

include 'wt.inc';
\cleanup_wiredtiger_on_shutdown();

$wt_home = dirname(__FILE__) . '/wt-value-format.test-db';
$wt_uri = 'table:value-format';

$formats = array(
    'iS' => array(array(1, 'test'),
                  array(2, 'example')),
    'SiH' => array(array('first', 3, 12345),
                   array('second', 2, 578)),
    'SSS' => array(array('a', 'b', 'c'),
                   array('d', 'e', 'f')),
    'qrrSS' => array(array(32, 1, 2, 'billing', 'unavailable'),
                     array(33, 1, 2, 'billing', 'available'),
                     array(34, 1, 2, 'reminder', 'unavailable')),
    '5sii' => array(array('abcde', 1, 2),
                    array('efghi', 3, 4),
                    array('A', 5, 6),
                    array('ABCDEFGH', 7, 8)),
    'SSHH' => array(array('a', 'A', 10, 5),
                    array('bb', 'BB', 11, 4)),
    '5sHQ' => array(array('abcde', 123, 456),
                    array('xyz', 567, 789),
                    array('qwertyui', 2484, 47585)),
);

$db = new Db($wt_home, 'create');

foreach ($formats as $fmt => $value) {
    echo "*** Format ***\n";
    echo $fmt, "\n";

    $uri = $wt_uri . '-' . $fmt;
    $db->create($uri, 'value_format=' . $fmt);

    $cursor = $db->open($uri);

    // Setting key
    foreach ($value as $key => $val) {
        //$cursor->set($key, ...);
        $args = $val;
        array_unshift($args, $key);
        call_user_func_array(array($cursor, 'set'), $args);
    }

    if (strncmp($fmt, '5s', 2) == 0) {
        $valid = array();
        foreach ($value as $key => $val) {
            $val[0] = substr($val[0], 0, 5);
            array_push($valid, $val);
        }
    } else {
        $valid = $value;
    }

    echo "--- Get ---\n";
    $ret = array();
    foreach ($value as $key => $val) {
        $v = $cursor->get($key);
        //var_dump($v);
        array_push($ret, $v);
    }
    var_dump($ret === $valid);

    echo "--- Iterator ---\n";
    $ret = array();
    foreach ($cursor as $val) {
        array_push($ret, $val);
    }
    var_dump($ret === $valid);
}
?>
==DONE==
--EXPECTF--
*** Format ***
iS
--- Get ---
bool(true)
--- Iterator ---
bool(true)
*** Format ***
SiH
--- Get ---
bool(true)
--- Iterator ---
bool(true)
*** Format ***
SSS
--- Get ---
bool(true)
--- Iterator ---
bool(true)
*** Format ***
qrrSS
--- Get ---
bool(true)
--- Iterator ---
bool(true)
*** Format ***
5sii
--- Get ---
bool(true)
--- Iterator ---
bool(true)
*** Format ***
SSHH
--- Get ---
bool(true)
--- Iterator ---
bool(true)
*** Format ***
5sHQ
--- Get ---
bool(true)
--- Iterator ---
bool(true)
==DONE==