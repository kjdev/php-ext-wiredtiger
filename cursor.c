#ifdef HAVE_CONFIG_H
#    include "config.h"
#endif

#include <php.h>
#include <php_ini.h>
#include <zend_extensions.h>
#include <zend_interfaces.h>
#include <ext/standard/php_smart_str.h>
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

/*
ZEND_BEGIN_ARG_INFO_EX(arginfo_wt_cursor_set, 0, ZEND_RETURN_VALUE, 2)
    ZEND_ARG_INFO(0, key)
    ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO()
*/
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
    smart_str conf = { 0 };

    PHP_WT_CURSOR_OBJ(intern, obj, 0);

    intern->db = db;
    zval_add_ref(&intern->db);

    intern->finished_prev = 0;
    intern->finished_next = 0;

    session = php_wt_db_object_get_session(db TSRMLS_CC);
    if (!session) {
        PHP_WT_EXCEPTION(0, "WiredTiger\\Cursor: Invalid session object");
        return;
    }

    /* Use raw config */
    if (config) {
        smart_str_appendl(&conf, config, config_len);
        smart_str_appendc(&conf, ',');
    }
    smart_str_appendl(&conf, "raw", 3);
    smart_str_0(&conf);

    /* Check append */
    if (!php_memnstr(config, "append", 6, config + config_len)) {
        intern->append = 1;
    }

    ret = session->open_cursor(session, uri, NULL, conf.c, &intern->cursor);
    if (ret != 0) {
        smart_str_free(&conf);
        PHP_WT_EXCEPTION(0, "WiredTiger\\Cursor: Can not open cursor: %s",
                         wiredtiger_strerror(ret));
        return;
    }

    intern->current.key.data = NULL;
    intern->current.key.size = 0;
    intern->current.value.data = NULL;
    intern->current.value.size = 0;

    smart_str_free(&conf);
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

typedef struct php_wt_cursor_pack_item
{
    char *data;
    size_t size;
    size_t asize;
} php_wt_cursor_pack_item_t;


static void
php_wt_cursor_pack_item_free(php_wt_cursor_pack_item_t *item TSRMLS_DC)
{
    if (item->data) {
        efree(item->data);
        item->data = NULL;
    }
    item->size = 0;
    item->asize = 0;
}

static int
php_wt_cursor_pack_key_scalar(php_wt_cursor_t *intern,
                              php_wt_cursor_pack_item_t *item,
                              zval *key TSRMLS_DC)
{
    int ret;
    const char *format;
    size_t i, format_len;
    WT_PACK_STREAM *stream;

    format = intern->cursor->key_format;
    format_len = strlen(format);

    item->asize = format_len;

    for (i = 0; i < format_len; i++) {
        switch (format[i]) {
            case 'x':
                break;
            case 'b':
            case 'h':
            case 'i':
            case 'l':
            case 'q':
            case 'B':
            case 'H':
            case 'I':
            case 'L':
            case 'Q':
            case 'r':
            case 't':
                if (Z_TYPE_P(key) != IS_LONG) {
                    convert_to_long(key);
                }
                item->asize += 4;
                break;
            case 's':
            case 'S':
            case 'u':
                if (Z_TYPE_P(key) != IS_STRING) {
                    convert_to_string(key);
                }
                item->asize += Z_STRLEN_P(key);
                break;
        }
    }

    item->data = emalloc(item->asize);
    if (!item->data) {
        PHP_WT_ERR(E_WARNING, "Invalid allocate memory");
        return 1;
    }

    ret = wiredtiger_pack_start(intern->cursor->session,
                                intern->cursor->key_format,
                                item->data, item->asize, &stream);
    if (ret != 0) {
        PHP_WT_ERR(E_WARNING, "%s", wiredtiger_strerror(ret));
        return ret;
    }

    for (i = 0; i < format_len; i++) {
        switch (format[i]) {
            case 'x':
                break;
            case 'b':
            case 'h':
            case 'i':
            case 'l':
            case 'q':
                ret = wiredtiger_pack_int(stream, (int64_t)Z_LVAL_P(key));
                if (ret != 0) {
                    PHP_WT_ERR(E_WARNING, "%s", wiredtiger_strerror(ret));
                    return ret;
                }
                break;
            case 'B':
            case 'H':
            case 'I':
            case 'L':
            case 'Q':
            case 'r':
            case 't':
                ret = wiredtiger_pack_uint(stream, (uint64_t)Z_LVAL_P(key));
                if (ret != 0) {
                    PHP_WT_ERR(E_WARNING, "%s", wiredtiger_strerror(ret));
                    return ret;
                }
                break;
            case 's':
            case 'S':
                ret = wiredtiger_pack_str(stream, Z_STRVAL_P(key));
                if (ret != 0) {
                    PHP_WT_ERR(E_WARNING, "%s", wiredtiger_strerror(ret));
                    return ret;
                }
                break;
            case 'u': {
                WT_ITEM value;

                value.data = Z_STRVAL_P(key);
                value.size = Z_STRLEN_P(key);

                ret = wiredtiger_pack_item(stream, &value);
                if (ret != 0) {
                    PHP_WT_ERR(E_WARNING, "%s", wiredtiger_strerror(ret));
                    return ret;
                }
                break;
            }
        }
    }

    ret = wiredtiger_pack_close(stream, &item->size);
    if (ret != 0) {
        PHP_WT_ERR(E_WARNING, "%s", wiredtiger_strerror(ret));
    }

    return 0;
}

