package main

import (
	"bufio"
	"flag"
	"fmt"
	"math/rand"
	"os"
	"strings"
	"time"
)

func main() {
	nodes := flag.Int("nodes", 1000, "Number of nodes")
	edgesPerNode := flag.Int("edges", 10, "Average edges per node")
	output := flag.String("output", "data.txt", "Output file")
	flag.Parse()

	rand.New(rand.NewSource(time.Now().UnixNano()))

	f, err := os.Create(*output)
	if err != nil {
		fmt.Printf("Error creating file: %v\n", err)
		return
	}
	defer f.Close()

	writer := bufio.NewWriterSize(f, 1024*1024) // 1MB buffer
	defer writer.Flush()

	rank := 1.0 / float64(*nodes)

	for i := 0; i < *nodes; i++ {
		// Use a builder to avoid multiple strings.Join allocations
		var line strings.Builder
		fmt.Fprintf(&line, "Node%d\t%.10f\t", i, rank)

		numEdges := rand.Intn(*edgesPerNode * 2)
		for j := range numEdges {
			targetIdx := rand.Intn(*nodes)
			if targetIdx == i {
				continue // Simple self-loop skip
			}
			fmt.Fprintf(&line, "Node%d", targetIdx)
			if j < numEdges-1 {
				line.WriteByte(',')
			}
		}
		line.WriteByte('\n')
		writer.WriteString(line.String())

		if i%100000 == 0 && i > 0 {
			fmt.Printf("Generated %d nodes...\n", i)
		}
	}
}
