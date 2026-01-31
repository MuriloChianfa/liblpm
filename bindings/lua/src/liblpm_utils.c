/**
 * @file liblpm_utils.c
 * @brief Utility functions for Lua bindings - IP address and CIDR parsing
 *
 * This file provides functions to parse IPv4 and IPv6 addresses in various
 * formats (dotted-decimal, colon-hex, CIDR notation).
 *
 * @author Murilo Chianfa
 * @license Boost Software License 1.0
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>

/* ============================================================================
 * IPv4 Parsing
 * ============================================================================ */

/**
 * Parse an IPv4 address in dotted-decimal notation.
 *
 * @param str Input string (e.g., "192.168.1.1")
 * @param out Output buffer (4 bytes)
 * @return 0 on success, -1 on error
 */
int lpm_lua_parse_ipv4(const char *str, uint8_t out[4]) {
    if (str == NULL || out == NULL) {
        return -1;
    }
    
    unsigned int a, b, c, d;
    char extra;
    
    /* Parse dotted-decimal format */
    int n = sscanf(str, "%u.%u.%u.%u%c", &a, &b, &c, &d, &extra);
    
    if (n != 4) {
        return -1;
    }
    
    /* Validate ranges */
    if (a > 255 || b > 255 || c > 255 || d > 255) {
        return -1;
    }
    
    out[0] = (uint8_t)a;
    out[1] = (uint8_t)b;
    out[2] = (uint8_t)c;
    out[3] = (uint8_t)d;
    
    return 0;
}

/**
 * Parse an IPv4 CIDR notation string.
 *
 * @param str Input string (e.g., "192.168.0.0/16")
 * @param prefix Output prefix buffer (4 bytes)
 * @param prefix_len Output prefix length
 * @return 0 on success, -1 on error
 */
int lpm_lua_parse_ipv4_cidr(const char *str, uint8_t prefix[4], uint8_t *prefix_len) {
    if (str == NULL || prefix == NULL || prefix_len == NULL) {
        return -1;
    }
    
    /* Find the '/' separator */
    const char *slash = strchr(str, '/');
    if (slash == NULL) {
        return -1;
    }
    
    /* Parse the address part */
    size_t addr_len = slash - str;
    if (addr_len >= 16) {  /* Max IPv4 string length */
        return -1;
    }
    
    char addr_buf[16];
    memcpy(addr_buf, str, addr_len);
    addr_buf[addr_len] = '\0';
    
    if (lpm_lua_parse_ipv4(addr_buf, prefix) != 0) {
        return -1;
    }
    
    /* Parse the prefix length */
    const char *len_str = slash + 1;
    if (*len_str == '\0') {
        return -1;
    }
    
    char *endptr;
    errno = 0;
    long len = strtol(len_str, &endptr, 10);
    
    if (errno != 0 || *endptr != '\0' || len < 0 || len > 32) {
        return -1;
    }
    
    *prefix_len = (uint8_t)len;
    
    /* Zero out bits beyond the prefix length */
    if (*prefix_len < 32) {
        int full_bytes = *prefix_len / 8;
        int remaining_bits = *prefix_len % 8;
        
        if (remaining_bits > 0) {
            uint8_t mask = (uint8_t)(0xFF << (8 - remaining_bits));
            prefix[full_bytes] &= mask;
            full_bytes++;
        }
        
        for (int i = full_bytes; i < 4; i++) {
            prefix[i] = 0;
        }
    }
    
    return 0;
}

/* ============================================================================
 * IPv6 Parsing
 * ============================================================================ */

/**
 * Parse a 16-bit hexadecimal group.
 *
 * @param str Input string
 * @param end Pointer to end of parsed portion
 * @param value Output value
 * @return 0 on success, -1 on error
 */
static int parse_hex_group(const char *str, const char **end, uint16_t *value) {
    if (str == NULL || !isxdigit((unsigned char)*str)) {
        return -1;
    }
    
    char *endptr;
    errno = 0;
    unsigned long val = strtoul(str, &endptr, 16);
    
    if (errno != 0 || endptr == str || val > 0xFFFF) {
        return -1;
    }
    
    /* Check that we didn't consume more than 4 hex digits */
    if (endptr - str > 4) {
        return -1;
    }
    
    *value = (uint16_t)val;
    *end = endptr;
    return 0;
}

/**
 * Parse an IPv6 address in colon-hex notation.
 *
 * Supports:
 * - Full notation: "2001:0db8:0000:0000:0000:0000:0000:0001"
 * - Compressed: "2001:db8::1"
 * - Mixed: "::ffff:192.168.1.1"
 *
 * @param str Input string
 * @param out Output buffer (16 bytes)
 * @return 0 on success, -1 on error
 */