static int
php_wt_cursor_pack_key_array(php_wt_cursor_t *intern,
                             php_wt_cursor_pack_item_t *item,
                             zval *key TSRMLS_DC)
{
    int ret;
    const char *format;
    size_t i, format_len;
    HashTable *ht;
    HashPosition pos;
    zval **tmp;
    WT_PACK_STREAM *stream;

    format = intern->cursor->key_format;
    format_len = strlen(format);

    item->asize = format_len;

    ht = HASH_OF(key);
    zend_hash_internal_pointer_reset_ex(ht, &pos);

    for (i = 0; i < format_len; i++) {
        switch (format[i]) {
            case 'x':
                break;
            case 'b':
            case 'h':
            case 'i':
            case 'l':
            case 'q':
            case 'B':
            case 'H':
            case 'I':
            case 'L':
            case 'Q':
            case 'r':
            case 't':
                zend_hash_move_forward_ex(ht, &pos);
                item->asize += 4;
                break;
            case 's':
            case 'S':
            case 'u':
                if (zend_hash_get_current_data_ex(
                        ht, (void **)&tmp, &pos) != SUCCESS) {
                    break;
                }
                zend_hash_move_forward_ex(ht, &pos);

                if (Z_TYPE_PP(tmp) != IS_STRING) {
                    convert_to_string(*tmp);
                }
                item->asize += Z_STRLEN_PP(tmp);
                break;
        }
    }

    item->data = emalloc(item->asize);
    if (!item->data) {
        PHP_WT_ERR(E_WARNING, "Invalid allocate memory");
        return 1;
    }

    ret = wiredtiger_pack_start(intern->cursor->session,
                                intern->cursor->key_format,
                                item->data, item->asize, &stream);
    if (ret != 0) {
        PHP_WT_ERR(E_WARNING, "%s", wiredtiger_strerror(ret));
        return ret;
    }

    zend_hash_internal_pointer_reset_ex(ht, &pos);

    for (i = 0; i < format_len; i++) {
        switch (format[i]) {
            case 'x':
                break;
            case 'b':
            case 'h':
            case 'i':
            case 'l':
            case 'q':
                if (zend_hash_get_current_data_ex(
                        ht, (void **)&tmp, &pos) != SUCCESS) {
                    break;
                }
                zend_hash_move_forward_ex(ht, &pos);

                if (Z_TYPE_PP(tmp) != IS_LONG) {
                    convert_to_long(*tmp);
                }

                ret = wiredtiger_pack_int(stream, (int64_t)Z_LVAL_PP(tmp));
                if (ret != 0) {
                    PHP_WT_ERR(E_WARNING, "%s", wiredtiger_strerror(ret));
                    return ret;
                }
                break;
            case 'B':
            case 'H':
            case 'I':
            case 'L':
            case 'Q':
            case 'r':
            case 't':
                if (zend_hash_get_current_data_ex(
                        ht, (void **)&tmp, &pos) != SUCCESS) {
                    break;
                }
                zend_hash_move_forward_ex(ht, &pos);

                if (Z_TYPE_PP(tmp) != IS_LONG) {
                    convert_to_long(*tmp);
                }

                ret = wiredtiger_pack_uint(stream, (uint64_t)Z_LVAL_PP(tmp));
                if (ret != 0) {
                    PHP_WT_ERR(E_WARNING, "%s", wiredtiger_strerror(ret));
                    return ret;
                }
                break;
            case 's':
            case 'S':
                if (zend_hash_get_current_data_ex(
                        ht, (void **)&tmp, &pos) != SUCCESS) {
                    break;
                }
                zend_hash_move_forward_ex(ht, &pos);

                if (Z_TYPE_PP(tmp) != IS_STRING) {
                    convert_to_string(*tmp);
                }

                ret = wiredtiger_pack_str(stream, Z_STRVAL_PP(tmp));
                if (ret != 0) {
                    PHP_WT_ERR(E_WARNING, "%s", wiredtiger_strerror(ret));
                    return ret;
                }
                break;
            case 'u': {
                WT_ITEM value;

                if (zend_hash_get_current_data_ex(
                        ht, (void **)&tmp, &pos) != SUCCESS) {
                    break;
                }
                zend_hash_move_forward_ex(ht, &pos);

                if (Z_TYPE_PP(tmp) != IS_STRING) {
                    convert_to_string(*tmp);
                }

                value.data = Z_STRVAL_PP(tmp);
                value.size = Z_STRLEN_PP(tmp);

                ret = wiredtiger_pack_item(stream, &value);
                if (ret != 0) {
                    PHP_WT_ERR(E_WARNING, "%s", wiredtiger_strerror(ret));
                    return ret;
                }
                break;
            }
        }
    }

    ret = wiredtiger_pack_close(stream, &item->size);
    if (ret != 0) {
        PHP_WT_ERR(E_WARNING, "%s", wiredtiger_strerror(ret));
    }

    return 0;
}

