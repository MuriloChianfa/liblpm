// LpmAlgorithm.cs - Algorithm selection enum
// Defines the available LPM algorithms

namespace LibLpm
{
    /// <summary>
    /// Specifies the algorithm to use for the LPM trie.
    /// </summary>
    public enum LpmAlgorithm
    {
        /// <summary>
        /// Use the compile-time default algorithm.
        /// For IPv4: DIR-24-8
        /// For IPv6: Wide 16-bit stride
        /// </summary>
        Default = 0,

        /// <summary>
        /// IPv4 DIR-24-8 algorithm.
        /// Uses a 24-bit direct table with 8-bit extension tables.
        /// Optimal for IPv4 with 1-2 memory accesses per lookup.
        /// Memory usage: ~64MB base + extensions for /25-/32 routes.
        /// </summary>
        /// <remarks>
        /// Only valid for IPv4 tries.
        /// </remarks>
        Dir24 = 1,

        /// <summary>
        /// Wide 16-bit stride algorithm for IPv6.
        /// Uses 16-bit stride for the first level, then 8-bit strides.
        /// Optimal for IPv6 with common /48 allocations.
        /// </summary>
        /// <remarks>
        /// Only valid for IPv6 tries.
        /// </remarks>
        Wide16 = 2,

        /// <summary>
        /// Standard 8-bit stride algorithm.
        /// Works for both IPv4 (4 levels max) and IPv6 (16 levels max).
        /// Good balance of memory efficiency and lookup speed.
        /// Memory-efficient for sparse prefix sets.
        /// </summary>
        Stride8 = 3
    }
}
