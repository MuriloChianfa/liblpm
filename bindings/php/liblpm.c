/*
 * liblpm.c - PHP extension for liblpm
 *
 * Main extension implementation providing both OOP and procedural APIs
 * for high-performance longest prefix match operations.
 *
 * Copyright (c) Murilo Chianfa
 * Licensed under the Boost Software License 1.0
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php_liblpm.h"

#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>

/* Exception class entries */
zend_class_entry *liblpm_exception_ce = NULL;
zend_class_entry *liblpm_invalid_prefix_exception_ce = NULL;
zend_class_entry *liblpm_operation_exception_ce = NULL;

/* Table class entries */
zend_class_entry *liblpm_table_ipv4_ce = NULL;
zend_class_entry *liblpm_table_ipv6_ce = NULL;

/* Object handlers */
zend_object_handlers liblpm_table_object_handlers;

/* Resource type ID for procedural API */
int le_liblpm_table;

#if defined(ZTS) && defined(COMPILE_DL_LIBLPM)
ZEND_TSRMLS_CACHE_DEFINE()
#endif

/* ============================================================================
 * STRING PARSING UTILITIES
 * ============================================================================ */

/**
 * Parse IPv4 address string to byte array
 */
int liblpm_parse_ipv4_addr(const char *str, uint8_t *addr)
{
    struct in_addr in;
    if (inet_pton(AF_INET, str, &in) != 1) {
        return 0;
    }
    memcpy(addr, &in.s_addr, 4);
    return 1;
}

/**
 * Parse IPv6 address string to byte array
 */
int liblpm_parse_ipv6_addr(const char *str, uint8_t *addr)
{
    struct in6_addr in6;
    if (inet_pton(AF_INET6, str, &in6) != 1) {
        return 0;
    }
    memcpy(addr, &in6.s6_addr, 16);
    return 1;
}

/**
 * Parse IPv4 CIDR prefix (e.g., "192.168.0.0/16")
 */
int liblpm_parse_ipv4_prefix(const char *str, uint8_t *addr, uint8_t *prefix_len)
{
    char addr_buf[64];
    const char *slash;
    size_t addr_len;
    long len;
    char *endptr;

    /* Find the '/' separator */
    slash = strchr(str, '/');
    if (!slash) {
        return 0;
    }

    /* Extract address part */
    addr_len = slash - str;
    if (addr_len >= sizeof(addr_buf)) {
        return 0;
    }
    memcpy(addr_buf, str, addr_len);
    addr_buf[addr_len] = '\0';

    /* Parse the prefix length */
    len = strtol(slash + 1, &endptr, 10);
    if (*endptr != '\0' || len < 0 || len > 32) {
        return 0;
    }
    *prefix_len = (uint8_t)len;

    /* Parse the address */
    return liblpm_parse_ipv4_addr(addr_buf, addr);
}

/**
 * Parse IPv6 CIDR prefix (e.g., "2001:db8::/32")
 */
int liblpm_parse_ipv6_prefix(const char *str, uint8_t *addr, uint8_t *prefix_len)
{
    char addr_buf[64];
    const char *slash;
    size_t addr_len;
    long len;
    char *endptr;

    /* Find the '/' separator */
    slash = strchr(str, '/');
    if (!slash) {
        return 0;
    }

    /* Extract address part */
    addr_len = slash - str;
    if (addr_len >= sizeof(addr_buf)) {
        return 0;
    }
    memcpy(addr_buf, str, addr_len);
    addr_buf[addr_len] = '\0';

    /* Parse the prefix length */
    len = strtol(slash + 1, &endptr, 10);
    if (*endptr != '\0' || len < 0 || len > 128) {
        return 0;
    }
    *prefix_len = (uint8_t)len;

    /* Parse the address */
    return liblpm_parse_ipv6_addr(addr_buf, addr);
}

/* ============================================================================
 * OBJECT HANDLERS
 * ============================================================================ */

/**
 * Create a new table object
 */
static zend_object *liblpm_table_create_object(zend_class_entry *ce)
{
    liblpm_table_object *intern;

    intern = zend_object_alloc(sizeof(liblpm_table_object), ce);

    intern->trie = NULL;
    intern->is_ipv6 = 0;
    intern->closed = 0;

    zend_object_std_init(&intern->std, ce);
    object_properties_init(&intern->std, ce);

    intern->std.handlers = &liblpm_table_object_handlers;

    return &intern->std;
}

/**
 * Free a table object
 */
static void liblpm_table_free_object(zend_object *object)
{
    liblpm_table_object *intern = liblpm_table_from_obj(object);

    if (intern->trie && !intern->closed) {
        lpm_destroy(intern->trie);
        intern->trie = NULL;
        intern->closed = 1;
    }

    zend_object_std_dtor(&intern->std);
}

/**
 * Clone handler - disabled (throw exception)
 */
static zend_object *liblpm_table_clone_object(zend_object *object)
{
    zend_throw_exception(liblpm_operation_exception_ce, 
        "Cloning LpmTable objects is not supported", 0);
    return NULL;
}

/* ============================================================================
 * RESOURCE DESTRUCTOR (for procedural API)
 * ============================================================================ */

static void liblpm_resource_dtor(zend_resource *rsrc)
{
    liblpm_resource *res = (liblpm_resource *)rsrc->ptr;
    if (res) {
        if (res->trie && !res->closed) {
            lpm_destroy(res->trie);
        }
        efree(res);
    }
}

/* ============================================================================
 * LpmTableIPv4 CLASS METHODS
 * ============================================================================ */

/* {{{ proto void LpmTableIPv4::__construct([int $algorithm])
   Create a new IPv4 routing table */