static int
php_wt_cursor_pack_key(php_wt_cursor_t *intern,
                       php_wt_cursor_pack_item_t *item, zval *key TSRMLS_DC)
{
    if (Z_TYPE_P(key) == IS_ARRAY) {
        return php_wt_cursor_pack_key_array(intern, item, key TSRMLS_CC);
    } else {
        return php_wt_cursor_pack_key_scalar(intern, item, key TSRMLS_CC);
    }
}

static int
php_wt_cursor_pack_value(php_wt_cursor_t *intern,
                         php_wt_cursor_pack_item_t *item,
                         zval ***args, long argc TSRMLS_DC)
{
    int ret;
    const char *format;
    size_t i, j, format_len;
    WT_PACK_STREAM *stream;

    format = intern->cursor->value_format;
    format_len = strlen(format);

    item->asize = format_len;

    for (i = 0, j = 1; i < format_len; i++) {
        if (j >= argc) {
            break;
        }

        switch (format[i]) {
            case 'x':
                break;
            case 'b':
            case 'h':
            case 'i':
            case 'l':
            case 'q':
            case 'B':
            case 'H':
            case 'I':
            case 'L':
            case 'Q':
            case 'r':
            case 't':
                item->asize += 4;
                j++;
                break;
            case 's':
            case 'S':
            case 'u':
                if (Z_TYPE_P(*args[j]) != IS_STRING) {
                    convert_to_string(*args[j]);
                }
                item->asize += Z_STRLEN_P(*args[j]);
                j++;
                break;
        }
    }


    item->data = emalloc(item->asize);
    if (!item->data) {
        PHP_WT_ERR(E_WARNING, "Invalid allocate memory");
        return 1;
    }

    ret = wiredtiger_pack_start(intern->cursor->session,
                                intern->cursor->value_format,
                                item->data, item->asize, &stream);
    if (ret != 0) {
        PHP_WT_ERR(E_WARNING, "%s", wiredtiger_strerror(ret));
        return ret;
    }

    for (i = 0, j = 1; i < format_len; i++) {
        if (j >= argc) {
            break;
        }

        switch (format[i]) {
            case 'x':
                break;
            case 'b':
            case 'h':
            case 'i':
            case 'l':
            case 'q':
                if (Z_TYPE_P(*args[j]) != IS_LONG) {
                    convert_to_long(*args[j]);
                }

                ret = wiredtiger_pack_int(stream, (int64_t)Z_LVAL_P(*args[j]));
                if (ret != 0) {
                    PHP_WT_ERR(E_WARNING, "%s", wiredtiger_strerror(ret));
                    return ret;
                }
                j++;
                break;
            case 'B':
            case 'H':
            case 'I':
            case 'L':
            case 'Q':
            case 'r':
            case 't':
                if (Z_TYPE_P(*args[j]) != IS_LONG) {
                    convert_to_long(*args[j]);
                }

                ret = wiredtiger_pack_uint(stream, (uint64_t)Z_LVAL_P(*args[j]));
                if (ret != 0) {
                    PHP_WT_ERR(E_WARNING, "%s", wiredtiger_strerror(ret));
                    return ret;
                }
                j++;
                break;
            case 's':
            case 'S':
                if (Z_TYPE_P(*args[j]) != IS_STRING) {
                    convert_to_string(*args[j]);
                }

                ret = wiredtiger_pack_str(stream, Z_STRVAL_P(*args[j]));
                if (ret != 0) {
                    PHP_WT_ERR(E_WARNING, "%s", wiredtiger_strerror(ret));
                    return ret;
                }
                j++;
                break;
            case 'u': {
                WT_ITEM value = { 0, };

                if (Z_TYPE_P(*args[j]) != IS_STRING) {
                    convert_to_string(*args[j]);
                }

                value.data = Z_STRVAL_P(*args[j]);
                value.size = Z_STRLEN_P(*args[j]);

                ret = wiredtiger_pack_item(stream, &value);
                if (ret != 0) {
                    PHP_WT_ERR(E_WARNING, "%s", wiredtiger_strerror(ret));
                    return ret;
                }
                j++;
                break;
            }
        }
    }

    ret = wiredtiger_pack_close(stream, &item->size);
    if (ret != 0) {
        PHP_WT_ERR(E_WARNING, "%s", wiredtiger_strerror(ret));
    }

    return 0;
}

