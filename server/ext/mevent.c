/*
  +----------------------------------------------------------------------+
  | PHP Version 5                                                         |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2007 The PHP Group                                 |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,         |
  | that is bundled with this package in the file LICENSE, and is         |
  | available through the world-wide-web at the following url:             |
  | http://www.php.net/license/3_01.txt                                     |
  | If you did not receive a copy of the PHP license and are unable to     |
  | obtain it through the world-wide-web, please send a note to             |
  | license@php.net so we can mail you a copy immediately.                 |
  +----------------------------------------------------------------------+
  | Author:                                                                 |
  +----------------------------------------------------------------------+
*/

/* $Id: header,v 1.16.2.1.2.1 2007/01/01 19:32:09 iliaa Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "php_mevent.h"
#include "mevent.h"
#include "ClearSilver.h"

#define LONG_FIRST_BYTE                0x00000000000000ff
#define LONG_SECOND_BYTE               0x000000000000ff00
#define LONG_SECOND_SHORT              0x00000000ffff0000
#define MASK_REQUEST_ID                0x000000000fffffff

#define VERSION_AND_REQUEST_ID_LEN     4
#define REQUEST_COMMAND_LEN            2
#define FLAGS_LEN                      2
#define PLUGIN_NAME_LENGTH_LEN         4
#define VARIABLE_TYPE_LEN              4
#define VARIABLE_NAME_LENGTH_LEN       4
#define VARIABLE_VALUE_LENGTH_LEN      4

#define RETURN_ILLEGAL(a, b) {                                   \
    if ((a) < (b)) {                                             \
        add_assoc_long(return_value, "code", REP_ERR);           \
        return;                                                  \
    }                                                            \
}

#define SAFE_FREE(a) {                                           \
    if ((a) != NULL) {                                           \
        free(a);                                                 \
        (a) = NULL;                                              \
    }                                                            \
}

typedef enum {
    CNODE_TYPE_STRING = 100,
    CNODE_TYPE_BOOL,
    CNODE_TYPE_INT,
    CNODE_TYPE_INT64,
    CNODE_TYPE_FLOAT,
    CNODE_TYPE_DATETIME,
    CNODE_TYPE_TIMESTAMP,
    CNODE_TYPE_OBJECT,
    CNODE_TYPE_ARRAY,
    CNODE_TYPE_JS,
    CNODE_TYPE_SYMBOL,
    CNODE_TYPE_OID, /**< 12byte ObjectID (uint) */

    CNODE_TYPE_POINT = 120,
    CNODE_TYPE_BOX,
    CNODE_TYPE_PATH,
    CNODE_TYPE_TIME,
    CNODE_TYPE_BITOP
} CnodeType;

/* If you declare any globals in php_mevent.h uncomment this:
   ZEND_DECLARE_MODULE_GLOBALS(mevent)
*/

/* True global resources - no need for thread safety here */
static int le_mevent;
//static int le_mevent;

/* {{{ mevent_functions[]
 *
 * Every user visible function must have an entry in mevent_functions[].
 */
zend_function_entry mevent_functions[] = {
    PHP_FE(mevent_init_plugin,    NULL)
    PHP_FE(mevent_free,    NULL)
    PHP_FE(mevent_add_str,    NULL)
    PHP_FE(mevent_add_int,    NULL)
    PHP_FE(mevent_add_bool,    NULL)
    PHP_FE(mevent_add_float,    NULL)
    PHP_FE(mevent_trigger,    NULL)
    PHP_FE(mevent_result,    NULL)
    PHP_FE(mevent_decode,    NULL)
    PHP_FE(mevent_reply,    NULL)
    {NULL, NULL, NULL}    /* Must be the last line in mevent_functions[] */
};
/* }}} */

/* {{{ mevent_module_entry
 */
zend_module_entry mevent_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
    STANDARD_MODULE_HEADER,
