#ifdef HAVE_CONFIG_H
#    include "config.h"
#endif

#include <php.h>
#include <php_ini.h>
#include <zend_extensions.h>
#include <ext/standard/file.h>
#include <ext/standard/php_filestat.h>
#include <ext/standard/php_smart_str.h>
#include "php_wiredtiger.h"
#include "db.h"

ZEND_EXTERN_MODULE_GLOBALS(wiredtiger)

zend_class_entry *php_wt_db_ce;
static zend_object_handlers php_wt_db_handlers;

ZEND_BEGIN_ARG_INFO_EX(arginfo_wt_db___construct, 0, ZEND_RETURN_VALUE, 0)
    ZEND_ARG_INFO(0, home)
    ZEND_ARG_INFO(0, config)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_wt_db_create, 0, ZEND_RETURN_VALUE, 1)
    ZEND_ARG_INFO(0, uri)
    ZEND_ARG_INFO(0, config)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_wt_db_open, 0, ZEND_RETURN_VALUE, 1)
    ZEND_ARG_INFO(0, uri)
    ZEND_ARG_INFO(0, config)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_wt_db_close, 0, ZEND_RETURN_VALUE, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_wt_db_drop, 0, ZEND_RETURN_VALUE, 1)
    ZEND_ARG_INFO(0, uri)
    ZEND_ARG_INFO(0, config)
ZEND_END_ARG_INFO()

/*
ZEND_BEGIN_ARG_INFO_EX(arginfo_wt_db_destroy, 0, ZEND_RETURN_VALUE, 1)
    ZEND_ARG_INFO(0, home)
ZEND_END_ARG_INFO()
*/

static int
php_wt_handle_error(WT_EVENT_HANDLER *handler, WT_SESSION *session,
                    int error, const char *message)
{
    TSRMLS_FETCH();
    (void)(handler);
    (void)(session);

    /* TODO: verbose error */
    if (message) {
        PHP_WT_ERR(E_STRICT, "%s", message);
    }

    return 0;
}

static int
php_wt_handle_message(WT_EVENT_HANDLER *handler, WT_SESSION *session,
                      const char *message)
{
    TSRMLS_FETCH();
    (void)(handler);
    (void)(session);

    /* TODO: verbose error */
    if (message) {
        PHP_WT_ERR(E_STRICT, "%p: %s", session, message);
    }

    return 0;
}

static int
php_wt_handle_progress(WT_EVENT_HANDLER *handler, WT_SESSION *session,
                       const char *operation, uint64_t progress)
{
    static int lastlen = 0;
    int len;
    char msg[128];
    TSRMLS_FETCH();

    (void)(handler);
    (void)(session);

    if (operation == NULL) {
        return 0;
    }

    if (progress == 0) {
        len = snprintf(msg, sizeof(msg), "%s", operation);
    } else {
        len = snprintf(msg, sizeof(msg), "%s: %" PRIu64, operation, progress);
    }

    if (lastlen > len) {
        memset(msg + len, ' ', (size_t)(lastlen - len));
        msg[lastlen] = '\0';
    }

    lastlen = len;

    /* TODO: verbose error */
    PHP_WT_ERR(E_STRICT, "%s", msg);

    return 0;
}

static int
php_wt_handle_close(WT_EVENT_HANDLER *handler, WT_SESSION *session,
                    WT_CURSOR *cursor)
{
    TSRMLS_FETCH();
    (void)(handler);
    (void)(session);
    (void)(cursor);
    return 0;
}


static WT_EVENT_HANDLER php_wt_event_handler = {
    php_wt_handle_error,
    php_wt_handle_message,
    php_wt_handle_progress,
    php_wt_handle_close
};