static int
php_wt_cursor_pack_value_array(php_wt_cursor_t *intern,
                               php_wt_cursor_pack_item_t *item,
                               zval *args TSRMLS_DC)
{
    int ret;
    const char *format;
    size_t i, format_len;
    HashTable *ht;
    HashPosition pos;
    zval **tmp;
    WT_PACK_STREAM *stream;

    format = intern->cursor->value_format;
    format_len = strlen(format);

    item->asize = format_len;

    ht = HASH_OF(args);
    zend_hash_internal_pointer_reset_ex(ht, &pos);

    for (i = 0; i < format_len; i++) {
        switch (format[i]) {
            case 'x':
                break;
            case 'b':
            case 'h':
            case 'i':
            case 'l':
            case 'q':
            case 'B':
            case 'H':
            case 'I':
            case 'L':
            case 'Q':
            case 'r':
            case 't':
                zend_hash_move_forward_ex(ht, &pos);
                item->asize += 4;
                break;
            case 's':
            case 'S':
            case 'u':
                if (zend_hash_get_current_data_ex(
                        ht, (void **)&tmp, &pos) != SUCCESS) {
                    break;
                }
                zend_hash_move_forward_ex(ht, &pos);

                if (Z_TYPE_PP(tmp) != IS_STRING) {
                    convert_to_string(*tmp);
                }

                item->asize += Z_STRLEN_PP(tmp);
                break;
        }
    }

    item->data = emalloc(item->asize);
    if (!item->data) {
        PHP_WT_ERR(E_WARNING, "Invalid allocate memory");
        return 1;
    }

    ret = wiredtiger_pack_start(intern->cursor->session,
                                intern->cursor->value_format,
                                item->data, item->asize, &stream);
    if (ret != 0) {
        PHP_WT_ERR(E_WARNING, "%s", wiredtiger_strerror(ret));
        return ret;
    }

    zend_hash_internal_pointer_reset_ex(ht, &pos);

    for (i = 0; i < format_len; i++) {
        switch (format[i]) {
            case 'x':
                break;
            case 'b':
            case 'h':
            case 'i':
            case 'l':
            case 'q':
                if (zend_hash_get_current_data_ex(
                        ht, (void **)&tmp, &pos) != SUCCESS) {
                    break;
                }
                zend_hash_move_forward_ex(ht, &pos);

                if (Z_TYPE_PP(tmp) != IS_LONG) {
                    convert_to_long(*tmp);
                }

                ret = wiredtiger_pack_int(stream, (int64_t)Z_LVAL_PP(tmp));
                if (ret != 0) {
                    PHP_WT_ERR(E_WARNING, "%s", wiredtiger_strerror(ret));
                    return ret;
                }
                break;
            case 'B':
            case 'H':
            case 'I':
            case 'L':
            case 'Q':
            case 'r':
            case 't':
                if (zend_hash_get_current_data_ex(
                        ht, (void **)&tmp, &pos) != SUCCESS) {
                    break;
                }
                zend_hash_move_forward_ex(ht, &pos);

                if (Z_TYPE_PP(tmp) != IS_LONG) {
                    convert_to_long(*tmp);
                }

                ret = wiredtiger_pack_uint(stream, (uint64_t)Z_LVAL_PP(tmp));
                if (ret != 0) {
                    PHP_WT_ERR(E_WARNING, "%s", wiredtiger_strerror(ret));
                    return ret;
                }
                break;
            case 's':
            case 'S':
                if (zend_hash_get_current_data_ex(
                        ht, (void **)&tmp, &pos) != SUCCESS) {
                    break;
                }
                zend_hash_move_forward_ex(ht, &pos);

                if (Z_TYPE_PP(tmp) != IS_STRING) {
                    convert_to_string(*tmp);
                }

                ret = wiredtiger_pack_str(stream, Z_STRVAL_PP(tmp));
                if (ret != 0) {
                    PHP_WT_ERR(E_WARNING, "%s", wiredtiger_strerror(ret));
                    return ret;
                }
                break;
            case 'u': {
                WT_ITEM value = { 0, };

                if (zend_hash_get_current_data_ex(
                        ht, (void **)&tmp, &pos) != SUCCESS) {
                    break;
                }
                zend_hash_move_forward_ex(ht, &pos);

                if (Z_TYPE_PP(tmp) != IS_STRING) {
                    convert_to_string(*tmp);
                }

                value.data = Z_STRVAL_PP(tmp);
                value.size = Z_STRLEN_PP(tmp);
                ret = wiredtiger_pack_item(stream, &value);
                if (ret != 0) {
                    PHP_WT_ERR(E_WARNING, "%s", wiredtiger_strerror(ret));
                    return ret;
                }
                break;
            }
        }
    }

    ret = wiredtiger_pack_close(stream, &item->size);
    if (ret != 0) {
        PHP_WT_ERR(E_WARNING, "%s", wiredtiger_strerror(ret));
    }

    return 0;
}