PHP_METHOD(LpmTableIPv4, __construct)
{
    zend_long algorithm = LIBLPM_ALGO_IPV4_DIR24;
    liblpm_table_object *intern;

    ZEND_PARSE_PARAMETERS_START(0, 1)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(algorithm)
    ZEND_PARSE_PARAMETERS_END();

    intern = Z_LIBLPM_TABLE_P(ZEND_THIS);
    intern->is_ipv6 = 0;
    intern->closed = 0;

    switch (algorithm) {
        case LIBLPM_ALGO_IPV4_DIR24:
            intern->trie = lpm_create_ipv4_dir24();
            break;
        case LIBLPM_ALGO_IPV4_8STRIDE:
            intern->trie = lpm_create_ipv4_8stride();
            break;
        default:
            zend_throw_exception_ex(liblpm_operation_exception_ce, 0,
                "Invalid algorithm: %ld", algorithm);
            return;
    }

    if (!intern->trie) {
        zend_throw_exception(liblpm_operation_exception_ce,
            "Failed to create IPv4 routing table", 0);
        return;
    }
}
/* }}} */

/* {{{ proto bool LpmTableIPv4::insert(string $prefix, int $nextHop)
   Insert a route into the table */
PHP_METHOD(LpmTableIPv4, insert)
{
    char *prefix;
    size_t prefix_len;
    zend_long next_hop;
    liblpm_table_object *intern;
    uint8_t addr[4];
    uint8_t plen;
    int result;

    ZEND_PARSE_PARAMETERS_START(2, 2)
        Z_PARAM_STRING(prefix, prefix_len)
        Z_PARAM_LONG(next_hop)
    ZEND_PARSE_PARAMETERS_END();

    intern = Z_LIBLPM_TABLE_P(ZEND_THIS);

    if (intern->closed || !intern->trie) {
        zend_throw_exception(liblpm_operation_exception_ce,
            "Cannot operate on closed table", 0);
        RETURN_FALSE;
    }

    if (!liblpm_parse_ipv4_prefix(prefix, addr, &plen)) {
        zend_throw_exception_ex(liblpm_invalid_prefix_exception_ce, 0,
            "Invalid IPv4 prefix: %s", prefix);
        RETURN_FALSE;
    }

    if (next_hop < 0 || next_hop > UINT32_MAX) {
        zend_throw_exception_ex(liblpm_invalid_prefix_exception_ce, 0,
            "Invalid next hop value: must be 0-%u", UINT32_MAX);
        RETURN_FALSE;
    }

    result = lpm_add(intern->trie, addr, plen, (uint32_t)next_hop);
    RETURN_BOOL(result == 0);
}
/* }}} */

/* {{{ proto bool LpmTableIPv4::delete(string $prefix)
   Delete a route from the table */
PHP_METHOD(LpmTableIPv4, delete)
{
    char *prefix;
    size_t prefix_len;
    liblpm_table_object *intern;
    uint8_t addr[4];
    uint8_t plen;
    int result;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_STRING(prefix, prefix_len)
    ZEND_PARSE_PARAMETERS_END();

    intern = Z_LIBLPM_TABLE_P(ZEND_THIS);

    if (intern->closed || !intern->trie) {
        zend_throw_exception(liblpm_operation_exception_ce,
            "Cannot operate on closed table", 0);
        RETURN_FALSE;
    }

    if (!liblpm_parse_ipv4_prefix(prefix, addr, &plen)) {
        zend_throw_exception_ex(liblpm_invalid_prefix_exception_ce, 0,
            "Invalid IPv4 prefix: %s", prefix);
        RETURN_FALSE;
    }

    result = lpm_delete(intern->trie, addr, plen);
    RETURN_BOOL(result == 0);
}
/* }}} */

/* {{{ proto int|false LpmTableIPv4::lookup(string $addr)
   Lookup an address in the table */
PHP_METHOD(LpmTableIPv4, lookup)
{
    char *addr_str;
    size_t addr_len;
    liblpm_table_object *intern;
    uint8_t addr[4];
    uint32_t result;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_STRING(addr_str, addr_len)
    ZEND_PARSE_PARAMETERS_END();

    intern = Z_LIBLPM_TABLE_P(ZEND_THIS);

    if (intern->closed || !intern->trie) {
        zend_throw_exception(liblpm_operation_exception_ce,
            "Cannot operate on closed table", 0);
        RETURN_FALSE;
    }

    if (!liblpm_parse_ipv4_addr(addr_str, addr)) {
        zend_throw_exception_ex(liblpm_invalid_prefix_exception_ce, 0,
            "Invalid IPv4 address: %s", addr_str);
        RETURN_FALSE;
    }

    /* Convert to host byte order uint32_t for lookup */
    uint32_t addr_u32 = ((uint32_t)addr[0] << 24) | ((uint32_t)addr[1] << 16) |
                        ((uint32_t)addr[2] << 8) | (uint32_t)addr[3];

    result = lpm_lookup_ipv4(intern->trie, addr_u32);

    if (result == LPM_INVALID_NEXT_HOP) {
        RETURN_FALSE;
    }

    RETURN_LONG((zend_long)result);
}
/* }}} */

/* {{{ proto array LpmTableIPv4::lookupBatch(array $addresses)
   Batch lookup addresses in the table */
