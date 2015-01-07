#ifndef PHP_WIREDTIGER_EXCEPTION_H
#define PHP_WIREDTIGER_EXCEPTION_H

#include "zend_exceptions.h"

#include "php_wiredtiger.h"

extern PHP_WT_API zend_class_entry *php_wt_exception_ce;

PHP_WT_API int php_wt_exception_class_register(TSRMLS_D);

#define PHP_WT_ERR(_flag, ...) \
    php_error_docref(NULL TSRMLS_CC, _flag, __VA_ARGS__)

#define PHP_WT_EXCEPTION(_code, ...) \
    zend_throw_exception_ex(php_wt_exception_ce, _code TSRMLS_CC, __VA_ARGS__)

#endif