static int
php_wt_cursor_unpack_ex(php_wt_cursor_t *intern, zval *return_value,
                        const char *format, WT_ITEM *item TSRMLS_DC)
{
    int ret;
    size_t i, size, format_len, len = 0;
    zval *values;
    WT_PACK_STREAM *stream;

    MAKE_STD_ZVAL(values);
    array_init(values);

    ret = wiredtiger_unpack_start(intern->cursor->session, format,
                                  item->data, item->size, &stream);
    if (ret != 0) {
        PHP_WT_ERR(E_WARNING, "%s", wiredtiger_strerror(ret));
        return ret;
    }

    format_len = strlen(format);
    size = 0;

    for (i = 0; i < format_len; i++) {
        switch (format[i]) {
            case 'x':
                add_next_index_null(values);
                size = 0;
                break;
            case 'b':
            case 'h':
            case 'i':
            case 'l':
            case 'q': {
                int64_t value;
                ret = wiredtiger_unpack_int(stream, &value);
                if (ret != 0) {
                    PHP_WT_ERR(E_WARNING, "%s", wiredtiger_strerror(ret));
                    add_next_index_null(values);
                } else {
                    add_next_index_long(values, value);
                }
                size = 0;
                break;
            }
            case 'B':
            case 'H':
            case 'I':
            case 'L':
            case 'Q':
            case 'r':
            case 't': {
                uint64_t value;
                ret = wiredtiger_unpack_uint(stream, &value);
                if (ret != 0) {
                    PHP_WT_ERR(E_WARNING, "%s", wiredtiger_strerror(ret));
                    add_next_index_null(values);
                } else {
                    add_next_index_long(values, value);
                }
                size = 0;
                break;
            }
            case 's':
            case 'S': {
                const char *value = NULL;
                ret = wiredtiger_unpack_str(stream, &value);
                if (ret != 0) {
                    PHP_WT_ERR(E_WARNING, "%s", wiredtiger_strerror(ret));
                    add_next_index_null(values);
                } else {
                    len = strlen(value);
                    if (size && size < len) {
                        add_next_index_stringl(values, value, size, 1);
                    } else {
                        add_next_index_string(values, value, 1);
                    }
                }
                size = 0;
                break;
            }
            case 'u': {
                WT_ITEM value = { 0, };
                ret = wiredtiger_unpack_item(stream, &value);
                if (ret != 0) {
                    PHP_WT_ERR(E_WARNING, "%s", wiredtiger_strerror(ret));
                    add_next_index_null(values);
                } else {
                    add_next_index_stringl(values, value.data, value.size, 1);
                }
                size = 0;
                break;
            }
            default:
                if (isdigit(format[i])) {
                    if (size) {
                        size = (size * 10) + (format[i] - '0');
                    } else {
                        size = (format[i] - '0');
                    }
                } else {
                    size = 0;
                }
        }
    }

    ret = wiredtiger_pack_close(stream, &size);
    if (ret != 0) {
        PHP_WT_ERR(E_WARNING, "%s", wiredtiger_strerror(ret));
    }

    switch (zend_hash_num_elements(HASH_OF(values))) {
        case 0:
            RETVAL_FALSE;
            zval_ptr_dtor(&values);
            break;
        case 1: {
            zval **tmp;
            zend_hash_get_current_data(HASH_OF(values), (void **)&tmp);
            RETVAL_ZVAL(*tmp, 1, 1);
            zval_ptr_dtor(&values);
            break;
        }
        default:
            RETVAL_ZVAL(values, 1, 1);
            break;
    }

    return 0;
}