PHP_METHOD(LpmTableIPv4, lookupBatch)
{
    zval *addresses;
    liblpm_table_object *intern;
    HashTable *ht;
    zval *entry;
    size_t count;
    uint32_t *addrs_array = NULL;
    uint32_t *results_array = NULL;
    size_t i = 0;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_ARRAY(addresses)
    ZEND_PARSE_PARAMETERS_END();

    intern = Z_LIBLPM_TABLE_P(ZEND_THIS);

    if (intern->closed || !intern->trie) {
        zend_throw_exception(liblpm_operation_exception_ce,
            "Cannot operate on closed table", 0);
        RETURN_FALSE;
    }

    ht = Z_ARRVAL_P(addresses);
    count = zend_hash_num_elements(ht);

    if (count == 0) {
        array_init(return_value);
        return;
    }

    /* Allocate arrays for C library */
    addrs_array = emalloc(count * sizeof(uint32_t));
    results_array = emalloc(count * sizeof(uint32_t));

    /* Parse addresses */
    ZEND_HASH_FOREACH_VAL(ht, entry) {
        uint8_t addr[4];
        zend_string *str;

        if (Z_TYPE_P(entry) != IS_STRING) {
            convert_to_string_ex(entry);
        }
        str = Z_STR_P(entry);

        if (!liblpm_parse_ipv4_addr(ZSTR_VAL(str), addr)) {
            efree(addrs_array);
            efree(results_array);
            zend_throw_exception_ex(liblpm_invalid_prefix_exception_ce, 0,
                "Invalid IPv4 address in batch: %s", ZSTR_VAL(str));
            RETURN_FALSE;
        }

        addrs_array[i] = ((uint32_t)addr[0] << 24) | ((uint32_t)addr[1] << 16) |
                         ((uint32_t)addr[2] << 8) | (uint32_t)addr[3];
        i++;
    } ZEND_HASH_FOREACH_END();

    /* Perform batch lookup */
    lpm_lookup_batch_ipv4(intern->trie, addrs_array, results_array, count);

    /* Build result array */
    array_init_size(return_value, count);
    for (i = 0; i < count; i++) {
        if (results_array[i] == LPM_INVALID_NEXT_HOP) {
            add_next_index_bool(return_value, 0);
        } else {
            add_next_index_long(return_value, (zend_long)results_array[i]);
        }
    }

    efree(addrs_array);
    efree(results_array);
}
/* }}} */

/* {{{ proto int LpmTableIPv4::size()
   Get the number of prefixes in the table */
PHP_METHOD(LpmTableIPv4, size)
{
    liblpm_table_object *intern;

    ZEND_PARSE_PARAMETERS_NONE();

    intern = Z_LIBLPM_TABLE_P(ZEND_THIS);

    if (intern->closed || !intern->trie) {
        RETURN_LONG(0);
    }

    RETURN_LONG((zend_long)intern->trie->num_prefixes);
}
/* }}} */

/* {{{ proto void LpmTableIPv4::close()
   Close the table and release resources */
PHP_METHOD(LpmTableIPv4, close)
{
    liblpm_table_object *intern;

    ZEND_PARSE_PARAMETERS_NONE();

    intern = Z_LIBLPM_TABLE_P(ZEND_THIS);

    if (intern->trie && !intern->closed) {
        lpm_destroy(intern->trie);
        intern->trie = NULL;
        intern->closed = 1;
    }
}
/* }}} */

/* ============================================================================
 * LpmTableIPv6 CLASS METHODS
 * ============================================================================ */

/* {{{ proto void LpmTableIPv6::__construct([int $algorithm])
   Create a new IPv6 routing table */
PHP_METHOD(LpmTableIPv6, __construct)
{
    zend_long algorithm = LIBLPM_ALGO_IPV6_WIDE16;
    liblpm_table_object *intern;

    ZEND_PARSE_PARAMETERS_START(0, 1)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(algorithm)
    ZEND_PARSE_PARAMETERS_END();

    intern = Z_LIBLPM_TABLE_P(ZEND_THIS);
    intern->is_ipv6 = 1;
    intern->closed = 0;

    switch (algorithm) {
        case LIBLPM_ALGO_IPV6_WIDE16:
            intern->trie = lpm_create_ipv6_wide16();
            break;
        case LIBLPM_ALGO_IPV6_8STRIDE:
            intern->trie = lpm_create_ipv6_8stride();
            break;
        default:
            zend_throw_exception_ex(liblpm_operation_exception_ce, 0,
                "Invalid algorithm: %ld", algorithm);
            return;
    }

    if (!intern->trie) {
        zend_throw_exception(liblpm_operation_exception_ce,
            "Failed to create IPv6 routing table", 0);
        return;
    }
}
/* }}} */

/* {{{ proto bool LpmTableIPv6::insert(string $prefix, int $nextHop)
   Insert a route into the table */
PHP_METHOD(LpmTableIPv6, insert)
{
    char *prefix;
    size_t prefix_len;
    zend_long next_hop;
    liblpm_table_object *intern;
    uint8_t addr[16];
    uint8_t plen;
    int result;

    ZEND_PARSE_PARAMETERS_START(2, 2)
        Z_PARAM_STRING(prefix, prefix_len)
        Z_PARAM_LONG(next_hop)
    ZEND_PARSE_PARAMETERS_END();

    intern = Z_LIBLPM_TABLE_P(ZEND_THIS);

    if (intern->closed || !intern->trie) {
        zend_throw_exception(liblpm_operation_exception_ce,
            "Cannot operate on closed table", 0);
        RETURN_FALSE;
    }

    if (!liblpm_parse_ipv6_prefix(prefix, addr, &plen)) {
        zend_throw_exception_ex(liblpm_invalid_prefix_exception_ce, 0,
            "Invalid IPv6 prefix: %s", prefix);
        RETURN_FALSE;
    }

    if (next_hop < 0 || next_hop > UINT32_MAX) {
        zend_throw_exception_ex(liblpm_invalid_prefix_exception_ce, 0,
            "Invalid next hop value: must be 0-%u", UINT32_MAX);
        RETURN_FALSE;
    }

    result = lpm_add(intern->trie, addr, plen, (uint32_t)next_hop);
    RETURN_BOOL(result == 0);
}
/* }}} */

