// NativeLibraryLoader.cs - Cross-platform native library loading
// Handles discovery and loading of the native liblpm library

using System;
using System.IO;
using System.Reflection;
using System.Runtime.InteropServices;

namespace LibLpm
{
    /// <summary>
    /// Handles cross-platform native library discovery and loading.
    /// </summary>
    public static class NativeLibraryLoader
    {
        private static readonly object _initLock = new object();
        private static bool _initialized = false;
        private static IntPtr _libraryHandle = IntPtr.Zero;

        /// <summary>
        /// Gets the runtime identifier for the current platform.
        /// </summary>
        public static string RuntimeIdentifier
        {
            get
            {
                if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
                {
                    return RuntimeInformation.ProcessArchitecture switch
                    {
                        Architecture.X64 => "win-x64",
                        Architecture.X86 => "win-x86",
                        Architecture.Arm64 => "win-arm64",
                        _ => "win-x64"
                    };
                }
                else if (RuntimeInformation.IsOSPlatform(OSPlatform.Linux))
                {
                    return RuntimeInformation.ProcessArchitecture switch
                    {
                        Architecture.X64 => "linux-x64",
                        Architecture.Arm64 => "linux-arm64",
                        Architecture.Arm => "linux-arm",
                        _ => "linux-x64"
                    };
                }
                else if (RuntimeInformation.IsOSPlatform(OSPlatform.OSX))
                {
                    return RuntimeInformation.ProcessArchitecture switch
                    {
                        Architecture.X64 => "osx-x64",
                        Architecture.Arm64 => "osx-arm64",
                        _ => "osx-x64"
                    };
                }
                else
                {
                    throw new PlatformNotSupportedException($"Unsupported platform: {RuntimeInformation.OSDescription}");
                }
            }
        }

        /// <summary>
        /// Gets the native library file name for the current platform.
        /// </summary>
        public static string NativeLibraryName
        {
            get
            {
                if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
                {
                    return "lpm.dll";
                }
                else if (RuntimeInformation.IsOSPlatform(OSPlatform.OSX))
                {
                    return "liblpm.dylib";
                }
                else
                {
                    return "liblpm.so";
                }
            }
        }

        /// <summary>
        /// Initializes the native library resolver.
        /// Call this early in your application startup to ensure the native library can be found.
        /// </summary>
        public static void Initialize()
        {
            if (_initialized) return;

            lock (_initLock)
            {
                if (_initialized) return;

                // Try to set up the DLL import resolver if available (requires .NET Core 3.0+)
                TrySetupDllImportResolver();
                _initialized = true;
            }
        }

        /// <summary>
        /// Tries to set up the DLL import resolver using reflection to avoid compile-time dependency.
        /// </summary>
        private static void TrySetupDllImportResolver()
        {
            try
            {
                // Use reflection to access NativeLibrary.SetDllImportResolver if available
                var nativeLibraryType = Type.GetType("System.Runtime.InteropServices.NativeLibrary, System.Runtime.InteropServices.RuntimeInformation")
                    ?? Type.GetType("System.Runtime.InteropServices.NativeLibrary, System.Runtime.InteropServices")
                    ?? Type.GetType("System.Runtime.InteropServices.NativeLibrary, System.Private.CoreLib")
                    ?? Type.GetType("System.Runtime.InteropServices.NativeLibrary");

                if (nativeLibraryType == null)
                {
                    // NativeLibrary not available, rely on default P/Invoke behavior
                    return;
                }

                // Store the type for later use
                _nativeLibraryType = nativeLibraryType;

                // Get SetDllImportResolver method
                var setResolverMethod = nativeLibraryType.GetMethod("SetDllImportResolver", 
                    BindingFlags.Public | BindingFlags.Static);

                if (setResolverMethod != null)
                {
                    // Create delegate type dynamically
                    var delegateType = setResolverMethod.GetParameters()[1].ParameterType;
                    var resolverDelegate = Delegate.CreateDelegate(delegateType, typeof(NativeLibraryLoader), 
                        nameof(DllImportResolverImpl));
                    
                    setResolverMethod.Invoke(null, new object[] { typeof(NativeLibraryLoader).Assembly, resolverDelegate });
                }
            }
            catch
            {
                // Ignore errors - fall back to default P/Invoke behavior
            }
        }

        private static Type? _nativeLibraryType;

        /// <summary>
        /// DLL import resolver implementation called via reflection.
        /// </summary>
        private static IntPtr DllImportResolverImpl(string libraryName, Assembly assembly, DllImportSearchPath? searchPath)
        {
            if (libraryName != NativeMethods.LibraryName)
            {
                return IntPtr.Zero;
            }

            // Try to use cached handle
            if (_libraryHandle != IntPtr.Zero)
            {
                return _libraryHandle;
            }

            IntPtr handle = IntPtr.Zero;

            // Try each search path
            foreach (var path in GetSearchPaths())
            {
                if (TryLoadLibraryReflection(path, out handle))
                {
                    _libraryHandle = handle;
                    return handle;
                }
            }

            // Fall back to system library search via reflection
            if (TryLoadLibraryWithAssemblyReflection(libraryName, assembly, searchPath, out handle))
            {
                _libraryHandle = handle;
                return handle;
            }

            // Try loading just the library name (system paths)
            if (TryLoadLibraryReflection(NativeLibraryName, out handle))
            {
                _libraryHandle = handle;
                return handle;
            }

            // Return IntPtr.Zero to let the default resolver try
            return IntPtr.Zero;
        }

