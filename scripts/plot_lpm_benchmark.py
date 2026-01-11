#!/usr/bin/env python3
"""
LPM Benchmark Visualization Script

This script reads CSV benchmark data and generates publication-quality charts
comparing LPM lookup performance across different CPUs and algorithms.

Usage:
    # Compare same algorithm across CPUs
    python plot_lpm_benchmark.py \\
        benchmarks/data/cpu_comparison/dir24_ipv4_single/*.csv \\
        --output docs/images/dir24_ipv4_cpu_comparison.png

    # Compare algorithms on same CPU
    python plot_lpm_benchmark.py \\
        benchmarks/data/algorithm_comparison/my_cpu_ipv4_single/*.csv \\
        --output docs/images/ipv4_algorithm_comparison.png

    # Auto-generate title
    python plot_lpm_benchmark.py benchmarks/data/*.csv --auto-title
"""

import argparse
import csv
import sys
import re
from pathlib import Path
from typing import Dict, List, Tuple, Optional

import matplotlib.pyplot as plt
import matplotlib.ticker as ticker
import numpy as np


def read_csv_data(filepath: str) -> Tuple[List[int], List[float], List[float], List[float], 
                                          List[float], List[float], List[int], Dict[str, str]]:
    """
    Read benchmark CSV data from file.
    
    Returns:
        Tuple of (num_prefixes, median_lps, mean_lps, stddev_lps, min_lps, max_lps, memory_bytes, metadata)
    """
    num_prefixes = []
    median_lps = []
    mean_lps = []
    stddev_lps = []
    min_lps = []
    max_lps = []
    memory_bytes = []
    metadata = {}
    
    try:
        with open(filepath, 'r') as f:
            # Read metadata from comments
            lines = f.readlines()
            data_lines = []
            for line in lines:
                if line.startswith('#'):
                    # Parse metadata
                    if ':' in line:
                        key, value = line[1:].strip().split(':', 1)
                        metadata[key.strip().lower()] = value.strip()
                else:
                    data_lines.append(line)
            
            # Parse CSV data
            reader = csv.DictReader(data_lines)
            for row in reader:
                num_prefixes.append(int(row['num_prefixes']))
                median_lps.append(float(row['median_lookups_per_sec']))
                mean_lps.append(float(row['mean_lookups_per_sec']))
                stddev_lps.append(float(row['stddev_lookups_per_sec']))
                min_lps.append(float(row['min_lookups_per_sec']))
                max_lps.append(float(row['max_lookups_per_sec']))
                memory_bytes.append(int(row['memory_bytes']))
        
        return num_prefixes, median_lps, mean_lps, stddev_lps, min_lps, max_lps, memory_bytes, metadata
    
    except FileNotFoundError:
        print(f"Error: File '{filepath}' not found.", file=sys.stderr)
        sys.exit(1)
    except KeyError as e:
        print(f"Error: Missing column {e} in CSV file '{filepath}'.", file=sys.stderr)
        sys.exit(1)
    except ValueError as e:
        print(f"Error: Invalid data format in '{filepath}': {e}", file=sys.stderr)
        sys.exit(1)


