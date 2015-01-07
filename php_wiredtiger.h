#ifndef PHP_WIREDTIGER_H
#define PHP_WIREDTIGER_H

#define PHP_WT_EXT_VERSION "0.0.0"

#define PHP_WT_NS "WiredTiger"
#define PHP_WT_ZEND_METHOD(classname, name) \
ZEND_METHOD(WiredTiger_##classname, name)
#define PHP_WT_ZEND_ME(classname, name, arg_info, flags) \
ZEND_ME(WiredTiger_##classname, name, arg_info, flags)
#define PHP_WT_ZEND_MALIAS(classname, name, alias, arg_info, flags) \
ZEND_MALIAS(WiredTiger_##classname, name, alias, arg_info, flags)
#define PHP_WT_LONG_CONSTANT(name, val) \
REGISTER_NS_LONG_CONSTANT(PHP_WT_NS, name, val, CONST_CS|CONST_PERSISTENT)
#define PHP_WT_STRING_CONSTANT(name, val) \
REGISTER_NS_STRING_CONSTANT(PHP_WT_NS, name, val, CONST_CS|CONST_PERSISTENT)

extern zend_module_entry wiredtiger_module_entry;
#define phpext_wiredtiger_ptr &wiredtiger_module_entry

#ifdef PHP_WIN32
#    define PHP_WT_API __declspec(dllexport)
#elif defined(__GNUC__) && __GNUC__ >= 4
#    define PHP_WT_API __attribute__ ((visibility("default")))
#else
#    define PHP_WT_API
#endif

#ifdef ZTS
#    include "TSRM.h"
#endif

ZEND_BEGIN_MODULE_GLOBALS(wiredtiger)
ZEND_END_MODULE_GLOBALS(wiredtiger)

#ifdef ZTS
#    define PHP_WT_G(v) TSRMG(wiredtiger_globals_id, zend_wiredtiger_globals *, v)
#else
#    define PHP_WT_G(v) (wiredtiger_globals.v)
#endif

#endif  /* PHP_WIREDTIGER_H */