#endif
    "mevent",
    mevent_functions,
    PHP_MINIT(mevent),
    PHP_MSHUTDOWN(mevent),
    PHP_RINIT(mevent),        /* Replace with NULL if there's nothing to do at request start */
    PHP_RSHUTDOWN(mevent),    /* Replace with NULL if there's nothing to do at request end */
    PHP_MINFO(mevent),
#if ZEND_MODULE_API_NO >= 20010901
    "1.0", /* Replace with version number for your extension */
#endif
    STANDARD_MODULE_PROPERTIES
};
/* }}} */

#ifdef COMPILE_DL_MEVENT
ZEND_GET_MODULE(mevent)
#endif


static void php_mevent_dtor(
    zend_rsrc_list_entry *rsrc TSRMLS_DC)
{
    mevent_t *mevent_p = (mevent_t*) rsrc->ptr;
    if (mevent_p) {
        mevent_free(mevent_p);
    }
}

static char* mutil_obj_attr(HDF *hdf, char*key)
{
    if (hdf == NULL || key == NULL)
        return NULL;

    HDF_ATTR *attr = hdf_obj_attr(hdf);
    while (attr != NULL) {
        if (!strcmp(attr->key, key)) {
            return attr->value;
        }
    }
    return NULL;
}

static long char2int16(char a, char b) {
    long h = ((long)a) & LONG_FIRST_BYTE;
    long l = ((long)b) & LONG_FIRST_BYTE;
    return ((h << 8) & LONG_SECOND_BYTE) | l;
}

static long char2int32(char a, char b, char c, char d) {
    long h = char2int16(a, b);
    long l = char2int16(c, d);
    return ((h << 16) & LONG_SECOND_SHORT) | l;
}

static char* int32_tostring(long n) {
    char *r = (char*)malloc(sizeof(char) * 4);
    int i = 0;
    for (; i < 4; i++) {
        r[3 - i] = (char)(n & LONG_FIRST_BYTE);
        n >>= 8;
    }
    return r;
}

static int mevent_equal(char *s1, char *s2, int l) {
    int i = 0;
    for (; i < l; i++) {
        if (s1[i] != s2[i]) {
            return 0;
        }
    }
    return 1;
}

static char* mevent_substring(char *s, int a, int b) {
    if (a > b) {
        return NULL;
    }
    char *r = (char*)malloc(sizeof(char) * (b - a + 1));
    int i = a;
    for (; i < b; i++) {
        r[i - a] = s[i];
    }
    r[i - a] = '\0';
    return r;
}

static void mevent_set_string(char *d, char *s, int l, int f) {
    if (d == NULL || s == NULL) {
        return;
    }
    int i = 0;
    for (; i < l; i++) {
        d[f + i] = s[i];
    }
}

static char* mevent_normal_string(char *s, int l) {
    char *r = (char*)malloc(sizeof(char) * (l + 1));
    int i = 0;
    for (; i < l; i++) {
        r[i] = s[i];
    }
    r[l] = '\0';
    return r;
}

static void mevent_zval_string(char *n, char **s, int *l) {
    SAFE_FREE(*s);
    *l = strlen(n);
    *s = (char*)malloc(sizeof(char) * (*l));
    int i = 0;
    for (; i < *l; i++) {
        *((*s) + i) = n[i];
    }
}

static void mevent_construct_packet(long request_id, long reply_code,
                                    char *hdf, int hdf_len, char **packet,
                                    int *packet_len) {
    char *request_id_string = int32_tostring(request_id);
    char *reply_code_string = int32_tostring(reply_code);
    long variable_type = DATA_TYPE_STRING;
    char *variable_type_string = int32_tostring(variable_type);
    long name_len = 4;
    char *name_len_string = int32_tostring(name_len);
    char *name = strdup("root");
    long vsize = hdf_len;
    char *vsize_string = int32_tostring(vsize);
    long len = 28 + vsize;
    char *len_string = int32_tostring(len);
    SAFE_FREE(*packet);
    *packet = (char*)malloc(sizeof(char) * len);
    mevent_set_string(*packet, request_id_string, 4, 0);
    mevent_set_string(*packet, reply_code_string, 4, 4);
    mevent_set_string(*packet, len_string, 4, 8);
    mevent_set_string(*packet, variable_type_string, 4, 12);
    mevent_set_string(*packet, name_len_string, 4, 16);
    mevent_set_string(*packet, name, 4, 20);
    mevent_set_string(*packet, vsize_string, 4, 24);
    mevent_set_string(*packet, hdf, hdf_len, 28);
    *packet_len = len;
    SAFE_FREE(request_id_string);
    SAFE_FREE(reply_code_string);
    SAFE_FREE(variable_type_string);
    SAFE_FREE(name_len_string);
    SAFE_FREE(name);
    SAFE_FREE(vsize_string);
    SAFE_FREE(len_string);
}

