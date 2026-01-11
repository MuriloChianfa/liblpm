# Benchmark Charts

This directory contains performance benchmark charts for liblpm across different CPUs and algorithms.

## Algorithm Rankings

<p align="center">
<img src="algorithm_ranking_ipv4.png" width="48%" alt="IPv4 Algorithm Ranking">
<img src="algorithm_ranking_ipv6.png" width="48%" alt="IPv6 Algorithm Ranking"><br>
<sub><i>Overall performance comparison at small prefix counts (32-128 prefixes)</i></sub>
</p>

## CPU Comparison (same algorithm across CPUs)

### 4stride8 (IPv4)
<p align="center">
<img src="4stride8_ipv4_single_cpu_comparison.png" width="48%" alt="4stride8 IPv4 Single">
<img src="4stride8_ipv4_batch_cpu_comparison.png" width="48%" alt="4stride8 IPv4 Batch">
</p>

### dir24 (IPv4)
<p align="center">
<img src="dir24_ipv4_single_cpu_comparison.png" width="48%" alt="dir24 IPv4 Single">
<img src="dir24_ipv4_batch_cpu_comparison.png" width="48%" alt="dir24 IPv4 Batch">
</p>

### wide16 (IPv6)
<p align="center">
<img src="wide16_ipv6_single_cpu_comparison.png" width="48%" alt="wide16 IPv6 Single">
<img src="wide16_ipv6_batch_cpu_comparison.png" width="48%" alt="wide16 IPv6 Batch">
</p>

### 6stride8 (IPv6)
<p align="center">
<img src="6stride8_ipv6_single_cpu_comparison.png" width="48%" alt="6stride8 IPv6 Single">
<img src="6stride8_ipv6_batch_cpu_comparison.png" width="48%" alt="6stride8 IPv6 Batch">
</p>

### DPDK (IPv4)
<p align="center">
<img src="dpdk_ipv4_single_cpu_comparison.png" width="48%" alt="DPDK IPv4 Single">
<img src="dpdk_ipv4_batch_cpu_comparison.png" width="48%" alt="DPDK IPv4 Batch">
</p>

### DPDK (IPv6)
<p align="center">
<img src="dpdk6_ipv6_single_cpu_comparison.png" width="48%" alt="DPDK IPv6 Single">
<img src="dpdk6_ipv6_batch_cpu_comparison.png" width="48%" alt="DPDK IPv6 Batch">
</p>

### Patricia Trie (IPv4)
<p align="center">
<img src="patricia_ipv4_single_cpu_comparison.png" width="48%" alt="Patricia IPv4 Single">
</p>

### rmind/liblpm
<p align="center">
<img src="rmindlpm_ipv4_single_cpu_comparison.png" width="48%" alt="rmindlpm IPv4 Single">
<img src="rmindlpm6_ipv6_single_cpu_comparison.png" width="48%" alt="rmindlpm IPv6 Single">
</p>

## Algorithm Comparison (same CPU across algorithms)

### AMD Ryzen 9 9950X3D
<p align="center">
<img src="amd_ryzen_9_9950x3d_16_core_ipv4_single.png" width="48%" alt="Ryzen 9950X3D IPv4 Single">
<img src="amd_ryzen_9_9950x3d_16_core_ipv4_batch.png" width="48%" alt="Ryzen 9950X3D IPv4 Batch">
</p>
<p align="center">
<img src="amd_ryzen_9_9950x3d_16_core_ipv6_single.png" width="48%" alt="Ryzen 9950X3D IPv6 Single">
<img src="amd_ryzen_9_9950x3d_16_core_ipv6_batch.png" width="48%" alt="Ryzen 9950X3D IPv6 Batch">
</p>

### Intel Xeon Gold 6426Y
<p align="center">
<img src="intelr_xeonr_gold_6426y_ipv4_single.png" width="48%" alt="Xeon Gold 6426Y IPv4 Single">
<img src="intelr_xeonr_gold_6426y_ipv4_batch.png" width="48%" alt="Xeon Gold 6426Y IPv4 Batch">
</p>
<p align="center">
<img src="intelr_xeonr_gold_6426y_ipv6_single.png" width="48%" alt="Xeon Gold 6426Y IPv6 Single">
<img src="intelr_xeonr_gold_6426y_ipv6_batch.png" width="48%" alt="Xeon Gold 6426Y IPv6 Batch">
</p>

### Intel Xeon E5-2683 v4
<p align="center">
<img src="intelr_xeonr_cpu_e5_2683_v4_ipv4_single.png" width="48%" alt="Xeon E5-2683 v4 IPv4 Single">
<img src="intelr_xeonr_cpu_e5_2683_v4_ipv4_batch.png" width="48%" alt="Xeon E5-2683 v4 IPv4 Batch">
</p>
<p align="center">
<img src="intelr_xeonr_cpu_e5_2683_v4_ipv6_single.png" width="48%" alt="Xeon E5-2683 v4 IPv6 Single">
<img src="intelr_xeonr_cpu_e5_2683_v4_ipv6_batch.png" width="48%" alt="Xeon E5-2683 v4 IPv6 Batch">
</p>

### Intel Xeon E3-1220 v3
<p align="center">
<img src="intelr_xeonr_cpu_e3_1220_v3_ipv4_single.png" width="48%" alt="Xeon E3-1220 v3 IPv4 Single">
<img src="intelr_xeonr_cpu_e3_1220_v3_ipv6_single.png" width="48%" alt="Xeon E3-1220 v3 IPv6 Single">
</p>

### Intel Xeon X5650
<p align="center">
<img src="intelr_xeonr_cpu_x5650_ipv4_single.png" width="48%" alt="Xeon X5650 IPv4 Single">
<img src="intelr_xeonr_cpu_x5650_ipv6_single.png" width="48%" alt="Xeon X5650 IPv6 Single">
</p>

### AMD EPYC 7452
<p align="center">
<img src="amd_epyc_7452_32_core_ipv4_single.png" width="48%" alt="EPYC 7452 IPv4 Single">
<img src="amd_epyc_7452_32_core_ipv6_single.png" width="48%" alt="EPYC 7452 IPv6 Single">
</p>

### AMD EPYC 7301
<p align="center">
<img src="amd_epyc_7301_16_core_ipv4_single.png" width="48%" alt="EPYC 7301 IPv4 Single">
<img src="amd_epyc_7301_16_core_ipv6_single.png" width="48%" alt="EPYC 7301 IPv6 Single">
</p>
