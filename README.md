# Distributed Storage System 

A simple **distributed file storage system** written in C that simulates a cluster of N single-threaded nodes over TCP.  
Each node uses non-blocking sockets (assuming exactly one thread per node), and all files are stored **in RAM**. A command-line client interface is provided under `client/`.

## Design Philosophy: Simulating Constrained Hardware

This project intentionally **simulates severely limited hardware conditions**. Each node is restricted to:
- **A single operating system thread**


These constraints are **not accidental limitations**, but deliberate design choices. The goal is to explore an important systems idea:

> A large number of individually weak, constrained, or "bad" nodes can collectively form a **fast and effective distributed network**.

By enforcing minimal per-node capabilities, the system emphasizes **horizontal scaling over vertical power**. Instead of relying on powerful machines, the architecture demonstrates how performance can emerge from **many inexpensive or resource-constrained nodes working together**.

## Overview

This project is an educational / experimental distributed-storage system. It aims to demonstrate how a minimal distributed storage cluster can be built using only basic OS primitives (non-blocking sockets, single-threaded nodes) and in-memory storage.  

It is **not** designed for production use. Rather, it's a proof-of-concept to explore distributed storage design, networking, and inter-node communication.

## Features

- Simple TCP-based networking between nodes  
- Each node runs as a single thread, using non-blocking sockets  
- Files stored entirely in RAM (no persistent disk storage)  
- Basic client interface (via CLI) for storing / retrieving files  
- Lightweight dependencies — plain C, minimal libraries  

## Requirements

- A Unix-like operating system (e.g. Linux)  
- A C compiler (e.g. `gcc`)  
- `make` (or compatible build tools)  
- Network connectivity between nodes (TCP)  

## Installation & Build

```bash
# Clone the repository
git clone https://github.com/wolke412/distributed-storage-system.git
cd distributed-storage-system

# Build the main program
make
