#!/usr/bin/env python3
"""
LPM Algorithm Ranking Heatmap Generator

This script generates publication-quality heatmap visualizations showing
algorithm performance rankings across different CPUs for IPv4 and IPv6.

Usage:
    # Generate both IPv4 and IPv6 heatmaps
    python plot_algorithm_ranking.py

    # Generate only IPv4 heatmap
    python plot_algorithm_ranking.py --ipv4-only

    # Generate only IPv6 heatmap
    python plot_algorithm_ranking.py --ipv6-only

    # Custom output directory
    python plot_algorithm_ranking.py --output-dir docs/images/

    # Use only single lookups (no batch)
    python plot_algorithm_ranking.py --single-only
"""

import argparse
import csv
import os
import sys
from pathlib import Path
from typing import Dict, List, Tuple, Optional, Set

import matplotlib.pyplot as plt
import matplotlib.colors as mcolors
import numpy as np


# Default paths relative to script location
SCRIPT_DIR = Path(__file__).parent
PROJECT_ROOT = SCRIPT_DIR.parent
DATA_DIR = PROJECT_ROOT / "benchmarks" / "data" / "algorithm_comparison"
OUTPUT_DIR = PROJECT_ROOT / "docs" / "images"

# Prefix counts to average for small table performance
SMALL_TABLE_PREFIX_COUNTS = [32, 64, 128]

# Algorithm display names (for prettier labels)
ALGORITHM_DISPLAY_NAMES = {
    'dir24': 'DIR-24-8',
    '4stride8': '4-Stride-8',
    '6stride8': '6-Stride-8',
    'wide16': 'Wide-16',
    'dpdk': 'DPDK IPv4',
    'dpdk6': 'DPDK IPv6',
    'patricia': 'Patricia Trie',
    'rmindlpm': 'rmind/liblpm',
    'rmindlpm6': 'rmind/liblpm IPv6',
}

# CPU display names (for shorter labels)
CPU_SHORT_NAMES = {
    'amd_ryzen_9_9950x3d_16_core': 'Ryzen 9 9950X3D',
    'intelr_xeonr_gold_6426y': 'Xeon Gold 6426Y',
    'amd_epyc_7452_32_core': 'EPYC 7452',
    'amd_epyc_7301_16_core': 'EPYC 7301',
    'intelr_xeonr_cpu_e5_2683_v4': 'Xeon E5-2683 v4',
    'intelr_xeonr_cpu_e3_1220_v3': 'Xeon E3-1220 v3',
    'intelr_xeonr_cpu_x5650': 'Xeon X5650',
}


def read_csv_performance(filepath: Path) -> Tuple[Dict[int, float], Dict[str, str]]:
    """
    Read benchmark CSV and return performance data.
    
    Returns:
        Tuple of (prefix_count -> median_mlps dict, metadata dict)
    """
    performance = {}
    metadata = {}
    
    try:
        with open(filepath, 'r') as f:
            lines = f.readlines()
            data_lines = []
            for line in lines:
                if line.startswith('#'):
                    if ':' in line:
                        key, value = line[1:].strip().split(':', 1)
                        metadata[key.strip().lower()] = value.strip()
                else:
                    data_lines.append(line)
            
            reader = csv.DictReader(data_lines)
            for row in reader:
                prefix_count = int(row['num_prefixes'])
                median_lps = float(row['median_lookups_per_sec'])
                # Convert to Mlps (million lookups per second)
                performance[prefix_count] = median_lps / 1_000_000
        
        return performance, metadata
    except Exception as e:
        print(f"Warning: Could not read {filepath}: {e}", file=sys.stderr)
        return {}, {}


def discover_data(data_dir: Path, ip_version: str, single_only: bool = False) -> Dict[str, Dict[str, float]]:
    """
    Discover all CPU/algorithm combinations and their performance.
    
    Returns:
        Dict[cpu_name, Dict[algorithm_name, avg_mlps]]
    """
    result = {}
    
    # Find all directories matching pattern
    lookup_types = ['single'] if single_only else ['single', 'batch']
    
    for entry in data_dir.iterdir():
        if not entry.is_dir():
            continue
        
        dir_name = entry.name
        
        # Parse directory name: {cpu_name}_{ipv4|ipv6}_{single|batch}
        parts = dir_name.rsplit('_', 2)
        if len(parts) < 3:
            continue
        
        cpu_name = '_'.join(parts[:-2])
        dir_ip_version = parts[-2]
        lookup_type = parts[-1]
        
        # Filter by IP version and lookup type
        if dir_ip_version != ip_version:
            continue
        if lookup_type not in lookup_types:
            continue
        
        # Initialize CPU entry
        if cpu_name not in result:
            result[cpu_name] = {}
        
        # Read all algorithm CSVs in this directory
        for csv_file in entry.glob('*.csv'):
            algo_name = csv_file.stem
            performance, metadata = read_csv_performance(csv_file)
            
            if not performance:
                continue
            
            # Calculate average performance at small prefix counts
            small_table_perf = [
                performance[pc] for pc in SMALL_TABLE_PREFIX_COUNTS 
                if pc in performance
            ]
            
            if small_table_perf:
                avg_perf = sum(small_table_perf) / len(small_table_perf)
                
                # Keep the best performance if we have both single and batch
                if algo_name in result[cpu_name]:
                    result[cpu_name][algo_name] = max(result[cpu_name][algo_name], avg_perf)
                else:
                    result[cpu_name][algo_name] = avg_perf
    
    return result