static int
php_wt_cursor_unpack_key(php_wt_cursor_t *intern, zval *return_value,
                         WT_ITEM *item TSRMLS_DC)
{
    return php_wt_cursor_unpack_ex(intern, return_value,
                                   intern->cursor->key_format, item TSRMLS_CC);
}

static int
php_wt_cursor_unpack_value(php_wt_cursor_t *intern, zval *return_value,
                           WT_ITEM *item TSRMLS_DC)
{
    return php_wt_cursor_unpack_ex(intern, return_value,
                                   intern->cursor->value_format, item TSRMLS_CC);
}


PHP_WT_ZEND_METHOD(Cursor, get)
{
    int ret;
    zval *key;
    php_wt_cursor_t *intern;
    php_wt_cursor_pack_item_t pk = { 0, };
    WT_ITEM item;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC,
                              "z", &key) == FAILURE) {
        RETURN_FALSE;
    }

    PHP_WT_CURSOR_OBJ(intern, getThis(), 1);

    /* Set key */
    ret = php_wt_cursor_pack_key(intern, &pk, key TSRMLS_CC);
    if (ret != 0) {
        php_wt_cursor_pack_item_free(&pk TSRMLS_CC);
        PHP_WT_EXCEPTION(0, "WiredTiger\\Cursor: Can not setting key");
        RETURN_FALSE;
    }

    item.data = pk.data;
    item.size = pk.size;

    intern->cursor->set_key(intern->cursor, &item);

    /* Get value */
    if (intern->cursor->search(intern->cursor) != 0) {
        RETVAL_FALSE;
    } else {
        ret = intern->cursor->get_value(intern->cursor, &item);
        if (ret != 0) {
            PHP_WT_ERR(E_WARNING, "%s", wiredtiger_strerror(ret));
            RETVAL_FALSE;
        }

        ret = php_wt_cursor_unpack_value(intern, return_value, &item TSRMLS_CC);
        if (ret != 0) {
            php_wt_cursor_pack_item_free(&pk TSRMLS_CC);
            PHP_WT_EXCEPTION(0, "WiredTiger\\Cursor: Can not getting value");
            RETURN_FALSE;
        }
    }

    php_wt_cursor_pack_item_free(&pk TSRMLS_CC);
}

