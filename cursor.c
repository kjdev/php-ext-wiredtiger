#ifdef HAVE_CONFIG_H
#    include "config.h"
#endif

#include <php.h>
#include <php_ini.h>
#include <zend_extensions.h>
#include <zend_interfaces.h>
#include "php_wiredtiger.h"
#include "db.h"
#include "cursor.h"

ZEND_EXTERN_MODULE_GLOBALS(wiredtiger)

zend_class_entry *php_wt_cursor_ce;
static zend_object_handlers php_wt_cursor_handlers;

ZEND_BEGIN_ARG_INFO_EX(arginfo_wt_cursor___construct, 0, ZEND_RETURN_VALUE, 2)
    ZEND_ARG_INFO(0, db)
    ZEND_ARG_INFO(0, uri)
    ZEND_ARG_INFO(0, config)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_wt_cursor_get, 0, ZEND_RETURN_VALUE, 1)
    ZEND_ARG_INFO(0, key)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_wt_cursor_set, 0, ZEND_RETURN_VALUE, 2)
    ZEND_ARG_INFO(0, key)
    ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_wt_cursor_remove, 0, ZEND_RETURN_VALUE, 1)
    ZEND_ARG_INFO(0, key)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_wt_cursor_close, 0, ZEND_RETURN_VALUE, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_wt_cursor_seek, 0, ZEND_RETURN_VALUE, 1)
    ZEND_ARG_INFO(0, key)
    ZEND_ARG_INFO(0, near)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_wt_cursor_no_parameters, 0, ZEND_RETURN_VALUE, 0)
ZEND_END_ARG_INFO()


static void
php_wt_cursor_create(zval *obj, zval *db,
                     char *uri, int uri_len,
                     char *config, int config_len TSRMLS_DC)
{
    php_wt_cursor_t *intern;
    WT_SESSION *session = NULL;
    int ret;

    PHP_WT_CURSOR_OBJ(intern, obj, 0);

    intern->db = db;
    zval_add_ref(&intern->db);

    intern->finished_prev = 0;
    intern->finished_next = 0;

    session = php_wt_db_object_get_session(db TSRMLS_CC);
    if (!session) {
        PHP_WT_EXCEPTION(0, "WiredTiger\\Cursor invalid session object");
        return;
    }

    ret = session->open_cursor(session, uri, NULL, config, &intern->cursor);
    if (ret != 0) {
        PHP_WT_EXCEPTION(0, "WiredTiger\\Cursor opening a table: %s",
                         wiredtiger_strerror(ret));
        return;
    }

    intern->current.key = NULL;
    intern->current.value = NULL;
}

PHP_WT_ZEND_METHOD(Cursor, __construct)
{
    zval *db;
    char *uri = NULL, *config = NULL;
    int uri_len = 0, config_len = 0;
    int ret;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "Os|s",
                              &db, php_wt_db_ce, &uri, &uri_len,
                              &config, &config_len) == FAILURE) {
        RETURN_FALSE;
    }

    php_wt_cursor_create(getThis(), db,
                         uri, uri_len, config, config_len TSRMLS_CC);
}

PHP_WT_ZEND_METHOD(Cursor, get)
{
    php_wt_cursor_t *intern;
    char *key;
    int ret, key_len;
    const char *value = NULL;
    WT_ITEM item;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s",
                              &key, &key_len) == FAILURE) {
        RETURN_FALSE;
    }

    PHP_WT_CURSOR_OBJ(intern, getThis(), 1);

    intern->cursor->set_key(intern->cursor, key);

    /* TODO: format and unpack */
    if (intern->cursor->search(intern->cursor) == 0) {
        if ((ret = intern->cursor->get_value(intern->cursor, &value)) != 0) {
            PHP_WT_ERR(E_WARNING, "%s", wiredtiger_strerror(ret));
            RETURN_FALSE;
        }
        if (!value) {
            RETURN_FALSE;
        } else if (strlen(value) == 0) {
            RETURN_EMPTY_STRING();
        } else {
            RETURN_STRING(value, 1);
        }
    } else {
        RETURN_FALSE;
    }
}

PHP_WT_ZEND_METHOD(Cursor, set)
{
    php_wt_cursor_t *intern;
    char *key, *value;
    int ret, key_len, value_len;
    WT_ITEM item;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss",
                              &key, &key_len,
                              &value, &value_len) == FAILURE) {
        RETURN_FALSE;
    }

    PHP_WT_CURSOR_OBJ(intern, getThis(), 1);

    intern->cursor->set_key(intern->cursor, key);

    /* TODO: format and pack */
    intern->cursor->set_value(intern->cursor, value);

    if (intern->cursor->insert(intern->cursor) != 0) {
        PHP_WT_ERR(E_WARNING, "%s", wiredtiger_strerror(ret));
        RETURN_FALSE;
    }

    RETURN_TRUE;
}