static void mevent_fetch_array(HDF *node, zval **re)
{
    if (node == NULL) return;

    char *key, *name, *type, *val, *n;
    zval *cre;
    int ctype;

    node = hdf_obj_child(node);

    while (node) {
        type = mutil_obj_attr(node, "type");
        if (type) ctype = atoi(type);
        else ctype = CNODE_TYPE_STRING;

        if (hdf_obj_child(node) ||
            ctype == CNODE_TYPE_ARRAY ||
            ctype == CNODE_TYPE_OBJECT) {
            key = hdf_obj_name(node) ? hdf_obj_name(node): "unkown";

            ALLOC_INIT_ZVAL(cre);
            array_init(cre);
            mevent_fetch_array(node, &cre);

            if (ctype == CNODE_TYPE_ARRAY) {
                /* convert cre ===> array (copied from ext/standard/array.c) */
                /* TODO memory leak? */
                zval *array, **entry;
                HashPosition pos;
                ALLOC_INIT_ZVAL(array);
                array_init_size(array, zend_hash_num_elements(Z_ARRVAL_P(cre)));

                zend_hash_internal_pointer_reset_ex(Z_ARRVAL_P(cre), &pos);
                while (zend_hash_get_current_data_ex(Z_ARRVAL_P(cre),
                                                     (void **)&entry,
                                                     &pos) == SUCCESS) {
                    zval_add_ref(entry);
                    zend_hash_next_index_insert(Z_ARRVAL_P(array), entry,
                                                sizeof(zval *), NULL);
                    zend_hash_move_forward_ex(Z_ARRVAL_P(cre), &pos);
                }
                add_assoc_zval(*re, key, array);
            } else {
                add_assoc_zval(*re, key, cre);
            }
        } else if ((val = hdf_obj_value(node)) != NULL) {
            name = hdf_obj_name(node);

            switch (ctype) {
            case CNODE_TYPE_INT:
            case CNODE_TYPE_BOOL:
            case CNODE_TYPE_INT64:
            case CNODE_TYPE_DATETIME:
            case CNODE_TYPE_TIMESTAMP:
                add_assoc_long(*re, name, atol(val));
                break;
            case CNODE_TYPE_FLOAT:
                add_assoc_double(*re, name, strtod(val, &n));
                break;
            default:
                add_assoc_stringl(*re, name, val, strlen(val), 1);
            }
        }

        node = hdf_obj_next(node);
    }
}

