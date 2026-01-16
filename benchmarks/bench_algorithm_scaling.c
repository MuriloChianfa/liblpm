/*
 * liblpm - Algorithm Scaling Benchmark
 * 
 * This benchmark measures lookup performance across all algorithms
 * with varying prefix counts to enable cross-CPU and cross-algorithm
 * comparisons.
 * 
 * Features:
 * - CPU pinning for consistent, fair measurements
 * - Multiple trials with statistical analysis (stddev, min, max)
 * - CSV output for visualization
 * - All algorithms: dir24, 4stride8 (IPv4), wide16, 6stride8 (IPv6)
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <sched.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <ctype.h>

#include "../include/lpm.h"

/* DPDK Headers - conditional */
#ifdef HAVE_DPDK
#include <rte_eal.h>
#include <rte_lpm.h>
#include <rte_lpm6.h>
#include <rte_memory.h>
#include <rte_errno.h>
#include <rte_version.h>
#endif

/* External LPM Libraries - conditional (Docker builds only) */
#ifdef HAVE_EXTERNAL_LPM

#ifdef HAVE_RMIND_LPM
/* rmind/liblpm - renamed to avoid conflicts with our lpm.h */
typedef struct lpm rmind_lpm_t;
rmind_lpm_t *rmind_lpm_create(void);
void rmind_lpm_destroy(rmind_lpm_t *);
int rmind_lpm_insert(rmind_lpm_t *, const void *, size_t, unsigned, void *);
void *rmind_lpm_lookup(rmind_lpm_t *, const void *, size_t);
#endif

#ifdef HAVE_LIBPATRICIA
#include "patricia.h"
#endif

#endif /* HAVE_EXTERNAL_LPM */

/* ============================================================================
 * Configuration
 * ============================================================================ */

#define BENCH_DURATION_SEC 3.0  /* Duration per trial in seconds */
#define NUM_TRIALS 3            /* Trials for stddev calculation */
#define BATCH_SIZE 256          /* Batch size for batch lookups */
#define WARMUP_LOOKUPS 1000     /* Cache warmup iterations */
#define TEST_ADDR_COUNT 100000  /* Number of pre-generated test addresses */
#define NUM_LOOKUPS 1000000     /* Fallback for old-style benchmarks */

/* Prefix counts to test */
static const int PREFIX_COUNTS[] = {32, 64, 128, 256, 512, 1024, 2048, 4096, 8192};
#define NUM_PREFIX_COUNTS (sizeof(PREFIX_COUNTS) / sizeof(PREFIX_COUNTS[0]))

/* DPDK Configuration */
#ifdef HAVE_DPDK
#define DPDK_LPM_MAX_RULES     100000
#define DPDK_LPM_TBL8_NUM_GROUPS  256
#define DPDK_LPM6_MAX_RULES    100000
#define DPDK_LPM6_NUMBER_TBL8S 65536
static bool dpdk_initialized = false;
#endif

/* ============================================================================
 * Algorithm definitions
 * ============================================================================ */

typedef enum {
    ALGO_DIR24,
    ALGO_4STRIDE8,
    ALGO_WIDE16,
    ALGO_6STRIDE8,
#ifdef HAVE_DPDK
    ALGO_DPDK_IPV4,
    ALGO_DPDK_IPV6,
#endif
#ifdef HAVE_RMIND_LPM
    ALGO_RMIND_IPV4,
    ALGO_RMIND_IPV6,
#endif
#ifdef HAVE_LIBPATRICIA
    ALGO_PATRICIA_IPV4,
#endif
    ALGO_COUNT
} algorithm_t;

typedef enum {
    IP_V4,
    IP_V6
} ip_version_t;

typedef enum {
    LOOKUP_SINGLE,
    LOOKUP_BATCH
} lookup_type_t;

typedef struct {
    const char *name;
    const char *display_name;
    ip_version_t ip_version;
    lpm_trie_t *(*create)(void);
    int (*add)(lpm_trie_t *, const uint8_t *, uint8_t, uint32_t);
} algorithm_info_t;

static const algorithm_info_t ALGORITHMS[] = {
    [ALGO_DIR24] = {
        .name = "dir24",
        .display_name = "DIR-24-8",
        .ip_version = IP_V4,
        .create = lpm_create_ipv4_dir24,
        .add = lpm_add_ipv4_dir24
    },
    [ALGO_4STRIDE8] = {
        .name = "4stride8",
        .display_name = "IPv4 8-bit Stride",
        .ip_version = IP_V4,
        .create = lpm_create_ipv4_8stride,
        .add = lpm_add_ipv4_8stride
    },
    [ALGO_WIDE16] = {
        .name = "wide16",
        .display_name = "IPv6 Wide 16-bit",
        .ip_version = IP_V6,
        .create = lpm_create_ipv6_wide16,
        .add = lpm_add_ipv6_wide16
    },
    [ALGO_6STRIDE8] = {
        .name = "6stride8",
        .display_name = "IPv6 8-bit Stride",
        .ip_version = IP_V6,
        .create = lpm_create_ipv6_8stride,
        .add = lpm_add_ipv6_8stride
    },
#ifdef HAVE_DPDK
    [ALGO_DPDK_IPV4] = {
        .name = "dpdk",
        .display_name = "DPDK LPM",
        .ip_version = IP_V4,
        .create = NULL,  /* DPDK uses different API */
        .add = NULL
    },
    [ALGO_DPDK_IPV6] = {
        .name = "dpdk6",
        .display_name = "DPDK LPM6",
        .ip_version = IP_V6,
        .create = NULL,  /* DPDK uses different API */
        .add = NULL
    },
#endif
#ifdef HAVE_RMIND_LPM
    [ALGO_RMIND_IPV4] = {
        .name = "rmindlpm",
        .display_name = "rmind/liblpm IPv4",
        .ip_version = IP_V4,
        .create = NULL,  /* External lib uses different API */
        .add = NULL
    },
    [ALGO_RMIND_IPV6] = {
        .name = "rmindlpm6",
        .display_name = "rmind/liblpm IPv6",
        .ip_version = IP_V6,
        .create = NULL,
        .add = NULL
    },
#endif
#ifdef HAVE_LIBPATRICIA
    [ALGO_PATRICIA_IPV4] = {
        .name = "patricia",
        .display_name = "Patricia Trie IPv4",
        .ip_version = IP_V4,
        .create = NULL,
        .add = NULL
    },
#endif
};

/* ============================================================================
 * Utility functions
 * ============================================================================ */

/* Generate random IPv4 address */
static void generate_random_ipv4(uint8_t addr[4])
{
    for (int i = 0; i < 4; i++) {
        addr[i] = rand() % 256;
    }
}

/* Generate random IPv6 address */
static void generate_random_ipv6(uint8_t addr[16])
{
    for (int i = 0; i < 16; i++) {
        addr[i] = rand() % 256;
    }
}

/* Convert uint8_t[4] to uint32_t */
static inline uint32_t ipv4_to_uint32(const uint8_t addr[4])
{
    return ((uint32_t)addr[0] << 24) | ((uint32_t)addr[1] << 16) | 
           ((uint32_t)addr[2] << 8) | addr[3];
}

/* Measure time difference in microseconds */
static double time_diff_us(const struct timespec *start, const struct timespec *end)
{
    return (end->tv_sec - start->tv_sec) * 1000000.0 + 
           (end->tv_nsec - start->tv_nsec) / 1000.0;
}

/* Compare function for qsort */
static int compare_double(const void *a, const void *b)
{
    double da = *(const double *)a;
    double db = *(const double *)b;
    if (da < db) return -1;
    if (da > db) return 1;
    return 0;
}

/* Calculate statistics from results array */
static void calculate_stats(double *results, int count,
                           double *median, double *mean, double *stddev,
                           double *min, double *max)
{
    /* Sort for median */
    qsort(results, count, sizeof(double), compare_double);
    
    *min = results[0];
    *max = results[count - 1];
    *median = results[count / 2];
    
    /* Calculate mean */
    double sum = 0;
    for (int i = 0; i < count; i++) {
        sum += results[i];
    }
    *mean = sum / count;
    
    /* Calculate stddev */
    double variance_sum = 0;
    for (int i = 0; i < count; i++) {
        double diff = results[i] - *mean;
        variance_sum += diff * diff;
    }
    *stddev = sqrt(variance_sum / count);
}

/* Pin to CPU core */
static int pin_to_cpu(int cpu_id)
{
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu_id, &cpuset);
    
    if (sched_setaffinity(0, sizeof(cpuset), &cpuset) != 0) {
        perror("sched_setaffinity");
        return -1;
    }
    return 0;
}

/* Get CPU model name from /proc/cpuinfo */
static void get_cpu_model(char *buffer, size_t size)
{
    FILE *f = fopen("/proc/cpuinfo", "r");
    if (!f) {
        snprintf(buffer, size, "Unknown");
        return;
    }
    
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "model name", 10) == 0) {
            char *colon = strchr(line, ':');
            if (colon) {
                colon++; /* Skip ':' */
                while (*colon == ' ' || *colon == '\t') colon++; /* Skip whitespace */
                /* Remove newline */
                char *nl = strchr(colon, '\n');
                if (nl) *nl = '\0';
                snprintf(buffer, size, "%s", colon);
                fclose(f);
                return;
            }
        }
    }
    fclose(f);
    snprintf(buffer, size, "Unknown");
}