PHP_WT_ZEND_METHOD(Cursor, set)
{
    php_wt_cursor_t *intern;
    zval ***args;
    long argc = ZEND_NUM_ARGS();
    php_wt_cursor_pack_item_t pk = { 0, }, pv = { 0, };
    int ret, autoindex = 0;
    WT_ITEM item;

    if (argc < 2) {
        WRONG_PARAM_COUNT;
    }

    args = emalloc(sizeof(zval **) * argc);
    if (zend_get_parameters_array_ex(argc, args) != SUCCESS) {
        efree(args);
        WRONG_PARAM_COUNT;
    }

    PHP_WT_CURSOR_OBJ(intern, getThis(), 0);
    if (!intern->cursor) {
        efree(args);
        PHP_WT_EXCEPTION(0, "WiredTiger\\Cursor: "
                         "Can not operate on closed cursor");
        return;
    }

    /* Set key */
    if (strcmp(intern->cursor->key_format, "r") == 0 &&
        Z_TYPE_P(*args[0]) == IS_LONG && Z_LVAL_P(*args[0]) == 0) {
        autoindex = 1; /* Not set key */
    } else {
        ret = php_wt_cursor_pack_key(intern, &pk, *args[0] TSRMLS_CC);
        if (ret != 0) {
            efree(args);
            php_wt_cursor_pack_item_free(&pk TSRMLS_CC);
            PHP_WT_EXCEPTION(0, "WiredTiger\\Cursor: Can not setting key");
            RETURN_FALSE;
        }

        item.data = pk.data;
        item.size = pk.size;

        intern->cursor->set_key(intern->cursor, &item);
    }

    /* Set value */
    if (argc == 2 && Z_TYPE_P(*args[1]) == IS_ARRAY) {
        ret = php_wt_cursor_pack_value_array(intern, &pv, *args[1] TSRMLS_CC);
    } else {
        ret = php_wt_cursor_pack_value(intern, &pv, args, argc TSRMLS_CC);
    }
    if (ret != 0) {
        efree(args);
        php_wt_cursor_pack_item_free(&pk TSRMLS_CC);
        php_wt_cursor_pack_item_free(&pv TSRMLS_CC);
        PHP_WT_EXCEPTION(0, "WiredTiger\\Cursor: Can not setting value");
        RETURN_FALSE;
    }

    item.data = pv.data;
    item.size = pv.size;

    intern->cursor->set_value(intern->cursor, &item);

    if (autoindex) {
        ret = intern->cursor->insert(intern->cursor);
    } else {
        ret = intern->cursor->update(intern->cursor);
    }

    if (ret != 0) {
        PHP_WT_ERR(E_WARNING, "%s", wiredtiger_strerror(ret));
        RETVAL_FALSE;
    } else {
        RETVAL_TRUE;
    }

    efree(args);
    php_wt_cursor_pack_item_free(&pk TSRMLS_CC);
    php_wt_cursor_pack_item_free(&pv TSRMLS_CC);
}

PHP_WT_ZEND_METHOD(Cursor, remove)
{
    int ret;
    zval *key;
    php_wt_cursor_t *intern;
    php_wt_cursor_pack_item_t pk = { 0, };
    WT_ITEM item;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC,
                              "z", &key) == FAILURE) {
        RETURN_FALSE;
    }

    PHP_WT_CURSOR_OBJ(intern, getThis(), 1);

    /* Set key */
    ret = php_wt_cursor_pack_key(intern, &pk, key TSRMLS_CC);
    if (ret != 0) {
        php_wt_cursor_pack_item_free(&pk TSRMLS_CC);
        PHP_WT_EXCEPTION(0, "WiredTiger\\Cursor: Can not setting key");
        RETURN_FALSE;
    }

    item.data = pk.data;
    item.size = pk.size;

    intern->cursor->set_key(intern->cursor, &item);

    ret = intern->cursor->search(intern->cursor);
    if (ret != 0) {
        PHP_WT_ERR(E_WARNING, "%s", wiredtiger_strerror(ret));
        RETVAL_FALSE;
    } else {
        ret = intern->cursor->remove(intern->cursor);
        if (ret != 0) {
            PHP_WT_ERR(E_WARNING, "%s", wiredtiger_strerror(ret));
            RETVAL_FALSE;
        } else {
            RETVAL_TRUE;
        }
    }

    php_wt_cursor_pack_item_free(&pk TSRMLS_CC);
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
    if (intern->current.key.data || intern->current.key.size != 0) {
        intern->current.key.data = NULL;
        intern->current.key.size = 0;
    }
    if (intern->current.value.data || intern->current.value.size != 0) {
        intern->current.value.data = NULL;
        intern->current.value.size = 0;
    }
}