def infer_label_from_filepath(filepath: str, metadata: Dict[str, str]) -> str:
    """
    Infer a human-readable label from the filepath and metadata.
    
    Priority:
    1. CPU model from metadata
    2. Algorithm name from metadata
    3. Parsed from filepath
    """
    path = Path(filepath)
    
    # Try to get CPU from metadata
    cpu = metadata.get('cpu', '')
    algorithm = metadata.get('algorithm', '')
    hostname = metadata.get('hostname', '')
    
    # Check if this is a CPU comparison (filename is CPU name, not algorithm)
    # cpu_comparison directories have format: {algo}_{ip}_{type}/
    parent = path.parent.name
    filename = path.stem
    
    # If parent looks like a CPU comparison directory, use CPU name
    if parent.count('_') >= 2:  # e.g., "dir24_ipv4_single"
        parts = parent.split('_')
        # Check if first part is an algorithm name
        if parts[0] in ['dir24', '4stride8', 'wide16', '6stride8', 'dpdk', 'dpdk6', 'patricia', 'rmindlpm', 'rmindlpm6']:
            # This is cpu_comparison - use CPU name from filename or metadata
            if cpu:
                cpu_short = cpu
                # Remove common suffixes to shorten CPU names
                # For Intel Xeon, keep the model number (E5-2683 v4) but remove frequency
                if 'Xeon' in cpu_short:
                    # Remove frequency info but keep model
                    cpu_short = re.sub(r'\s*@\s*[\d.]+GHz', '', cpu_short)
                    # Remove "CPU" between Xeon and model number
                    cpu_short = cpu_short.replace(' CPU ', ' ')
                else:
                    # For other CPUs, remove standard suffixes
                    for suffix in [' with Radeon Graphics', ' Processor']:
                        if suffix in cpu_short:
                            cpu_short = cpu_short.split(suffix)[0]
                
                cpu_short = cpu_short.strip()
                if len(cpu_short) > 60:
                    cpu_short = cpu_short[:60] + '...'
                return cpu_short
            else:
                # Use filename (sanitized CPU name)
                return filename.replace('_', ' ').title()
    
    # Otherwise, algorithm comparison - use algorithm name
    if algorithm:
        return algorithm
    
    # Parse from filepath
    # Expected patterns: 
    #   benchmarks/data/algorithm_comparison/{hostname}_ipv4_single/{algo}.csv
    #   benchmarks/data/cpu_comparison/{algo}_ipv4_single/{cpu_name}.csv
    
    # Check if parent directory contains IP version and lookup type
    if '_ipv4_' in parent or '_ipv6_' in parent or '_single' in parent or '_batch' in parent:
        # This is likely the algorithm name or CPU name
        return filename.upper() if filename in ['dir24', '4stride8', '6stride8', 'wide16'] else filename
    
    return filename.replace('_', ' ').title()