PHP_WT_ZEND_METHOD(Db, __construct)
{
    php_wt_db_t *intern;
    char *home = NULL, *config = NULL;
    int home_len = 0, config_len = 0;
    int ret;
    zval exists;
    long mode = 0777;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|ss",
                              &home, &home_len,
                              &config, &config_len) == FAILURE) {
        RETURN_FALSE;
    }

    if (!home || home_len == 0) {
        RETURN_FALSE;
    }

    PHP_WT_DB_OBJ(intern, getThis(), 0);

    /* Create home directory */
    php_stat(home, home_len, FS_EXISTS, &exists TSRMLS_CC);
    if (!Z_BVAL(exists)) {
        php_mkdir_ex(home, mode, 0 TSRMLS_CC);
    }

    /* Default config */
    if (!config) {
        config = "create";
    }

    /* Open a connection to the database */
    ret = wiredtiger_open(home, &php_wt_event_handler, config, &intern->conn);
    if (ret != 0) {
        PHP_WT_EXCEPTION(0, "WiredTiger\\Db: Object has not been "
                         "correctly initialized by its constructor: %s",
                         wiredtiger_strerror(ret));
        RETURN_FALSE;
    }

    ret = intern->conn->open_session(intern->conn, NULL, NULL, &intern->session);
    if (ret != 0) {
        PHP_WT_EXCEPTION(0, "WiredTiger\\Db: Can not open session: %s",
                         wiredtiger_strerror(ret));
        RETURN_FALSE;
    }
}

PHP_WT_ZEND_METHOD(Db, create)
{
    php_wt_db_t *intern;
    char *uri, *config = NULL;
    int uri_len, config_len = 0;
    int ret;
    smart_str conf = { 0 };

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|s",
                              &uri, &uri_len,
                              &config, &config_len) == FAILURE) {
        RETURN_FALSE;
    }

    if (!uri || uri_len == 0) {
        RETURN_FALSE;
    }

    /* Default key and value format: key_format=S,value_format=S */
    if (!config || config_len == 0) {
        smart_str_appendl(&conf, "key_format=S,value_format=S", 27);
    } else {
        char *t;
        size_t t_len;

        smart_str_appendl(&conf, config, config_len);

        php_strtolower(config, config_len);

        t = "key_format";
        t_len = strlen(t);
        if (!php_memnstr(config, t, t_len, config + config_len)) {
            smart_str_appendc(&conf, ',');
            smart_str_appendl(&conf, "key_format=S", 12);
        }

        t = "value_format";
        t_len = strlen(t);
        if (!php_memnstr(config, t, t_len, config + config_len)) {
            smart_str_appendc(&conf, ',');
            smart_str_appendl(&conf, "value_format=S", 14);
        }
    }
    smart_str_0(&conf);

    PHP_WT_DB_OBJ(intern, getThis(), 1);

    ret = intern->session->create(intern->session, uri, conf.c);
    if (ret != 0) {
        smart_str_free(&conf);
        PHP_WT_EXCEPTION(0, "WiredTiger\\Db: Can not create table: %s",
                         wiredtiger_strerror(ret));
        RETURN_FALSE;
    }

    smart_str_free(&conf);

    RETURN_TRUE;
}

PHP_WT_ZEND_METHOD(Db, open)
{
    php_wt_db_t *intern;
    char *uri, *config = NULL;
    int uri_len, config_len = 0;
    int ret;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|s",
                              &uri, &uri_len,
                              &config, &config_len) == FAILURE) {
        RETURN_FALSE;
    }

    if (!uri || uri_len == 0) {
        RETURN_FALSE;
    }

    PHP_WT_DB_OBJ(intern, getThis(), 1);

    php_wt_cursor_construct(return_value, getThis(),
                            uri, uri_len, config, config_len TSRMLS_CC);
}

PHP_WT_ZEND_METHOD(Db, close)
{
    php_wt_db_t *intern;
    int ret;

    if (zend_parse_parameters_none() == FAILURE) {
        RETURN_FALSE;
    }

    PHP_WT_DB_OBJ(intern, getThis(), 0);

    if (Z_REFCOUNT_P(getThis()) > 2) {
        PHP_WT_ERR(E_WARNING, "Used session objects");
        RETURN_FALSE;
    }

    if (intern->session) {
        ret = intern->session->close(intern->session, NULL);
        if (ret != 0) {
            PHP_WT_ERR(E_WARNING, "%s", wiredtiger_strerror(ret));
            RETURN_FALSE;
        }
        intern->session = NULL;
    }

    if (intern->conn) {
        ret = intern->conn->close(intern->conn, NULL);
        if (ret != 0) {
            PHP_WT_ERR(E_WARNING, "%s", wiredtiger_strerror(ret));
            RETURN_FALSE;
        }
        intern->conn = NULL;
    }

    RETURN_TRUE;
}

