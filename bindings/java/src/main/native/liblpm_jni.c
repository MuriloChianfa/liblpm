/*
 * liblpm JNI Native Bridge
 * 
 * Copyright (c) 2024 Murilo Chianfa
 * Licensed under the MIT License.
 * 
 * This file implements the JNI native methods for the liblpm Java bindings.
 * It provides a bridge between Java and the C liblpm library.
 */

#include <jni.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <lpm.h>

/* ============================================================================
 * JNI Class and Method Cache
 * ============================================================================ */

/* Cached class references */
static jclass exceptionClass = NULL;
static jclass invalidPrefixClass = NULL;
static jclass illegalStateClass = NULL;
static jclass outOfMemoryClass = NULL;

/* Initialize cached references (called on library load) */
static int initClassCache(JNIEnv *env) {
    jclass cls;
    
    /* Cache exception classes */
    cls = (*env)->FindClass(env, "com/github/murilochianfa/liblpm/LpmException");
    if (cls == NULL) return -1;
    exceptionClass = (*env)->NewGlobalRef(env, cls);
    (*env)->DeleteLocalRef(env, cls);
    
    cls = (*env)->FindClass(env, "com/github/murilochianfa/liblpm/InvalidPrefixException");
    if (cls == NULL) return -1;
    invalidPrefixClass = (*env)->NewGlobalRef(env, cls);
    (*env)->DeleteLocalRef(env, cls);
    
    cls = (*env)->FindClass(env, "java/lang/IllegalStateException");
    if (cls == NULL) return -1;
    illegalStateClass = (*env)->NewGlobalRef(env, cls);
    (*env)->DeleteLocalRef(env, cls);
    
    cls = (*env)->FindClass(env, "java/lang/OutOfMemoryError");
    if (cls == NULL) return -1;
    outOfMemoryClass = (*env)->NewGlobalRef(env, cls);
    (*env)->DeleteLocalRef(env, cls);
    
    return 0;
}

/* Clean up cached references (called on library unload) */
static void cleanupClassCache(JNIEnv *env) {
    if (exceptionClass) {
        (*env)->DeleteGlobalRef(env, exceptionClass);
        exceptionClass = NULL;
    }
    if (invalidPrefixClass) {
        (*env)->DeleteGlobalRef(env, invalidPrefixClass);
        invalidPrefixClass = NULL;
    }
    if (illegalStateClass) {
        (*env)->DeleteGlobalRef(env, illegalStateClass);
        illegalStateClass = NULL;
    }
    if (outOfMemoryClass) {
        (*env)->DeleteGlobalRef(env, outOfMemoryClass);
        outOfMemoryClass = NULL;
    }
}

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/* Throw a Java exception */
static void throwException(JNIEnv *env, jclass exClass, const char *message) {
    (*env)->ThrowNew(env, exClass, message);
}

/* Throw InvalidPrefixException */
static void throwInvalidPrefix(JNIEnv *env, const char *message) {
    throwException(env, invalidPrefixClass, message);
}

/* Throw IllegalStateException */
static void throwIllegalState(JNIEnv *env, const char *message) {
    throwException(env, illegalStateClass, message);
}

/* Throw OutOfMemoryError */
static void throwOutOfMemory(JNIEnv *env, const char *message) {
    throwException(env, outOfMemoryClass, message);
}

/* Check if an exception is pending */
static inline int checkException(JNIEnv *env) {
    return (*env)->ExceptionCheck(env);
}

/* Get trie pointer from handle, validating it's not null */
static inline lpm_trie_t* getTrieHandle(JNIEnv *env, jlong handle) {
    if (handle == 0) {
        throwIllegalState(env, "LpmTable has been closed");
        return NULL;
    }
    return (lpm_trie_t*)(uintptr_t)handle;
}

/* Algorithm codes matching Algorithm.java enum */
#define ALGO_DIR24   0
#define ALGO_STRIDE8 1
#define ALGO_WIDE16  2

/* ============================================================================
 * JNI Lifecycle Functions
 * ============================================================================ */

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved) {
    JNIEnv *env;
    (void)reserved;
    
    if ((*vm)->GetEnv(vm, (void**)&env, JNI_VERSION_1_8) != JNI_OK) {
        return JNI_ERR;
    }
    
    if (initClassCache(env) != 0) {
        return JNI_ERR;
    }
    
    return JNI_VERSION_1_8;
}

JNIEXPORT void JNICALL JNI_OnUnload(JavaVM *vm, void *reserved) {
    JNIEnv *env;
    (void)reserved;
    
    if ((*vm)->GetEnv(vm, (void**)&env, JNI_VERSION_1_8) == JNI_OK) {
        cleanupClassCache(env);
    }
}

/* ============================================================================
 * Creation Functions
 * ============================================================================ */