def plot_benchmark(datasets: Dict[str, Tuple], output_path: str, title: str,
                   y_scale: str = 'linear', show_memory: bool = False,
                   auto_title: bool = False):
    """
    Generate benchmark comparison chart.
    
    Args:
        datasets: Dict mapping label -> (num_prefixes, median_lps, mean_lps, stddev_lps, 
                                         min_lps, max_lps, memory_bytes, metadata)
        output_path: Path to save the output image
        title: Chart title
        y_scale: 'linear' or 'log' for y-axis scale
        show_memory: Whether to show memory usage on secondary axis
        auto_title: Whether to auto-generate title from data
    """
    fig, ax = plt.subplots(figsize=(12, 7))
    
    # Sort datasets by performance (best to worst at largest prefix count)
    sorted_items = sorted(datasets.items(),
                         key=lambda x: x[1][1][-1],  # median_lps at largest prefix
                         reverse=True)  # Higher is better
    datasets = dict(sorted_items)
    
    # Color scheme - expanded palette
    colors = [
        '#1f77b4', '#ff7f0e', '#2ca02c', '#d62728', '#9467bd', '#8c564b',
        '#e377c2', '#7f7f7f', '#bcbd22', '#17becf', '#aec7e8', '#ffbb78',
        '#98df8a', '#ff9896', '#c5b0d5', '#c49c94', '#f7b6d2', '#c7c7c7',
        '#dbdb8d', '#9edae5', '#393b79', '#637939', '#8c6d31', '#843c39'
    ]
    markers = ['o', 's', '^', 'D', 'v', 'p', '*', 'X', 'P', 'h', '<', '>',
               'd', '8', 'H', '1', '2', '3', '4', '+', 'x']
    
    for idx, (label, data) in enumerate(datasets.items()):
        num_prefixes, median_lps, mean_lps, stddev_lps, min_lps, max_lps, memory_bytes, metadata = data
        
        color = colors[idx % len(colors)]
        marker = markers[idx % len(markers)]
        
        # Convert to numpy arrays
        x = np.array(num_prefixes)
        y = np.array(median_lps) / 1e6  # Convert to millions
        yerr = np.array(stddev_lps) / 1e6
        
        # Plot main line (median)
        ax.plot(x, y,
                marker=marker,
                markersize=5,
                linewidth=1.5,
                label=label,
                color=color,
                zorder=3)
        
        # Add shaded error region (±1 std dev)
        ax.fill_between(x, y - yerr, y + yerr,
                       alpha=0.2,
                       color=color,
                       zorder=1)
        
        # Add error bars
        ax.errorbar(x, y, yerr=yerr,
                   fmt='none',
                   ecolor=color,
                   alpha=0.4,
                   capsize=2,
                   capthick=0.5,
                   linewidth=0.8,
                   zorder=2)
    
    # Generate auto title if requested
    if auto_title and datasets:
        first_data = next(iter(datasets.values()))
        metadata = first_data[7]
        ip_version = metadata.get('ip version', 'IPv4').upper()
        lookup_type = metadata.get('lookup type', 'single').title()
        cpu_name = metadata.get('cpu', '')
        hostname = metadata.get('hostname', '')
        
        # Shorten CPU name for title
        cpu_short = cpu_name
        for suffix in [' with Radeon Graphics', ' Processor', ' CPU @ ', '@']:
            if suffix in cpu_short:
                cpu_short = cpu_short.split(suffix)[0]
        if len(cpu_short) > 50:
            cpu_short = cpu_short[:50] + '...'
        
        algo_names = list(datasets.keys())
        
        # Check if all datasets have same CPU (algorithm comparison)
        all_same_cpu = all(d[7].get('cpu', '') == cpu_name for d in datasets.values())
        
        # Check if all datasets have same algorithm (CPU comparison)
        first_algo = metadata.get('algorithm', '')
        all_same_algo = all(d[7].get('algorithm', '') == first_algo for d in datasets.values())
        
        if all_same_cpu and len(algo_names) > 1:
            # Algorithm comparison on same CPU
            title = f"{ip_version} {lookup_type} Lookup - {cpu_short}"
        elif all_same_algo and len(algo_names) > 1:
            # CPU comparison for same algorithm - include algorithm name
            algo_display = first_algo.split('(')[0].strip() if '(' in first_algo else first_algo
            # Don't duplicate IP version if already in algorithm name
            if ip_version.lower() in algo_display.lower():
                title = f"{algo_display} {lookup_type} - Comparing {len(algo_names)} CPUs"
            else:
                title = f"{algo_display} {ip_version} {lookup_type} - Comparing {len(algo_names)} CPUs"
        elif len(algo_names) == 1:
            # Single configuration
            title = f"LPM Lookup Performance - {algo_names[0]}"
        else:
            # Multiple CPUs, multiple algorithms
            title = f"LPM {ip_version} {lookup_type} Lookup - Comparing {len(algo_names)} CPUs"
    
    # Labels and title
    ax.set_xlabel('Number of Prefixes in Trie', fontsize=11)
    ax.set_ylabel('Mlps - higher is better', fontsize=11)
    ax.set_title(title, fontsize=13, fontweight='bold')
    
    # Set y-axis scale
    if y_scale == 'log':
        ax.set_yscale('log')
    else:
        ax.set_ylim(bottom=0)
        # Add more y-axis ticks for better readability
        ax.yaxis.set_major_locator(ticker.MaxNLocator(nbins=20, integer=False))
        ax.yaxis.set_minor_locator(ticker.AutoMinorLocator(2))
    
    # X-axis formatting (log scale for prefix counts)
    ax.set_xscale('log', base=2)
    ax.set_xlim(left=min(num_prefixes) * 0.8, right=max(num_prefixes) * 1.2)
    
    # Custom x-axis labels
    prefix_counts = [32, 64, 128, 256, 512, 1024, 2048, 4096, 8192]
    ax.set_xticks(prefix_counts)
    ax.set_xticklabels([str(p) for p in prefix_counts])
    ax.tick_params(axis='x', rotation=0)  # Horizontal labels
    
    # Create legend
    from matplotlib.patches import Patch
    legend_elements = []
    for idx, label in enumerate(datasets.keys()):
        color = colors[idx % len(colors)]
        marker = markers[idx % len(markers)]
        legend_elements.append(plt.Line2D([0], [0], marker=marker, color=color,
                                          linewidth=1.5, markersize=5, label=label))
    
    legend_elements.append(Patch(facecolor='gray', alpha=0.2, label='±1σ region'))
    ax.legend(handles=legend_elements, loc='upper right', fontsize=9)
    
    # Grid
    ax.grid(True, alpha=0.3, linestyle='--', linewidth=0.5)
    ax.grid(True, which='minor', alpha=0.15, linestyle=':', linewidth=0.3)
    
    # Add note about trials (with more top margin)
    ax.text(0.5, -0.10, '5 trials per data point, 5M lookups per trial',
            transform=ax.transAxes,
            ha='center',
            fontsize=9,
            style='italic',
            color='gray')
    
    # Secondary y-axis for memory (optional)
    if show_memory and datasets:
        ax2 = ax.twinx()
        first_data = next(iter(datasets.values()))
        memory_mb = np.array(first_data[6]) / (1024 * 1024)
        x = np.array(first_data[0])
        ax2.plot(x, memory_mb, '--', color='gray', alpha=0.5, label='Memory')
        ax2.set_ylabel('Memory (MB)', fontsize=10, color='gray')
        ax2.tick_params(axis='y', colors='gray')
    
    # Tight layout
    plt.tight_layout()
    
    # Save figure
    output_file = Path(output_path)
    output_file.parent.mkdir(parents=True, exist_ok=True)
    plt.savefig(output_path, dpi=150, bbox_inches='tight')
    print(f"Chart saved to: {output_path}")
    plt.close()


