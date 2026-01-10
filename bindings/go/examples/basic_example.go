package main

import (
	"fmt"
	"net/netip"

	"github.com/MuriloChianfa/liblpm/go/liblpm"
)

func main() {
	// Create an IPv4 routing table
	table, err := liblpm.NewTableIPv4()
	if err != nil {
		panic(err)
	}
	defer table.Close()

	// Add some routes
	routes := []struct {
		prefix  string
		nextHop liblpm.NextHop
		name    string
	}{
		{"10.0.0.0/8", 100, "Private Network A"},
		{"192.168.0.0/16", 200, "Private Network C"},
		{"172.16.0.0/12", 300, "Private Network B"},
		{"8.8.8.0/24", 400, "Google DNS"},
	}

	fmt.Println("Adding routes to table:")
	for _, route := range routes {
		prefix := netip.MustParsePrefix(route.prefix)
		err := table.Insert(prefix, route.nextHop)
		if err != nil {
			fmt.Printf("  Error adding %s: %v\n", route.prefix, err)
		} else {
			fmt.Printf("  Added: %s -> next hop %d (%s)\n", route.prefix, route.nextHop, route.name)
		}
	}

	// Perform some lookups
	fmt.Println("\nPerforming lookups:")
	lookupAddrs := []string{
		"10.1.1.1",
		"192.168.1.1",
		"172.16.5.10",
		"8.8.8.8",
		"1.1.1.1", // No match
	}

	for _, addrStr := range lookupAddrs {
		addr := netip.MustParseAddr(addrStr)
		nextHop, found := table.Lookup(addr)
		
		if found {
			// Find the route name
			routeName := "Unknown"
			for _, route := range routes {
				if uint32(route.nextHop) == uint32(nextHop) {
					routeName = route.name
					break
				}
			}
			fmt.Printf("  %s -> next hop %d (%s)\n", addrStr, nextHop, routeName)
		} else {
			fmt.Printf("  %s -> No route found\n", addrStr)
		}
	}

	// Demonstrate batch lookup
	fmt.Println("\nBatch lookup example:")
	batchAddrs := []netip.Addr{
		netip.MustParseAddr("10.1.1.1"),
		netip.MustParseAddr("192.168.1.1"),
		netip.MustParseAddr("172.16.5.10"),
		netip.MustParseAddr("1.1.1.1"),
	}

	results, err := table.LookupBatch(batchAddrs)
	if err != nil {
		panic(err)
	}

	for i, addr := range batchAddrs {
		if results[i].IsValid() {
			fmt.Printf("  %s -> next hop %d\n", addr, results[i])
		} else {
			fmt.Printf("  %s -> No route\n", addr)
		}
	}

	// IPv6 example
	fmt.Println("\n--- IPv6 Example ---")
	tableV6, err := liblpm.NewTableIPv6()
	if err != nil {
		panic(err)
	}
	defer tableV6.Close()

	// Add IPv6 routes
	ipv6Routes := []struct {
		prefix  string
		nextHop liblpm.NextHop
	}{
		{"2001:db8::/32", 1000},
		{"2001:db9::/32", 2000},
		{"fc00::/7", 3000}, // ULA
	}

	fmt.Println("Adding IPv6 routes:")
	for _, route := range ipv6Routes {
		prefix := netip.MustParsePrefix(route.prefix)
		err := tableV6.Insert(prefix, route.nextHop)
		if err != nil {
			fmt.Printf("  Error adding %s: %v\n", route.prefix, err)
		} else {
			fmt.Printf("  Added: %s -> next hop %d\n", route.prefix, route.nextHop)
		}
	}

	// IPv6 lookups
	fmt.Println("\nIPv6 lookups:")
	ipv6Addrs := []string{
		"2001:db8::1",
		"2001:db9::1",
		"fc00::1",
		"2001:dba::1", // No match
	}

	for _, addrStr := range ipv6Addrs {
		addr := netip.MustParseAddr(addrStr)
		nextHop, found := tableV6.Lookup(addr)
		
		if found {
			fmt.Printf("  %s -> next hop %d\n", addrStr, nextHop)
		} else {
			fmt.Printf("  %s -> No route found\n", addrStr)
		}
	}
}