/*
 * Class:     com_github_murilochianfa_liblpm_LpmTable
 * Method:    nativeCreateIPv4
 * Signature: (I)J
 */
JNIEXPORT jlong JNICALL Java_com_github_murilochianfa_liblpm_LpmTable_nativeCreateIPv4
  (JNIEnv *env, jclass cls, jint algorithm) {
    (void)cls;
    
    lpm_trie_t *trie = NULL;
    
    switch (algorithm) {
        case ALGO_DIR24:
            trie = lpm_create_ipv4_dir24();
            break;
        case ALGO_STRIDE8:
            trie = lpm_create_ipv4_8stride();
            break;
        default:
            throwInvalidPrefix(env, "Invalid algorithm for IPv4");
            return 0;
    }
    
    if (trie == NULL) {
        throwOutOfMemory(env, "Failed to allocate IPv4 trie");
        return 0;
    }
    
    return (jlong)(uintptr_t)trie;
}

/*
 * Class:     com_github_murilochianfa_liblpm_LpmTable
 * Method:    nativeCreateIPv6
 * Signature: (I)J
 */
JNIEXPORT jlong JNICALL Java_com_github_murilochianfa_liblpm_LpmTable_nativeCreateIPv6
  (JNIEnv *env, jclass cls, jint algorithm) {
    (void)cls;
    
    lpm_trie_t *trie = NULL;
    
    switch (algorithm) {
        case ALGO_WIDE16:
            trie = lpm_create_ipv6_wide16();
            break;
        case ALGO_STRIDE8:
            trie = lpm_create_ipv6_8stride();
            break;
        default:
            throwInvalidPrefix(env, "Invalid algorithm for IPv6");
            return 0;
    }
    
    if (trie == NULL) {
        throwOutOfMemory(env, "Failed to allocate IPv6 trie");
        return 0;
    }
    
    return (jlong)(uintptr_t)trie;
}

/* ============================================================================
 * Add/Delete Functions
 * ============================================================================ */

/*
 * Class:     com_github_murilochianfa_liblpm_LpmTable
 * Method:    nativeAdd
 * Signature: (J[BII)I
 */
JNIEXPORT jint JNICALL Java_com_github_murilochianfa_liblpm_LpmTable_nativeAdd
  (JNIEnv *env, jclass cls, jlong handle, jbyteArray prefix, jint prefixLen, jint nextHop) {
    (void)cls;
    
    lpm_trie_t *trie = getTrieHandle(env, handle);
    if (trie == NULL) return -1;
    
    if (prefix == NULL) {
        throwInvalidPrefix(env, "Prefix cannot be null");
        return -1;
    }
    
    jsize len = (*env)->GetArrayLength(env, prefix);
    if (len != 4 && len != 16) {
        throwInvalidPrefix(env, "Prefix must be 4 bytes (IPv4) or 16 bytes (IPv6)");
        return -1;
    }
    
    /* Validate prefix length */
    int maxLen = (len == 4) ? 32 : 128;
    if (prefixLen < 0 || prefixLen > maxLen) {
        throwInvalidPrefix(env, "Prefix length out of range");
        return -1;
    }
    
    /* Get prefix bytes using critical section for zero-copy */
    jbyte *prefixBytes = (*env)->GetPrimitiveArrayCritical(env, prefix, NULL);
    if (prefixBytes == NULL) {
        throwOutOfMemory(env, "Failed to access prefix array");
        return -1;
    }
    
    int result = lpm_add(trie, (const uint8_t*)prefixBytes, (uint8_t)prefixLen, (uint32_t)nextHop);
    
    (*env)->ReleasePrimitiveArrayCritical(env, prefix, prefixBytes, JNI_ABORT);
    
    if (result != 0) {
        throwException(env, exceptionClass, "Failed to add prefix");
        return -1;
    }
    
    return 0;
}

/*
 * Class:     com_github_murilochianfa_liblpm_LpmTable
 * Method:    nativeDelete
 * Signature: (J[BI)I
 */
JNIEXPORT jint JNICALL Java_com_github_murilochianfa_liblpm_LpmTable_nativeDelete
  (JNIEnv *env, jclass cls, jlong handle, jbyteArray prefix, jint prefixLen) {
    (void)cls;
    
    lpm_trie_t *trie = getTrieHandle(env, handle);
    if (trie == NULL) return -1;
    
    if (prefix == NULL) {
        throwInvalidPrefix(env, "Prefix cannot be null");
        return -1;
    }
    
    jsize len = (*env)->GetArrayLength(env, prefix);
    if (len != 4 && len != 16) {
        throwInvalidPrefix(env, "Prefix must be 4 bytes (IPv4) or 16 bytes (IPv6)");
        return -1;
    }
    
    /* Validate prefix length */
    int maxLen = (len == 4) ? 32 : 128;
    if (prefixLen < 0 || prefixLen > maxLen) {
        throwInvalidPrefix(env, "Prefix length out of range");
        return -1;
    }
    
    jbyte *prefixBytes = (*env)->GetPrimitiveArrayCritical(env, prefix, NULL);
    if (prefixBytes == NULL) {
        throwOutOfMemory(env, "Failed to access prefix array");
        return -1;
    }
    
    int result = lpm_delete(trie, (const uint8_t*)prefixBytes, (uint8_t)prefixLen);
    
    (*env)->ReleasePrimitiveArrayCritical(env, prefix, prefixBytes, JNI_ABORT);
    
    /* Return result (0 = deleted, -1 = not found) */
    return result;
}