def get_display_name(name: str, name_map: Dict[str, str]) -> str:
    """Get display name from mapping or return original."""
    return name_map.get(name, name)


def generate_heatmap(
    data: Dict[str, Dict[str, float]],
    ip_version: str,
    output_path: Path,
    title: Optional[str] = None
):
    """
    Generate a publication-quality heatmap visualization.
    
    Args:
        data: Dict[cpu_name, Dict[algorithm_name, avg_mlps]]
        ip_version: 'ipv4' or 'ipv6'
        output_path: Path to save the image
        title: Optional custom title
    """
    if not data:
        print(f"No data available for {ip_version.upper()}", file=sys.stderr)
        return
    
    # Collect all algorithms
    all_algorithms: Set[str] = set()
    for cpu_data in data.values():
        all_algorithms.update(cpu_data.keys())
    
    # Sort algorithms by average performance (descending)
    algo_avg_perf = {}
    for algo in all_algorithms:
        perfs = [data[cpu].get(algo, 0) for cpu in data]
        valid_perfs = [p for p in perfs if p > 0]
        algo_avg_perf[algo] = sum(valid_perfs) / len(valid_perfs) if valid_perfs else 0
    
    algorithms = sorted(all_algorithms, key=lambda a: algo_avg_perf[a], reverse=True)
    
    # Sort CPUs by average performance (descending)
    cpu_avg_perf = {}
    for cpu, cpu_data in data.items():
        valid_perfs = [p for p in cpu_data.values() if p > 0]
        cpu_avg_perf[cpu] = sum(valid_perfs) / len(valid_perfs) if valid_perfs else 0
    
    cpus = sorted(data.keys(), key=lambda c: cpu_avg_perf[c], reverse=True)
    
    # Build the matrix
    matrix = np.zeros((len(cpus), len(algorithms)))
    mask = np.zeros((len(cpus), len(algorithms)), dtype=bool)
    
    for i, cpu in enumerate(cpus):
        for j, algo in enumerate(algorithms):
            if algo in data[cpu] and data[cpu][algo] > 0:
                matrix[i, j] = data[cpu][algo]
            else:
                mask[i, j] = True  # Mark as missing
    
    # Create figure with appropriate dimensions for horizontal labels
    fig_width = max(12, len(algorithms) * 2.2)
    fig_height = max(5, len(cpus) * 0.75 + 1.5)
    fig, ax = plt.subplots(figsize=(fig_width, fig_height))
    
    # Create masked array
    masked_matrix = np.ma.masked_array(matrix, mask=mask)
    
    # Get data range
    vmin = masked_matrix.min() if masked_matrix.count() > 0 else 0
    vmax = masked_matrix.max() if masked_matrix.count() > 0 else 100
    
    # Create custom colormap: black -> dark red -> red -> yellow -> green
    # Values below 15 Mlps scale towards black
    threshold = 15.0
    threshold_norm = (threshold - vmin) / (vmax - vmin) if vmax > vmin else 0.1
    threshold_norm = min(max(threshold_norm, 0.05), 0.3)  # Keep it reasonable
    
    colors = [
        (0.0, '#1a0a0a'),           # Very dark (near black) for lowest
        (threshold_norm * 0.5, '#4a1010'),  # Dark red
        (threshold_norm, '#d73027'),        # Red at threshold (15 Mlps)
        (0.5, '#fee08b'),                   # Yellow at middle
        (1.0, '#1a9850')                    # Green at highest
    ]
    
    from matplotlib.colors import LinearSegmentedColormap
    cmap = LinearSegmentedColormap.from_list('custom_rdylgn', 
                                              [(pos, color) for pos, color in colors])
    cmap.set_bad(color='#e8e8e8')  # Light gray for missing data
    
    im = ax.imshow(masked_matrix, cmap=cmap, aspect='auto', vmin=vmin, vmax=vmax)
    
    # Add colorbar
    cbar = fig.colorbar(im, ax=ax, shrink=0.8, pad=0.02)
    cbar.set_label('Throughput (Mlps)', fontsize=11, labelpad=10)
    cbar.ax.tick_params(labelsize=9)
    
    # Set ticks and labels
    ax.set_xticks(np.arange(len(algorithms)))
    ax.set_yticks(np.arange(len(cpus)))
    
    # Use display names - horizontal labels
    algo_labels = [get_display_name(a, ALGORITHM_DISPLAY_NAMES) for a in algorithms]
    cpu_labels = [get_display_name(c, CPU_SHORT_NAMES) for c in cpus]
    
    ax.set_xticklabels(algo_labels, fontsize=11)
    ax.set_yticklabels(cpu_labels, fontsize=11)
    
    # Move x-axis to top
    ax.xaxis.set_ticks_position('top')
    ax.xaxis.set_label_position('top')
    
    # Add axis labels
    ax.set_xlabel('Algorithm', fontsize=12, fontweight='bold', labelpad=10)
    ax.set_ylabel('Processor', fontsize=12, fontweight='bold', labelpad=10)
    
    # Add value annotations
    for i in range(len(cpus)):
        for j in range(len(algorithms)):
            if not mask[i, j]:
                value = matrix[i, j]
                # Choose text color based on background brightness
                norm_val = (value - vmin) / (vmax - vmin) if vmax > vmin else 0.5
                # Custom colormap: dark/black (low) -> red -> yellow (mid) -> green (high)
                # Use white text for dark backgrounds (low values and high values)
                # Use black text for yellow middle region
                if norm_val < threshold_norm or norm_val > 0.7:
                    text_color = 'white'
                elif 0.3 < norm_val < 0.6:
                    text_color = 'black'
                else:
                    text_color = 'white'
                text = f'{value:.1f}' if value < 100 else f'{value:.0f}'
                ax.text(j, i, text, ha='center', va='center', 
                       fontsize=11, color=text_color, fontweight='bold')
            else:
                ax.text(j, i, 'N/A', ha='center', va='center',
                       fontsize=9, color='#888888', style='italic')
    
    # Add grid lines between cells
    ax.set_xticks(np.arange(len(algorithms) + 1) - 0.5, minor=True)
    ax.set_yticks(np.arange(len(cpus) + 1) - 0.5, minor=True)
    ax.grid(which='minor', color='white', linestyle='-', linewidth=2)
    ax.tick_params(which='minor', size=0)
    
    # Title
    if title is None:
        title = f"LPM Algorithm Performance Comparison - {ip_version.upper()}"
    fig.suptitle(title, fontsize=14, fontweight='bold', y=1.02)
    
    # Caption at bottom
    caption = f'Mean throughput at {", ".join(map(str, SMALL_TABLE_PREFIX_COUNTS))} prefixes (higher is better)'
    fig.text(0.5, -0.01, caption, ha='center', fontsize=10, color='#666666')
    
    # Adjust layout
    plt.tight_layout()
    
    # Save with high DPI for publication quality
    output_path.parent.mkdir(parents=True, exist_ok=True)
    plt.savefig(output_path, dpi=150, bbox_inches='tight', 
                facecolor='white', edgecolor='none', pad_inches=0.2)
    plt.close()
    
    print(f"Saved: {output_path}")


