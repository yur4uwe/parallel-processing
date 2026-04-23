package main

import (
	"bufio"
	"flag"
	"fmt"
	"os"
	"page-rank/graph"
	"strconv"
	"strings"
)

func main() {
	damping := flag.Float64("damping", 0.85, "Damping factor")
	totalNodes := flag.Int("total", 1, "Total number of nodes (N)")
	danglingMass := flag.Float64("danglingMass", 0.0, "Total rank from dangling nodes")
	flag.Parse()

	var currentID string
	var currentSum float64
	var currentLinks string

	scanner := bufio.NewScanner(os.Stdin)
	for scanner.Scan() {
		line := scanner.Text()
		parts := strings.Split(line, "\t")
		if len(parts) < 3 {
			continue
		}

		nodeID := parts[0]
		typ := parts[1]
		val := parts[2]

		if nodeID != currentID && currentID != "" {
			emit(currentID, currentSum, currentLinks, *damping, *totalNodes, *danglingMass)
			currentSum = 0
			currentLinks = ""
		}

		currentID = nodeID

		switch typ {
		case "STRUCT":
			currentLinks = val
		case "VOTE":
			v, _ := strconv.ParseFloat(val, 64)
			currentSum += v
		}
	}

	if currentID != "" {
		emit(currentID, currentSum, currentLinks, *damping, *totalNodes, *danglingMass)
	}
}

func emit(id string, sum float64, links string, d float64, n int, dMass float64) {
	if id == "__DANGLING__" {
		fmt.Printf("__DANGLING__\t%.10f\t\n", sum)
		return
	}

	// USE SERIAL FORMULA
	newRank := graph.ComputeNewRank(sum, dMass, d, n)
	fmt.Printf("%s\t%.10f\t%s\n", id, newRank, links)
}