PHP_WT_ZEND_METHOD(Cursor, remove)
{
    php_wt_cursor_t *intern;
    char *key;
    int ret, key_len;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s",
                              &key, &key_len) == FAILURE) {
        RETURN_FALSE;
    }

    PHP_WT_CURSOR_OBJ(intern, getThis(), 1);

    intern->cursor->set_key(intern->cursor, key);
    if (intern->cursor->search(intern->cursor) != 0) {
        RETURN_FALSE;
    }
    if ((ret = intern->cursor->remove(intern->cursor)) != 0) {
        PHP_WT_ERR(E_WARNING, "%s", wiredtiger_strerror(ret));
        RETURN_FALSE;
    }

    RETURN_TRUE;
}

PHP_WT_ZEND_METHOD(Cursor, close)
{
    php_wt_cursor_t *intern;
    int ret;

    if (zend_parse_parameters_none() == FAILURE) {
        RETURN_FALSE;
    }

    PHP_WT_CURSOR_OBJ(intern, getThis(), 0);

    if (intern->cursor) {
        ret = intern->cursor->close(intern->cursor);
        if (ret != 0) {
            PHP_WT_ERR(E_WARNING, "%s", wiredtiger_strerror(ret));
            RETURN_FALSE;
        }
        intern->cursor = NULL;
    }

    RETURN_TRUE;
}

static void
php_wt_cursor_reset_current(php_wt_cursor_t *intern TSRMLS_DC)
{
    if (intern->current.key) {
        intern->current.key = NULL;
    }
    if (intern->current.value) {
        intern->current.value = NULL;
    }
}

static int
php_wt_cursor_load_current(php_wt_cursor_t *intern TSRMLS_DC)
{
    int ret;

    ret = intern->cursor->get_key(intern->cursor, &intern->current.key);
    if (ret != 0) {
        PHP_WT_EXCEPTION(0, "WiredTiger\\Cursor invalid key: %s",
                         wiredtiger_strerror(ret));
        return FAILURE;
    }

    ret = intern->cursor->get_value(intern->cursor, &intern->current.value);
    if (ret != 0) {
        PHP_WT_EXCEPTION(0, "WiredTiger\\Cursor invalid value: %s",
                         wiredtiger_strerror(ret));
        return FAILURE;
    }

    intern->started_iterating = 1;

    return SUCCESS;
}

PHP_WT_ZEND_METHOD(Cursor, current)
{
    php_wt_cursor_t *intern;
    int ret;

    if (zend_parse_parameters_none() == FAILURE) {
        RETURN_FALSE;
    }

    PHP_WT_CURSOR_OBJ(intern, getThis(), 1);

    /* TODO */
    if (!intern->started_iterating) {
        php_wt_cursor_reset_current(intern TSRMLS_CC);

        if (intern->cursor->next(intern->cursor) != 0) {
            intern->finished_next = 1;
            RETURN_FALSE;
        }

        php_wt_cursor_load_current(intern TSRMLS_CC);
    }

    if (!intern->current.value) {
        RETURN_FALSE;
    } else if (strlen(intern->current.value) == 0) {
        RETURN_EMPTY_STRING();
    } else {
        RETURN_STRING(intern->current.value, 1);
    }
}

PHP_WT_ZEND_METHOD(Cursor, key)
{
    php_wt_cursor_t *intern;

    if (zend_parse_parameters_none() == FAILURE) {
        RETURN_FALSE;
    }

    PHP_WT_CURSOR_OBJ(intern, getThis(), 1);

   if (!intern->started_iterating) {
        php_wt_cursor_reset_current(intern TSRMLS_CC);

        if (intern->cursor->next(intern->cursor) != 0) {
            intern->finished_next = 1;
            RETURN_FALSE;
        }

        php_wt_cursor_load_current(intern TSRMLS_CC);
    }

   if (!intern->current.key) {
        RETURN_FALSE;
    } else if (strlen(intern->current.key) == 0) {
        RETURN_EMPTY_STRING();
    } else {
        RETURN_STRING(intern->current.key, 1);
    }
}

