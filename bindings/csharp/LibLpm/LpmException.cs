// LpmException.cs - Exception hierarchy for liblpm
// Provides meaningful exception types for various error conditions

using System;

namespace LibLpm
{
    /// <summary>
    /// Base exception class for all liblpm-related errors.
    /// </summary>
    public class LpmException : Exception
    {
        /// <summary>
        /// Initializes a new instance of the <see cref="LpmException"/> class.
        /// </summary>
        public LpmException()
            : base("An error occurred in the LPM library.")
        {
        }

        /// <summary>
        /// Initializes a new instance of the <see cref="LpmException"/> class with a specified error message.
        /// </summary>
        /// <param name="message">The message that describes the error.</param>
        public LpmException(string message)
            : base(message)
        {
        }

        /// <summary>
        /// Initializes a new instance of the <see cref="LpmException"/> class with a specified error message
        /// and a reference to the inner exception that is the cause of this exception.
        /// </summary>
        /// <param name="message">The message that describes the error.</param>
        /// <param name="innerException">The exception that is the cause of the current exception.</param>
        public LpmException(string message, Exception innerException)
            : base(message, innerException)
        {
        }
    }

    /// <summary>
    /// Exception thrown when creating an LPM trie fails.
    /// This typically indicates a memory allocation failure.
    /// </summary>
    public class LpmCreationException : LpmException
    {
        /// <summary>
        /// Gets the algorithm that was requested when the creation failed.
        /// </summary>
        public LpmAlgorithm? Algorithm { get; }

        /// <summary>
        /// Gets whether the trie was being created for IPv6.
        /// </summary>
        public bool IsIPv6 { get; }

        /// <summary>
        /// Initializes a new instance of the <see cref="LpmCreationException"/> class.
        /// </summary>
        public LpmCreationException()
            : base("Failed to create LPM trie. The native library may have failed to allocate memory.")
        {
        }

        /// <summary>
        /// Initializes a new instance of the <see cref="LpmCreationException"/> class with a specified error message.
        /// </summary>
        /// <param name="message">The message that describes the error.</param>
        public LpmCreationException(string message)
            : base(message)
        {
        }

        /// <summary>
        /// Initializes a new instance of the <see cref="LpmCreationException"/> class with details about the creation attempt.
        /// </summary>
        /// <param name="isIPv6">Whether the trie was being created for IPv6.</param>
        /// <param name="algorithm">The algorithm that was requested.</param>
        public LpmCreationException(bool isIPv6, LpmAlgorithm? algorithm = null)
            : base($"Failed to create {(isIPv6 ? "IPv6" : "IPv4")} LPM trie" +
                   (algorithm.HasValue ? $" with algorithm {algorithm.Value}" : "") +
                   ". The native library may have failed to allocate memory.")
        {
            IsIPv6 = isIPv6;
            Algorithm = algorithm;
        }

        /// <summary>
        /// Initializes a new instance of the <see cref="LpmCreationException"/> class with a specified error message
        /// and a reference to the inner exception that is the cause of this exception.
        /// </summary>
        /// <param name="message">The message that describes the error.</param>
        /// <param name="innerException">The exception that is the cause of the current exception.</param>
        public LpmCreationException(string message, Exception innerException)
            : base(message, innerException)
        {
        }
    }

    /// <summary>
    /// Exception thrown when an invalid prefix is provided to an LPM operation.
    /// </summary>
    public class LpmInvalidPrefixException : LpmException
    {
        /// <summary>
        /// Gets the prefix string that was invalid (if available).
        /// </summary>
        public string? PrefixString { get; }

        /// <summary>
        /// Gets the prefix length that was invalid (if available).
        /// </summary>
        public byte? PrefixLength { get; }

        /// <summary>
        /// Gets the maximum allowed prefix length for the address type.
        /// </summary>
        public byte? MaxPrefixLength { get; }

        /// <summary>
        /// Initializes a new instance of the <see cref="LpmInvalidPrefixException"/> class.
        /// </summary>
        public LpmInvalidPrefixException()
            : base("Invalid prefix format or length.")
        {
        }

        /// <summary>
        /// Initializes a new instance of the <see cref="LpmInvalidPrefixException"/> class with a specified error message.
        /// </summary>
        /// <param name="message">The message that describes the error.</param>
        public LpmInvalidPrefixException(string message)
            : base(message)
        {
        }

        /// <summary>
        /// Initializes a new instance of the <see cref="LpmInvalidPrefixException"/> class for an invalid prefix string.
        /// </summary>
        /// <param name="prefixString">The invalid prefix string.</param>
        /// <param name="reason">Additional reason for the failure (optional).</param>
        public LpmInvalidPrefixException(string prefixString, string? reason = null)
            : base($"Invalid prefix format: '{prefixString}'" + (reason != null ? $". {reason}" : ""))
        {
            PrefixString = prefixString;
        }

