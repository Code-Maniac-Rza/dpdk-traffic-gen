# High-Performance Network Traffic Generator Using DPDK

## Overview
This project is a high-performance Linux-based traffic generator built using the Data Plane Development Kit (DPDK). It is capable of saturating high-speed network interfaces (10G or higher) by generating millions of packets per second. This tool is designed for network performance testing, including throughput, latency, and stress testing for high-speed networks.

---

## Features
- Generates customizable network traffic with adjustable headers, payloads, and packet sizes.
- High throughput, capable of achieving up to 14 million packets per second.
- Multi-threaded design for scalability and efficient CPU utilization.
- Real-time performance monitoring for packet throughput and latency.

---

## Prerequisites
1. **Hardware Requirements:**
   - A multi-core CPU (e.g., Intel Xeon or AMD Ryzen).
   - A DPDK-compatible Network Interface Card (NIC), such as Intel X710 or Mellanox ConnectX.

2. **Software Requirements:**
   - Linux OS (e.g., Ubuntu 22.04 or CentOS 8).
   - DPDK library installed on the system.
   - GCC compiler or equivalent for building the code.

---

## Installation
### 1. Install DPDK
Follow the [official DPDK guide](https://doc.dpdk.org/guides/linux_gsg/index.html) to install and configure DPDK on your Linux system.

### 2. Clone the Repository
```bash
git clone https://github.com/Code-Maniac-Rza/dpdk-traffic-gen.git
cd dpdk-traffic-generator
```

### 3. Compile the Code
```bash
gcc -o traffic_generator traffic_generator.c $(pkg-config --cflags --libs libdpdk)
```

### 4. Configure the NIC
Bind your NIC to a DPDK-compatible driver (e.g., `vfio-pci`):
```bash
sudo dpdk-devbind.py --bind=vfio-pci <NIC PCI address>
```

To list available NICs:
```bash
sudo dpdk-devbind.py --status
```

---

## Usage
Run the traffic generator with the following command:
```bash
sudo ./traffic_generator -l 0-3 -n 4
```
### Parameters:
- `-l`: Specifies the CPU cores to use.
- `-n`: Specifies the number of memory channels.

---

## How It Works
1. **Initialization:**
   - The DPDK Environment Abstraction Layer (EAL) is initialized.
   - A memory pool is created for storing packet buffers.

2. **Port Configuration:**
   - The Ethernet device is configured with RX and TX queues.
   - Promiscuous mode is enabled to capture all incoming packets.

3. **Packet Generation:**
   - Packets are allocated and filled with dummy data.
   - Packets are transmitted in bursts to maximize throughput.

4. **Performance Monitoring:**
   - The number of transmitted packets is tracked in real-time.

---

## Future Enhancements
- Support for multiple NICs to generate traffic on multiple network interfaces.
- Integration of a configuration file for defining traffic patterns.
- Advanced analytics, including latency distribution and packet loss metrics.
- Real-time visualization of network performance metrics using a web-based dashboard.

---

## Troubleshooting
1. **NIC Not Detected:**
   - Ensure the NIC is bound to a DPDK-compatible driver.
   - Check the NIC status using `dpdk-devbind.py`.

2. **Low Throughput:**
   - Verify CPU isolation and ensure sufficient cores are allocated.
   - Disable power-saving modes in the BIOS for consistent performance.

3. **Packet Drops:**
   - Increase buffer sizes in the code or tune NIC parameters.

---

## Acknowledgments
This project leverages the [Data Plane Development Kit (DPDK)](https://www.dpdk.org/) for high-speed packet processing and is inspired by the official DPDK-pktgen tool.

---

## Contact
For questions or support, please contact:
- **Name:** Shahryar Rza
- **Email:** rzashahryar896@gmail.com

