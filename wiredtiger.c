#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <php.h>
#include <php_ini.h>
#include <ext/standard/info.h>
#include "php_wiredtiger.h"
#include "exception.h"
#include "db.h"
#include "cursor.h"

ZEND_DECLARE_MODULE_GLOBALS(wiredtiger)

ZEND_MINIT_FUNCTION(wiredtiger)
{
    php_wt_exception_class_register(TSRMLS_C);
    php_wt_db_class_register(TSRMLS_C);
    php_wt_cursor_class_register(TSRMLS_C);

    return SUCCESS;
}

ZEND_MSHUTDOWN_FUNCTION(wiredtiger)
{
    return SUCCESS;
}

/*
ZEND_RINIT_FUNCTION(wiredtiger)
{
    return SUCCESS;
}

ZEND_RSHUTDOWN_FUNCTION(wiredtiger)
{
    return SUCCESS;
}
*/

ZEND_MINFO_FUNCTION(wiredtiger)
{
    php_info_print_table_start();
    php_info_print_table_header(2, "WiredTiger support", "enabled");
    php_info_print_table_header(2, "extension version", PHP_WT_EXT_VERSION);
    php_info_print_table_header(2, "library version", WIREDTIGER_VERSION_STRING);
    php_info_print_table_end();
}

const zend_function_entry wiredtiger_functions[] = {
    ZEND_FE_END
};

zend_module_entry wiredtiger_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
    STANDARD_MODULE_HEADER,
#endif
    "wiredtiger",
    NULL,
    ZEND_MINIT(wiredtiger),
    ZEND_MSHUTDOWN(wiredtiger),
    NULL, /* ZEND_RINIT(wiredtiger), */
    NULL, /* ZEND_RSHUTDOWN(wiredtiger), */
    ZEND_MINFO(wiredtiger),
#if ZEND_MODULE_API_NO >= 20010901
    PHP_WT_EXT_VERSION,
#endif
    STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_WIREDTIGER
ZEND_GET_MODULE(wiredtiger)
#endif