        /// <summary>
        /// Attempts to load a native library from the specified path using reflection.
        /// </summary>
        private static bool TryLoadLibraryReflection(string path, out IntPtr handle)
        {
            handle = IntPtr.Zero;
            
            if (string.IsNullOrEmpty(path))
            {
                return false;
            }

            // Check if file exists (for absolute paths)
            if (Path.IsPathRooted(path) && !File.Exists(path))
            {
                return false;
            }

            try
            {
                if (_nativeLibraryType == null) return false;

                var tryLoadMethod = _nativeLibraryType.GetMethod("TryLoad", 
                    new[] { typeof(string), typeof(IntPtr).MakeByRefType() });
                
                if (tryLoadMethod == null) return false;

                var args = new object?[] { path, IntPtr.Zero };
                var result = (bool)tryLoadMethod.Invoke(null, args)!;
                if (result)
                {
                    handle = (IntPtr)args[1]!;
                }
                return result;
            }
            catch
            {
                return false;
            }
        }

        /// <summary>
        /// Attempts to load a native library with assembly context using reflection.
        /// </summary>
        private static bool TryLoadLibraryWithAssemblyReflection(string libraryName, Assembly assembly, 
            DllImportSearchPath? searchPath, out IntPtr handle)
        {
            handle = IntPtr.Zero;
            
            try
            {
                if (_nativeLibraryType == null) return false;

                var tryLoadMethod = _nativeLibraryType.GetMethod("TryLoad", 
                    new[] { typeof(string), typeof(Assembly), typeof(DllImportSearchPath?), typeof(IntPtr).MakeByRefType() });
                
                if (tryLoadMethod == null) return false;

                var args = new object?[] { libraryName, assembly, searchPath, IntPtr.Zero };
                var result = (bool)tryLoadMethod.Invoke(null, args)!;
                if (result)
                {
                    handle = (IntPtr)args[3]!;
                }
                return result;
            }
            catch
            {
                return false;
            }
        }

        /// <summary>
        /// Gets the list of paths to search for the native library.
        /// </summary>
        public static string[] GetSearchPaths()
        {
            var paths = new System.Collections.Generic.List<string>();
            var rid = RuntimeIdentifier;
            var libName = NativeLibraryName;

            // Get the directory containing this assembly
            var assemblyDir = Path.GetDirectoryName(typeof(NativeLibraryLoader).Assembly.Location);
            if (!string.IsNullOrEmpty(assemblyDir))
            {
                // 1. runtimes/{rid}/native/{libname} relative to assembly
                paths.Add(Path.Combine(assemblyDir, "runtimes", rid, "native", libName));
                
                // 2. Same directory as assembly
                paths.Add(Path.Combine(assemblyDir, libName));
                
                // 3. Native subdirectory
                paths.Add(Path.Combine(assemblyDir, "native", libName));
            }

            // 4. Current directory
            paths.Add(Path.Combine(Environment.CurrentDirectory, libName));
            paths.Add(Path.Combine(Environment.CurrentDirectory, "runtimes", rid, "native", libName));

            // 5. App base directory
            var baseDir = AppContext.BaseDirectory;
            if (!string.IsNullOrEmpty(baseDir))
            {
                paths.Add(Path.Combine(baseDir, "runtimes", rid, "native", libName));
                paths.Add(Path.Combine(baseDir, libName));
            }

            // 6. LD_LIBRARY_PATH / DYLD_LIBRARY_PATH directories (Linux/macOS)
            if (!RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
            {
                var ldPath = RuntimeInformation.IsOSPlatform(OSPlatform.OSX)
                    ? Environment.GetEnvironmentVariable("DYLD_LIBRARY_PATH")
                    : Environment.GetEnvironmentVariable("LD_LIBRARY_PATH");

                if (!string.IsNullOrEmpty(ldPath))
                {
                    foreach (var dir in ldPath.Split(':'))
                    {
                        if (!string.IsNullOrEmpty(dir))
                        {
                            paths.Add(Path.Combine(dir, libName));
                        }
                    }
                }

                // 7. Standard system paths (Linux)
                if (RuntimeInformation.IsOSPlatform(OSPlatform.Linux))
                {
                    paths.Add($"/usr/local/lib/{libName}");
                    paths.Add($"/usr/lib/{libName}");
                    paths.Add($"/usr/lib/x86_64-linux-gnu/{libName}");
                    paths.Add($"/lib/x86_64-linux-gnu/{libName}");
                }
            }

            return paths.ToArray();
        }

        /// <summary>
        /// Attempts to load the native library and returns the first path that works.
        /// </summary>
        /// <returns>The path to the loaded library, or null if not found.</returns>
        public static string? FindLibraryPath()
        {
            foreach (var path in GetSearchPaths())
            {
                if (File.Exists(path))
                {
                    return path;
                }
            }
            return null;
        }

        /// <summary>
        /// Gets the library version string from the native library.
        /// </summary>
        /// <returns>The version string, or null if the library is not loaded.</returns>
        public static string? GetVersion()
        {
            try
            {
                Initialize();
                var ptr = NativeMethods.lpm_get_version();
                if (ptr == IntPtr.Zero)
                {
                    return null;
                }
                return Marshal.PtrToStringAnsi(ptr);
            }
            catch
            {
                return null;
            }
        }
    }
}
