#!/usr/bin/env bash

NODE_SIZES=(1000 10000 100000 250000 500000 750000 1000000)
NODE_DEGREE=20
TOLERANCE="0.0001"
DAMPING_FACTOR="0.85"

discard_output() {
    "$@" > /dev/null 2>&1
}

build_bin() {
    echo "Building binaries..."
    # Build for container (Linux/AMD64)
    discard_output GOOS=linux GOARCH=amd64 go build -o bin/mapper cmd/mapper/main.go
    discard_output GOOS=linux GOARCH=amd64 go build -o bin/reducer cmd/reducer/main.go
    discard_output GOOS=linux GOARCH=amd64 go build -o bin/driver cmd/driver/main.go
    
    # Build for local host (to run generator and serial rank)
    discard_output go build -o bin/generator cmd/generator/main.go
    discard_output go build -o bin/rank cmd/rank/main.go
    
    if [ $? -ne 0 ]; then
        echo "Error building binaries."
        exit 1
    fi
    echo "Done."
}

start_cluster() {
    echo "Starting Hadoop cluster..."
    discard_output docker compose up -d

    echo "Waiting for Docker containers..."
    while ! docker compose ps | grep -q "Up"; do
        sleep 1
    done

    echo "Waiting for HDFS to exit Safe Mode..."
    until docker exec namenode hdfs dfsadmin -safemode wait | grep -q "Safe mode is OFF"; do
        sleep 2
    done
    echo "Cluster is ready."
}

stop_cluster() {
    echo "Stopping Hadoop cluster..."
    discard_output docker compose down
    echo "Done."
}

run_hadoop() {
    local node_size="$1"
    local log_file="$2"
    local input_file="tmp/data_$node_size.txt"
    
    # Cleanup previous HDFS runs
    docker exec namenode hadoop fs -rm -r "/output_$node_size" >> "$log_file" 2>&1
    docker exec namenode hadoop fs -mkdir -p /input >> "$log_file" 2>&1

    # Transfer data from host to container, then to HDFS
    docker cp "$input_file" namenode:/tmp/data.txt >> "$log_file" 2>&1
    docker exec namenode hadoop fs -put -f /tmp/data.txt "/input/data_$node_size.txt" >> "$log_file" 2>&1

    # Execute the driver inside the container and log output to file
    docker exec namenode /opt/binaries/driver \
        -mapper /opt/binaries/mapper \
        -reducer /opt/binaries/reducer \
        -input "/input/data_$node_size.txt" \
        -output "/output_$node_size" \
        -total "$node_size" \
        -iter 100 \
        -tol "$TOLERANCE" \
        -damping "$DAMPING_FACTOR" >> "$log_file" 2>&1

    # Discard the output directory from HDFS to save space
    docker exec namenode hadoop fs -rm -r "/output_$node_size" >> "$log_file" 2>&1
}

# Script should be run from the project root
cd "$(dirname "$0")/.." || exit

# Setup directories
mkdir -p tmp bin perf logs
rm -rf logs/* # Clear old logs

build_bin
start_cluster

REPORT_FILE="perf/report_$(date +%d%m%H%M).csv"
echo "time_ms,nodes,type,degree" > "$REPORT_FILE"
ln -sf "$REPORT_FILE" perf/latest_report.csv

for NODE_SIZE in "${NODE_SIZES[@]}"; do
    INPUT_FILE="tmp/data_$NODE_SIZE.txt"
    SERIAL_LOG="logs/serial_$NODE_SIZE.log"
    HADOOP_LOG="logs/hadoop_$NODE_SIZE.log"
    
    echo "--- Scaling Test: $NODE_SIZE nodes ---"
    
    # 1. Generate Data
    echo "  Generating data..."
    ./bin/generator -nodes "$NODE_SIZE" -edges "$NODE_DEGREE" -output "$INPUT_FILE" > /dev/null

    # 2. Run Serial
    echo "  Running Serial (Logs: $SERIAL_LOG)..."
    START_TIME=$(date +%s%3N)
    ./bin/rank -iter 100 -tol "$TOLERANCE" -damping "$DAMPING_FACTOR" "$INPUT_FILE" > "$SERIAL_LOG" 2>&1
    END_TIME=$(date +%s%3N)
    SERIAL_TIME=$(LC_ALL=C printf "%d" "$((END_TIME - START_TIME))")
    echo "  Serial finished in ${SERIAL_TIME}ms"
    echo "$SERIAL_TIME,$NODE_SIZE,serial,$NODE_DEGREE" >> "$REPORT_FILE"

    # 3. Run Hadoop
    echo "  Running Hadoop (Logs: $HADOOP_LOG)..."
    START_TIME=$(date +%s%3N)
    run_hadoop "$NODE_SIZE" "$HADOOP_LOG"
    END_TIME=$(date +%s%3N)
    HADOOP_TIME=$(LC_ALL=C printf "%d" "$((END_TIME - START_TIME))")
    echo "  Hadoop finished in ${HADOOP_TIME}ms"
    echo "$HADOOP_TIME,$NODE_SIZE,hadoop,$NODE_DEGREE" >> "$REPORT_FILE"

    # Cleanup temp data for this size
    rm "$INPUT_FILE"
done

stop_cluster

echo "-------------------------------------------"
echo "Benchmark complete."
echo "CSV Report: $REPORT_FILE"
echo "Detailed Logs: ./logs/"
echo "-------------------------------------------"