/* Sanitize CPU name for use in directory/file names */
static void sanitize_cpu_name(const char *cpu_model, char *buffer, size_t size)
{
    /* Convert CPU model to lowercase filename-friendly format
     * Example: "AMD Ryzen 9 9950X3D 16-Core Processor" -> "amd_ryzen_9_9950x3d_16"
     * Example: "Intel(R) Xeon(R) CPU E5-2683 v4 @ 2.10GHz" -> "intel_xeon_e5_2683_v4" */
    
    size_t out_idx = 0;
    bool last_was_space = false;
    
    for (size_t i = 0; cpu_model[i] && out_idx < size - 1; i++) {
        char c = cpu_model[i];
        
        /* Stop at certain markers that come after the model info */
        if (c == '@' ||
            strncmp(&cpu_model[i], " with ", 6) == 0 ||
            strncmp(&cpu_model[i], " Processor", 10) == 0) {
            break;
        }
        
        /* Skip parentheses */
        if (c == '(' || c == ')') {
            continue;
        }
        
        /* Convert to lowercase and replace spaces/special chars with underscore */
        if (isalnum(c)) {
            buffer[out_idx++] = tolower(c);
            last_was_space = false;
        } else if (!last_was_space && out_idx > 0) {
            buffer[out_idx++] = '_';
            last_was_space = true;
        }
    }
    
    /* Remove trailing underscore */
    if (out_idx > 0 && buffer[out_idx - 1] == '_') {
        out_idx--;
    }
    
    buffer[out_idx] = '\0';
    
    /* Fallback if result is too short */
    if (out_idx < 3) {
        snprintf(buffer, size, "unknown_cpu");
    }
}

/* Create directory recursively */
static int mkdir_recursive(const char *path)
{
    char tmp[512];
    char *p = NULL;
    size_t len;
    
    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);
    if (tmp[len - 1] == '/') {
        tmp[len - 1] = 0;
    }
    
    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
                return -1;
            }
            *p = '/';
        }
    }
    
    if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
        return -1;
    }
    return 0;
}

/* ============================================================================
 * Benchmark functions
 * ============================================================================ */

typedef struct {
    double median_lookups_per_sec;
    double mean_lookups_per_sec;
    double stddev_lookups_per_sec;
    double min_lookups_per_sec;
    double max_lookups_per_sec;
    size_t memory_bytes;
} benchmark_result_t;

/* Benchmark single IPv4 lookup */
/* Helper: get elapsed seconds */
static inline double get_elapsed_sec(struct timespec *start)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (now.tv_sec - start->tv_sec) + (now.tv_nsec - start->tv_nsec) / 1e9;
}

static benchmark_result_t benchmark_ipv4_single(algorithm_t algo, int num_prefixes)
{
    benchmark_result_t result = {0};
    const algorithm_info_t *info = &ALGORITHMS[algo];
    
    double trial_results[NUM_TRIALS];
    
    for (int trial = 0; trial < NUM_TRIALS; trial++) {
        /* Reset random seed for reproducibility */
        srand(42 + trial);
        
        /* Create trie */
        lpm_trie_t *trie = info->create();
        if (!trie) {
            fprintf(stderr, "Failed to create trie for %s\n", info->name);
            return result;
        }
        
        /* Add prefixes */
        for (int i = 0; i < num_prefixes; i++) {
            uint8_t prefix[4];
            generate_random_ipv4(prefix);
            uint8_t prefix_len = 8 + (rand() % 25); /* 8 to 32 */
            info->add(trie, prefix, prefix_len, i);
        }
        
        /* Generate test addresses */
        uint32_t *test_addrs_u32 = malloc(TEST_ADDR_COUNT * sizeof(uint32_t));
        if (!test_addrs_u32) {
            fprintf(stderr, "Failed to allocate test addresses\n");
            lpm_destroy(trie);
            return result;
        }
        
        for (int i = 0; i < TEST_ADDR_COUNT; i++) {
            uint8_t addr[4];
            generate_random_ipv4(addr);
            test_addrs_u32[i] = ipv4_to_uint32(addr);
        }
        
        /* Warmup cache */
        for (int i = 0; i < WARMUP_LOOKUPS; i++) {
            if (algo == ALGO_DIR24) {
                volatile uint32_t nh = lpm_lookup_ipv4_dir24(trie, test_addrs_u32[i % TEST_ADDR_COUNT]);
                (void)nh;
            } else {
                volatile uint32_t nh = lpm_lookup_ipv4_8stride(trie, test_addrs_u32[i % TEST_ADDR_COUNT]);
                (void)nh;
            }
        }
        
        /* Time-based benchmark */
        struct timespec start;
        clock_gettime(CLOCK_MONOTONIC, &start);
        
        long long total_lookups = 0;
        int idx = 0;
        
        if (algo == ALGO_DIR24) {
            while (get_elapsed_sec(&start) < BENCH_DURATION_SEC) {
                for (int i = 0; i < 1000; i++) {
                    volatile uint32_t nh = lpm_lookup_ipv4_dir24(trie, test_addrs_u32[idx]);
                    (void)nh;
                    idx = (idx + 1) % TEST_ADDR_COUNT;
                }
                total_lookups += 1000;
            }
        } else {
            while (get_elapsed_sec(&start) < BENCH_DURATION_SEC) {
                for (int i = 0; i < 1000; i++) {
                    volatile uint32_t nh = lpm_lookup_ipv4_8stride(trie, test_addrs_u32[idx]);
                    (void)nh;
                    idx = (idx + 1) % TEST_ADDR_COUNT;
                }
                total_lookups += 1000;
            }
        }
        
        double elapsed_sec = get_elapsed_sec(&start);
        trial_results[trial] = (double)total_lookups / elapsed_sec;
        
        /* Save memory on last trial */
        if (trial == NUM_TRIALS - 1) {
            if (algo == ALGO_DIR24) {
                result.memory_bytes = LPM_IPV4_DIR24_SIZE * sizeof(struct lpm_dir24_entry) +
                                     trie->tbl8_groups_used * 256 * sizeof(struct lpm_tbl8_entry);
            } else {
                result.memory_bytes = trie->pool_used * sizeof(lpm_node_t);
            }
        }
        
        free(test_addrs_u32);
        lpm_destroy(trie);
    }
    
    calculate_stats(trial_results, NUM_TRIALS,
                   &result.median_lookups_per_sec,
                   &result.mean_lookups_per_sec,
                   &result.stddev_lookups_per_sec,
                   &result.min_lookups_per_sec,
                   &result.max_lookups_per_sec);
    
    return result;
}

/* Benchmark batch IPv4 lookup */
static benchmark_result_t benchmark_ipv4_batch(algorithm_t algo, int num_prefixes)
{
    benchmark_result_t result = {0};
    const algorithm_info_t *info = &ALGORITHMS[algo];
    
    double trial_results[NUM_TRIALS];
    
    for (int trial = 0; trial < NUM_TRIALS; trial++) {
        srand(42 + trial);
        
        lpm_trie_t *trie = info->create();
        if (!trie) {
            fprintf(stderr, "Failed to create trie for %s\n", info->name);
            return result;
        }
        
        /* Add prefixes */
        for (int i = 0; i < num_prefixes; i++) {
            uint8_t prefix[4];
            generate_random_ipv4(prefix);
            uint8_t prefix_len = 8 + (rand() % 25);
            info->add(trie, prefix, prefix_len, i);
        }
        
        /* Generate test addresses */
        uint32_t *test_addrs = malloc(TEST_ADDR_COUNT * sizeof(uint32_t));
        uint32_t *next_hops = malloc(BATCH_SIZE * sizeof(uint32_t));
        
        if (!test_addrs || !next_hops) {
            fprintf(stderr, "Failed to allocate test data\n");
            free(test_addrs);
            free(next_hops);
            lpm_destroy(trie);
            return result;
        }
        
        for (int i = 0; i < TEST_ADDR_COUNT; i++) {
            uint8_t addr[4];
            generate_random_ipv4(addr);
            test_addrs[i] = ipv4_to_uint32(addr);
        }
        
        /* Warmup */
        if (algo == ALGO_DIR24) {
            lpm_lookup_batch_ipv4_dir24(trie, test_addrs, next_hops, BATCH_SIZE);
        } else {
            lpm_lookup_batch_ipv4_8stride(trie, test_addrs, next_hops, BATCH_SIZE);
        }
        
        /* Time-based benchmark */
        struct timespec start;
        clock_gettime(CLOCK_MONOTONIC, &start);
        
        long long total_lookups = 0;
        int batch_idx = 0;
        
        while (get_elapsed_sec(&start) < BENCH_DURATION_SEC) {
            if (algo == ALGO_DIR24) {
                lpm_lookup_batch_ipv4_dir24(trie, &test_addrs[batch_idx], next_hops, BATCH_SIZE);
            } else {
                lpm_lookup_batch_ipv4_8stride(trie, &test_addrs[batch_idx], next_hops, BATCH_SIZE);
            }
            total_lookups += BATCH_SIZE;
            batch_idx = (batch_idx + BATCH_SIZE) % (TEST_ADDR_COUNT - BATCH_SIZE);
        }
        
        double elapsed_sec = get_elapsed_sec(&start);
        trial_results[trial] = (double)total_lookups / elapsed_sec;
        
        if (trial == NUM_TRIALS - 1) {
            if (algo == ALGO_DIR24) {
                result.memory_bytes = LPM_IPV4_DIR24_SIZE * sizeof(struct lpm_dir24_entry) +
                                     trie->tbl8_groups_used * 256 * sizeof(struct lpm_tbl8_entry);
            } else {
                result.memory_bytes = trie->pool_used * sizeof(lpm_node_t);
            }
        }
        
        free(test_addrs);
        free(next_hops);
        lpm_destroy(trie);
    }
    
    calculate_stats(trial_results, NUM_TRIALS,
                   &result.median_lookups_per_sec,
                   &result.mean_lookups_per_sec,
                   &result.stddev_lookups_per_sec,
                   &result.min_lookups_per_sec,
                   &result.max_lookups_per_sec);
    
    return result;
}

