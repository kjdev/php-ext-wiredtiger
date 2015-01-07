#ifndef PHP_WIREDTIGER_DB_H
#define PHP_WIREDTIGER_DB_H

#include "php_wiredtiger.h"
#include "wiredtiger.h"
#include "exception.h"

typedef struct {
    zend_object std;
    WT_CONNECTION *conn;
    WT_SESSION *session;
} php_wt_db_t;

extern PHP_WT_API zend_class_entry *php_wt_db_ce;

PHP_WT_API int php_wt_db_class_register(TSRMLS_D);
PHP_WT_API WT_SESSION * php_wt_db_object_get_session(zval *db TSRMLS_DC);

#define PHP_WT_DB_OBJ(self, obj, check) \
    do { \
        self = (php_wt_db_t *)zend_object_store_get_object(obj TSRMLS_CC); \
        if ((check) && !(self)->session) { \
            PHP_WT_EXCEPTION(0, "WiredTiger\\Db: Can not operate on closed session"); \
            return; \
        } \
    } while(0)

#endif