        /// <summary>
        /// Initializes a new instance of the <see cref="LpmInvalidPrefixException"/> class for an invalid prefix length.
        /// </summary>
        /// <param name="prefixLength">The invalid prefix length.</param>
        /// <param name="maxPrefixLength">The maximum allowed prefix length.</param>
        public LpmInvalidPrefixException(byte prefixLength, byte maxPrefixLength)
            : base($"Invalid prefix length: {prefixLength}. Maximum allowed is {maxPrefixLength}.")
        {
            PrefixLength = prefixLength;
            MaxPrefixLength = maxPrefixLength;
        }

        /// <summary>
        /// Initializes a new instance of the <see cref="LpmInvalidPrefixException"/> class with a specified error message
        /// and a reference to the inner exception that is the cause of this exception.
        /// </summary>
        /// <param name="message">The message that describes the error.</param>
        /// <param name="innerException">The exception that is the cause of the current exception.</param>
        public LpmInvalidPrefixException(string message, Exception innerException)
            : base(message, innerException)
        {
        }
    }

    /// <summary>
    /// Exception thrown when an LPM operation (add, delete) fails.
    /// </summary>
    public class LpmOperationException : LpmException
    {
        /// <summary>
        /// Gets the operation that failed.
        /// </summary>
        public string? Operation { get; }

        /// <summary>
        /// Initializes a new instance of the <see cref="LpmOperationException"/> class.
        /// </summary>
        public LpmOperationException()
            : base("An LPM operation failed.")
        {
        }

        /// <summary>
        /// Initializes a new instance of the <see cref="LpmOperationException"/> class with a specified error message.
        /// </summary>
        /// <param name="message">The message that describes the error.</param>
        public LpmOperationException(string message)
            : base(message)
        {
        }

        /// <summary>
        /// Initializes a new instance of the <see cref="LpmOperationException"/> class for a specific operation.
        /// </summary>
        /// <param name="operation">The operation that failed (e.g., "add", "delete").</param>
        /// <param name="details">Additional details about the failure (optional).</param>
        public LpmOperationException(string operation, string? details)
            : base($"LPM {operation} operation failed" + (details != null ? $": {details}" : "."))
        {
            Operation = operation;
        }

        /// <summary>
        /// Initializes a new instance of the <see cref="LpmOperationException"/> class with a specified error message
        /// and a reference to the inner exception that is the cause of this exception.
        /// </summary>
        /// <param name="message">The message that describes the error.</param>
        /// <param name="innerException">The exception that is the cause of the current exception.</param>
        public LpmOperationException(string message, Exception innerException)
            : base(message, innerException)
        {
        }
    }

    /// <summary>
    /// Exception thrown when the native library cannot be loaded.
    /// </summary>
    public class LpmLibraryNotFoundException : LpmException
    {
        /// <summary>
        /// Gets the paths that were searched for the library.
        /// </summary>
        public string[]? SearchedPaths { get; }

        /// <summary>
        /// Initializes a new instance of the <see cref="LpmLibraryNotFoundException"/> class.
        /// </summary>
        public LpmLibraryNotFoundException()
            : base("The native liblpm library could not be found. Ensure it is installed or included in the runtimes directory.")
        {
        }

        /// <summary>
        /// Initializes a new instance of the <see cref="LpmLibraryNotFoundException"/> class with a specified error message.
        /// </summary>
        /// <param name="message">The message that describes the error.</param>
        public LpmLibraryNotFoundException(string message)
            : base(message)
        {
        }

        /// <summary>
        /// Initializes a new instance of the <see cref="LpmLibraryNotFoundException"/> class with searched paths.
        /// </summary>
        /// <param name="searchedPaths">The paths that were searched for the library.</param>
        public LpmLibraryNotFoundException(string[] searchedPaths)
            : base("The native liblpm library could not be found. Searched paths: " +
                   string.Join(", ", searchedPaths))
        {
            SearchedPaths = searchedPaths;
        }

        /// <summary>
        /// Initializes a new instance of the <see cref="LpmLibraryNotFoundException"/> class with a specified error message
        /// and a reference to the inner exception that is the cause of this exception.
        /// </summary>
        /// <param name="message">The message that describes the error.</param>
        /// <param name="innerException">The exception that is the cause of the current exception.</param>
        public LpmLibraryNotFoundException(string message, Exception innerException)
            : base(message, innerException)
        {
        }
    }
}