/* Benchmark single IPv6 lookup */
static benchmark_result_t benchmark_ipv6_single(algorithm_t algo, int num_prefixes)
{
    benchmark_result_t result = {0};
    const algorithm_info_t *info = &ALGORITHMS[algo];
    
    double trial_results[NUM_TRIALS];
    
    for (int trial = 0; trial < NUM_TRIALS; trial++) {
        srand(42 + trial);
        
        lpm_trie_t *trie = info->create();
        if (!trie) {
            fprintf(stderr, "Failed to create trie for %s\n", info->name);
            return result;
        }
        
        /* Add prefixes */
        for (int i = 0; i < num_prefixes; i++) {
            uint8_t prefix[16];
            generate_random_ipv6(prefix);
            uint8_t prefix_len = 8 + (rand() % 121); /* 8 to 128 */
            info->add(trie, prefix, prefix_len, i);
        }
        
        /* Generate test addresses */
        uint8_t (*test_addrs)[16] = malloc(NUM_LOOKUPS * sizeof(*test_addrs));
        if (!test_addrs) {
            fprintf(stderr, "Failed to allocate test addresses\n");
            lpm_destroy(trie);
            return result;
        }
        
        for (int i = 0; i < NUM_LOOKUPS; i++) {
            generate_random_ipv6(test_addrs[i]);
        }
        
        /* Warmup */
        for (int i = 0; i < WARMUP_LOOKUPS && i < NUM_LOOKUPS; i++) {
            if (algo == ALGO_WIDE16) {
                volatile uint32_t nh = lpm_lookup_ipv6_wide16(trie, test_addrs[i]);
                (void)nh;
            } else {
                volatile uint32_t nh = lpm_lookup_ipv6_8stride(trie, test_addrs[i]);
                (void)nh;
            }
        }
        
        /* Benchmark */
        struct timespec start, end;
        clock_gettime(CLOCK_MONOTONIC, &start);
        
        if (algo == ALGO_WIDE16) {
            for (int i = 0; i < NUM_LOOKUPS; i++) {
                volatile uint32_t nh = lpm_lookup_ipv6_wide16(trie, test_addrs[i]);
                (void)nh;
            }
        } else {
            for (int i = 0; i < NUM_LOOKUPS; i++) {
                volatile uint32_t nh = lpm_lookup_ipv6_8stride(trie, test_addrs[i]);
                (void)nh;
            }
        }
        
        clock_gettime(CLOCK_MONOTONIC, &end);
        
        double elapsed_us = time_diff_us(&start, &end);
        trial_results[trial] = (NUM_LOOKUPS / elapsed_us) * 1000000.0;
        
        if (trial == NUM_TRIALS - 1) {
            if (algo == ALGO_WIDE16) {
                result.memory_bytes = trie->wide_pool_used * sizeof(struct lpm_node_16) +
                                     trie->pool_used * sizeof(lpm_node_t);
            } else {
                result.memory_bytes = trie->pool_used * sizeof(lpm_node_t);
            }
        }
        
        free(test_addrs);
        lpm_destroy(trie);
    }
    
    calculate_stats(trial_results, NUM_TRIALS,
                   &result.median_lookups_per_sec,
                   &result.mean_lookups_per_sec,
                   &result.stddev_lookups_per_sec,
                   &result.min_lookups_per_sec,
                   &result.max_lookups_per_sec);
    
    return result;
}

/* Benchmark batch IPv6 lookup */
static benchmark_result_t benchmark_ipv6_batch(algorithm_t algo, int num_prefixes)
{
    benchmark_result_t result = {0};
    const algorithm_info_t *info = &ALGORITHMS[algo];
    
    double trial_results[NUM_TRIALS];
    
    for (int trial = 0; trial < NUM_TRIALS; trial++) {
        srand(42 + trial);
        
        lpm_trie_t *trie = info->create();
        if (!trie) {
            fprintf(stderr, "Failed to create trie for %s\n", info->name);
            return result;
        }
        
        /* Add prefixes */
        for (int i = 0; i < num_prefixes; i++) {
            uint8_t prefix[16];
            generate_random_ipv6(prefix);
            uint8_t prefix_len = 8 + (rand() % 121);
            info->add(trie, prefix, prefix_len, i);
        }
        
        /* Generate test addresses */
        int num_batches = NUM_LOOKUPS / BATCH_SIZE;
        uint8_t (*test_addrs)[16] = malloc(num_batches * BATCH_SIZE * sizeof(*test_addrs));
        uint32_t *next_hops = malloc(BATCH_SIZE * sizeof(uint32_t));
        
        if (!test_addrs || !next_hops) {
            fprintf(stderr, "Failed to allocate test data\n");
            free(test_addrs);
            free(next_hops);
            lpm_destroy(trie);
            return result;
        }
        
        for (int i = 0; i < num_batches * BATCH_SIZE; i++) {
            generate_random_ipv6(test_addrs[i]);
        }
        
        /* Warmup */
        if (algo == ALGO_WIDE16) {
            lpm_lookup_batch_ipv6_wide16(trie, test_addrs, next_hops, BATCH_SIZE);
        } else {
            lpm_lookup_batch_ipv6_8stride(trie, test_addrs, next_hops, BATCH_SIZE);
        }
        
        /* Benchmark */
        struct timespec start, end;
        volatile uint32_t checksum = 0;
        
        clock_gettime(CLOCK_MONOTONIC, &start);
        
        for (int batch = 0; batch < num_batches; batch++) {
            if (algo == ALGO_WIDE16) {
                lpm_lookup_batch_ipv6_wide16(trie, &test_addrs[batch * BATCH_SIZE], 
                                             next_hops, BATCH_SIZE);
            } else {
                lpm_lookup_batch_ipv6_8stride(trie, &test_addrs[batch * BATCH_SIZE], 
                                              next_hops, BATCH_SIZE);
            }
            checksum += next_hops[0];
        }
        
        clock_gettime(CLOCK_MONOTONIC, &end);
        (void)checksum;
        
        double elapsed_us = time_diff_us(&start, &end);
        double total_lookups = num_batches * BATCH_SIZE;
        trial_results[trial] = (total_lookups / elapsed_us) * 1000000.0;
        
        if (trial == NUM_TRIALS - 1) {
            if (algo == ALGO_WIDE16) {
                result.memory_bytes = trie->wide_pool_used * sizeof(struct lpm_node_16) +
                                     trie->pool_used * sizeof(lpm_node_t);
            } else {
                result.memory_bytes = trie->pool_used * sizeof(lpm_node_t);
            }
        }
        
        free(test_addrs);
        free(next_hops);
        lpm_destroy(trie);
    }
    
    calculate_stats(trial_results, NUM_TRIALS,
                   &result.median_lookups_per_sec,
                   &result.mean_lookups_per_sec,
                   &result.stddev_lookups_per_sec,
                   &result.min_lookups_per_sec,
                   &result.max_lookups_per_sec);
    
    return result;
}

/* ============================================================================
 * DPDK Benchmark functions
 * ============================================================================ */

#ifdef HAVE_DPDK

