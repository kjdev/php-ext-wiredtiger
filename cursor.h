#ifndef PHP_WIREDTIGER_CURSOR_H
#define PHP_WIREDTIGER_CURSOR_H

#include "php_wiredtiger.h"
#include "wiredtiger.h"
#include "exception.h"

typedef struct {
    char *key;
    char *value;
} php_wt_cursor_current_t;

typedef struct {
    zend_object std;
    zval *db;
    WT_CURSOR *cursor;
    php_wt_cursor_current_t current;
    zend_bool started_iterating;
    zend_bool finished_prev;
    zend_bool finished_next;
} php_wt_cursor_t;

extern PHP_WT_API zend_class_entry *php_wt_cursor_ce;

PHP_WT_API int php_wt_cursor_class_register(TSRMLS_D);
PHP_WT_API void php_wt_cursor_construct(zval *return_value, zval *db, char *uri, int uri_len, char *config, int config_len TSRMLS_DC);

#define PHP_WT_CURSOR_OBJ(self, obj, check) \
    do { \
        self = (php_wt_cursor_t *)zend_object_store_get_object(obj TSRMLS_CC); \
        if ((check) && !(self)->cursor) { \
            PHP_WT_EXCEPTION(0, "WiredTiger\\Cursor: Can not operate on closed cursor"); \
            return; \
        } \
    } while(0)

#endif
