# Benchmark Guide

Quick reference for running liblpm benchmarks locally and remotely.

## Prerequisites

- **Local:** CMake, GCC/Clang
- **DPDK:** Docker with `--privileged` access
- **Remote:** SSH key authentication

## Local Benchmarks

### Standard (No DPDK)

```bash
# Build and run all algorithms
./scripts/run_algorithm_benchmarks.sh

# Run specific algorithm
./scripts/run_algorithm_benchmarks.sh -a dir24

# Run only batch lookups on CPU core 2
./scripts/run_algorithm_benchmarks.sh -t batch -c 2
```

Results: `benchmarks/data/algorithm_comparison/<cpu>_<ip>_<type>/`

### With DPDK

```bash
# Run locally
./scripts/run_dpdk_algorithm_scaling.sh

# Force rebuild Docker image
./scripts/run_dpdk_algorithm_scaling.sh --rebuild

# Custom output directory
./scripts/run_dpdk_algorithm_scaling.sh -o /tmp/results
```

Results include DPDK comparison data.

## Remote Benchmarks

### Standard (Static Binary)

```bash
# Upload and run
./scripts/upload_and_benchmark.sh user@server.com

# With specific options
./scripts/upload_and_benchmark.sh user@server.com -a dir24 -t single
```

Automatically organizes results for CPU comparison.

### With DPDK (Docker Image)

```bash
# Upload pre-built Docker image and run
./scripts/run_dpdk_benchmark.sh root@192.168.0.13
```

Faster than building on remote server.

## Generate Charts

### Algorithm Comparison (per CPU)

```bash
./scripts/generate_algorithm_charts.sh
```

Compares different algorithms on the same CPU.

Output: `docs/images/<cpu>_<ip>_<type>.png`

### CPU Comparison (per algorithm)

```bash
./scripts/generate_cpu_comparison_charts.sh
```

Compares same algorithm across different CPUs.

Output: `docs/images/<algo>_<ip>_<type>_cpu_comparison.png`

### Manual Chart Generation

```bash
# Custom chart
python scripts/plot_lpm_benchmark.py \
    benchmarks/data/algorithm_comparison/my_cpu_ipv4_single/*.csv \
    --output docs/images/my_chart.png \
    --title "Custom Title"

# CPU comparison
python scripts/plot_lpm_benchmark.py \
    benchmarks/data/cpu_comparison/dir24_ipv4_single/*.csv \
    --output docs/images/dir24_cpu_comparison.png
```

## Quick Workflow

### Single Machine

```bash
# 1. Run benchmarks
./scripts/run_algorithm_benchmarks.sh

# 2. Generate charts
./scripts/generate_algorithm_charts.sh
```

### Multiple CPUs

```bash
# 1. Run on first machine
./scripts/run_algorithm_benchmarks.sh

# 2. Run on additional machines
./scripts/upload_and_benchmark.sh user@server1.com
./scripts/upload_and_benchmark.sh user@server2.com

# 3. Generate comparison charts
./scripts/generate_cpu_comparison_charts.sh
```

### With DPDK

```bash
# 1. Local
./scripts/run_dpdk_algorithm_scaling.sh

# 2. Remote
./scripts/run_dpdk_benchmark.sh user@server.com

# 3. Organize CPU comparison data
python3 << 'EOF'
from pathlib import Path
algo_comp = Path("benchmarks/data/algorithm_comparison")
cpu_comp = Path("benchmarks/data/cpu_comparison")
cpu_comp.mkdir(exist_ok=True)

for cpu_dir in algo_comp.iterdir():
    if not cpu_dir.is_dir():
        continue
    parts = cpu_dir.name.rsplit("_", 2)
    if len(parts) == 3:
        cpu_name, ip_ver, lookup = parts
        for csv in cpu_dir.glob("*.csv"):
            target = cpu_comp / f"{csv.stem}_{ip_ver}_{lookup}"
            target.mkdir(exist_ok=True)
            (target / f"{cpu_name}.csv").write_bytes(csv.read_bytes())
EOF

# 4. Generate all charts
./scripts/generate_algorithm_charts.sh
./scripts/generate_cpu_comparison_charts.sh
```

## Output Structure

```
benchmarks/data/
├── algorithm_comparison/          # Per-CPU results
│   ├── <cpu>_ipv4_single/
│   │   ├── dir24.csv
│   │   ├── 4stride8.csv
│   │   └── dpdk.csv
│   └── <cpu>_ipv6_batch/...
│
└── cpu_comparison/                # Per-algorithm results
    ├── dir24_ipv4_single/
    │   ├── <cpu1>.csv
    │   └── <cpu2>.csv
    └── dpdk_ipv4_batch/...

docs/images/
├── <cpu>_ipv4_single.png          # Algorithm comparison
└── dir24_ipv4_single_cpu_comparison.png  # CPU comparison
```

## Performance Tips

- **Pin to CPU:** Use `-c` flag to pin to specific core
- **Static binary:** `bench_algorithm_scaling` is statically linked for easy remote deployment
- **DPDK Docker:** Pre-build locally, upload image (faster than remote build)
- **Batch runs:** Use loops for multiple remote servers

## Common Issues

**"Benchmark binary not found"**
```bash
mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make bench_algorithm_scaling
```

**"Docker privilege error"**
- DPDK requires `--privileged` flag (automatically added by scripts)

**"SSH connection failed"**
```bash
ssh-copy-id user@server
ssh user@server 'echo OK'
```

## More Info

- Algorithm details: See `README.md`
- Docker setup: See `docs/DOCKER.md`
- CI/CD integration: See `docs/CICD.md`