/* Benchmark DPDK IPv4 single lookup */
static benchmark_result_t benchmark_dpdk_ipv4_single(int num_prefixes)
{
    benchmark_result_t result = {0};
    
    if (!dpdk_initialized) {
        return result;
    }
    
    double trial_results[NUM_TRIALS];
    
    for (int trial = 0; trial < NUM_TRIALS; trial++) {
        srand(42 + trial);
        
        struct rte_lpm_config config = {
            .max_rules = DPDK_LPM_MAX_RULES,
            .number_tbl8s = DPDK_LPM_TBL8_NUM_GROUPS,
            .flags = 0
        };
        
        char lpm_name[32];
        snprintf(lpm_name, sizeof(lpm_name), "lpm_ipv4_%d_%d", num_prefixes, trial);
        
        struct rte_lpm *lpm = rte_lpm_create(lpm_name, SOCKET_ID_ANY, &config);
        if (lpm == NULL) {
            fprintf(stderr, "Failed to create DPDK LPM: %s\n", rte_strerror(rte_errno));
            return result;
        }
        
        /* Add prefixes */
        for (int i = 0; i < num_prefixes; i++) {
            uint8_t prefix[4];
            generate_random_ipv4(prefix);
            uint8_t prefix_len = 8 + (rand() % 25);
            uint32_t ip = ipv4_to_uint32(prefix);
            uint32_t next_hop = i % 256;
            rte_lpm_add(lpm, ip, prefix_len, next_hop);
        }
        
        /* Generate test addresses */
        uint32_t *test_addrs = malloc(NUM_LOOKUPS * sizeof(uint32_t));
        if (!test_addrs) {
            fprintf(stderr, "Failed to allocate test addresses\n");
            rte_lpm_free(lpm);
            return result;
        }
        
        for (int i = 0; i < NUM_LOOKUPS; i++) {
            uint8_t addr[4];
            generate_random_ipv4(addr);
            test_addrs[i] = ipv4_to_uint32(addr);
        }
        
        /* Warmup */
        uint32_t next_hop;
        for (int i = 0; i < WARMUP_LOOKUPS && i < NUM_LOOKUPS; i++) {
            rte_lpm_lookup(lpm, test_addrs[i], &next_hop);
        }
        
        /* Benchmark */
        struct timespec start, end;
        clock_gettime(CLOCK_MONOTONIC, &start);
        
        for (int i = 0; i < NUM_LOOKUPS; i++) {
            volatile int ret = rte_lpm_lookup(lpm, test_addrs[i], &next_hop);
            (void)ret;
        }
        
        clock_gettime(CLOCK_MONOTONIC, &end);
        
        double elapsed_us = time_diff_us(&start, &end);
        trial_results[trial] = (NUM_LOOKUPS / elapsed_us) * 1000000.0;
        
        if (trial == NUM_TRIALS - 1) {
            /* Estimate memory: tbl24 + tbl8 groups */
            result.memory_bytes = (1 << 24) * 4 + DPDK_LPM_TBL8_NUM_GROUPS * 256 * 4;
        }
        
        free(test_addrs);
        rte_lpm_free(lpm);
    }
    
    calculate_stats(trial_results, NUM_TRIALS,
                   &result.median_lookups_per_sec,
                   &result.mean_lookups_per_sec,
                   &result.stddev_lookups_per_sec,
                   &result.min_lookups_per_sec,
                   &result.max_lookups_per_sec);
    
    return result;
}

/* Benchmark DPDK IPv4 batch lookup */
static benchmark_result_t benchmark_dpdk_ipv4_batch(int num_prefixes)
{
    benchmark_result_t result = {0};
    
    if (!dpdk_initialized) {
        return result;
    }
    
    double trial_results[NUM_TRIALS];
    
    for (int trial = 0; trial < NUM_TRIALS; trial++) {
        srand(42 + trial);
        
        struct rte_lpm_config config = {
            .max_rules = DPDK_LPM_MAX_RULES,
            .number_tbl8s = DPDK_LPM_TBL8_NUM_GROUPS,
            .flags = 0
        };
        
        char lpm_name[32];
        snprintf(lpm_name, sizeof(lpm_name), "lpm_ipv4_b_%d_%d", num_prefixes, trial);
        
        struct rte_lpm *lpm = rte_lpm_create(lpm_name, SOCKET_ID_ANY, &config);
        if (lpm == NULL) {
            fprintf(stderr, "Failed to create DPDK LPM: %s\n", rte_strerror(rte_errno));
            return result;
        }
        
        /* Add prefixes */
        for (int i = 0; i < num_prefixes; i++) {
            uint8_t prefix[4];
            generate_random_ipv4(prefix);
            uint8_t prefix_len = 8 + (rand() % 25);
            uint32_t ip = ipv4_to_uint32(prefix);
            uint32_t next_hop = i % 256;
            rte_lpm_add(lpm, ip, prefix_len, next_hop);
        }
        
        /* Generate test addresses */
        int num_batches = NUM_LOOKUPS / BATCH_SIZE;
        uint32_t *test_addrs = malloc(num_batches * BATCH_SIZE * sizeof(uint32_t));
        uint32_t *next_hops = malloc(BATCH_SIZE * sizeof(uint32_t));
        
        if (!test_addrs || !next_hops) {
            fprintf(stderr, "Failed to allocate test data\n");
            free(test_addrs);
            free(next_hops);
            rte_lpm_free(lpm);
            return result;
        }
        
        for (int i = 0; i < num_batches * BATCH_SIZE; i++) {
            uint8_t addr[4];
            generate_random_ipv4(addr);
            test_addrs[i] = ipv4_to_uint32(addr);
        }
        
        /* Warmup */
        rte_lpm_lookup_bulk(lpm, test_addrs, next_hops, BATCH_SIZE);
        
        /* Benchmark */
        struct timespec start, end;
        volatile uint32_t checksum = 0;
        
        clock_gettime(CLOCK_MONOTONIC, &start);
        
        for (int batch = 0; batch < num_batches; batch++) {
            rte_lpm_lookup_bulk(lpm, &test_addrs[batch * BATCH_SIZE], 
                               next_hops, BATCH_SIZE);
            checksum += next_hops[0];
        }
        
        clock_gettime(CLOCK_MONOTONIC, &end);
        (void)checksum;
        
        double elapsed_us = time_diff_us(&start, &end);
        double total_lookups = num_batches * BATCH_SIZE;
        trial_results[trial] = (total_lookups / elapsed_us) * 1000000.0;
        
        if (trial == NUM_TRIALS - 1) {
            result.memory_bytes = (1 << 24) * 4 + DPDK_LPM_TBL8_NUM_GROUPS * 256 * 4;
        }
        
        free(test_addrs);
        free(next_hops);
        rte_lpm_free(lpm);
    }
    
    calculate_stats(trial_results, NUM_TRIALS,
                   &result.median_lookups_per_sec,
                   &result.mean_lookups_per_sec,
                   &result.stddev_lookups_per_sec,
                   &result.min_lookups_per_sec,
                   &result.max_lookups_per_sec);
    
    return result;
}

/* Benchmark DPDK IPv6 single lookup */
static benchmark_result_t benchmark_dpdk_ipv6_single(int num_prefixes)
{
    benchmark_result_t result = {0};
    
    if (!dpdk_initialized) {
        return result;
    }
    
    double trial_results[NUM_TRIALS];
    
    for (int trial = 0; trial < NUM_TRIALS; trial++) {
        srand(42 + trial);
        
        struct rte_lpm6_config config = {
            .max_rules = DPDK_LPM6_MAX_RULES,
            .number_tbl8s = DPDK_LPM6_NUMBER_TBL8S,
            .flags = 0
        };
        
        char lpm_name[32];
        snprintf(lpm_name, sizeof(lpm_name), "lpm_ipv6_%d_%d", num_prefixes, trial);
        
        struct rte_lpm6 *lpm6 = rte_lpm6_create(lpm_name, SOCKET_ID_ANY, &config);
        if (lpm6 == NULL) {
            fprintf(stderr, "Failed to create DPDK LPM6: %s\n", rte_strerror(rte_errno));
            return result;
        }
        
        /* Add prefixes */
        for (int i = 0; i < num_prefixes; i++) {
            uint8_t prefix[16];
            generate_random_ipv6(prefix);
            uint8_t prefix_len = 8 + (rand() % 121);
            uint32_t next_hop = i;
            rte_lpm6_add(lpm6, (const struct rte_ipv6_addr *)prefix, prefix_len, next_hop);
        }
        
        /* Generate test addresses */
        uint8_t (*test_addrs)[16] = malloc(NUM_LOOKUPS * sizeof(*test_addrs));
        if (!test_addrs) {
            fprintf(stderr, "Failed to allocate test addresses\n");
            rte_lpm6_free(lpm6);
            return result;
        }
        
        for (int i = 0; i < NUM_LOOKUPS; i++) {
            generate_random_ipv6(test_addrs[i]);
        }
        
        /* Warmup */
        uint32_t next_hop;
        for (int i = 0; i < WARMUP_LOOKUPS && i < NUM_LOOKUPS; i++) {
            rte_lpm6_lookup(lpm6, (const struct rte_ipv6_addr *)test_addrs[i], &next_hop);
        }
        
        /* Benchmark */
        struct timespec start, end;
        clock_gettime(CLOCK_MONOTONIC, &start);
        
        for (int i = 0; i < NUM_LOOKUPS; i++) {
            volatile int ret = rte_lpm6_lookup(lpm6, (const struct rte_ipv6_addr *)test_addrs[i], &next_hop);
            (void)ret;
        }
        
        clock_gettime(CLOCK_MONOTONIC, &end);
        
        double elapsed_us = time_diff_us(&start, &end);
        trial_results[trial] = (NUM_LOOKUPS / elapsed_us) * 1000000.0;
        
        if (trial == NUM_TRIALS - 1) {
            /* Estimate LPM6 memory */
            result.memory_bytes = DPDK_LPM6_NUMBER_TBL8S * 256 * 8;
        }
        
        free(test_addrs);
        rte_lpm6_free(lpm6);
    }
    
    calculate_stats(trial_results, NUM_TRIALS,
                   &result.median_lookups_per_sec,
                   &result.mean_lookups_per_sec,
                   &result.stddev_lookups_per_sec,
                   &result.min_lookups_per_sec,
                   &result.max_lookups_per_sec);
    
    return result;
}

