/*
 * php_liblpm.h - PHP extension for liblpm
 *
 * High-performance PHP bindings for the liblpm C library.
 * Provides IPv4 and IPv6 longest prefix match operations.
 *
 * Copyright (c) Murilo Chianfa
 * Licensed under the Boost Software License 1.0
 */

#ifndef PHP_LIBLPM_H
#define PHP_LIBLPM_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "zend_exceptions.h"
#include "zend_interfaces.h"

/* Include liblpm header */
#include <lpm/lpm.h>

#define PHP_LIBLPM_VERSION "1.0.0"
#define PHP_LIBLPM_EXTNAME "liblpm"

/* Module entry */
extern zend_module_entry liblpm_module_entry;
#define phpext_liblpm_ptr &liblpm_module_entry

/* Exception classes */
extern zend_class_entry *liblpm_exception_ce;
extern zend_class_entry *liblpm_invalid_prefix_exception_ce;
extern zend_class_entry *liblpm_operation_exception_ce;

/* Table classes */
extern zend_class_entry *liblpm_table_ipv4_ce;
extern zend_class_entry *liblpm_table_ipv6_ce;

/* Algorithm constants */
#define LIBLPM_ALGO_IPV4_DIR24    0
#define LIBLPM_ALGO_IPV4_8STRIDE  1
#define LIBLPM_ALGO_IPV6_WIDE16   0
#define LIBLPM_ALGO_IPV6_8STRIDE  1

/* Internal table object structure */
typedef struct {
    lpm_trie_t *trie;           /* C library handle */
    zend_bool is_ipv6;          /* IPv4 or IPv6 */
    zend_bool closed;           /* Prevent use-after-close */
    zend_object std;            /* Standard Zend object (must be last) */
} liblpm_table_object;

/* Resource type for procedural API */
typedef struct {
    lpm_trie_t *trie;
    zend_bool is_ipv6;
    zend_bool closed;
} liblpm_resource;

extern int le_liblpm_table;

/* Object handlers */
extern zend_object_handlers liblpm_table_object_handlers;

/* Helper to get custom object from zend_object */
static inline liblpm_table_object *liblpm_table_from_obj(zend_object *obj) {
    return (liblpm_table_object *)((char *)(obj) - XtOffsetOf(liblpm_table_object, std));
}

/* Helper macro for fetching object from zval */
#define Z_LIBLPM_TABLE_P(zv) liblpm_table_from_obj(Z_OBJ_P(zv))

/* Function declarations */
void liblpm_register_exception_classes(void);
void liblpm_register_table_classes(void);
void liblpm_register_procedural_functions(void);

/* String parsing utilities */
int liblpm_parse_ipv4_prefix(const char *str, uint8_t *addr, uint8_t *prefix_len);
int liblpm_parse_ipv6_prefix(const char *str, uint8_t *addr, uint8_t *prefix_len);
int liblpm_parse_ipv4_addr(const char *str, uint8_t *addr);
int liblpm_parse_ipv6_addr(const char *str, uint8_t *addr);

/* Thread safety for ZTS builds */
#ifdef ZTS
#include "TSRM.h"
#endif

#if defined(ZTS) && defined(COMPILE_DL_LIBLPM)
ZEND_TSRMLS_CACHE_EXTERN()
#endif

#endif /* PHP_LIBLPM_H */
