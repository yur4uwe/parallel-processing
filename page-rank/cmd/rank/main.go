package main

import (
	"fmt"
	"os"
	"page-rank/format"
	"page-rank/graph"
	"sort"
)

func main() {
	fileName := os.Args[1]
	dampingFactor := 0.85
	tolerance := 0.0001
	maxIterations := 20

	f, err := os.Open(fileName)
	if err != nil {
		panic(err)
	}
	defer f.Close()

	adj, err := format.ParseText(f, '\t', false)
	if err != nil {
		panic(err)
	}

	result := graph.PageRank(adj, dampingFactor, tolerance, maxIterations)

	fmt.Printf("Converged in %d iterations\n", result.Iterations)
	fmt.Println("Top 10 Nodes:")

	type pair struct {
		ID   graph.NodeID
		Rank float64
	}
	var sorted []pair
	for id, rank := range result.Ranks {
		sorted = append(sorted, pair{id, rank})
	}
	sort.Slice(sorted, func(i, j int) bool {
		return sorted[i].Rank > sorted[j].Rank
	})

	for i := 0; i < len(sorted) && i < 10; i++ {
		fmt.Printf("%d. %s: %.6f\n", i+1, sorted[i].ID, sorted[i].Rank)
	}
}