/* Benchmark DPDK IPv6 batch lookup */
static benchmark_result_t benchmark_dpdk_ipv6_batch(int num_prefixes)
{
    benchmark_result_t result = {0};
    
    if (!dpdk_initialized) {
        return result;
    }
    
    double trial_results[NUM_TRIALS];
    
    for (int trial = 0; trial < NUM_TRIALS; trial++) {
        srand(42 + trial);
        
        struct rte_lpm6_config config = {
            .max_rules = DPDK_LPM6_MAX_RULES,
            .number_tbl8s = DPDK_LPM6_NUMBER_TBL8S,
            .flags = 0
        };
        
        char lpm_name[32];
        snprintf(lpm_name, sizeof(lpm_name), "lpm_ipv6_b_%d_%d", num_prefixes, trial);
        
        struct rte_lpm6 *lpm6 = rte_lpm6_create(lpm_name, SOCKET_ID_ANY, &config);
        if (lpm6 == NULL) {
            fprintf(stderr, "Failed to create DPDK LPM6: %s\n", rte_strerror(rte_errno));
            return result;
        }
        
        /* Add prefixes */
        for (int i = 0; i < num_prefixes; i++) {
            uint8_t prefix[16];
            generate_random_ipv6(prefix);
            uint8_t prefix_len = 8 + (rand() % 121);
            uint32_t next_hop = i;
            rte_lpm6_add(lpm6, (const struct rte_ipv6_addr *)prefix, prefix_len, next_hop);
        }
        
        /* Generate test addresses */
        int num_batches = NUM_LOOKUPS / BATCH_SIZE;
        uint8_t (*test_addrs)[16] = malloc(num_batches * BATCH_SIZE * sizeof(*test_addrs));
        int32_t *next_hops = malloc(BATCH_SIZE * sizeof(int32_t));
        
        if (!test_addrs || !next_hops) {
            fprintf(stderr, "Failed to allocate test data\n");
            free(test_addrs);
            free(next_hops);
            rte_lpm6_free(lpm6);
            return result;
        }
        
        for (int i = 0; i < num_batches * BATCH_SIZE; i++) {
            generate_random_ipv6(test_addrs[i]);
        }
        
        /* Warmup */
        rte_lpm6_lookup_bulk_func(lpm6, 
                                  (struct rte_ipv6_addr *)test_addrs,
                                  next_hops, BATCH_SIZE);
        
        /* Benchmark */
        struct timespec start, end;
        volatile int32_t checksum = 0;
        
        clock_gettime(CLOCK_MONOTONIC, &start);
        
        for (int batch = 0; batch < num_batches; batch++) {
            rte_lpm6_lookup_bulk_func(lpm6, 
                                      (struct rte_ipv6_addr *)&test_addrs[batch * BATCH_SIZE],
                                      next_hops, BATCH_SIZE);
            checksum += next_hops[0];
        }
        
        clock_gettime(CLOCK_MONOTONIC, &end);
        (void)checksum;
        
        double elapsed_us = time_diff_us(&start, &end);
        double total_lookups = num_batches * BATCH_SIZE;
        trial_results[trial] = (total_lookups / elapsed_us) * 1000000.0;
        
        if (trial == NUM_TRIALS - 1) {
            result.memory_bytes = DPDK_LPM6_NUMBER_TBL8S * 256 * 8;
        }
        
        free(test_addrs);
        free(next_hops);
        rte_lpm6_free(lpm6);
    }
    
    calculate_stats(trial_results, NUM_TRIALS,
                   &result.median_lookups_per_sec,
                   &result.mean_lookups_per_sec,
                   &result.stddev_lookups_per_sec,
                   &result.min_lookups_per_sec,
                   &result.max_lookups_per_sec);
    
    return result;
}

#endif /* HAVE_DPDK */

/* ============================================================================
 * External LPM Library Benchmark functions
 * ============================================================================ */

#ifdef HAVE_RMIND_LPM

/* Benchmark rmind/liblpm IPv4 single lookup */
static benchmark_result_t benchmark_rmind_ipv4_single(int num_prefixes)
{
    benchmark_result_t result = {0};
    double trial_results[NUM_TRIALS];
    
    for (int trial = 0; trial < NUM_TRIALS; trial++) {
        srand(42 + trial);
        
        /* Create LPM structure */
        rmind_lpm_t *lpm = rmind_lpm_create();
        if (!lpm) {
            fprintf(stderr, "Failed to create rmind LPM\n");
            return result;
        }
        
        /* Add prefixes */
        for (int i = 0; i < num_prefixes; i++) {
            uint8_t prefix[4];
            generate_random_ipv4(prefix);
            uint8_t prefix_len = 8 + (rand() % 25);
            /* rmind/liblpm uses network byte order and stores pointer */
            rmind_lpm_insert(lpm, prefix, 4, prefix_len, (void *)(uintptr_t)(i + 1));
        }
        
        /* Generate test addresses */
        uint8_t (*test_addrs)[4] = malloc(NUM_LOOKUPS * sizeof(*test_addrs));
        if (!test_addrs) {
            rmind_lpm_destroy(lpm);
            return result;
        }
        
        for (int i = 0; i < NUM_LOOKUPS; i++) {
            generate_random_ipv4(test_addrs[i]);
        }
        
        /* Warmup */
        for (int i = 0; i < WARMUP_LOOKUPS && i < NUM_LOOKUPS; i++) {
            volatile void *val = rmind_lpm_lookup(lpm, test_addrs[i], 4);
            (void)val;
        }
        
        /* Benchmark */
        struct timespec start, end;
        clock_gettime(CLOCK_MONOTONIC, &start);
        
        for (int i = 0; i < NUM_LOOKUPS; i++) {
            volatile void *val = rmind_lpm_lookup(lpm, test_addrs[i], 4);
            (void)val;
        }
        
        clock_gettime(CLOCK_MONOTONIC, &end);
        
        double elapsed_us = time_diff_us(&start, &end);
        trial_results[trial] = (NUM_LOOKUPS / elapsed_us) * 1000000.0;
        
        free(test_addrs);
        rmind_lpm_destroy(lpm);
    }
    
    calculate_stats(trial_results, NUM_TRIALS,
                   &result.median_lookups_per_sec,
                   &result.mean_lookups_per_sec,
                   &result.stddev_lookups_per_sec,
                   &result.min_lookups_per_sec,
                   &result.max_lookups_per_sec);
    
    return result;
}

/* Benchmark rmind/liblpm IPv4 batch lookup (no native batch, emulate) */
static benchmark_result_t benchmark_rmind_ipv4_batch(int num_prefixes)
{
    benchmark_result_t result = {0};
    double trial_results[NUM_TRIALS];
    
    for (int trial = 0; trial < NUM_TRIALS; trial++) {
        srand(42 + trial);
        
        rmind_lpm_t *lpm = rmind_lpm_create();
        if (!lpm) {
            return result;
        }
        
        for (int i = 0; i < num_prefixes; i++) {
            uint8_t prefix[4];
            generate_random_ipv4(prefix);
            uint8_t prefix_len = 8 + (rand() % 25);
            rmind_lpm_insert(lpm, prefix, 4, prefix_len, (void *)(uintptr_t)(i + 1));
        }
        
        int num_batches = NUM_LOOKUPS / BATCH_SIZE;
        uint8_t (*test_addrs)[4] = malloc(num_batches * BATCH_SIZE * sizeof(*test_addrs));
        if (!test_addrs) {
            rmind_lpm_destroy(lpm);
            return result;
        }
        
        for (int i = 0; i < num_batches * BATCH_SIZE; i++) {
            generate_random_ipv4(test_addrs[i]);
        }
        
        /* Warmup */
        for (int i = 0; i < BATCH_SIZE; i++) {
            volatile void *val = rmind_lpm_lookup(lpm, test_addrs[i], 4);
            (void)val;
        }
        
        struct timespec start, end;
        volatile uintptr_t checksum = 0;
        
        clock_gettime(CLOCK_MONOTONIC, &start);
        
        for (int batch = 0; batch < num_batches; batch++) {
            for (int i = 0; i < BATCH_SIZE; i++) {
                void *val = rmind_lpm_lookup(lpm, test_addrs[batch * BATCH_SIZE + i], 4);
                checksum += (uintptr_t)val;
            }
        }
        
        clock_gettime(CLOCK_MONOTONIC, &end);
        (void)checksum;
        
        double elapsed_us = time_diff_us(&start, &end);
        double total_lookups = num_batches * BATCH_SIZE;
        trial_results[trial] = (total_lookups / elapsed_us) * 1000000.0;
        
        free(test_addrs);
        rmind_lpm_destroy(lpm);
    }
    
    calculate_stats(trial_results, NUM_TRIALS,
                   &result.median_lookups_per_sec,
                   &result.mean_lookups_per_sec,
                   &result.stddev_lookups_per_sec,
                   &result.min_lookups_per_sec,
                   &result.max_lookups_per_sec);
    
    return result;
}