/* ============================================================================
 * Lookup Functions
 * ============================================================================ */

/*
 * Class:     com_github_murilochianfa_liblpm_LpmTable
 * Method:    nativeLookup
 * Signature: (J[B)I
 */
JNIEXPORT jint JNICALL Java_com_github_murilochianfa_liblpm_LpmTable_nativeLookup
  (JNIEnv *env, jclass cls, jlong handle, jbyteArray address) {
    (void)cls;
    
    lpm_trie_t *trie = getTrieHandle(env, handle);
    if (trie == NULL) return (jint)LPM_INVALID_NEXT_HOP;
    
    if (address == NULL) {
        throwInvalidPrefix(env, "Address cannot be null");
        return (jint)LPM_INVALID_NEXT_HOP;
    }
    
    jsize len = (*env)->GetArrayLength(env, address);
    if (len != 4 && len != 16) {
        throwInvalidPrefix(env, "Address must be 4 bytes (IPv4) or 16 bytes (IPv6)");
        return (jint)LPM_INVALID_NEXT_HOP;
    }
    
    jbyte *addrBytes = (*env)->GetPrimitiveArrayCritical(env, address, NULL);
    if (addrBytes == NULL) {
        return (jint)LPM_INVALID_NEXT_HOP;
    }
    
    uint32_t result;
    if (len == 4) {
        /* IPv4 lookup - convert bytes to uint32_t in network byte order */
        uint32_t addr = ((uint32_t)(uint8_t)addrBytes[0] << 24) |
                        ((uint32_t)(uint8_t)addrBytes[1] << 16) |
                        ((uint32_t)(uint8_t)addrBytes[2] << 8)  |
                        ((uint32_t)(uint8_t)addrBytes[3]);
        result = lpm_lookup_ipv4(trie, addr);
    } else {
        /* IPv6 lookup */
        result = lpm_lookup_ipv6(trie, (const uint8_t*)addrBytes);
    }
    
    (*env)->ReleasePrimitiveArrayCritical(env, address, addrBytes, JNI_ABORT);
    
    return (jint)result;
}

/*
 * Class:     com_github_murilochianfa_liblpm_LpmTable
 * Method:    nativeLookupIPv4
 * Signature: (JI)I
 * 
 * Optimized IPv4 lookup that takes the address as an int (network byte order)
 */
JNIEXPORT jint JNICALL Java_com_github_murilochianfa_liblpm_LpmTable_nativeLookupIPv4
  (JNIEnv *env, jclass cls, jlong handle, jint addressAsInt) {
    (void)cls;
    
    lpm_trie_t *trie = getTrieHandle(env, handle);
    if (trie == NULL) return (jint)LPM_INVALID_NEXT_HOP;
    
    return (jint)lpm_lookup_ipv4(trie, (uint32_t)addressAsInt);
}

/* ============================================================================
 * Batch Lookup Functions
 * ============================================================================ */

/*
 * Class:     com_github_murilochianfa_liblpm_LpmTable
 * Method:    nativeLookupBatch
 * Signature: (J[[B[I)V
 */