/* {{{ proto bool LpmTableIPv6::delete(string $prefix)
   Delete a route from the table */
PHP_METHOD(LpmTableIPv6, delete)
{
    char *prefix;
    size_t prefix_len;
    liblpm_table_object *intern;
    uint8_t addr[16];
    uint8_t plen;
    int result;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_STRING(prefix, prefix_len)
    ZEND_PARSE_PARAMETERS_END();

    intern = Z_LIBLPM_TABLE_P(ZEND_THIS);

    if (intern->closed || !intern->trie) {
        zend_throw_exception(liblpm_operation_exception_ce,
            "Cannot operate on closed table", 0);
        RETURN_FALSE;
    }

    if (!liblpm_parse_ipv6_prefix(prefix, addr, &plen)) {
        zend_throw_exception_ex(liblpm_invalid_prefix_exception_ce, 0,
            "Invalid IPv6 prefix: %s", prefix);
        RETURN_FALSE;
    }

    result = lpm_delete(intern->trie, addr, plen);
    RETURN_BOOL(result == 0);
}
/* }}} */

/* {{{ proto int|false LpmTableIPv6::lookup(string $addr)
   Lookup an address in the table */
PHP_METHOD(LpmTableIPv6, lookup)
{
    char *addr_str;
    size_t addr_len;
    liblpm_table_object *intern;
    uint8_t addr[16];
    uint32_t result;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_STRING(addr_str, addr_len)
    ZEND_PARSE_PARAMETERS_END();

    intern = Z_LIBLPM_TABLE_P(ZEND_THIS);

    if (intern->closed || !intern->trie) {
        zend_throw_exception(liblpm_operation_exception_ce,
            "Cannot operate on closed table", 0);
        RETURN_FALSE;
    }

    if (!liblpm_parse_ipv6_addr(addr_str, addr)) {
        zend_throw_exception_ex(liblpm_invalid_prefix_exception_ce, 0,
            "Invalid IPv6 address: %s", addr_str);
        RETURN_FALSE;
    }

    result = lpm_lookup_ipv6(intern->trie, addr);

    if (result == LPM_INVALID_NEXT_HOP) {
        RETURN_FALSE;
    }

    RETURN_LONG((zend_long)result);
}
/* }}} */

/* {{{ proto array LpmTableIPv6::lookupBatch(array $addresses)
   Batch lookup addresses in the table */
PHP_METHOD(LpmTableIPv6, lookupBatch)
{
    zval *addresses;
    liblpm_table_object *intern;
    HashTable *ht;
    zval *entry;
    size_t count;
    uint8_t (*addrs_array)[16] = NULL;
    uint32_t *results_array = NULL;
    size_t i = 0;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_ARRAY(addresses)
    ZEND_PARSE_PARAMETERS_END();

    intern = Z_LIBLPM_TABLE_P(ZEND_THIS);

    if (intern->closed || !intern->trie) {
        zend_throw_exception(liblpm_operation_exception_ce,
            "Cannot operate on closed table", 0);
        RETURN_FALSE;
    }

    ht = Z_ARRVAL_P(addresses);
    count = zend_hash_num_elements(ht);

    if (count == 0) {
        array_init(return_value);
        return;
    }

    /* Allocate arrays for C library */
    addrs_array = emalloc(count * 16);
    results_array = emalloc(count * sizeof(uint32_t));

    /* Parse addresses */
    ZEND_HASH_FOREACH_VAL(ht, entry) {
        zend_string *str;

        if (Z_TYPE_P(entry) != IS_STRING) {
            convert_to_string_ex(entry);
        }
        str = Z_STR_P(entry);

        if (!liblpm_parse_ipv6_addr(ZSTR_VAL(str), addrs_array[i])) {
            efree(addrs_array);
            efree(results_array);
            zend_throw_exception_ex(liblpm_invalid_prefix_exception_ce, 0,
                "Invalid IPv6 address in batch: %s", ZSTR_VAL(str));
            RETURN_FALSE;
        }

        i++;
    } ZEND_HASH_FOREACH_END();

    /* Perform batch lookup */
    lpm_lookup_batch_ipv6(intern->trie, addrs_array, results_array, count);

    /* Build result array */
    array_init_size(return_value, count);
    for (i = 0; i < count; i++) {
        if (results_array[i] == LPM_INVALID_NEXT_HOP) {
            add_next_index_bool(return_value, 0);
        } else {
            add_next_index_long(return_value, (zend_long)results_array[i]);
        }
    }

    efree(addrs_array);
    efree(results_array);
}
/* }}} */

/* {{{ proto int LpmTableIPv6::size()
   Get the number of prefixes in the table */
PHP_METHOD(LpmTableIPv6, size)
{
    liblpm_table_object *intern;

    ZEND_PARSE_PARAMETERS_NONE();

    intern = Z_LIBLPM_TABLE_P(ZEND_THIS);

    if (intern->closed || !intern->trie) {
        RETURN_LONG(0);
    }

    RETURN_LONG((zend_long)intern->trie->num_prefixes);
}
/* }}} */

/* {{{ proto void LpmTableIPv6::close()
   Close the table and release resources */
PHP_METHOD(LpmTableIPv6, close)
{
    liblpm_table_object *intern;

    ZEND_PARSE_PARAMETERS_NONE();

    intern = Z_LIBLPM_TABLE_P(ZEND_THIS);

    if (intern->trie && !intern->closed) {
        lpm_destroy(intern->trie);
        intern->trie = NULL;
        intern->closed = 1;
    }
}
/* }}} */

/* ============================================================================
 * PROCEDURAL API FUNCTIONS
 * ============================================================================ */

/* {{{ proto resource|false lpm_create_ipv4([int $algorithm])
   Create a new IPv4 routing table */