int lpm_lua_parse_ipv6(const char *str, uint8_t out[16]) {
    if (str == NULL || out == NULL) {
        return -1;
    }
    
    memset(out, 0, 16);
    
    uint16_t groups[8] = {0};
    int num_groups = 0;
    int double_colon_pos = -1;
    const char *p = str;
    
    /* Handle leading :: */
    if (p[0] == ':' && p[1] == ':') {
        double_colon_pos = 0;
        p += 2;
        if (*p == '\0') {
            /* Just "::" - all zeros */
            return 0;
        }
    } else if (*p == ':') {
        /* Single leading colon is invalid */
        return -1;
    }
    
    while (*p != '\0' && num_groups < 8) {
        /* Check for :: */
        if (p[0] == ':' && p[1] == ':') {
            if (double_colon_pos >= 0) {
                /* Only one :: allowed */
                return -1;
            }
            double_colon_pos = num_groups;
            p += 2;
            if (*p == '\0') {
                break;
            }
            continue;
        }
        
        /* Skip single colon between groups */
        if (*p == ':') {
            p++;
        }
        
        /* Check for embedded IPv4 (only valid in last 32 bits) */
        const char *dot = strchr(p, '.');
        if (dot != NULL && (dot - p) <= 3) {
            /* Parse embedded IPv4 */
            uint8_t ipv4[4];
            if (lpm_lua_parse_ipv4(p, ipv4) != 0) {
                return -1;
            }
            
            /* IPv4 takes 2 groups */
            if (num_groups > 6) {
                return -1;
            }
            
            groups[num_groups++] = ((uint16_t)ipv4[0] << 8) | ipv4[1];
            groups[num_groups++] = ((uint16_t)ipv4[2] << 8) | ipv4[3];
            break;
        }
        
        /* Parse hex group */
        const char *end;
        uint16_t value;
        if (parse_hex_group(p, &end, &value) != 0) {
            return -1;
        }
        
        groups[num_groups++] = value;
        p = end;
        
        if (*p == '\0' || (*p == '/' || *p == '%')) {
            break;
        }
        
        if (*p != ':') {
            return -1;
        }
    }
    
    /* Expand :: if present */
    if (double_colon_pos >= 0) {
        int num_zeros = 8 - num_groups;
        if (num_zeros < 0) {
            return -1;
        }
        
        /* Shift groups after :: */
        int groups_after = num_groups - double_colon_pos;
        for (int i = 7; i >= double_colon_pos + num_zeros; i--) {
            groups[i] = groups[i - num_zeros];
        }
        
        /* Fill with zeros */
        for (int i = double_colon_pos; i < double_colon_pos + num_zeros; i++) {
            groups[i] = 0;
        }
    } else if (num_groups != 8) {
        return -1;
    }
    
    /* Convert to bytes */
    for (int i = 0; i < 8; i++) {
        out[i * 2] = (uint8_t)(groups[i] >> 8);
        out[i * 2 + 1] = (uint8_t)(groups[i] & 0xFF);
    }
    
    return 0;
}

/**
 * Parse an IPv6 CIDR notation string.
 *
 * @param str Input string (e.g., "2001:db8::/32")
 * @param prefix Output prefix buffer (16 bytes)
 * @param prefix_len Output prefix length
 * @return 0 on success, -1 on error
 */
int lpm_lua_parse_ipv6_cidr(const char *str, uint8_t prefix[16], uint8_t *prefix_len) {
    if (str == NULL || prefix == NULL || prefix_len == NULL) {
        return -1;
    }
    
    /* Find the '/' separator */
    const char *slash = strchr(str, '/');
    if (slash == NULL) {
        return -1;
    }
    
    /* Parse the address part */
    size_t addr_len = slash - str;
    if (addr_len >= 46) {  /* Max IPv6 string length */
        return -1;
    }
    
    char addr_buf[46];
    memcpy(addr_buf, str, addr_len);
    addr_buf[addr_len] = '\0';
    
    if (lpm_lua_parse_ipv6(addr_buf, prefix) != 0) {
        return -1;
    }
    
    /* Parse the prefix length */
    const char *len_str = slash + 1;
    if (*len_str == '\0') {
        return -1;
    }
    
    char *endptr;
    errno = 0;
    long len = strtol(len_str, &endptr, 10);
    
    if (errno != 0 || *endptr != '\0' || len < 0 || len > 128) {
        return -1;
    }
    
    *prefix_len = (uint8_t)len;
    
    /* Zero out bits beyond the prefix length */
    if (*prefix_len < 128) {
        int full_bytes = *prefix_len / 8;
        int remaining_bits = *prefix_len % 8;
        
        if (remaining_bits > 0) {
            uint8_t mask = (uint8_t)(0xFF << (8 - remaining_bits));
            prefix[full_bytes] &= mask;
            full_bytes++;
        }
        
        for (int i = full_bytes; i < 16; i++) {
            prefix[i] = 0;
        }
    }
    
    return 0;
}
