// SafeLpmHandle.cs - Safe handle for native LPM trie resources
// Ensures proper cleanup of native memory even in case of exceptions

using System;
using System.Runtime.InteropServices;

namespace LibLpm
{
    /// <summary>
    /// A safe handle wrapper for the native lpm_trie_t pointer.
    /// Ensures that lpm_destroy() is always called when the handle is released,
    /// even if an exception occurs or the object is garbage collected.
    /// </summary>
    public sealed class SafeLpmHandle : SafeHandle
    {
        /// <summary>
        /// Creates a new invalid SafeLpmHandle.
        /// </summary>
        public SafeLpmHandle() : base(IntPtr.Zero, ownsHandle: true)
        {
        }

        /// <summary>
        /// Creates a SafeLpmHandle wrapping the specified native pointer.
        /// </summary>
        /// <param name="handle">The native lpm_trie_t pointer.</param>
        /// <param name="ownsHandle">Whether this handle owns the native resource.</param>
        internal SafeLpmHandle(IntPtr handle, bool ownsHandle = true) : base(IntPtr.Zero, ownsHandle)
        {
            SetHandle(handle);
        }

        /// <summary>
        /// Gets a value indicating whether the handle value is invalid.
        /// A handle is invalid if it is IntPtr.Zero.
        /// </summary>
        public override bool IsInvalid => handle == IntPtr.Zero;

        /// <summary>
        /// Gets the underlying native pointer.
        /// </summary>
        /// <remarks>
        /// Use this property when you need to pass the handle to native methods.
        /// Prefer using DangerousGetHandle() for explicit P/Invoke calls.
        /// </remarks>
        public IntPtr Handle => handle;

        /// <summary>
        /// Releases the native handle by calling lpm_destroy().
        /// </summary>
        /// <returns>True if the handle was released successfully.</returns>
        protected override bool ReleaseHandle()
        {
            if (handle != IntPtr.Zero)
            {
                NativeMethods.lpm_destroy(handle);
                handle = IntPtr.Zero;
            }
            return true;
        }

        /// <summary>
        /// Creates an invalid (null) handle.
        /// </summary>
        public static SafeLpmHandle Invalid => new SafeLpmHandle();

        /// <summary>
        /// Explicitly converts an IntPtr to a SafeLpmHandle.
        /// </summary>
        /// <param name="ptr">The native pointer.</param>
        public static explicit operator SafeLpmHandle(IntPtr ptr)
        {
            return new SafeLpmHandle(ptr);
        }

        /// <summary>
        /// Gets the native pointer from the safe handle.
        /// </summary>
        /// <param name="handle">The safe handle.</param>
        public static implicit operator IntPtr(SafeLpmHandle handle)
        {
            return handle?.handle ?? IntPtr.Zero;
        }
    }
}