/* Benchmark rmind/liblpm IPv6 single lookup */
static benchmark_result_t benchmark_rmind_ipv6_single(int num_prefixes)
{
    benchmark_result_t result = {0};
    double trial_results[NUM_TRIALS];
    
    for (int trial = 0; trial < NUM_TRIALS; trial++) {
        srand(42 + trial);
        
        rmind_lpm_t *lpm = rmind_lpm_create();
        if (!lpm) {
            return result;
        }
        
        for (int i = 0; i < num_prefixes; i++) {
            uint8_t prefix[16];
            generate_random_ipv6(prefix);
            uint8_t prefix_len = 8 + (rand() % 121);
            rmind_lpm_insert(lpm, prefix, 16, prefix_len, (void *)(uintptr_t)(i + 1));
        }
        
        uint8_t (*test_addrs)[16] = malloc(NUM_LOOKUPS * sizeof(*test_addrs));
        if (!test_addrs) {
            rmind_lpm_destroy(lpm);
            return result;
        }
        
        for (int i = 0; i < NUM_LOOKUPS; i++) {
            generate_random_ipv6(test_addrs[i]);
        }
        
        for (int i = 0; i < WARMUP_LOOKUPS && i < NUM_LOOKUPS; i++) {
            volatile void *val = rmind_lpm_lookup(lpm, test_addrs[i], 16);
            (void)val;
        }
        
        struct timespec start, end;
        clock_gettime(CLOCK_MONOTONIC, &start);
        
        for (int i = 0; i < NUM_LOOKUPS; i++) {
            volatile void *val = rmind_lpm_lookup(lpm, test_addrs[i], 16);
            (void)val;
        }
        
        clock_gettime(CLOCK_MONOTONIC, &end);
        
        double elapsed_us = time_diff_us(&start, &end);
        trial_results[trial] = (NUM_LOOKUPS / elapsed_us) * 1000000.0;
        
        free(test_addrs);
        rmind_lpm_destroy(lpm);
    }
    
    calculate_stats(trial_results, NUM_TRIALS,
                   &result.median_lookups_per_sec,
                   &result.mean_lookups_per_sec,
                   &result.stddev_lookups_per_sec,
                   &result.min_lookups_per_sec,
                   &result.max_lookups_per_sec);
    
    return result;
}

/* Benchmark rmind/liblpm IPv6 batch lookup */
static benchmark_result_t benchmark_rmind_ipv6_batch(int num_prefixes)
{
    benchmark_result_t result = {0};
    double trial_results[NUM_TRIALS];
    
    for (int trial = 0; trial < NUM_TRIALS; trial++) {
        srand(42 + trial);
        
        rmind_lpm_t *lpm = rmind_lpm_create();
        if (!lpm) {
            return result;
        }
        
        for (int i = 0; i < num_prefixes; i++) {
            uint8_t prefix[16];
            generate_random_ipv6(prefix);
            uint8_t prefix_len = 8 + (rand() % 121);
            rmind_lpm_insert(lpm, prefix, 16, prefix_len, (void *)(uintptr_t)(i + 1));
        }
        
        int num_batches = NUM_LOOKUPS / BATCH_SIZE;
        uint8_t (*test_addrs)[16] = malloc(num_batches * BATCH_SIZE * sizeof(*test_addrs));
        if (!test_addrs) {
            rmind_lpm_destroy(lpm);
            return result;
        }
        
        for (int i = 0; i < num_batches * BATCH_SIZE; i++) {
            generate_random_ipv6(test_addrs[i]);
        }
        
        for (int i = 0; i < BATCH_SIZE; i++) {
            volatile void *val = rmind_lpm_lookup(lpm, test_addrs[i], 16);
            (void)val;
        }
        
        struct timespec start, end;
        volatile uintptr_t checksum = 0;
        
        clock_gettime(CLOCK_MONOTONIC, &start);
        
        for (int batch = 0; batch < num_batches; batch++) {
            for (int i = 0; i < BATCH_SIZE; i++) {
                void *val = rmind_lpm_lookup(lpm, test_addrs[batch * BATCH_SIZE + i], 16);
                checksum += (uintptr_t)val;
            }
        }
        
        clock_gettime(CLOCK_MONOTONIC, &end);
        (void)checksum;
        
        double elapsed_us = time_diff_us(&start, &end);
        double total_lookups = num_batches * BATCH_SIZE;
        trial_results[trial] = (total_lookups / elapsed_us) * 1000000.0;
        
        free(test_addrs);
        rmind_lpm_destroy(lpm);
    }
    
    calculate_stats(trial_results, NUM_TRIALS,
                   &result.median_lookups_per_sec,
                   &result.mean_lookups_per_sec,
                   &result.stddev_lookups_per_sec,
                   &result.min_lookups_per_sec,
                   &result.max_lookups_per_sec);
    
    return result;
}

#endif /* HAVE_RMIND_LPM */


/* ============================================================================
 * libpatricia (brandt/network-patricia) Benchmark Functions
 * ============================================================================ */

#ifdef HAVE_LIBPATRICIA

/* Benchmark libpatricia IPv4 single lookup */
static benchmark_result_t benchmark_patricia_ipv4_single(int num_prefixes)
{
    benchmark_result_t result = {0};
    double trial_results[NUM_TRIALS];
    
    for (int trial = 0; trial < NUM_TRIALS; trial++) {
        srand(42 + trial);
        
        /* Create head node with (key=0, mask=0) as required by libpatricia 
         * Per ptest.c:
         * 1. Give it an address of 0.0.0.0 and a mask of 0x00000000 (matches everything)
         * 2. Set the bit position (p_b) to 0
         * 3. Set the number of masks to 1 (the default one)
         * 4. Point the head's 'left' and 'right' pointers to itself
         */
        struct ptree *head = malloc(sizeof(struct ptree));
        if (!head) {
            return result;
        }
        memset(head, 0, sizeof(struct ptree));
        head->p_m = malloc(sizeof(struct ptree_mask));
        if (!head->p_m) {
            free(head);
            return result;
        }
        memset(head->p_m, 0, sizeof(struct ptree_mask));
        head->p_m->pm_mask = 0;
        head->p_m->pm_data = NULL;  /* Default route has no data */
        head->p_mlen = 1;
        head->p_b = 0;
        head->p_key = 0;
        head->p_left = head;
        head->p_right = head;
        
        /* Insert prefixes */
        for (int i = 0; i < num_prefixes; i++) {
            uint8_t prefix[4];
            generate_random_ipv4(prefix);
            uint8_t prefix_len = 8 + (rand() % 25);
            
            struct ptree *node = malloc(sizeof(struct ptree));
            if (!node) continue;
            memset(node, 0, sizeof(struct ptree));
            
            node->p_key = ipv4_to_uint32(prefix);
            node->p_m = malloc(sizeof(struct ptree_mask));
            if (node->p_m) {
                node->p_m->pm_mask = (0xFFFFFFFFUL << (32 - prefix_len)) & 0xFFFFFFFFUL;
                node->p_m->pm_data = (void *)(uintptr_t)(i + 1);
                node->p_mlen = 1;
            }
            node->p_b = prefix_len;
            pat_insert(node, head);
        }
        
        /* Generate test addresses */
        uint32_t *test_addrs = malloc(NUM_LOOKUPS * sizeof(uint32_t));
        if (!test_addrs) {
            /* Clean up patricia trie - simplified cleanup */
            free(head);
            return result;
        }
        
        for (int i = 0; i < NUM_LOOKUPS; i++) {
            uint8_t addr[4];
            generate_random_ipv4(addr);
            test_addrs[i] = ipv4_to_uint32(addr);
        }
        
        /* Warmup */
        for (int i = 0; i < WARMUP_LOOKUPS; i++) {
            volatile struct ptree *found = pat_search(test_addrs[i], head);
            (void)found;
        }
        
        /* Timed lookups */
        struct timespec start, end;
        volatile uintptr_t checksum = 0;
        
        clock_gettime(CLOCK_MONOTONIC, &start);
        for (int i = 0; i < NUM_LOOKUPS; i++) {
            struct ptree *found = pat_search(test_addrs[i], head);
            if (found && found->p_m) {
                checksum += (uintptr_t)found->p_m->pm_data;
            }
        }
        clock_gettime(CLOCK_MONOTONIC, &end);
        
        (void)checksum;
        double elapsed_us = time_diff_us(&start, &end);
        trial_results[trial] = ((double)NUM_LOOKUPS / elapsed_us) * 1000000.0;
        
        free(test_addrs);
        /* Note: full cleanup of patricia trie would require tree traversal */
        free(head);
    }
    
    calculate_stats(trial_results, NUM_TRIALS,
                   &result.median_lookups_per_sec,
                   &result.mean_lookups_per_sec,
                   &result.stddev_lookups_per_sec,
                   &result.min_lookups_per_sec,
                   &result.max_lookups_per_sec);
    
    return result;
}