static void mevent_fetch_hdf(HashTable *hash, HDF *node, int *type) {
    if (hash == NULL || node == NULL) {
        return;
    }
    zval **data;
    HashPosition pointer;
    int is_array = 0;
    for (zend_hash_internal_pointer_reset_ex(hash, &pointer);
         zend_hash_get_current_data_ex(hash, (void**) &data, &pointer) == SUCCESS;
         zend_hash_move_forward_ex(hash, &pointer)) {
        char *key = NULL;
        int key_len = 0;
        long index = 0;
        int hash_key_type = zend_hash_get_current_key_ex(hash, &key, &key_len,
                                                         &index, 0, &pointer);
        char *normal_key = NULL;
        if (hash_key_type == HASH_KEY_IS_LONG) {
            is_array = 1;
        }
        switch (Z_TYPE_PP(data)) {
            case IS_NULL:
            break;
            case IS_LONG: {
                long lval = Z_LVAL_PP(data);
                if (hash_key_type == HASH_KEY_IS_STRING) {
                    normal_key = mevent_normal_string(key, key_len);
                } else if (hash_key_type == HASH_KEY_IS_LONG) {
                    normal_key = (char*)malloc(sizeof(char) * 64);
                    snprintf(normal_key, 64, "%d", index);
                }
                hdf_set_int_value(node, normal_key, lval);
                char node_type[64];
                snprintf(node_type, 64, "%d", CNODE_TYPE_INT);
                hdf_set_attr(node, normal_key, "type", node_type);
                SAFE_FREE(normal_key);
            }
            break;
            case IS_DOUBLE: {
                double dval = Z_DVAL_PP(data);
                if (hash_key_type == HASH_KEY_IS_STRING) {
                    normal_key = mevent_normal_string(key, key_len);
                } else if (hash_key_type == HASH_KEY_IS_LONG) {
                    normal_key = (char*)malloc(sizeof(char) * 64);
                    snprintf(normal_key, 64, "%d", index);
                }
                char str_dval[512];
                snprintf(str_dval, 512, "%f", dval);
                hdf_set_value(node, normal_key, str_dval);
                char node_type[64];
                snprintf(node_type, 64, "%d", CNODE_TYPE_FLOAT);
                hdf_set_attr(node, normal_key, "type", node_type);
                SAFE_FREE(normal_key);
            }
            break;
            case IS_BOOL:
            break;
            case IS_RESOURCE:
            break;
            case IS_STRING: {
                char *value = Z_STRVAL_PP(data);
                int value_len = Z_STRLEN_PP(data);
                char *normal_value = mevent_normal_string(value, value_len);
                if (hash_key_type == HASH_KEY_IS_STRING) {
                    normal_key = mevent_normal_string(key, key_len);
                } else if (hash_key_type == HASH_KEY_IS_LONG) {
                    normal_key = (char*)malloc(sizeof(char) * 64);
                    snprintf(normal_key, 64, "%d", index);
                }
                hdf_set_value(node, normal_key, normal_value);
                char node_type[64];
                snprintf(node_type, 64, "%d", CNODE_TYPE_STRING);
                hdf_set_attr(node, normal_key, "type", node_type);
                SAFE_FREE(normal_key);
                SAFE_FREE(normal_value);
            }
            break;
            case IS_ARRAY: {
                if (hash_key_type == HASH_KEY_IS_STRING) {
                    normal_key = mevent_normal_string(key, key_len);
                } else if (hash_key_type == HASH_KEY_IS_LONG) {
                    normal_key = (char*)malloc(sizeof(char) * 64);
                    snprintf(normal_key, 64, "%d", index);
                }
                HDF *child = NULL;
                hdf_get_node(node, normal_key, &child);
                int c_type;
                mevent_fetch_hdf(Z_ARRVAL_PP(data), child, &c_type);
                char node_type[64];
                snprintf(node_type, 64, "%d", c_type);
                hdf_set_attr(node, normal_key, "type", node_type);

                SAFE_FREE(normal_key);
            }
            break;
            case IS_OBJECT:
            break;
            default:
            break;
        }
    }
    *type = (is_array == 1) ? CNODE_TYPE_ARRAY : CNODE_TYPE_OBJECT;
}

/* {{{ PHP_INI
 */
/* Remove comments and fill if you need to have entries in php.ini
   PHP_INI_BEGIN()
   STD_PHP_INI_ENTRY("mevent.global_value",      "42", PHP_INI_ALL, OnUpdateLong, global_value, zend_mevent_globals, mevent_globals)
   STD_PHP_INI_ENTRY("mevent.global_string", "foobar", PHP_INI_ALL, OnUpdateString, global_string, zend_mevent_globals, mevent_globals)
   PHP_INI_END()
*/
/* }}} */

