# PageRank with Hadoop and Go

This project implements the PageRank algorithm using Hadoop MapReduce with Go as the programming language for both the mapper and the reducer. It leverages Hadoop's Streaming utility to execute Go binaries across the cluster.

## Project Overview

- **Core Logic:** Go (`cmd/mapper/main.go` and `cmd/reducer/main.go`).
- **Distributed Framework:** Apache Hadoop 3.x.
- **Environment:** Dockerized cluster (Namenode, Datanode, Resource Manager, Node Manager).
- **Streaming:** Hadoop Streaming is used to pass data between the Go binaries and the HDFS filesystem.

## Repository Structure

- `bin/`: Target directory for compiled Go binaries (mapper and reducer). These are mounted into the Hadoop containers.
- `cmd/`: Source code for the mapper and reducer.
- `config/`: Hadoop configuration files (`core-site.xml`, `hdfs-site.xml`, `mapred-site.xml`, `yarn-site.xml`).
- `data/`: Local storage for HDFS metadata and data blocks.
- `compose.yml`: Docker Compose configuration for the Hadoop cluster.
- `hadoop.env`: Environment variables for the Hadoop services.

## Building and Running

### 1. Compile Go Binaries

The mapper and reducer must be compiled for the target architecture (Linux, as the Hadoop image is likely Linux-based).

```bash
# From the project root
GOOS=linux GOARCH=amd64 go build -o bin/mapper cmd/mapper/main.go
GOOS=linux GOARCH=amd64 go build -o bin/reducer cmd/reducer/main.go
```

### 2. Start the Hadoop Cluster

```bash
docker compose up -d
```

### 3. Run the PageRank Job

Once the cluster is up and data is loaded into HDFS, you can run the job using the Hadoop Streaming jar. (Example command below, adjust paths as needed):

```bash
# Example Hadoop Streaming command (TODO: verify exact jar path and arguments)
docker exec namenode hadoop jar /opt/hadoop/share/hadoop/tools/lib/hadoop-streaming-*.jar \
    -mapper /opt/binaries/mapper \
    -reducer /opt/binaries/reducer \
    -input /input \
    -output /output
```

## Development Conventions

- **Input/Output:** Mappers and Reducers communicate via standard input (`stdin`) and standard output (`stdout`).
- **Data Format:** Typically tab-separated key-value pairs as required by Hadoop Streaming.
- **Iterative Process:** PageRank is an iterative algorithm; a driver script (possibly in Go or shell) may be needed to run multiple rounds of MapReduce until convergence.