PHP_WT_ZEND_METHOD(Cursor, next)
{
    php_wt_cursor_t *intern;
    int ret;

    if (zend_parse_parameters_none() == FAILURE) {
        RETURN_FALSE;
    }

    PHP_WT_CURSOR_OBJ(intern, getThis(), 1);

    php_wt_cursor_reset_current(intern TSRMLS_CC);

    if (intern->finished_next) {
        RETURN_FALSE;
    }

    if (intern->cursor->next(intern->cursor) != 0) {
        intern->finished_next = 1;
        RETURN_FALSE;
    }
    intern->finished_prev = 0;

    php_wt_cursor_load_current(intern TSRMLS_CC);

    RETURN_TRUE;
}

PHP_WT_ZEND_METHOD(Cursor, rewind)
{
    php_wt_cursor_t *intern;
    int ret;

    if (zend_parse_parameters_none() == FAILURE) {
        RETURN_FALSE;
    }

    PHP_WT_CURSOR_OBJ(intern, getThis(), 1);

    php_wt_cursor_reset_current(intern TSRMLS_CC);

    ret = intern->cursor->reset(intern->cursor);
    if (ret != 0) {
        PHP_WT_EXCEPTION(0, "WiredTiger\\Cursor invalid rewind: %s",
                         wiredtiger_strerror(ret));
        RETURN_FALSE;
    }

    intern->finished_prev = 0;
    intern->finished_next = 0;

    if (intern->cursor->next(intern->cursor) != 0) {
        intern->finished_next = 1;
        RETURN_FALSE;
    }

    php_wt_cursor_load_current(intern TSRMLS_CC);

    RETURN_TRUE;
}

PHP_WT_ZEND_METHOD(Cursor, valid)
{
    php_wt_cursor_t *intern;
    int ret;

    if (zend_parse_parameters_none() == FAILURE) {
        RETURN_FALSE;
    }

    PHP_WT_CURSOR_OBJ(intern, getThis(), 1);

    if (intern->current.key != NULL && intern->current.value) {
        RETURN_TRUE;
    }

    RETURN_FALSE;
}

PHP_WT_ZEND_METHOD(Cursor, prev)
{
    php_wt_cursor_t *intern;
    int ret;

    if (zend_parse_parameters_none() == FAILURE) {
        RETURN_FALSE;
    }

    PHP_WT_CURSOR_OBJ(intern, getThis(), 1);

    php_wt_cursor_reset_current(intern TSRMLS_CC);

    if (intern->finished_prev) {
        RETURN_FALSE;
    }

    if (intern->cursor->prev(intern->cursor) != 0) {
        intern->finished_prev = 1;
        RETURN_FALSE;
    }
    intern->finished_next = 0;

    php_wt_cursor_load_current(intern TSRMLS_CC);

    RETURN_TRUE;
}

PHP_WT_ZEND_METHOD(Cursor, last)
{
    php_wt_cursor_t *intern;
    int ret;

    if (zend_parse_parameters_none() == FAILURE) {
        RETURN_FALSE;
    }

    PHP_WT_CURSOR_OBJ(intern, getThis(), 1);

    php_wt_cursor_reset_current(intern TSRMLS_CC);

    ret = intern->cursor->reset(intern->cursor);
    if (ret != 0) {
        PHP_WT_EXCEPTION(0, "WiredTiger\\Cursor invalid last: %s",
                         wiredtiger_strerror(ret));
        RETURN_FALSE;
    }

    while (intern->cursor->prev(intern->cursor) != 0);

    intern->finished_prev = 0;
    intern->finished_next = 1;

    php_wt_cursor_load_current(intern TSRMLS_CC);

    RETURN_TRUE;
}

PHP_WT_ZEND_METHOD(Cursor, seek)
{
    php_wt_cursor_t *intern;
    char *key;
    int ret, key_len, near = 0;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|b",
                              &key, &key_len, &near) == FAILURE) {
        RETURN_FALSE;
    }

    PHP_WT_CURSOR_OBJ(intern, getThis(), 1);

    php_wt_cursor_reset_current(intern TSRMLS_CC);

    intern->cursor->set_key(intern->cursor, key);

    if (intern->cursor->search(intern->cursor) != 0) {
        if (near) {
            int exact;
            ret = intern->cursor->search_near(intern->cursor, &exact);
            if (ret != 0) {
                RETURN_FALSE;
            }
        } else {
            RETURN_FALSE;
        }
    }

    intern->finished_next = 0;
    intern->finished_prev = 0;

    php_wt_cursor_load_current(intern TSRMLS_CC);

    RETURN_TRUE;
}

PHP_WT_API void
php_wt_cursor_construct(zval *return_value, zval *db,
                        char *uri, int uri_len,
                        char *config, int config_len TSRMLS_DC)
{
    object_init_ex(return_value, php_wt_cursor_ce);
    php_wt_cursor_create(return_value, db, uri, uri_len,
                         config, config_len TSRMLS_CC);
}

