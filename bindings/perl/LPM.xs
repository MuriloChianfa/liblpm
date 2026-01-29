/*
 * LPM.xs - XS bindings for liblpm
 *
 * High-performance Perl bindings for the liblpm C library.
 * Provides IPv4 and IPv6 longest prefix match operations.
 *
 * Copyright (c) Murilo Chianfa
 * Licensed under the Boost Software License 1.0
 */

#define PERL_NO_GET_CONTEXT
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>

/* Include liblpm header - path set in Makefile.PL */
#include <lpm/lpm.h>

/* Type for our blessed object */
typedef struct {
    lpm_trie_t *trie;
    int is_ipv6;
    int destroyed;
} NetLPM;

/* Helper: Parse IPv4 address string to network byte order uint32_t */
static int
parse_ipv4_addr(const char *str, uint32_t *addr)
{
    struct in_addr in;
    if (inet_pton(AF_INET, str, &in) != 1) {
        return 0;
    }
    *addr = ntohl(in.s_addr);
    return 1;
}

/* Helper: Parse IPv4 address string to byte array */
static int
parse_ipv4_addr_bytes(const char *str, uint8_t *bytes)
{
    struct in_addr in;
    if (inet_pton(AF_INET, str, &in) != 1) {
        return 0;
    }
    memcpy(bytes, &in.s_addr, 4);
    return 1;
}

/* Helper: Parse IPv6 address string to byte array */
static int
parse_ipv6_addr(const char *str, uint8_t *bytes)
{
    struct in6_addr in6;
    if (inet_pton(AF_INET6, str, &in6) != 1) {
        return 0;
    }
    memcpy(bytes, &in6.s6_addr, 16);
    return 1;
}

/* Helper: Parse CIDR prefix notation (e.g., "192.168.0.0/16") */
static int
parse_prefix(const char *str, uint8_t *addr_bytes, uint8_t *prefix_len, int is_ipv6)
{
    char addr_buf[64];
    const char *slash;
    size_t addr_len;
    long len;
    char *endptr;
    int max_len = is_ipv6 ? 128 : 32;
    
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
    
    /* Check for empty prefix length */
    if (*(slash + 1) == '\0') {
        return 0;
    }
    
    /* Parse the prefix length */
    len = strtol(slash + 1, &endptr, 10);
    if (*endptr != '\0' || len < 0 || len > max_len) {
        return 0;
    }
    *prefix_len = (uint8_t)len;
    
    /* Parse the address */
    if (is_ipv6) {
        if (!parse_ipv6_addr(addr_buf, addr_bytes)) {
            return 0;
        }
    } else {
        if (!parse_ipv4_addr_bytes(addr_buf, addr_bytes)) {
            return 0;
        }
    }
    
    return 1;
}

MODULE = Net::LPM    PACKAGE = Net::LPM

PROTOTYPES: DISABLE

# Create a new IPv4 LPM table
SV *
_xs_new_ipv4()
    PREINIT:
        NetLPM *self;
    CODE:
        Newxz(self, 1, NetLPM);
        self->trie = lpm_create_ipv4();
        if (!self->trie) {
            Safefree(self);
            croak("Failed to create IPv4 LPM table");
        }
        self->is_ipv6 = 0;
        self->destroyed = 0;
        RETVAL = newSViv(PTR2IV(self));
    OUTPUT:
        RETVAL

# Create a new IPv6 LPM table
SV *
_xs_new_ipv6()
    PREINIT:
        NetLPM *self;
    CODE:
        Newxz(self, 1, NetLPM);
        self->trie = lpm_create_ipv6();
        if (!self->trie) {
            Safefree(self);
            croak("Failed to create IPv6 LPM table");
        }
        self->is_ipv6 = 1;
        self->destroyed = 0;
        RETVAL = newSViv(PTR2IV(self));
    OUTPUT:
        RETVAL

# Destroy the LPM table
void
_xs_destroy(self_iv)
        IV self_iv
    PREINIT:
        NetLPM *self;
    CODE:
        self = INT2PTR(NetLPM*, self_iv);
        if (self && !self->destroyed) {
            if (self->trie) {
                lpm_destroy(self->trie);
                self->trie = NULL;
            }
            self->destroyed = 1;
        }

# Free the NetLPM structure (called from Perl DESTROY)
void
_xs_free(self_iv)
        IV self_iv
    PREINIT:
        NetLPM *self;
    CODE:
        self = INT2PTR(NetLPM*, self_iv);
        if (self) {
            if (!self->destroyed && self->trie) {
                lpm_destroy(self->trie);
            }
            Safefree(self);
        }