static int
php_wt_cursor_load_current(php_wt_cursor_t *intern TSRMLS_DC)
{
    int ret;

    /* Get key */
    ret = intern->cursor->get_key(intern->cursor, &intern->current.key);
    if (ret != 0) {
        PHP_WT_EXCEPTION(0, "WiredTiger\\Cursor: Can not getting key: %s",
                         wiredtiger_strerror(ret));
        return FAILURE;
    }

    /* Get value */
    ret = intern->cursor->get_value(intern->cursor, &intern->current.value);
    if (ret != 0) {
        PHP_WT_EXCEPTION(0, "WiredTiger\\Cursor: Can not getting value");
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

    if (!intern->started_iterating) {
        php_wt_cursor_reset_current(intern TSRMLS_CC);

        if (intern->cursor->next(intern->cursor) != 0) {
            intern->finished_next = 1;
            RETURN_FALSE;
        }

        php_wt_cursor_load_current(intern TSRMLS_CC);
    }

    if (!intern->current.value.data) {
        RETURN_FALSE;
    } else if (intern->current.value.size == 0) {
        RETURN_EMPTY_STRING();
    } else {
        ret = php_wt_cursor_unpack_value(intern, return_value,
                                         &intern->current.value TSRMLS_CC);
        if (ret != 0) {
            PHP_WT_EXCEPTION(0, "WiredTiger\\Cursor: Can not getting value");
            RETURN_FALSE;
        }
    }
}

PHP_WT_ZEND_METHOD(Cursor, key)
{
    int ret;
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

   if (!intern->current.key.data) {
        RETURN_FALSE;
    } else if (intern->current.key.size == 0) {
        RETURN_EMPTY_STRING();
    } else {
       ret = php_wt_cursor_unpack_key(intern, return_value,
                                      &intern->current.key TSRMLS_CC);
       if (ret != 0) {
           PHP_WT_EXCEPTION(0, "WiredTiger\\Cursor: Can not getting key");
           RETURN_FALSE;
       }
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
        PHP_WT_EXCEPTION(0, "WiredTiger\\Cursor: Invalid rewind: %s",
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

    if (intern->current.key.data != NULL &&
        intern->current.value.data != NULL) {
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
        PHP_WT_EXCEPTION(0, "WiredTiger\\Cursor: Invalid last: %s",
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
    zval *key;
    int ret, near = 0;
    php_wt_cursor_t *intern;
    php_wt_cursor_pack_item_t pk = { 0, };
    WT_ITEM item;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC,
                              "z|b", &key, &near) == FAILURE) {
        RETURN_FALSE;
    }

    PHP_WT_CURSOR_OBJ(intern, getThis(), 1);

    php_wt_cursor_reset_current(intern TSRMLS_CC);

    /* Set key */
    ret = php_wt_cursor_pack_key(intern, &pk, key TSRMLS_CC);
    if (ret != 0) {
        php_wt_cursor_pack_item_free(&pk TSRMLS_CC);
        PHP_WT_EXCEPTION(0, "WiredTiger\\Cursor: Can not setting key");
        RETURN_FALSE;
    }

    item.data = pk.data;
    item.size = pk.size;

    intern->cursor->set_key(intern->cursor, &item);

    if (intern->cursor->search(intern->cursor) != 0) {
        if (near) {
            int exact;
            ret = intern->cursor->search_near(intern->cursor, &exact);
            if (ret != 0) {
                php_wt_cursor_pack_item_free(&pk TSRMLS_CC);
                RETURN_FALSE;
            }
        } else {
            php_wt_cursor_pack_item_free(&pk TSRMLS_CC);
            RETURN_FALSE;
        }
    }

    intern->finished_next = 0;
    intern->finished_prev = 0;

    php_wt_cursor_load_current(intern TSRMLS_CC);

    php_wt_cursor_pack_item_free(&pk TSRMLS_CC);

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