PHP_FUNCTION(lpm_create_ipv4)
{
    zend_long algorithm = LIBLPM_ALGO_IPV4_DIR24;
    liblpm_resource *res;
    lpm_trie_t *trie;

    ZEND_PARSE_PARAMETERS_START(0, 1)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(algorithm)
    ZEND_PARSE_PARAMETERS_END();

    switch (algorithm) {
        case LIBLPM_ALGO_IPV4_DIR24:
            trie = lpm_create_ipv4_dir24();
            break;
        case LIBLPM_ALGO_IPV4_8STRIDE:
            trie = lpm_create_ipv4_8stride();
            break;
        default:
            php_error_docref(NULL, E_WARNING, "Invalid algorithm: %ld", algorithm);
            RETURN_FALSE;
    }

    if (!trie) {
        php_error_docref(NULL, E_WARNING, "Failed to create IPv4 routing table");
        RETURN_FALSE;
    }

    res = emalloc(sizeof(liblpm_resource));
    res->trie = trie;
    res->is_ipv6 = 0;
    res->closed = 0;

    RETURN_RES(zend_register_resource(res, le_liblpm_table));
}
/* }}} */

/* {{{ proto resource|false lpm_create_ipv6([int $algorithm])
   Create a new IPv6 routing table */
PHP_FUNCTION(lpm_create_ipv6)
{
    zend_long algorithm = LIBLPM_ALGO_IPV6_WIDE16;
    liblpm_resource *res;
    lpm_trie_t *trie;

    ZEND_PARSE_PARAMETERS_START(0, 1)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(algorithm)
    ZEND_PARSE_PARAMETERS_END();

    switch (algorithm) {
        case LIBLPM_ALGO_IPV6_WIDE16:
            trie = lpm_create_ipv6_wide16();
            break;
        case LIBLPM_ALGO_IPV6_8STRIDE:
            trie = lpm_create_ipv6_8stride();
            break;
        default:
            php_error_docref(NULL, E_WARNING, "Invalid algorithm: %ld", algorithm);
            RETURN_FALSE;
    }

    if (!trie) {
        php_error_docref(NULL, E_WARNING, "Failed to create IPv6 routing table");
        RETURN_FALSE;
    }

    res = emalloc(sizeof(liblpm_resource));
    res->trie = trie;
    res->is_ipv6 = 1;
    res->closed = 0;

    RETURN_RES(zend_register_resource(res, le_liblpm_table));
}
/* }}} */

/* {{{ proto bool lpm_insert(resource $table, string $prefix, int $nextHop)
   Insert a route into the table */
PHP_FUNCTION(lpm_insert)
{
    zval *zres;
    char *prefix;
    size_t prefix_len;
    zend_long next_hop;
    liblpm_resource *res;
    uint8_t addr[16];
    uint8_t plen;
    int result;

    ZEND_PARSE_PARAMETERS_START(3, 3)
        Z_PARAM_RESOURCE(zres)
        Z_PARAM_STRING(prefix, prefix_len)
        Z_PARAM_LONG(next_hop)
    ZEND_PARSE_PARAMETERS_END();

    res = (liblpm_resource *)zend_fetch_resource(Z_RES_P(zres), "liblpm table", le_liblpm_table);
    if (!res) {
        RETURN_FALSE;
    }

    if (res->closed || !res->trie) {
        php_error_docref(NULL, E_WARNING, "Cannot operate on closed table");
        RETURN_FALSE;
    }

    if (res->is_ipv6) {
        if (!liblpm_parse_ipv6_prefix(prefix, addr, &plen)) {
            php_error_docref(NULL, E_WARNING, "Invalid IPv6 prefix: %s", prefix);
            RETURN_FALSE;
        }
    } else {
        if (!liblpm_parse_ipv4_prefix(prefix, addr, &plen)) {
            php_error_docref(NULL, E_WARNING, "Invalid IPv4 prefix: %s", prefix);
            RETURN_FALSE;
        }
    }

    if (next_hop < 0 || next_hop > UINT32_MAX) {
        php_error_docref(NULL, E_WARNING, "Invalid next hop value");
        RETURN_FALSE;
    }

    result = lpm_add(res->trie, addr, plen, (uint32_t)next_hop);
    RETURN_BOOL(result == 0);
}
/* }}} */

/* {{{ proto bool lpm_delete(resource $table, string $prefix)
   Delete a route from the table */
PHP_FUNCTION(lpm_delete)
{
    zval *zres;
    char *prefix;
    size_t prefix_len;
    liblpm_resource *res;
    uint8_t addr[16];
    uint8_t plen;
    int result;

    ZEND_PARSE_PARAMETERS_START(2, 2)
        Z_PARAM_RESOURCE(zres)
        Z_PARAM_STRING(prefix, prefix_len)
    ZEND_PARSE_PARAMETERS_END();

    res = (liblpm_resource *)zend_fetch_resource(Z_RES_P(zres), "liblpm table", le_liblpm_table);
    if (!res) {
        RETURN_FALSE;
    }

    if (res->closed || !res->trie) {
        php_error_docref(NULL, E_WARNING, "Cannot operate on closed table");
        RETURN_FALSE;
    }

    if (res->is_ipv6) {
        if (!liblpm_parse_ipv6_prefix(prefix, addr, &plen)) {
            php_error_docref(NULL, E_WARNING, "Invalid IPv6 prefix: %s", prefix);
            RETURN_FALSE;
        }
    } else {
        if (!liblpm_parse_ipv4_prefix(prefix, addr, &plen)) {
            php_error_docref(NULL, E_WARNING, "Invalid IPv4 prefix: %s", prefix);
            RETURN_FALSE;
        }
    }

    result = lpm_delete(res->trie, addr, plen);
    RETURN_BOOL(result == 0);
}
/* }}} */

