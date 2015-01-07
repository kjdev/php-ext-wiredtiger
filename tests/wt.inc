<?php
function cleanup_wiredtiger_on_shutdown() {
    register_shutdown_function('cleanup_wiredtiger');
}

function cleanup_wiredtiger() {
    global $wt_home, $db;
    if (file_exists($wt_home)) {
        if ($db instanceof \WiredTiger\Db) {
            $db->close();
        }
        cleanup_wiredtiger_tree($wt_home);
    }
}

function cleanup_wiredtiger_tree($dir) {
    $files = array_diff(scandir($dir), array('.','..'));
    foreach ($files as $file) {
        if (is_dir("$dir/$file")) {
            cleanup_wiredtiger_tree("$dir/$file");
        } else if (is_file("$dir/$file")) {
            unlink("$dir/$file");
        }
    }
    return rmdir($dir);
}

function wiredtiger_empty($cursor) {
    for($cursor->rewind(); $cursor->valid(); $cursor->next()) {
        $cursor->remove($cursort->key());
    }
}
