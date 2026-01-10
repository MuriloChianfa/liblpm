/*
 * liblpm - IPv6 8-bit Stride Algorithm (6stride8)
 * Internal declarations
 */
#ifndef _LPM_ALGO_6STRIDE8_H_
#define _LPM_ALGO_6STRIDE8_H_

#include "../lpm.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * IPv6 8-bit Stride Algorithm (6stride8)
 * ============================================================================ */

/* Creation and management */
lpm_trie_t *lpm_create_ipv6_8stride(void);
int lpm_add_ipv6_8stride(lpm_trie_t *trie, const uint8_t *prefix, uint8_t prefix_len, uint32_t next_hop);
int lpm_delete_ipv6_8stride(lpm_trie_t *trie, const uint8_t *prefix, uint8_t prefix_len);

/* Single lookup - ifunc dispatched */
uint32_t lpm_lookup_ipv6_8stride(const lpm_trie_t *trie, const uint8_t addr[16]);

/* Batch lookup - ifunc dispatched */
void lpm_lookup_batch_ipv6_8stride(const lpm_trie_t *trie, const uint8_t (*addrs)[16],
                                    uint32_t *next_hops, size_t count);

/* Internal SIMD variants (used by ifunc resolver) */
uint32_t lpm_lookup_ipv6_8stride_scalar(const lpm_trie_t *trie, const uint8_t *addr);
uint32_t lpm_lookup_ipv6_8stride_sse2(const lpm_trie_t *trie, const uint8_t *addr);
uint32_t lpm_lookup_ipv6_8stride_sse42(const lpm_trie_t *trie, const uint8_t *addr);
uint32_t lpm_lookup_ipv6_8stride_avx(const lpm_trie_t *trie, const uint8_t *addr);
uint32_t lpm_lookup_ipv6_8stride_avx2(const lpm_trie_t *trie, const uint8_t *addr);
uint32_t lpm_lookup_ipv6_8stride_avx512(const lpm_trie_t *trie, const uint8_t *addr);

void lpm_lookup_batch_ipv6_8stride_scalar(const lpm_trie_t *trie, const uint8_t **addrs,
                                           uint32_t *next_hops, size_t count);
void lpm_lookup_batch_ipv6_8stride_sse2(const lpm_trie_t *trie, const uint8_t **addrs,
                                         uint32_t *next_hops, size_t count);
void lpm_lookup_batch_ipv6_8stride_sse42(const lpm_trie_t *trie, const uint8_t **addrs,
                                          uint32_t *next_hops, size_t count);
void lpm_lookup_batch_ipv6_8stride_avx(const lpm_trie_t *trie, const uint8_t **addrs,
                                        uint32_t *next_hops, size_t count);
void lpm_lookup_batch_ipv6_8stride_avx2(const lpm_trie_t *trie, const uint8_t **addrs,
                                         uint32_t *next_hops, size_t count);
void lpm_lookup_batch_ipv6_8stride_avx512(const lpm_trie_t *trie, const uint8_t **addrs,
                                           uint32_t *next_hops, size_t count);

#ifdef __cplusplus
}
#endif

#endif /* _LPM_ALGO_6STRIDE8_H_ */
