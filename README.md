# The-Order-Book-Kinetic
-------------------------------
## Overview
The system uses a Single-Producer, Single-Consumer (SPSC) architecture to separate network ingestion from data processing, allowing both tasks to run independently with minimal synchronization overhead.
The UDP receiver serves as the data entry point. Instead of relying on TCP, it binds a POSIX socket directly to the network interface and receives raw binary UDP packets, avoiding the latency introduced by connection management and retransmissions.
Incoming packets are passed to the processing thread through a lock-free ring buffer. The buffer is pre-allocated and uses std::atomic with acquire/release memory ordering to safely transfer data between threads without the overhead of std::mutex locks.
The order book maps packet data directly into C++ structures using #pragma pack, eliminating unnecessary copies and padding. Market state is stored in a pre-allocated Array of Structs (AoS), providing constant-time (O(1)) updates while keeping memory access predictable and cache-friendly.
Finally, the visualizer converts the live order book into a real-time ASCII display, highlighting metrics such as Order Book Imbalance (OBI) so changes in market liquidity can be observed as they happen.
