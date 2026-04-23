#!/bin/bash
set -e

echo "Building PageRank binaries for Hadoop (Linux/AMD64)..."

# Ensure bin directory exists
mkdir -p bin

# Compile Mapper
GOOS=linux GOARCH=amd64 go build -o bin/mapper cmd/mapper/main.go
echo "✓ Mapper built: bin/mapper"

# Compile Reducer
GOOS=linux GOARCH=amd64 go build -o bin/reducer cmd/reducer/main.go
echo "✓ Reducer built: bin/reducer"

# Compile Driver
GOOS=linux GOARCH=amd64 go build -o bin/driver cmd/driver/main.go
echo "✓ Driver built: bin/driver"

echo "Build complete."
