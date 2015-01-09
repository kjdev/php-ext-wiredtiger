#ifdef HAVE_CONFIG_H
#    include "config.h"
#endif

#include <php.h>
#include <php_ini.h>
#include <zend_extensions.h>
#include "php_wiredtiger.h"
#include "exception.h"

ZEND_EXTERN_MODULE_GLOBALS(wiredtiger)

zend_class_entry *php_wt_exception_ce;

PHP_WT_API int
php_wt_exception_class_register(TSRMLS_D)
{
    zend_class_entry ce;

    INIT_CLASS_ENTRY(ce, ZEND_NS_NAME(PHP_WT_NS, "Exception"), NULL);

    php_wt_exception_ce = zend_register_internal_class_ex(
        &ce, zend_exception_get_default(TSRMLS_C), NULL TSRMLS_CC);
    if (php_wt_exception_ce == NULL) {
        return FAILURE;
    }

    return SUCCESS;
}