/* Benchmark libpatricia IPv4 batch lookup */
static benchmark_result_t benchmark_patricia_ipv4_batch(int num_prefixes)
{
    benchmark_result_t result = {0};
    double trial_results[NUM_TRIALS];
    
    for (int trial = 0; trial < NUM_TRIALS; trial++) {
        srand(42 + trial);
        
        /* Create head node with (key=0, mask=0) as required by libpatricia */
        struct ptree *head = malloc(sizeof(struct ptree));
        if (!head) {
            return result;
        }
        memset(head, 0, sizeof(struct ptree));
        head->p_m = malloc(sizeof(struct ptree_mask));
        if (!head->p_m) {
            free(head);
            return result;
        }
        memset(head->p_m, 0, sizeof(struct ptree_mask));
        head->p_m->pm_mask = 0;
        head->p_m->pm_data = NULL;
        head->p_mlen = 1;
        head->p_b = 0;
        head->p_key = 0;
        head->p_left = head;
        head->p_right = head;
        
        /* Insert prefixes */
        for (int i = 0; i < num_prefixes; i++) {
            uint8_t prefix[4];
            generate_random_ipv4(prefix);
            uint8_t prefix_len = 8 + (rand() % 25);
            
            struct ptree *node = malloc(sizeof(struct ptree));
            if (!node) continue;
            memset(node, 0, sizeof(struct ptree));
            
            node->p_key = ipv4_to_uint32(prefix);
            node->p_m = malloc(sizeof(struct ptree_mask));
            if (node->p_m) {
                node->p_m->pm_mask = (0xFFFFFFFFUL << (32 - prefix_len)) & 0xFFFFFFFFUL;
                node->p_m->pm_data = (void *)(uintptr_t)(i + 1);
                node->p_mlen = 1;
            }
            node->p_b = prefix_len;
            pat_insert(node, head);
        }
        
        /* Generate test addresses */
        int num_batches = NUM_LOOKUPS / BATCH_SIZE;
        int total_lookups = num_batches * BATCH_SIZE;
        uint32_t *test_addrs = malloc(total_lookups * sizeof(uint32_t));
        if (!test_addrs) {
            free(head);
            return result;
        }
        
        for (int i = 0; i < total_lookups; i++) {
            uint8_t addr[4];
            generate_random_ipv4(addr);
            test_addrs[i] = ipv4_to_uint32(addr);
        }
        
        /* Warmup */
        for (int i = 0; i < BATCH_SIZE; i++) {
            volatile struct ptree *found = pat_search(test_addrs[i], head);
            (void)found;
        }
        
        /* Timed batch lookups */
        struct timespec start, end;
        volatile uintptr_t checksum = 0;
        
        clock_gettime(CLOCK_MONOTONIC, &start);
        for (int b = 0; b < num_batches; b++) {
            for (int i = 0; i < BATCH_SIZE; i++) {
                struct ptree *found = pat_search(test_addrs[b * BATCH_SIZE + i], head);
                if (found && found->p_m) {
                    checksum += (uintptr_t)found->p_m->pm_data;
                }
            }
        }
        clock_gettime(CLOCK_MONOTONIC, &end);
        
        (void)checksum;
        double elapsed_us = time_diff_us(&start, &end);
        trial_results[trial] = ((double)total_lookups / elapsed_us) * 1000000.0;
        
        free(test_addrs);
        free(head);
    }
    
    calculate_stats(trial_results, NUM_TRIALS,
                   &result.median_lookups_per_sec,
                   &result.mean_lookups_per_sec,
                   &result.stddev_lookups_per_sec,
                   &result.min_lookups_per_sec,
                   &result.max_lookups_per_sec);
    
    return result;
}

#endif /* HAVE_LIBPATRICIA */

/* Run benchmark for a specific algorithm and lookup type */
static benchmark_result_t run_benchmark(algorithm_t algo, lookup_type_t lookup_type, int num_prefixes)
{
#ifdef HAVE_DPDK
    /* Handle DPDK algorithms separately */
    if (algo == ALGO_DPDK_IPV4) {
        if (lookup_type == LOOKUP_SINGLE) {
            return benchmark_dpdk_ipv4_single(num_prefixes);
        } else {
            return benchmark_dpdk_ipv4_batch(num_prefixes);
        }
    }
    if (algo == ALGO_DPDK_IPV6) {
        if (lookup_type == LOOKUP_SINGLE) {
            return benchmark_dpdk_ipv6_single(num_prefixes);
        } else {
            return benchmark_dpdk_ipv6_batch(num_prefixes);
        }
    }
#endif

#ifdef HAVE_RMIND_LPM
    /* Handle rmind/liblpm algorithms */
    if (algo == ALGO_RMIND_IPV4) {
        if (lookup_type == LOOKUP_SINGLE) {
            return benchmark_rmind_ipv4_single(num_prefixes);
        } else {
            return benchmark_rmind_ipv4_batch(num_prefixes);
        }
    }
    if (algo == ALGO_RMIND_IPV6) {
        if (lookup_type == LOOKUP_SINGLE) {
            return benchmark_rmind_ipv6_single(num_prefixes);
        } else {
            return benchmark_rmind_ipv6_batch(num_prefixes);
        }
    }
#endif

#ifdef HAVE_LIBPATRICIA
    /* Handle libpatricia algorithm */
    if (algo == ALGO_PATRICIA_IPV4) {
        if (lookup_type == LOOKUP_SINGLE) {
            return benchmark_patricia_ipv4_single(num_prefixes);
        } else {
            return benchmark_patricia_ipv4_batch(num_prefixes);
        }
    }
#endif

    const algorithm_info_t *info = &ALGORITHMS[algo];
    
    if (info->ip_version == IP_V4) {
        if (lookup_type == LOOKUP_SINGLE) {
            return benchmark_ipv4_single(algo, num_prefixes);
        } else {
            return benchmark_ipv4_batch(algo, num_prefixes);
        }
    } else {
        if (lookup_type == LOOKUP_SINGLE) {
            return benchmark_ipv6_single(algo, num_prefixes);
        } else {
            return benchmark_ipv6_batch(algo, num_prefixes);
        }
    }
}

/* ============================================================================
 * Output functions
 * ============================================================================ */

static void write_csv_header(FILE *f)
{
    fprintf(f, "num_prefixes,median_lookups_per_sec,mean_lookups_per_sec,stddev_lookups_per_sec,"
               "min_lookups_per_sec,max_lookups_per_sec,memory_bytes\n");
}

static void write_csv_row(FILE *f, int num_prefixes, const benchmark_result_t *result)
{
    fprintf(f, "%d,%.2f,%.2f,%.2f,%.2f,%.2f,%zu\n",
            num_prefixes,
            result->median_lookups_per_sec,
            result->mean_lookups_per_sec,
            result->stddev_lookups_per_sec,
            result->min_lookups_per_sec,
            result->max_lookups_per_sec,
            result->memory_bytes);
}

/* ============================================================================
 * Main
 * ============================================================================ */

static void print_usage(const char *prog)
{
    fprintf(stderr, "Usage: %s [OPTIONS]\n", prog);
    fprintf(stderr, "\nOptions:\n");
    fprintf(stderr, "  -a, --algorithm ALGO    Run only specified algorithm\n");
    fprintf(stderr, "  -t, --type TYPE         Run only specified lookup type (single, batch)\n");
    fprintf(stderr, "  -o, --output DIR        Output directory (default: benchmarks/data/algorithm_comparison)\n");
    fprintf(stderr, "  -c, --cpu CPU           Pin to specific CPU core (default: 0)\n");
    fprintf(stderr, "  -n, --name NAME         Override hostname for output files\n");
    fprintf(stderr, "  -q, --quiet             Suppress progress output\n");
    fprintf(stderr, "  -d, --debug             Run debug verification tests and exit\n");
    fprintf(stderr, "  -h, --help              Show this help\n");
    fprintf(stderr, "\nAlgorithms:\n");
    fprintf(stderr, "  dir24     - IPv4 DIR-24-8 (fastest for IPv4)\n");
    fprintf(stderr, "  4stride8  - IPv4 8-bit stride trie\n");
    fprintf(stderr, "  wide16    - IPv6 16-bit wide stride\n");
    fprintf(stderr, "  6stride8  - IPv6 8-bit stride trie\n");
#ifdef HAVE_DPDK
    fprintf(stderr, "  dpdk      - DPDK LPM (IPv4)\n");
    fprintf(stderr, "  dpdk6     - DPDK LPM6 (IPv6)\n");
#endif
#ifdef HAVE_RMIND_LPM
    fprintf(stderr, "  rmindlpm  - rmind/liblpm (IPv4)\n");
    fprintf(stderr, "  rmindlpm6 - rmind/liblpm (IPv6)\n");
#endif
#ifdef HAVE_LIBPATRICIA
    fprintf(stderr, "  patricia  - Patricia Trie (IPv4)\n");
#endif
}