JNIEXPORT void JNICALL Java_com_github_murilochianfa_liblpm_LpmTable_nativeLookupBatch
  (JNIEnv *env, jclass cls, jlong handle, jobjectArray addresses, jintArray results) {
    (void)cls;
    
    lpm_trie_t *trie = getTrieHandle(env, handle);
    if (trie == NULL) return;
    
    if (addresses == NULL || results == NULL) {
        throwInvalidPrefix(env, "Addresses and results cannot be null");
        return;
    }
    
    jsize count = (*env)->GetArrayLength(env, addresses);
    jsize resultsLen = (*env)->GetArrayLength(env, results);
    
    if (resultsLen < count) {
        throwInvalidPrefix(env, "Results array too small");
        return;
    }
    
    if (count == 0) return;
    
    /* Get results array */
    jint *resultsArray = (*env)->GetPrimitiveArrayCritical(env, results, NULL);
    if (resultsArray == NULL) {
        throwOutOfMemory(env, "Failed to access results array");
        return;
    }
    
    /* Process each address */
    for (jsize i = 0; i < count; i++) {
        jbyteArray addr = (*env)->GetObjectArrayElement(env, addresses, i);
        
        if (addr == NULL) {
            resultsArray[i] = (jint)LPM_INVALID_NEXT_HOP;
            continue;
        }
        
        jsize addrLen = (*env)->GetArrayLength(env, addr);
        
        jbyte *addrBytes = (*env)->GetPrimitiveArrayCritical(env, addr, NULL);
        if (addrBytes == NULL) {
            resultsArray[i] = (jint)LPM_INVALID_NEXT_HOP;
            (*env)->DeleteLocalRef(env, addr);
            continue;
        }
        
        if (addrLen == 4) {
            uint32_t ipv4Addr = ((uint32_t)(uint8_t)addrBytes[0] << 24) |
                                ((uint32_t)(uint8_t)addrBytes[1] << 16) |
                                ((uint32_t)(uint8_t)addrBytes[2] << 8)  |
                                ((uint32_t)(uint8_t)addrBytes[3]);
            resultsArray[i] = (jint)lpm_lookup_ipv4(trie, ipv4Addr);
        } else if (addrLen == 16) {
            resultsArray[i] = (jint)lpm_lookup_ipv6(trie, (const uint8_t*)addrBytes);
        } else {
            resultsArray[i] = (jint)LPM_INVALID_NEXT_HOP;
        }
        
        (*env)->ReleasePrimitiveArrayCritical(env, addr, addrBytes, JNI_ABORT);
        (*env)->DeleteLocalRef(env, addr);
    }
    
    (*env)->ReleasePrimitiveArrayCritical(env, results, resultsArray, 0);
}

/*
 * Class:     com_github_murilochianfa_liblpm_LpmTable
 * Method:    nativeLookupBatchIPv4
 * Signature: (J[I[I)V
 * 
 * Optimized batch IPv4 lookup using int arrays (network byte order)
 */
JNIEXPORT void JNICALL Java_com_github_murilochianfa_liblpm_LpmTable_nativeLookupBatchIPv4
  (JNIEnv *env, jclass cls, jlong handle, jintArray addresses, jintArray results) {
    (void)cls;
    
    lpm_trie_t *trie = getTrieHandle(env, handle);
    if (trie == NULL) return;
    
    if (addresses == NULL || results == NULL) {
        throwInvalidPrefix(env, "Addresses and results cannot be null");
        return;
    }
    
    jsize count = (*env)->GetArrayLength(env, addresses);
    jsize resultsLen = (*env)->GetArrayLength(env, results);
    
    if (resultsLen < count) {
        throwInvalidPrefix(env, "Results array too small");
        return;
    }
    
    if (count == 0) return;
    
    /* Get both arrays using critical sections for zero-copy */
    jint *addrArray = (*env)->GetPrimitiveArrayCritical(env, addresses, NULL);
    if (addrArray == NULL) {
        throwOutOfMemory(env, "Failed to access addresses array");
        return;
    }
    
    jint *resultsArray = (*env)->GetPrimitiveArrayCritical(env, results, NULL);
    if (resultsArray == NULL) {
        (*env)->ReleasePrimitiveArrayCritical(env, addresses, addrArray, JNI_ABORT);
        throwOutOfMemory(env, "Failed to access results array");
        return;
    }
    
    /* Use native batch lookup if available, otherwise loop */
    lpm_lookup_batch_ipv4(trie, (const uint32_t*)addrArray, (uint32_t*)resultsArray, (size_t)count);
    
    (*env)->ReleasePrimitiveArrayCritical(env, results, resultsArray, 0);
    (*env)->ReleasePrimitiveArrayCritical(env, addresses, addrArray, JNI_ABORT);
}

/* ============================================================================
 * Resource Management Functions
 * ============================================================================ */

/*
 * Class:     com_github_murilochianfa_liblpm_LpmTable
 * Method:    nativeDestroy
 * Signature: (J)V
 */
JNIEXPORT void JNICALL Java_com_github_murilochianfa_liblpm_LpmTable_nativeDestroy
  (JNIEnv *env, jclass cls, jlong handle) {
    (void)env;
    (void)cls;
    
    if (handle != 0) {
        lpm_trie_t *trie = (lpm_trie_t*)(uintptr_t)handle;
        lpm_destroy(trie);
    }
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/*
 * Class:     com_github_murilochianfa_liblpm_LpmTable
 * Method:    nativeGetVersion
 * Signature: ()Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_com_github_murilochianfa_liblpm_LpmTable_nativeGetVersion
  (JNIEnv *env, jclass cls) {
    (void)cls;
    
    const char *version = lpm_get_version();
    if (version == NULL) {
        return (*env)->NewStringUTF(env, "unknown");
    }
    return (*env)->NewStringUTF(env, version);
}