def main():
    parser = argparse.ArgumentParser(
        description='Generate LPM algorithm ranking heatmaps',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__
    )
    
    parser.add_argument(
        '--data-dir', '-d',
        type=Path,
        default=DATA_DIR,
        help='Directory containing algorithm comparison data'
    )
    
    parser.add_argument(
        '--output-dir', '-o',
        type=Path,
        default=OUTPUT_DIR,
        help='Directory to save output images'
    )
    
    parser.add_argument(
        '--ipv4-only',
        action='store_true',
        help='Generate only IPv4 heatmap'
    )
    
    parser.add_argument(
        '--ipv6-only',
        action='store_true',
        help='Generate only IPv6 heatmap'
    )
    
    parser.add_argument(
        '--single-only',
        action='store_true',
        help='Use only single lookup results (no batch)'
    )
    
    args = parser.parse_args()
    
    # Validate data directory
    if not args.data_dir.exists():
        print(f"Error: Data directory not found: {args.data_dir}", file=sys.stderr)
        sys.exit(1)
    
    # Determine which IP versions to process
    ip_versions = []
    if args.ipv4_only:
        ip_versions = ['ipv4']
    elif args.ipv6_only:
        ip_versions = ['ipv6']
    else:
        ip_versions = ['ipv4', 'ipv6']
    
    # Generate heatmaps
    for ip_version in ip_versions:
        print(f"\nProcessing {ip_version.upper()}...")
        
        # Discover data
        data = discover_data(args.data_dir, ip_version, args.single_only)
        
        if not data:
            print(f"  No data found for {ip_version.upper()}")
            continue
        
        # Report discovered data
        all_cpus = list(data.keys())
        all_algos = set()
        for cpu_data in data.values():
            all_algos.update(cpu_data.keys())
        
        print(f"  Found {len(all_cpus)} CPUs and {len(all_algos)} algorithms")
        
        # Generate heatmap
        output_path = args.output_dir / f"algorithm_ranking_{ip_version}.png"
        generate_heatmap(data, ip_version, output_path)
    
    print("\nDone!")


if __name__ == '__main__':
    main()
