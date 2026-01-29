#!/usr/bin/env python3
"""Compatibility shim for editable installs and legacy tools.

This setup.py exists only for compatibility with tools that don't
fully support PEP 517/518. All build configuration is in pyproject.toml.

For building, use:
    pip install .

For editable installs:
    pip install -e .

For building wheels:
    pip wheel . -w dist/
"""

import sys

if __name__ == "__main__":
    # Modern pip should use pyproject.toml directly
    # This is just a fallback for older tools
    try:
        from scikit_build_core import build
    except ImportError:
        print(
            "scikit-build-core is required to build this package.\n"
            "Install it with: pip install scikit-build-core cython",
            file=sys.stderr
        )
        sys.exit(1)
    
    # Delegate to scikit-build-core
    from setuptools import setup
    setup()