/* {{{ php_mevent_init_globals
 */
/* Uncomment this function if you have INI entries
   static void php_mevent_init_globals(zend_mevent_globals *mevent_globals)
   {
   mevent_globals->global_value = 0;
   mevent_globals->global_string = NULL;
   }
*/
/* }}} */

/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(mevent)
{

    /* If you have INI entries, uncomment these lines
       REGISTER_INI_ENTRIES();
    */
    le_mevent = zend_register_list_destructors_ex(
        php_mevent_dtor, NULL, PHP_MEVENT_RES_NAME,
        module_number);

    return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION
 */
PHP_MSHUTDOWN_FUNCTION(mevent)
{
    /* uncomment this line if you have INI entries
       UNREGISTER_INI_ENTRIES();
    */
    return SUCCESS;
}
/* }}} */

/* Remove if there's nothing to do at request start */
/* {{{ PHP_RINIT_FUNCTION
 */
PHP_RINIT_FUNCTION(mevent)
{
    return SUCCESS;
}
/* }}} */

/* Remove if there's nothing to do at request end */
/* {{{ PHP_RSHUTDOWN_FUNCTION
 */
PHP_RSHUTDOWN_FUNCTION(mevent)
{
    return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(mevent)
{
    php_info_print_table_start();
    php_info_print_table_header(2, "mevent support", "enabled");
    php_info_print_table_row(2, "Version", "2.1");
    php_info_print_table_row(2, "Copyright", "Hunantv.com");
    php_info_print_table_row(2, "author", "neo & bigml");
    php_info_print_table_end();

    /* Remove comments if you have entries in php.ini
       DISPLAY_INI_ENTRIES();
    */
}
/* }}} */


/* {{{ proto resource mevent_init_plugin(string ename)
 */
PHP_FUNCTION(mevent_init_plugin)
{
    int argc = ZEND_NUM_ARGS();
    char *ename = NULL;
    int ename_len;
    long cmd;
    int flags = 0;

    mevent_t *mevent_p;

    if (zend_parse_parameters(argc TSRMLS_CC, "s", &ename, &ename_len) == FAILURE)
        return;

    if (!strcmp(ename, ""))      ename       = "skeleton";

    mevent_p = mevent_init_plugin(ename, NULL);

    if (mevent_p == NULL)
        RETURN_LONG(-1);

    ZEND_REGISTER_RESOURCE(return_value, mevent_p, le_mevent);
}
/* }}} */


/* {{{ proto int mevent_free(resource db)
 */
PHP_FUNCTION(mevent_free)
{
    int argc = ZEND_NUM_ARGS();
    int db_id = -1;
    zval *db = NULL;
    mevent_t *mevent_p;
    int ret = -1;


    if (zend_parse_parameters(argc TSRMLS_CC, "r", &db) == FAILURE)
        return;

    if (db) {
        ZEND_FETCH_RESOURCE(mevent_p, mevent_t *, &db, db_id,
                            PHP_MEVENT_RES_NAME, le_mevent);
        zend_hash_index_del(&EG(regular_list),
                            Z_RESVAL_P(db));
        //mevent_free(db);
        RETURN_TRUE;
    }
    RETURN_LONG(-1);
}
/* }}} */


/* {{{ proto int mevent_add_str(resource db, string key, string val)
 */
PHP_FUNCTION(mevent_add_str)
{
    char *key = NULL;
    char *val = NULL;
    int argc = ZEND_NUM_ARGS();
    int db_id = -1;
    int key_len;
    int val_len;
    int ret = 0;
    zval *db = NULL;
    mevent_t *mevent_p;


    if (zend_parse_parameters(argc TSRMLS_CC, "rss", &db, &key, &key_len,
                              &val, &val_len) == FAILURE)
        return;

    if (db) {
        ZEND_FETCH_RESOURCE(mevent_p, mevent_t *, &db, db_id,
                            PHP_MEVENT_RES_NAME, le_mevent);
        if (mevent_p) {
            hdf_set_value(mevent_p->hdfsnd, key, val);
            char ztoka[64];
            snprintf(ztoka, 64, "%d", CNODE_TYPE_STRING);
            hdf_set_attr(mevent_p->hdfsnd, key, "type", ztoka);
            RETURN_LONG(0);
        }
    }
}
/* }}} */


/* {{{ proto int mevent_add_int(resource db, string key, int val)
 */
PHP_FUNCTION(mevent_add_int)
{
    char *key = NULL;

    int argc = ZEND_NUM_ARGS();
    int db_id = -1;
    int key_len;
    int parent_len;
    long val;
    int ret = 0;
    zval *db = NULL;
    mevent_t *mevent_p;


    if (zend_parse_parameters(argc TSRMLS_CC, "rsl", &db, &key,
                              &key_len, &val) == FAILURE)
        return;

    if (db) {
        ZEND_FETCH_RESOURCE(mevent_p, mevent_t *, &db, db_id,
                            PHP_MEVENT_RES_NAME, le_mevent);
        if (mevent_p) {
            hdf_set_int_value(mevent_p->hdfsnd, key, val);
            char ztoka[64];
            snprintf(ztoka, 64, "%d", CNODE_TYPE_INT);
            hdf_set_attr(mevent_p->hdfsnd, key, "type", ztoka);
            RETURN_LONG(0);
        }
    }
}
/* }}} */

/* {{{ proto int mevent_add_bool(resource db, string key, int val)
 */
PHP_FUNCTION(mevent_add_bool)
{
    char *key = NULL;

    int argc = ZEND_NUM_ARGS();
    int db_id = -1;
    int key_len;
    int parent_len;
    long val;
    int ret = 0;
    zval *db = NULL;
    mevent_t *mevent_p;


    if (zend_parse_parameters(argc TSRMLS_CC, "rsl", &db, &key,
                              &key_len, &val) == FAILURE)
        return;

    if (db) {
        ZEND_FETCH_RESOURCE(mevent_p, mevent_t *, &db, db_id,
                            PHP_MEVENT_RES_NAME, le_mevent);
        if (mevent_p) {
            hdf_set_int_value(mevent_p->hdfsnd, key, val);
            char ztoka[64];
            snprintf(ztoka, 64, "%d", CNODE_TYPE_BOOL);
            hdf_set_attr(mevent_p->hdfsnd, key, "type", ztoka);
            RETURN_LONG(0);
        }
    }
}
/* }}} */

/* {{{ proto int mevent_add_float(resource db, string key, double val)
 */
PHP_FUNCTION(mevent_add_float)
{
    char *key = NULL;

    int argc = ZEND_NUM_ARGS();
    int db_id = -1;
    int key_len;
    int parent_len;
    double val;
    int ret = 0;
    zval *db = NULL;
    mevent_t *mevent_p;


    if (zend_parse_parameters(argc TSRMLS_CC, "rsd", &db, &key,
                              &key_len, &val) == FAILURE)
        return;

    if (db) {
        ZEND_FETCH_RESOURCE(mevent_p, mevent_t *, &db, db_id,
                            PHP_MEVENT_RES_NAME, le_mevent);
        if (mevent_p) {
            char ztoka[64];
            snprintf(ztoka, 64, "%f", val);
            hdf_set_value(mevent_p->hdfsnd, key, ztoka);
            snprintf(ztoka, 64, "%d", CNODE_TYPE_FLOAT);
            hdf_set_attr(mevent_p->hdfsnd, key, "type", ztoka);
            RETURN_LONG(0);
        }
    }
}
/* }}} */

/* {{{ proto int mevent_trigger(resource db, string key, int cmd, int flags)
 */
PHP_FUNCTION(mevent_trigger)
{
    int argc = ZEND_NUM_ARGS();
    char *key; int key_len;
    double cmd, flags = 0;
    int db_id = -1;
    int ret = 0;

    zval *db = NULL;

    mevent_t *mevent_p;


    if (zend_parse_parameters(argc TSRMLS_CC, "rsdd", &db, &key, &key_len, &cmd, &flags) == FAILURE)
        return;

    if (db) {
        ZEND_FETCH_RESOURCE(mevent_p, mevent_t *, &db, db_id,
                            PHP_MEVENT_RES_NAME, le_mevent);
        if (mevent_p) {
            ret = mevent_trigger(mevent_p, key, cmd, flags);
            RETURN_LONG(ret);
        }
    }
}
/* }}} */


/* {{{ proto int mevent_fetch_array(resource db)
 */
PHP_FUNCTION(mevent_result)
{
    int argc = ZEND_NUM_ARGS();
    int db_id = -1;
    zval *db = NULL;

    //int i=0;
    mevent_t *mevent_p;

    if (zend_parse_parameters(argc TSRMLS_CC, "r", &db) == FAILURE)
        return;

    if (db) {
        ZEND_FETCH_RESOURCE(mevent_p, mevent_t *, &db, db_id,
                            PHP_MEVENT_RES_NAME, le_mevent);
        if (mevent_p) {

            array_init(return_value);

            if (mevent_p->hdfrcv != NULL) {
                mevent_fetch_array(mevent_p->hdfrcv, &return_value);
            }
        }
    }
}
/* }}} */

/* {{{ proto array mevent_decode(string packet)
 * decode packet, return an array to caller.
 * keys of array defined as follow:
 *
 * request_command
 * plugin_name
 * hdf
 * code
 */
PHP_FUNCTION(mevent_decode)
{
    char *packet; int len;
    int pos = 0;
    array_init(return_value);
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s",
        &packet, &len) == FAILURE) {
        return;
    }
    RETURN_ILLEGAL(len, VERSION_AND_REQUEST_ID_LEN
                   + REQUEST_COMMAND_LEN
                   + FLAGS_LEN
                   + PLUGIN_NAME_LENGTH_LEN);
    long version_and_request_id = char2int32(packet[pos], packet[pos + 1],
                                             packet[pos + 2], packet[pos + 3]);
    long request_id = version_and_request_id & MASK_REQUEST_ID;
    add_assoc_long(return_value, "request_id", request_id);
    pos += VERSION_AND_REQUEST_ID_LEN;
    long request_command = char2int16(packet[pos], packet[pos + 1]);
    pos += (REQUEST_COMMAND_LEN + FLAGS_LEN);
    long plugin_name_length = char2int32(packet[pos], packet[pos + 1],
                                         packet[pos + 2], packet[pos + 3]);
    pos += PLUGIN_NAME_LENGTH_LEN;
    RETURN_ILLEGAL(len, pos + plugin_name_length);
    long plugin_name_pos = pos;
    pos += plugin_name_length;
    RETURN_ILLEGAL(len, pos + VARIABLE_TYPE_LEN);
    long variable_type = char2int32(packet[pos], packet[pos + 1],
                                    packet[pos + 2], packet[pos + 3]);
    if (variable_type == DATA_TYPE_EOF) {
        add_assoc_long(return_value, "code", REP_ERR);
        return;
    }
    pos += VARIABLE_TYPE_LEN;
    RETURN_ILLEGAL(len, pos + VARIABLE_NAME_LENGTH_LEN);
    long variable_name_length = char2int32(packet[pos], packet[pos + 1],
                                           packet[pos + 2], packet[pos + 3]);
    pos += VARIABLE_NAME_LENGTH_LEN;
    RETURN_ILLEGAL(len, pos + variable_name_length);
    switch(variable_type) {
        case DATA_TYPE_EOF: {
            add_assoc_long(return_value, "code", REP_ERR);
        }
        break;
        case DATA_TYPE_U32: {
            add_assoc_long(return_value, "code", REP_ERR);
        }
        break;
        case DATA_TYPE_ULONG: {
            add_assoc_long(return_value, "code", REP_ERR);
        }
        break;
        case DATA_TYPE_STRING: {
            if (mevent_equal(packet + pos, "root", 4) == 1) {
                pos += variable_name_length;
                RETURN_ILLEGAL(len, pos + VARIABLE_VALUE_LENGTH_LEN);
                long variable_value_length = char2int32(packet[pos],
                                                        packet[pos + 1],
                                                        packet[pos + 2],
                                                        packet[pos + 3]);
                pos += VARIABLE_VALUE_LENGTH_LEN;
                RETURN_ILLEGAL(len, pos + variable_value_length);
                char *variable_value = mevent_substring(packet + pos, 0,
                                                        variable_value_length);
                HDF *hdf = NULL;
                hdf_init(&hdf);
                hdf_read_string(hdf, variable_value);
                SAFE_FREE(variable_value);
                zval *r;
                ALLOC_INIT_ZVAL(r);
                array_init(r);
                mevent_fetch_array(hdf, &r);
                hdf_destroy(&hdf);
                add_assoc_long(return_value, "request_command", request_command);
                add_assoc_stringl(return_value, "plugin_name",
                                  packet + plugin_name_pos,
                                  plugin_name_length, 1);
                add_assoc_zval(return_value, "hdf", r);
                add_assoc_long(return_value, "code", REP_OK);
            } else {
                add_assoc_long(return_value, "code", REP_ERR);
            }
        }
        break;
        case DATA_TYPE_ARRAY: {
            add_assoc_long(return_value, "code", REP_ERR);
        }
        break;
        default: {
            add_assoc_long(return_value, "code", REP_ERR);
        }
        break;
    }
}
/* }}} */