static zend_function_entry php_wt_cursor_methods[] = {
    PHP_WT_ZEND_ME(Cursor, __construct, arginfo_wt_cursor___construct,
                   ZEND_ACC_PUBLIC | ZEND_ACC_CTOR)
    PHP_WT_ZEND_ME(Cursor, get, arginfo_wt_cursor_get, ZEND_ACC_PUBLIC)
    PHP_WT_ZEND_ME(Cursor, set, arginfo_wt_cursor_set, ZEND_ACC_PUBLIC)
    PHP_WT_ZEND_ME(Cursor, remove, arginfo_wt_cursor_remove, ZEND_ACC_PUBLIC)
    PHP_WT_ZEND_ME(Cursor, close, arginfo_wt_cursor_close, ZEND_ACC_PUBLIC)
    PHP_WT_ZEND_ME(Cursor, current, arginfo_wt_cursor_no_parameters,
                   ZEND_ACC_PUBLIC)
    PHP_WT_ZEND_ME(Cursor, key, arginfo_wt_cursor_no_parameters,
                   ZEND_ACC_PUBLIC)
    PHP_WT_ZEND_ME(Cursor, next, arginfo_wt_cursor_no_parameters,
                   ZEND_ACC_PUBLIC)
    PHP_WT_ZEND_ME(Cursor, rewind, arginfo_wt_cursor_no_parameters,
                   ZEND_ACC_PUBLIC)
    PHP_WT_ZEND_ME(Cursor, valid, arginfo_wt_cursor_no_parameters,
                   ZEND_ACC_PUBLIC)
    PHP_WT_ZEND_ME(Cursor, prev, arginfo_wt_cursor_no_parameters,
                   ZEND_ACC_PUBLIC)
    PHP_WT_ZEND_ME(Cursor, last, arginfo_wt_cursor_no_parameters,
                   ZEND_ACC_PUBLIC)
    PHP_WT_ZEND_ME(Cursor, seek, arginfo_wt_cursor_seek, ZEND_ACC_PUBLIC)
    ZEND_FE_END
};

static void
php_wt_cursor_free_storage(void *object TSRMLS_DC)
{
    php_wt_cursor_t *intern = (php_wt_cursor_t *)object;

    if (!intern) {
        return;
    }

    /*
    if (intern->cursor) {
        intern->cursor->close(intern->cursor);
    }
    */

    if (intern->db) {
        zval_ptr_dtor(&intern->db);
    }

    zend_object_std_dtor(&intern->std TSRMLS_CC);
    efree(object);
}

static zend_object_value
php_wt_cursor_new_ex(zend_class_entry *ce,
                         php_wt_cursor_t **ptr TSRMLS_DC)
{
    php_wt_cursor_t *intern;
    zend_object_value retval;

    intern = (php_wt_cursor_t *)emalloc(sizeof(php_wt_cursor_t));
    memset(intern, 0, sizeof(php_wt_cursor_t));
    if (ptr) {
        *ptr = intern;
    }

    zend_object_std_init(&intern->std, ce TSRMLS_CC);
    object_properties_init(&intern->std, ce);

    retval.handle = zend_objects_store_put(
        intern, (zend_objects_store_dtor_t)zend_objects_destroy_object,
        (zend_objects_free_object_storage_t)php_wt_cursor_free_storage,
        NULL TSRMLS_CC);
    retval.handlers = &php_wt_cursor_handlers;

    return retval;
}

static zend_object_value
php_wt_cursor_new(zend_class_entry *ce TSRMLS_DC)
{
    return php_wt_cursor_new_ex(ce, NULL TSRMLS_CC);
}

PHP_WT_API int
php_wt_cursor_class_register(TSRMLS_D)
{
    zend_class_entry ce;

    INIT_CLASS_ENTRY(ce, ZEND_NS_NAME(PHP_WT_NS, "Cursor"),
                     php_wt_cursor_methods);

    ce.create_object = php_wt_cursor_new;

    php_wt_cursor_ce = zend_register_internal_class(&ce TSRMLS_CC);
    if (php_wt_cursor_ce == NULL) {
        return FAILURE;
    }
    zend_class_implements(php_wt_cursor_ce TSRMLS_CC, 1, zend_ce_iterator);

    memcpy(&php_wt_cursor_handlers, zend_get_std_object_handlers(),
           sizeof(zend_object_handlers));

    php_wt_cursor_handlers.clone_obj = NULL;

    return SUCCESS;
}
