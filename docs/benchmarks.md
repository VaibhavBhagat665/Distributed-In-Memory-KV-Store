# Benchmark Results

All benchmarks use memtier_benchmark with documented configurations. Numbers are from actual measured runs, not estimates.

## Hardware Specifications

[TODO: Document hardware specs when first benchmark is run]

- CPU: [TODO]
- Cores: [TODO]
- RAM: [TODO]
- Disk: [TODO]
- OS: [TODO]

## Benchmark Configuration

- Workload: 50% SET / 50% GET
- Value Size: 128 bytes
- Key Distribution: Zipfian
- Clients: 4 threads, 50 connections each (200 total)

## Results

### Single-Threaded Baseline

[TODO: Run benchmark - Phase 1]

- Operations per second: [TODO: run benchmark]
- p50 latency: [TODO: run benchmark]
- p99 latency: [TODO: run benchmark]

### Multi-Core Scaling

[TODO: Run benchmark - Phase 2]

| Threads | Ops/sec | Speedup vs 1-thread |
|---------|---------|---------------------|
| 1       | [TODO: run benchmark] | 1.0x |
| 2       | [TODO: run benchmark] | [TODO: run benchmark] |
| 4       | [TODO: run benchmark] | [TODO: run benchmark] |
| 8       | [TODO: run benchmark] | [TODO: run benchmark] |

### Global-Mutex Comparison

[TODO: Run benchmark - Phase 2]

Comparison between shared-nothing and global-mutex designs:

| Threads | Shared-Nothing Ops/sec | Global-Mutex Ops/sec | Advantage |
|---------|------------------------|----------------------|-----------|
| 1       | [TODO: run benchmark] | [TODO: run benchmark] | [TODO: run benchmark] |
| 4       | [TODO: run benchmark] | [TODO: run benchmark] | [TODO: run benchmark] |
| 8       | [TODO: run benchmark] | [TODO: run benchmark] | [TODO: run benchmark] |

### Network Backend Comparison

[TODO: Run benchmark - Phase 4]

| Backend   | Ops/sec | p50 latency | p99 latency |
|-----------|---------|-------------|-------------|
| epoll     | [TODO: run benchmark] | [TODO: run benchmark] | [TODO: run benchmark] |
| io_uring  | [TODO: run benchmark] | [TODO: run benchmark] | [TODO: run benchmark] |

**Chosen backend**: [TODO: Choose based on evidence]

### Persistence Overhead

[TODO: Run benchmark - Phase 3]

| Configuration | Ops/sec | Overhead |
|---------------|---------|----------|
| No WAL        | [TODO: run benchmark] | 0% |
| WAL enabled   | [TODO: run benchmark] | [TODO: run benchmark]% |

### Recovery Time

[TODO: Run benchmark - Phase 3]

| Dataset Size | Recovery Time |
|--------------|---------------|
| 100MB        | [TODO: run benchmark] |
| 1GB          | [TODO: run benchmark] |
| 10GB         | [TODO: run benchmark] |

### Cluster Performance

[TODO: Run benchmark - Phase 6]

#### Write Latency (3-node cluster)

- p50: [TODO: run benchmark]
- p99: [TODO: run benchmark]
- p999: [TODO: run benchmark]

#### Leader Failover Time

Time from leader SIGKILL to first successful write on new leader:

- Failover time: [TODO: run benchmark]

## Methodology

All benchmarks are run with:
- Commit SHA: [TODO: Record when benchmark runs]
- Date: [TODO: Record when benchmark runs]
- memtier command: [TODO: Record exact command]

Each result is the average of 3 runs after 1 warmup run.