/* {{{ proto int|false lpm_lookup(resource $table, string $addr)
   Lookup an address in the table */
PHP_FUNCTION(lpm_lookup)
{
    zval *zres;
    char *addr_str;
    size_t addr_len;
    liblpm_resource *res;
    uint8_t addr[16];
    uint32_t result;

    ZEND_PARSE_PARAMETERS_START(2, 2)
        Z_PARAM_RESOURCE(zres)
        Z_PARAM_STRING(addr_str, addr_len)
    ZEND_PARSE_PARAMETERS_END();

    res = (liblpm_resource *)zend_fetch_resource(Z_RES_P(zres), "liblpm table", le_liblpm_table);
    if (!res) {
        RETURN_FALSE;
    }

    if (res->closed || !res->trie) {
        php_error_docref(NULL, E_WARNING, "Cannot operate on closed table");
        RETURN_FALSE;
    }

    if (res->is_ipv6) {
        if (!liblpm_parse_ipv6_addr(addr_str, addr)) {
            php_error_docref(NULL, E_WARNING, "Invalid IPv6 address: %s", addr_str);
            RETURN_FALSE;
        }
        result = lpm_lookup_ipv6(res->trie, addr);
    } else {
        if (!liblpm_parse_ipv4_addr(addr_str, addr)) {
            php_error_docref(NULL, E_WARNING, "Invalid IPv4 address: %s", addr_str);
            RETURN_FALSE;
        }
        uint32_t addr_u32 = ((uint32_t)addr[0] << 24) | ((uint32_t)addr[1] << 16) |
                            ((uint32_t)addr[2] << 8) | (uint32_t)addr[3];
        result = lpm_lookup_ipv4(res->trie, addr_u32);
    }

    if (result == LPM_INVALID_NEXT_HOP) {
        RETURN_FALSE;
    }

    RETURN_LONG((zend_long)result);
}
/* }}} */

/* {{{ proto array lpm_lookup_batch(resource $table, array $addresses)
   Batch lookup addresses in the table */
PHP_FUNCTION(lpm_lookup_batch)
{
    zval *zres;
    zval *addresses;
    liblpm_resource *res;
    HashTable *ht;
    zval *entry;
    size_t count;
    uint32_t *results_array = NULL;
    size_t i = 0;

    ZEND_PARSE_PARAMETERS_START(2, 2)
        Z_PARAM_RESOURCE(zres)
        Z_PARAM_ARRAY(addresses)
    ZEND_PARSE_PARAMETERS_END();

    res = (liblpm_resource *)zend_fetch_resource(Z_RES_P(zres), "liblpm table", le_liblpm_table);
    if (!res) {
        RETURN_FALSE;
    }

    if (res->closed || !res->trie) {
        php_error_docref(NULL, E_WARNING, "Cannot operate on closed table");
        RETURN_FALSE;
    }

    ht = Z_ARRVAL_P(addresses);
    count = zend_hash_num_elements(ht);

    if (count == 0) {
        array_init(return_value);
        return;
    }

    results_array = emalloc(count * sizeof(uint32_t));

    if (res->is_ipv6) {
        uint8_t (*addrs_array)[16] = emalloc(count * 16);

        ZEND_HASH_FOREACH_VAL(ht, entry) {
            zend_string *str;
            if (Z_TYPE_P(entry) != IS_STRING) {
                convert_to_string_ex(entry);
            }
            str = Z_STR_P(entry);

            if (!liblpm_parse_ipv6_addr(ZSTR_VAL(str), addrs_array[i])) {
                efree(addrs_array);
                efree(results_array);
                php_error_docref(NULL, E_WARNING, "Invalid IPv6 address in batch: %s", ZSTR_VAL(str));
                RETURN_FALSE;
            }
            i++;
        } ZEND_HASH_FOREACH_END();

        lpm_lookup_batch_ipv6(res->trie, addrs_array, results_array, count);
        efree(addrs_array);
    } else {
        uint32_t *addrs_array = emalloc(count * sizeof(uint32_t));

        ZEND_HASH_FOREACH_VAL(ht, entry) {
            uint8_t addr[4];
            zend_string *str;
            if (Z_TYPE_P(entry) != IS_STRING) {
                convert_to_string_ex(entry);
            }
            str = Z_STR_P(entry);

            if (!liblpm_parse_ipv4_addr(ZSTR_VAL(str), addr)) {
                efree(addrs_array);
                efree(results_array);
                php_error_docref(NULL, E_WARNING, "Invalid IPv4 address in batch: %s", ZSTR_VAL(str));
                RETURN_FALSE;
            }
            addrs_array[i] = ((uint32_t)addr[0] << 24) | ((uint32_t)addr[1] << 16) |
                             ((uint32_t)addr[2] << 8) | (uint32_t)addr[3];
            i++;
        } ZEND_HASH_FOREACH_END();

        lpm_lookup_batch_ipv4(res->trie, addrs_array, results_array, count);
        efree(addrs_array);
    }

    /* Build result array */
    array_init_size(return_value, count);
    for (i = 0; i < count; i++) {
        if (results_array[i] == LPM_INVALID_NEXT_HOP) {
            add_next_index_bool(return_value, 0);
        } else {
            add_next_index_long(return_value, (zend_long)results_array[i]);
        }
    }

    efree(results_array);
}
/* }}} */

/* {{{ proto int lpm_size(resource $table)
   Get the number of prefixes in the table */
PHP_FUNCTION(lpm_size)
{
    zval *zres;
    liblpm_resource *res;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_RESOURCE(zres)
    ZEND_PARSE_PARAMETERS_END();

    res = (liblpm_resource *)zend_fetch_resource(Z_RES_P(zres), "liblpm table", le_liblpm_table);
    if (!res) {
        RETURN_LONG(0);
    }

    if (res->closed || !res->trie) {
        RETURN_LONG(0);
    }

    RETURN_LONG((zend_long)res->trie->num_prefixes);
}
/* }}} */