PHP_WT_ZEND_METHOD(Db, drop)
{
    php_wt_db_t *intern;
    char *uri, *config = NULL;
    int uri_len, config_len = 0;
    int ret;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|s",
                              &uri, &uri_len,
                              &config, &config_len) == FAILURE) {
        RETURN_FALSE;
    }

    if (!uri || uri_len == 0) {
        RETURN_FALSE;
    }

    PHP_WT_DB_OBJ(intern, getThis(), 1);

    ret = intern->session->drop(intern->session, uri, config);
    if (ret != 0) {
        PHP_WT_ERR(E_WARNING, "%s", wiredtiger_strerror(ret));
        RETURN_FALSE;
    }

    RETURN_TRUE;
}

/* TODO: destroy home directory
PHP_WT_ZEND_METHOD(Db, destroy)
{
    php_wt_db_t *intern;
    char *home;
    int home_len;
    zval exists;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s",
                              &home, &home_len) == FAILURE) {
        RETURN_FALSE;
    }

    if (!home || home_len == 0) {
        RETURN_FALSE;
    }

    php_stat(home, home_len, FS_EXISTS, &exists);
    if (!Z_BVAL(exists)) {
        RETURN_FALSE;
    }

    // destroy directory
}
*/

PHP_WT_API WT_SESSION *
php_wt_db_object_get_session(zval *db TSRMLS_DC)
{
    php_wt_db_t *intern;

    PHP_WT_DB_OBJ(intern, db, 0);

    if (intern->session) {
        return intern->session;
    } else {
        return NULL;
    }
}

static zend_function_entry php_wt_db_methods[] = {
    PHP_WT_ZEND_ME(Db, __construct, arginfo_wt_db___construct,
                   ZEND_ACC_PUBLIC | ZEND_ACC_CTOR)
    PHP_WT_ZEND_ME(Db, create, arginfo_wt_db_create, ZEND_ACC_PUBLIC)
    PHP_WT_ZEND_ME(Db, open, arginfo_wt_db_open, ZEND_ACC_PUBLIC)
    PHP_WT_ZEND_ME(Db, close, arginfo_wt_db_close, ZEND_ACC_PUBLIC)
    PHP_WT_ZEND_ME(Db, drop, arginfo_wt_db_drop, ZEND_ACC_PUBLIC)
    /*
    PHP_WT_ZEND_ME(Db, destroy, arginfo_wt_db_destroy,
                   ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    */
    ZEND_FE_END
};

static void
php_wt_db_free_storage(void *object TSRMLS_DC)
{
    php_wt_db_t *intern = (php_wt_db_t *)object;

    if (!intern) {
        return;
    }

    if (intern->session) {
        intern->session->close(intern->session, NULL);
    }

    if (intern->conn) {
        intern->conn->close(intern->conn, NULL);
    }

    zend_object_std_dtor(&intern->std TSRMLS_CC);
    efree(object);
}

static zend_object_value
php_wt_db_new_ex(zend_class_entry *ce, php_wt_db_t **ptr TSRMLS_DC)
{
    php_wt_db_t *intern;
    zend_object_value retval;

    intern = (php_wt_db_t *)emalloc(sizeof(php_wt_db_t));
    memset(intern, 0, sizeof(php_wt_db_t));
    if (ptr) {
        *ptr = intern;
    }

    zend_object_std_init(&intern->std, ce TSRMLS_CC);
    object_properties_init(&intern->std, ce);

    retval.handle = zend_objects_store_put(
        intern, (zend_objects_store_dtor_t)zend_objects_destroy_object,
        (zend_objects_free_object_storage_t)php_wt_db_free_storage,
        NULL TSRMLS_CC);
    retval.handlers = &php_wt_db_handlers;

    return retval;
}

static zend_object_value
php_wt_db_new(zend_class_entry *ce TSRMLS_DC)
{
    return php_wt_db_new_ex(ce, NULL TSRMLS_CC);
}

PHP_WT_API int
php_wt_db_class_register(TSRMLS_D)
{
    zend_class_entry ce;

    INIT_CLASS_ENTRY(ce, ZEND_NS_NAME(PHP_WT_NS, "Db"), php_wt_db_methods);

    ce.create_object = php_wt_db_new;

    php_wt_db_ce = zend_register_internal_class(&ce TSRMLS_CC);
    if (php_wt_db_ce == NULL) {
        return FAILURE;
    }

    memcpy(&php_wt_db_handlers, zend_get_std_object_handlers(),
           sizeof(zend_object_handlers));

    php_wt_db_handlers.clone_obj = NULL;

    return SUCCESS;
}
