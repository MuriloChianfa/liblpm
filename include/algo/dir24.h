/*
 * liblpm - IPv4 DIR-24-8 Algorithm
 * Internal declarations
 */
#ifndef _LPM_ALGO_DIR24_H_
#define _LPM_ALGO_DIR24_H_

#include "../lpm.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * IPv4 DIR-24-8 Algorithm (dir24)
 * ============================================================================ */

/* Creation and management */
lpm_trie_t *lpm_create_ipv4_dir24(void);
int lpm_add_ipv4_dir24(lpm_trie_t *trie, const uint8_t *prefix, uint8_t prefix_len, uint32_t next_hop);
int lpm_delete_ipv4_dir24(lpm_trie_t *trie, const uint8_t *prefix, uint8_t prefix_len);

/* Single lookup */
uint32_t lpm_lookup_ipv4_dir24(const lpm_trie_t *trie, uint32_t addr);
uint32_t lpm_lookup_ipv4_dir24_bytes(const lpm_trie_t *trie, const uint8_t addr[4]);

/* Batch lookup - ifunc dispatched */
void lpm_lookup_batch_ipv4_dir24(const lpm_trie_t *trie, const uint32_t *addrs,
                                  uint32_t *next_hops, size_t count);
void lpm_lookup_batch_ipv4_dir24_bytes(const lpm_trie_t *trie, const uint8_t (*addrs)[4],
                                        uint32_t *next_hops, size_t count);
void lpm_lookup_batch_ipv4_dir24_ptrs(const lpm_trie_t *trie, const uint8_t **addrs,
                                       uint32_t *next_hops, size_t count);

/* Internal SIMD variants */
void lpm_lookup_batch_ipv4_dir24_scalar(const lpm_trie_t *trie, const uint32_t *addrs,
                                         uint32_t *next_hops, size_t count);
void lpm_lookup_batch_ipv4_dir24_sse42(const lpm_trie_t *trie, const uint32_t *addrs,
                                        uint32_t *next_hops, size_t count);
void lpm_lookup_batch_ipv4_dir24_avx(const lpm_trie_t *trie, const uint32_t *addrs,
                                      uint32_t *next_hops, size_t count);
void lpm_lookup_batch_ipv4_dir24_avx2(const lpm_trie_t *trie, const uint32_t *addrs,
                                       uint32_t *next_hops, size_t count);
void lpm_lookup_batch_ipv4_dir24_avx512(const lpm_trie_t *trie, const uint32_t *addrs,
                                         uint32_t *next_hops, size_t count);

#ifdef __cplusplus
}
#endif

#endif /* _LPM_ALGO_DIR24_H_ */