/* {{{ proto void lpm_close(resource $table)
   Close the table and release resources */
PHP_FUNCTION(lpm_close)
{
    zval *zres;
    liblpm_resource *res;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_RESOURCE(zres)
    ZEND_PARSE_PARAMETERS_END();

    res = (liblpm_resource *)zend_fetch_resource(Z_RES_P(zres), "liblpm table", le_liblpm_table);
    if (!res) {
        return;
    }

    if (res->trie && !res->closed) {
        lpm_destroy(res->trie);
        res->trie = NULL;
        res->closed = 1;
    }
}
/* }}} */

/* {{{ proto string lpm_version()
   Get the liblpm library version */
PHP_FUNCTION(lpm_version)
{
    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_STRING(lpm_get_version());
}
/* }}} */

/* ============================================================================
 * ARGINFO DEFINITIONS (PHP 8.x requirement)
 * ============================================================================ */

/* LpmTableIPv4/IPv6 method arginfo */
ZEND_BEGIN_ARG_INFO_EX(arginfo_lpmtable_construct, 0, 0, 0)
    ZEND_ARG_TYPE_INFO(0, algorithm, IS_LONG, 1)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_lpmtable_insert, 0, 2, _IS_BOOL, 0)
    ZEND_ARG_TYPE_INFO(0, prefix, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO(0, nextHop, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_lpmtable_delete, 0, 1, _IS_BOOL, 0)
    ZEND_ARG_TYPE_INFO(0, prefix, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_lpmtable_lookup, 0, 0, 1)
    ZEND_ARG_TYPE_INFO(0, addr, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_lpmtable_lookupbatch, 0, 1, IS_ARRAY, 0)
    ZEND_ARG_TYPE_INFO(0, addresses, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_lpmtable_size, 0, 0, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_lpmtable_close, 0, 0, IS_VOID, 0)
ZEND_END_ARG_INFO()

/* Procedural function arginfo */
ZEND_BEGIN_ARG_INFO_EX(arginfo_lpm_create_ipv4, 0, 0, 0)
    ZEND_ARG_TYPE_INFO(0, algorithm, IS_LONG, 1)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_lpm_create_ipv6, 0, 0, 0)
    ZEND_ARG_TYPE_INFO(0, algorithm, IS_LONG, 1)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_lpm_insert, 0, 3, _IS_BOOL, 0)
    ZEND_ARG_INFO(0, table)
    ZEND_ARG_TYPE_INFO(0, prefix, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO(0, nextHop, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_lpm_delete, 0, 2, _IS_BOOL, 0)
    ZEND_ARG_INFO(0, table)
    ZEND_ARG_TYPE_INFO(0, prefix, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_lpm_lookup, 0, 0, 2)
    ZEND_ARG_INFO(0, table)
    ZEND_ARG_TYPE_INFO(0, addr, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_lpm_lookup_batch, 0, 2, IS_ARRAY, 0)
    ZEND_ARG_INFO(0, table)
    ZEND_ARG_TYPE_INFO(0, addresses, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_lpm_size, 0, 1, IS_LONG, 0)
    ZEND_ARG_INFO(0, table)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_lpm_close, 0, 1, IS_VOID, 0)
    ZEND_ARG_INFO(0, table)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_lpm_version, 0, 0, IS_STRING, 0)
ZEND_END_ARG_INFO()

/* ============================================================================
 * METHOD/FUNCTION TABLES
 * ============================================================================ */

/* LpmTableIPv4 method table */
static const zend_function_entry liblpm_table_ipv4_methods[] = {
    PHP_ME(LpmTableIPv4, __construct, arginfo_lpmtable_construct, ZEND_ACC_PUBLIC)
    PHP_ME(LpmTableIPv4, insert, arginfo_lpmtable_insert, ZEND_ACC_PUBLIC)
    PHP_ME(LpmTableIPv4, delete, arginfo_lpmtable_delete, ZEND_ACC_PUBLIC)
    PHP_ME(LpmTableIPv4, lookup, arginfo_lpmtable_lookup, ZEND_ACC_PUBLIC)
    PHP_ME(LpmTableIPv4, lookupBatch, arginfo_lpmtable_lookupbatch, ZEND_ACC_PUBLIC)
    PHP_ME(LpmTableIPv4, size, arginfo_lpmtable_size, ZEND_ACC_PUBLIC)
    PHP_ME(LpmTableIPv4, close, arginfo_lpmtable_close, ZEND_ACC_PUBLIC)
    PHP_FE_END
};

/* LpmTableIPv6 method table */
static const zend_function_entry liblpm_table_ipv6_methods[] = {
    PHP_ME(LpmTableIPv6, __construct, arginfo_lpmtable_construct, ZEND_ACC_PUBLIC)
    PHP_ME(LpmTableIPv6, insert, arginfo_lpmtable_insert, ZEND_ACC_PUBLIC)
    PHP_ME(LpmTableIPv6, delete, arginfo_lpmtable_delete, ZEND_ACC_PUBLIC)
    PHP_ME(LpmTableIPv6, lookup, arginfo_lpmtable_lookup, ZEND_ACC_PUBLIC)
    PHP_ME(LpmTableIPv6, lookupBatch, arginfo_lpmtable_lookupbatch, ZEND_ACC_PUBLIC)
    PHP_ME(LpmTableIPv6, size, arginfo_lpmtable_size, ZEND_ACC_PUBLIC)
    PHP_ME(LpmTableIPv6, close, arginfo_lpmtable_close, ZEND_ACC_PUBLIC)
    PHP_FE_END
};