/* {{{ proto string mevent_reply(array arr1)
 */
PHP_FUNCTION(mevent_reply)
{
    zval *arr, **data;
    HashTable *arr_hash;
    HashPosition pointer;
    int arr_count;
    int has_request_id = 0;
    long request_id = 0;
    long reply_code = REP_ERR;
    char *packet = NULL;
    int packet_len = 0;
    char *hdf = NULL;
    int hdf_len = 0;
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "a", &arr) == FAILURE
        || arr == NULL) {
        RETURN_STRINGL(NULL, 0, 0);
    }
    arr_hash = Z_ARRVAL_P(arr);
    arr_count = zend_hash_num_elements(arr_hash);
    if (arr_count == 0) {
        RETURN_STRINGL(NULL, 0, 0);
    }
    for (zend_hash_internal_pointer_reset_ex(arr_hash, &pointer);
         zend_hash_get_current_data_ex(arr_hash, (void**) &data,
         &pointer) == SUCCESS;
         zend_hash_move_forward_ex(arr_hash, &pointer)) {
        zval temp;
        char *key;
        int key_len;
        long index;
        if (zend_hash_get_current_key_ex(arr_hash, &key, &key_len, &index, 0,
            &pointer) == HASH_KEY_IS_STRING) {
            if (mevent_equal(key, "request_id", key_len)) {
                has_request_id = 1;
                request_id = Z_LVAL_PP(data);
            } else if (mevent_equal(key, "hdf", key_len)) {
                char *normal_hdf = NULL;
                HDF *node = NULL;
                hdf_init(&node);
                HashTable *hash = Z_ARRVAL_PP(data);
                int type;
                mevent_fetch_hdf(hash, node, &type);
                hdf_write_string(node, &normal_hdf);
                hdf_destroy(&node);
                mevent_zval_string(normal_hdf, &hdf, &hdf_len);
                SAFE_FREE(normal_hdf);
            } else if (mevent_equal(key, "code", key_len)) {
                reply_code = Z_LVAL_PP(data);
            }
        }
    }
    if (has_request_id == 0) {
        RETURN_STRINGL(NULL, 0, 0);
    }
    mevent_construct_packet(request_id, reply_code, hdf, hdf_len, &packet,
                            &packet_len);
    SAFE_FREE(hdf);
    RETVAL_STRINGL(packet, packet_len, 1);
    SAFE_FREE(packet);
    return;
}
/* }}} */


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