# Check if table is IPv6
int
_xs_is_ipv6(self_iv)
        IV self_iv
    PREINIT:
        NetLPM *self;
    CODE:
        self = INT2PTR(NetLPM*, self_iv);
        RETVAL = self ? self->is_ipv6 : 0;
    OUTPUT:
        RETVAL

# Check if table is destroyed
int
_xs_is_destroyed(self_iv)
        IV self_iv
    PREINIT:
        NetLPM *self;
    CODE:
        self = INT2PTR(NetLPM*, self_iv);
        RETVAL = self ? self->destroyed : 1;
    OUTPUT:
        RETVAL

# Insert a prefix into the table
int
_xs_insert(self_iv, prefix_str, next_hop)
        IV self_iv
        const char *prefix_str
        unsigned int next_hop
    PREINIT:
        NetLPM *self;
        uint8_t addr_bytes[16];
        uint8_t prefix_len;
        int result;
    CODE:
        self = INT2PTR(NetLPM*, self_iv);
        if (!self || self->destroyed || !self->trie) {
            croak("Cannot insert into destroyed or invalid table");
        }
        
        if (!parse_prefix(prefix_str, addr_bytes, &prefix_len, self->is_ipv6)) {
            croak("Invalid prefix format: %s", prefix_str);
        }
        
        result = lpm_add(self->trie, addr_bytes, prefix_len, (uint32_t)next_hop);
        if (result != 0) {
            croak("Failed to insert prefix: %s", prefix_str);
        }
        
        RETVAL = 1;
    OUTPUT:
        RETVAL

# Delete a prefix from the table
int
_xs_delete(self_iv, prefix_str)
        IV self_iv
        const char *prefix_str
    PREINIT:
        NetLPM *self;
        uint8_t addr_bytes[16];
        uint8_t prefix_len;
        int result;
    CODE:
        self = INT2PTR(NetLPM*, self_iv);
        if (!self || self->destroyed || !self->trie) {
            croak("Cannot delete from destroyed or invalid table");
        }
        
        if (!parse_prefix(prefix_str, addr_bytes, &prefix_len, self->is_ipv6)) {
            croak("Invalid prefix format: %s", prefix_str);
        }
        
        result = lpm_delete(self->trie, addr_bytes, prefix_len);
        if (result != 0) {
            /* Prefix not found is not an error - just return 0 */
            RETVAL = 0;
        } else {
            RETVAL = 1;
        }
    OUTPUT:
        RETVAL

# Lookup a single address
SV *
_xs_lookup(self_iv, addr_str)
        IV self_iv
        const char *addr_str
    PREINIT:
        NetLPM *self;
        uint32_t next_hop;
        uint32_t ipv4_addr;
        uint8_t ipv6_addr[16];
    CODE:
        self = INT2PTR(NetLPM*, self_iv);
        if (!self || self->destroyed || !self->trie) {
            croak("Cannot lookup in destroyed or invalid table");
        }
        
        if (self->is_ipv6) {
            if (!parse_ipv6_addr(addr_str, ipv6_addr)) {
                croak("Invalid IPv6 address: %s", addr_str);
            }
            next_hop = lpm_lookup_ipv6(self->trie, ipv6_addr);
        } else {
            if (!parse_ipv4_addr(addr_str, &ipv4_addr)) {
                croak("Invalid IPv4 address: %s", addr_str);
            }
            next_hop = lpm_lookup_ipv4(self->trie, ipv4_addr);
        }
        
        if (next_hop == LPM_INVALID_NEXT_HOP) {
            RETVAL = &PL_sv_undef;
        } else {
            RETVAL = newSVuv(next_hop);
        }
    OUTPUT:
        RETVAL