/* Procedural function table */
static const zend_function_entry liblpm_functions[] = {
    PHP_FE(lpm_create_ipv4, arginfo_lpm_create_ipv4)
    PHP_FE(lpm_create_ipv6, arginfo_lpm_create_ipv6)
    PHP_FE(lpm_insert, arginfo_lpm_insert)
    PHP_FE(lpm_delete, arginfo_lpm_delete)
    PHP_FE(lpm_lookup, arginfo_lpm_lookup)
    PHP_FE(lpm_lookup_batch, arginfo_lpm_lookup_batch)
    PHP_FE(lpm_size, arginfo_lpm_size)
    PHP_FE(lpm_close, arginfo_lpm_close)
    PHP_FE(lpm_version, arginfo_lpm_version)
    PHP_FE_END
};

/* ============================================================================
 * MODULE FUNCTIONS
 * ============================================================================ */

/* {{{ PHP_MINIT_FUNCTION */
PHP_MINIT_FUNCTION(liblpm)
{
    zend_class_entry ce;

    /* Register exception classes */
    INIT_CLASS_ENTRY(ce, "LpmException", NULL);
    liblpm_exception_ce = zend_register_internal_class_ex(&ce, zend_ce_exception);

    INIT_CLASS_ENTRY(ce, "LpmInvalidPrefixException", NULL);
    liblpm_invalid_prefix_exception_ce = zend_register_internal_class_ex(&ce, liblpm_exception_ce);

    INIT_CLASS_ENTRY(ce, "LpmOperationException", NULL);
    liblpm_operation_exception_ce = zend_register_internal_class_ex(&ce, liblpm_exception_ce);

    /* Register LpmTableIPv4 class */
    INIT_CLASS_ENTRY(ce, "LpmTableIPv4", liblpm_table_ipv4_methods);
    liblpm_table_ipv4_ce = zend_register_internal_class(&ce);
    liblpm_table_ipv4_ce->create_object = liblpm_table_create_object;

    /* Register class constants for LpmTableIPv4 */
    zend_declare_class_constant_long(liblpm_table_ipv4_ce, "ALGO_DIR24", sizeof("ALGO_DIR24") - 1, LIBLPM_ALGO_IPV4_DIR24);
    zend_declare_class_constant_long(liblpm_table_ipv4_ce, "ALGO_8STRIDE", sizeof("ALGO_8STRIDE") - 1, LIBLPM_ALGO_IPV4_8STRIDE);

    /* Register LpmTableIPv6 class */
    INIT_CLASS_ENTRY(ce, "LpmTableIPv6", liblpm_table_ipv6_methods);
    liblpm_table_ipv6_ce = zend_register_internal_class(&ce);
    liblpm_table_ipv6_ce->create_object = liblpm_table_create_object;

    /* Register class constants for LpmTableIPv6 */
    zend_declare_class_constant_long(liblpm_table_ipv6_ce, "ALGO_WIDE16", sizeof("ALGO_WIDE16") - 1, LIBLPM_ALGO_IPV6_WIDE16);
    zend_declare_class_constant_long(liblpm_table_ipv6_ce, "ALGO_8STRIDE", sizeof("ALGO_8STRIDE") - 1, LIBLPM_ALGO_IPV6_8STRIDE);

    /* Initialize object handlers */
    memcpy(&liblpm_table_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
    liblpm_table_object_handlers.offset = XtOffsetOf(liblpm_table_object, std);
    liblpm_table_object_handlers.free_obj = liblpm_table_free_object;
    liblpm_table_object_handlers.clone_obj = liblpm_table_clone_object;

    /* Register resource type */
    le_liblpm_table = zend_register_list_destructors_ex(liblpm_resource_dtor, NULL, "liblpm table", module_number);

    /* Register global constants for procedural API */
    REGISTER_LONG_CONSTANT("LPM_ALGO_IPV4_DIR24", LIBLPM_ALGO_IPV4_DIR24, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("LPM_ALGO_IPV4_8STRIDE", LIBLPM_ALGO_IPV4_8STRIDE, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("LPM_ALGO_IPV6_WIDE16", LIBLPM_ALGO_IPV6_WIDE16, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("LPM_ALGO_IPV6_8STRIDE", LIBLPM_ALGO_IPV6_8STRIDE, CONST_CS | CONST_PERSISTENT);

    return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION */
PHP_MSHUTDOWN_FUNCTION(liblpm)
{
    return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION */
PHP_MINFO_FUNCTION(liblpm)
{
    php_info_print_table_start();
    php_info_print_table_header(2, "liblpm support", "enabled");
    php_info_print_table_row(2, "Extension version", PHP_LIBLPM_VERSION);
    php_info_print_table_row(2, "Library version", lpm_get_version());
    php_info_print_table_row(2, "IPv4 algorithms", "DIR-24-8, 8-bit Stride");
    php_info_print_table_row(2, "IPv6 algorithms", "Wide 16-bit, 8-bit Stride");
    php_info_print_table_end();
}
/* }}} */

/* {{{ liblpm_module_entry */
zend_module_entry liblpm_module_entry = {
    STANDARD_MODULE_HEADER,
    PHP_LIBLPM_EXTNAME,
    liblpm_functions,
    PHP_MINIT(liblpm),
    PHP_MSHUTDOWN(liblpm),
    NULL,  /* RINIT */
    NULL,  /* RSHUTDOWN */
    PHP_MINFO(liblpm),
    PHP_LIBLPM_VERSION,
    STANDARD_MODULE_PROPERTIES
};
/* }}} */

#ifdef COMPILE_DL_LIBLPM
#ifdef ZTS
ZEND_TSRMLS_CACHE_DEFINE()
#endif
ZEND_GET_MODULE(liblpm)
#endif