int main(int argc, char **argv)
{
    /* Default options */
    int selected_algo = -1; /* -1 means all */
    int selected_lookup = -1; /* -1 means all */
    char output_dir[512] = "benchmarks/data/algorithm_comparison";
    int cpu_core = 0;
    char hostname[256] = "";
    bool quiet = false;
    
#ifdef HAVE_DPDK
    /* Initialize DPDK EAL (must be done early) */
    char *dpdk_argv[] = {
        argv[0],
        "-l", "0",           /* Use logical core 0 */
        "--proc-type", "primary",
        "--log-level", "error",
        "--no-pci",          /* Don't need PCI devices */
        "--no-huge",         /* Don't require hugepages (uses malloc) */
        "-m", "256",         /* Use 256MB of memory */
        "--",
        NULL
    };
    int dpdk_argc = sizeof(dpdk_argv) / sizeof(dpdk_argv[0]) - 1;
    
    int ret = rte_eal_init(dpdk_argc, dpdk_argv);
    if (ret < 0) {
        fprintf(stderr, "Warning: DPDK EAL initialization failed: %s\n", rte_strerror(rte_errno));
        fprintf(stderr, "DPDK benchmarks will be skipped.\n");
        dpdk_initialized = false;
    } else {
        dpdk_initialized = true;
    }
#endif
    
    /* Parse options */
    static struct option long_options[] = {
        {"algorithm", required_argument, 0, 'a'},
        {"type",      required_argument, 0, 't'},
        {"output",    required_argument, 0, 'o'},
        {"cpu",       required_argument, 0, 'c'},
        {"name",      required_argument, 0, 'n'},
        {"quiet",     no_argument,       0, 'q'},
        {"debug",     no_argument,       0, 'd'},
        {"help",      no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };
    
    int debug_mode = 0;
    int opt;
    while ((opt = getopt_long(argc, argv, "a:t:o:c:n:qdh", long_options, NULL)) != -1) {
        switch (opt) {
            case 'a':
                if (strcmp(optarg, "dir24") == 0) selected_algo = ALGO_DIR24;
                else if (strcmp(optarg, "4stride8") == 0) selected_algo = ALGO_4STRIDE8;
                else if (strcmp(optarg, "wide16") == 0) selected_algo = ALGO_WIDE16;
                else if (strcmp(optarg, "6stride8") == 0) selected_algo = ALGO_6STRIDE8;
#ifdef HAVE_DPDK
                else if (strcmp(optarg, "dpdk") == 0) selected_algo = ALGO_DPDK_IPV4;
                else if (strcmp(optarg, "dpdk6") == 0) selected_algo = ALGO_DPDK_IPV6;
#endif
#ifdef HAVE_RMIND_LPM
                else if (strcmp(optarg, "rmindlpm") == 0) selected_algo = ALGO_RMIND_IPV4;
                else if (strcmp(optarg, "rmindlpm6") == 0) selected_algo = ALGO_RMIND_IPV6;
#endif
#ifdef HAVE_LIBPATRICIA
                else if (strcmp(optarg, "patricia") == 0) selected_algo = ALGO_PATRICIA_IPV4;
#endif
                else {
                    fprintf(stderr, "Unknown algorithm: %s\n", optarg);
                    return 1;
                }
                break;
            case 't':
                if (strcmp(optarg, "single") == 0) selected_lookup = LOOKUP_SINGLE;
                else if (strcmp(optarg, "batch") == 0) selected_lookup = LOOKUP_BATCH;
                else {
                    fprintf(stderr, "Unknown lookup type: %s\n", optarg);
                    return 1;
                }
                break;
            case 'o':
                snprintf(output_dir, sizeof(output_dir), "%s", optarg);
                break;
            case 'c':
                cpu_core = atoi(optarg);
                break;
            case 'n':
                snprintf(hostname, sizeof(hostname), "%s", optarg);
                break;
            case 'q':
                quiet = true;
                break;
            case 'd':
                debug_mode = 1;
                break;
            case 'h':
            default:
                print_usage(argv[0]);
                return opt == 'h' ? 0 : 1;
        }
    }
    
    /* Get hostname if not provided */
    if (hostname[0] == '\0') {
        if (gethostname(hostname, sizeof(hostname)) != 0) {
            snprintf(hostname, sizeof(hostname), "unknown");
        }
    }
    
    /* Get CPU model */
    char cpu_model[256];
    get_cpu_model(cpu_model, sizeof(cpu_model));
    
    /* Sanitize CPU name for directory structure */
    char cpu_sanitized[256];
    sanitize_cpu_name(cpu_model, cpu_sanitized, sizeof(cpu_sanitized));
    
    /* Pin to CPU */
    if (pin_to_cpu(cpu_core) != 0) {
        fprintf(stderr, "Warning: Failed to pin to CPU %d\n", cpu_core);
    }
    
    if (!quiet) {
        printf("LPM Algorithm Scaling Benchmark\n");
        printf("================================\n");
        printf("CPU: %s\n", cpu_model);
        printf("Hostname: %s\n", hostname);
        printf("Pinned to core: %d\n", cpu_core);
        printf("Duration per point: %.1f seconds\n", BENCH_DURATION_SEC);
        printf("Trials: %d\n", NUM_TRIALS);
        printf("Batch size: %d\n", BATCH_SIZE);
        printf("Output directory: %s\n", output_dir);
#ifdef HAVE_DPDK
        if (dpdk_initialized) {
            printf("DPDK: enabled (version %d.%d.%d)\n", RTE_VER_YEAR, RTE_VER_MONTH, RTE_VER_MINOR);
        } else {
            printf("DPDK: initialization failed\n");
        }
#else
        printf("DPDK: not compiled in\n");
#endif
#ifdef HAVE_EXTERNAL_LPM
        printf("External LPM libraries:\n");
#ifdef HAVE_RMIND_LPM
        printf("  - rmind/liblpm: enabled\n");
#endif
#ifdef HAVE_LIBPATRICIA
        printf("  - libpatricia: enabled\n");
#endif
#else
        printf("External LPM libraries: not compiled in\n");
#endif
        printf("\n");
    }
    
    /* Debug mode - run verification tests and exit */
    if (debug_mode) {
        printf("Debug mode: no debug tests available\n");
        printf("Debug mode complete. Exiting.\n");
        return 0;
    }
    
    /* Create output directories */
    const char *ip_versions[] = {"ipv4", "ipv6"};
    const char *lookup_types[] = {"single", "batch"};
    
    for (int ip = 0; ip < 2; ip++) {
        for (int lt = 0; lt < 2; lt++) {
            char subdir[768];
            snprintf(subdir, sizeof(subdir), "%s/%s_%s_%s",
                    output_dir, cpu_sanitized, ip_versions[ip], lookup_types[lt]);
            if (mkdir_recursive(subdir) != 0) {
                fprintf(stderr, "Warning: Could not create directory %s\n", subdir);
            }
        }
    }
    
    /* Run benchmarks */
    for (int algo = 0; algo < ALGO_COUNT; algo++) {
        if (selected_algo >= 0 && algo != selected_algo) continue;
        
#ifdef HAVE_DPDK
        /* Skip DPDK algorithms if DPDK is not initialized */
        if ((algo == ALGO_DPDK_IPV4 || algo == ALGO_DPDK_IPV6) && !dpdk_initialized) {
            if (!quiet) {
                printf("Skipping DPDK algorithm (not initialized)\n\n");
            }
            continue;
        }
#endif
        
        const algorithm_info_t *info = &ALGORITHMS[algo];
        const char *ip_version = (info->ip_version == IP_V4) ? "ipv4" : "ipv6";
        
        for (int lt = 0; lt < 2; lt++) {
            if (selected_lookup >= 0 && lt != selected_lookup) continue;
            
            const char *lookup_name = (lt == 0) ? "single" : "batch";
            
            /* Open output file */
            char filepath[1024];
            snprintf(filepath, sizeof(filepath), "%s/%s_%s_%s/%s.csv",
                    output_dir, cpu_sanitized, ip_version, lookup_name, info->name);
            
            FILE *f = fopen(filepath, "w");
            if (!f) {
                fprintf(stderr, "Error: Could not open %s for writing\n", filepath);
                continue;
            }
            
            /* Write CSV header with metadata as comments */
            fprintf(f, "# LPM Benchmark Results\n");
            fprintf(f, "# Algorithm: %s (%s)\n", info->display_name, info->name);
            fprintf(f, "# IP Version: %s\n", ip_version);
            fprintf(f, "# Lookup Type: %s\n", lookup_name);
            fprintf(f, "# CPU: %s\n", cpu_model);
            fprintf(f, "# Hostname: %s\n", hostname);
            fprintf(f, "# Duration per point: %.1f seconds\n", BENCH_DURATION_SEC);
            fprintf(f, "# Trials: %d\n", NUM_TRIALS);
            fprintf(f, "#\n");
            write_csv_header(f);
            
            if (!quiet) {
                printf("Benchmarking %s %s %s...\n", info->name, ip_version, lookup_name);
            }
            
            /* Run for each prefix count */
            for (size_t pc = 0; pc < NUM_PREFIX_COUNTS; pc++) {
                int num_prefixes = PREFIX_COUNTS[pc];
                
                if (!quiet) {
                    printf("  %d prefixes... ", num_prefixes);
                    fflush(stdout);
                }
                
                benchmark_result_t result = run_benchmark(algo, lt, num_prefixes);
                write_csv_row(f, num_prefixes, &result);
                
                if (!quiet) {
                    printf("%.2f Mlookups/s\n", result.median_lookups_per_sec / 1e6);
                }
            }
            
            fclose(f);
            
            if (!quiet) {
                printf("  -> %s\n\n", filepath);
            }
        }
    }
    
    if (!quiet) {
        printf("Benchmark complete!\n");
    }
    
#ifdef HAVE_DPDK
    /* Cleanup DPDK */
    if (dpdk_initialized) {
        rte_eal_cleanup();
    }
#endif
    
    return 0;
}
