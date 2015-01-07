--TEST--
phpinfo() displays wiredtiger info
--SKIPIF--
--FILE--
<?php

phpinfo();
?>
--EXPECTF--
%a
wiredtiger

WiredTiger support => enabled
extension version => %d.%d.%d
library version => WiredTiger %d.%d.%d: (%s)
%a
