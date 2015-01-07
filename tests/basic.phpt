--TEST--
basic: get(), set(), remove()
--SKIPIF--
--FILE--
<?php
namespace WiredTiger;

include 'wt.inc';
\cleanup_wiredtiger_on_shutdown();

$wt_home = dirname(__FILE__) . '/wt-basic.test-db';
$wt_uri = 'table:basic';

$db = new Db($wt_home, 'create');
var_dump($db->create($wt_uri));

$cursor = $db->open($wt_uri);

var_dump($cursor->set('key', 'value'));
var_dump($cursor->get('key'));
var_dump($cursor->get('non-exists-key'));
var_dump($cursor->set('name', 'reeze'));
var_dump($cursor->get('name'));
var_dump($cursor->remove('name'));
var_dump($cursor->get('name'));
?>
==DONE==
--EXPECTF--
bool(true)
bool(true)
string(5) "value"
bool(false)
bool(true)
string(5) "reeze"
bool(true)
bool(false)
==DONE==
