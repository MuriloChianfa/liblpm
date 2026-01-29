#!/usr/bin/env python3
"""Example demonstrating context manager usage and resource management.

The LpmTable classes support the context manager protocol, which ensures
proper cleanup of C resources even if an exception occurs.
"""

from ipaddress import IPv4Address, IPv4Network

from liblpm import LpmTableIPv4, LpmClosedError


def basic_context_manager():
    """Basic context manager usage."""
    print("Basic context manager usage")
    print("-" * 40)
    
    # Resources are automatically cleaned up when exiting the 'with' block
    with LpmTableIPv4() as table:
        table.insert('192.168.0.0/16', 100)
        result = table.lookup('192.168.1.1')
        print(f"Lookup result: {result}")
        print(f"Table closed inside 'with': {table.closed}")
    
    print(f"Table closed after 'with': {table.closed}")


def exception_safety():
    """Demonstrate exception safety."""
    print("\n" + "-" * 40)
    print("Exception safety")
    print("-" * 40)
    
    table = None
    try:
        with LpmTableIPv4() as table:
            table.insert('192.168.0.0/16', 100)
            print("Inserted route, now raising exception...")
            raise ValueError("Simulated error")
    except ValueError as e:
        print(f"Caught exception: {e}")
    
    # Table should still be properly closed
    print(f"Table closed despite exception: {table.closed}")


def manual_resource_management():
    """Demonstrate manual resource management without context manager."""
    print("\n" + "-" * 40)
    print("Manual resource management")
    print("-" * 40)
    
    table = LpmTableIPv4()
    try:
        table.insert('192.168.0.0/16', 100)
        result = table.lookup('192.168.1.1')
        print(f"Lookup result: {result}")
    finally:
        table.close()
        print("Table manually closed")
    
    # Verify operations fail after close
    try:
        table.lookup('192.168.1.1')
        print("ERROR: Should have raised LpmClosedError")
    except LpmClosedError:
        print("Correctly raised LpmClosedError after close")


def multiple_close_is_safe():
    """Demonstrate that calling close() multiple times is safe."""
    print("\n" + "-" * 40)
    print("Multiple close() calls are safe")
    print("-" * 40)
    
    table = LpmTableIPv4()
    table.insert('192.168.0.0/16', 100)
    
    print("Closing table first time...")
    table.close()
    print(f"Closed: {table.closed}")
    
    print("Closing table second time (should not error)...")
    table.close()
    print(f"Still closed: {table.closed}")
    
    print("Closing table third time...")
    table.close()
    print("No errors!")


def nested_tables():
    """Demonstrate using multiple tables."""
    print("\n" + "-" * 40)
    print("Nested table usage")
    print("-" * 40)
    
    with LpmTableIPv4() as table_v4:
        table_v4.insert('192.168.0.0/16', 100)
        
        # Can have multiple tables open simultaneously
        with LpmTableIPv4() as table_v4_alt:
            table_v4_alt.insert('10.0.0.0/8', 200)
            
            # Look up in both tables
            result1 = table_v4.lookup('192.168.1.1')
            result2 = table_v4_alt.lookup('10.0.0.1')
            
            print(f"Table 1 lookup: {result1}")
            print(f"Table 2 lookup: {result2}")
        
        # table_v4_alt is now closed, but table_v4 is still open
        print(f"Inner table closed: {table_v4_alt.closed}")
        print(f"Outer table still open: {not table_v4.closed}")
    
    print(f"Both tables now closed: {table_v4.closed and table_v4_alt.closed}")


def main():
    """Run all context manager examples."""
    print("liblpm Python Bindings - Context Manager Example")
    print("=" * 50)
    print()
    
    basic_context_manager()
    exception_safety()
    manual_resource_management()
    multiple_close_is_safe()
    nested_tables()
    
    print("\n" + "=" * 50)
    print("Context manager examples completed!")


if __name__ == '__main__':
    main()