def main():
    parser = argparse.ArgumentParser(
        description='Generate benchmark visualization charts for liblpm',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Auto-infer labels from files
  python plot_lpm_benchmark.py benchmarks/data/algorithm_comparison/my_cpu_ipv4_single/*.csv
  
  # Single file with custom label
  python plot_lpm_benchmark.py --input results.csv:MyLabel
  
  # Custom output and title
  python plot_lpm_benchmark.py benchmarks/data/*.csv \\
    --output docs/images/comparison.png \\
    --title "My Custom Title"
  
  # Use log scale for y-axis
  python plot_lpm_benchmark.py benchmarks/data/*.csv --log-scale
        """
    )
    
    parser.add_argument(
        'files',
        nargs='*',
        metavar='FILE',
        help='Input CSV files (labels auto-inferred from files)'
    )
    
    parser.add_argument(
        '--input', '-i',
        nargs='+',
        metavar='FILE[:LABEL]',
        help='Input CSV files with optional labels, format: file.csv or file.csv:Label'
    )
    
    parser.add_argument(
        '--output', '-o',
        default='docs/images/lpm_benchmark.png',
        help='Output image path (default: docs/images/lpm_benchmark.png)'
    )
    
    parser.add_argument(
        '--title', '-t',
        default=None,
        help='Chart title (auto-generated if not specified)'
    )
    
    parser.add_argument(
        '--log-scale',
        action='store_true',
        help='Use logarithmic scale for y-axis'
    )
    
    parser.add_argument(
        '--show-memory',
        action='store_true',
        help='Show memory usage on secondary y-axis'
    )
    
    parser.add_argument(
        '--auto-title',
        action='store_true',
        help='Auto-generate title from data'
    )
    
    args = parser.parse_args()
    
    # Collect all input files
    input_specs = []
    
    if args.files:
        input_specs.extend(args.files)
    
    if args.input:
        input_specs.extend(args.input)
    
    if not input_specs:
        parser.error("No input files specified. Provide files as positional arguments or use --input")
    
    # Parse input files and labels
    datasets = {}
    for input_spec in input_specs:
        if ':' in input_spec and not input_spec.startswith('/'):
            # Explicit label provided (but not an absolute path on Unix)
            parts = input_spec.rsplit(':', 1)
            if len(parts) == 2 and not parts[1].endswith('.csv'):
                filepath, label = parts
            else:
                filepath = input_spec
                label = None
        else:
            filepath = input_spec
            label = None
        
        print(f"Reading {filepath}...")
        data = read_csv_data(filepath)
        
        if label is None:
            label = infer_label_from_filepath(filepath, data[7])
        
        print(f"  - Label: {label}")
        print(f"  - Data points: {len(data[0])}")
        if data[7]:
            if 'algorithm' in data[7]:
                print(f"  - Algorithm: {data[7]['algorithm']}")
            if 'cpu' in data[7]:
                print(f"  - CPU: {data[7]['cpu']}")
        
        datasets[label] = data
    
    # Determine title
    auto_title = args.auto_title or args.title is None
    title = args.title if args.title else "LPM Lookup Performance Comparison"
    
    # Generate the plot
    print(f"\nGenerating chart...")
    y_scale = 'log' if args.log_scale else 'linear'
    plot_benchmark(datasets, args.output, title, y_scale, args.show_memory, auto_title)
    print("Done!")


if __name__ == '__main__':
    main()
