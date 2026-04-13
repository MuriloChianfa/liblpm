/*
 * liblpm - IPv4 DIR-24-8 Algorithm
 * Internal declarations
 */
#ifndef LPM_ALGO_DIR24_H_
#define LPM_ALGO_DIR24_H_

#include "../lpm.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Internal SIMD variants (used by ifunc resolver)
 * Public API functions are declared in lpm.h
 * ============================================================================ */

void lpm_lookup_batch_ipv4_dir24_scalar(const lpm_trie_t *trie, const uint32_t *ips,
                                         uint32_t *next_hops, size_t count);
void lpm_lookup_batch_ipv4_dir24_sse42(const lpm_trie_t *trie, const uint32_t *ips,
                                        uint32_t *next_hops, size_t count);
void lpm_lookup_batch_ipv4_dir24_avx(const lpm_trie_t *trie, const uint32_t *ips,
                                      uint32_t *next_hops, size_t count);
void lpm_lookup_batch_ipv4_dir24_avx2(const lpm_trie_t *trie, const uint32_t *ips,
                                       uint32_t *next_hops, size_t count);
void lpm_lookup_batch_ipv4_dir24_avx512(const lpm_trie_t *trie, const uint32_t *ips,
                                         uint32_t *next_hops, size_t count);

#ifdef __cplusplus
}
#endif

#endif /* LPM_ALGO_DIR24_H_ */