# Batch lookup for IPv4 addresses
void
_xs_lookup_batch_ipv4(self_iv, addrs_av)
        IV self_iv
        SV *addrs_av
    PREINIT:
        NetLPM *self;
        AV *av;
        SSize_t count;
        SSize_t i;
        uint32_t *addrs = NULL;
        uint32_t *results = NULL;
        SV **elem;
        const char *addr_str;
    PPCODE:
        self = INT2PTR(NetLPM*, self_iv);
        if (!self || self->destroyed || !self->trie) {
            croak("Cannot lookup in destroyed or invalid table");
        }
        
        if (self->is_ipv6) {
            croak("Use lookup_batch for IPv6, not lookup_batch_ipv4");
        }
        
        if (!SvROK(addrs_av) || SvTYPE(SvRV(addrs_av)) != SVt_PVAV) {
            croak("lookup_batch_ipv4 requires an array reference");
        }
        av = (AV*)SvRV(addrs_av);
        
        count = av_len(av) + 1;
        if (count <= 0) {
            XSRETURN_EMPTY;
        }
        
        /* Allocate arrays */
        Newx(addrs, count, uint32_t);
        Newx(results, count, uint32_t);
        
        /* Parse all addresses */
        for (i = 0; i < count; i++) {
            elem = av_fetch(av, i, 0);
            if (!elem || !SvOK(*elem)) {
                Safefree(addrs);
                Safefree(results);
                croak("Invalid address at index %ld", (long)i);
            }
            addr_str = SvPV_nolen(*elem);
            if (!parse_ipv4_addr(addr_str, &addrs[i])) {
                Safefree(addrs);
                Safefree(results);
                croak("Invalid IPv4 address at index %ld: %s", (long)i, addr_str);
            }
        }
        
        /* Perform batch lookup */
        lpm_lookup_batch_ipv4(self->trie, addrs, results, (size_t)count);
        
        /* Push results onto the stack */
        EXTEND(SP, count);
        for (i = 0; i < count; i++) {
            if (results[i] == LPM_INVALID_NEXT_HOP) {
                PUSHs(&PL_sv_undef);
            } else {
                PUSHs(sv_2mortal(newSVuv(results[i])));
            }
        }
        
        Safefree(addrs);
        Safefree(results);

# Batch lookup for IPv6 addresses
void
_xs_lookup_batch_ipv6(self_iv, addrs_av)
        IV self_iv
        SV *addrs_av
    PREINIT:
        NetLPM *self;
        AV *av;
        SSize_t count;
        SSize_t i;
        uint8_t *addrs = NULL;  /* Flat array: count * 16 bytes */
        uint32_t *results = NULL;
        SV **elem;
        const char *addr_str;
    PPCODE:
        self = INT2PTR(NetLPM*, self_iv);
        if (!self || self->destroyed || !self->trie) {
            croak("Cannot lookup in destroyed or invalid table");
        }
        
        if (!self->is_ipv6) {
            croak("Use lookup_batch for IPv4, not lookup_batch_ipv6");
        }
        
        if (!SvROK(addrs_av) || SvTYPE(SvRV(addrs_av)) != SVt_PVAV) {
            croak("lookup_batch_ipv6 requires an array reference");
        }
        av = (AV*)SvRV(addrs_av);
        
        count = av_len(av) + 1;
        if (count <= 0) {
            XSRETURN_EMPTY;
        }
        
        /* Allocate arrays - use flat array for IPv6 addresses */
        Newx(addrs, count * 16, uint8_t);
        Newx(results, count, uint32_t);
        
        /* Parse all addresses */
        for (i = 0; i < count; i++) {
            elem = av_fetch(av, i, 0);
            if (!elem || !SvOK(*elem)) {
                Safefree(addrs);
                Safefree(results);
                croak("Invalid address at index %ld", (long)i);
            }
            addr_str = SvPV_nolen(*elem);
            if (!parse_ipv6_addr(addr_str, &addrs[i * 16])) {
                Safefree(addrs);
                Safefree(results);
                croak("Invalid IPv6 address at index %ld: %s", (long)i, addr_str);
            }
        }
        
        /* Perform batch lookup - cast to proper type */
        lpm_lookup_batch_ipv6(self->trie, (const uint8_t (*)[16])addrs, results, (size_t)count);
        
        /* Push results onto the stack */
        EXTEND(SP, count);
        for (i = 0; i < count; i++) {
            if (results[i] == LPM_INVALID_NEXT_HOP) {
                PUSHs(&PL_sv_undef);
            } else {
                PUSHs(sv_2mortal(newSVuv(results[i])));
            }
        }
        
        Safefree(addrs);
        Safefree(results);

# Get the liblpm version string
const char *
_xs_get_version()
    CODE:
        RETVAL = lpm_get_version();
    OUTPUT:
        RETVAL

# Get the invalid next hop constant
unsigned int
_xs_invalid_next_hop()
    CODE:
        RETVAL = LPM_INVALID_NEXT_HOP;
    OUTPUT:
        RETVAL

# Print stats (for debugging)
void
_xs_print_stats(self_iv)
        IV self_iv
    PREINIT:
        NetLPM *self;
    CODE:
        self = INT2PTR(NetLPM*, self_iv);
        if (self && !self->destroyed && self->trie) {
            lpm_print_stats(self->trie);
        }
