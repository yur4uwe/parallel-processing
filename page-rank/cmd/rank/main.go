package main

import (
	"encoding/json"
	"flag"
	"fmt"
	"os"
	"page-rank/format"
	"page-rank/graph"
	"sort"
)

func main() {
	jsonOut := flag.String("json", "", "output in JSON format")
	dampingFactor := flag.Float64("damping", 0.85, "damping factor")
	tolerance := flag.Float64("tol", 0.0001, "convergence tolerance")
	maxIterations := flag.Int("iter", 20, "max iterations")
	flag.Parse()

	args := flag.Args()
	if len(args) < 1 {
		fmt.Println("Usage: rank [-json] [-damping d] [-tol t] [-iter n] <input_file>")
		os.Exit(1)
	}
	fileName := args[0]

	f, err := os.Open(fileName)
	if err != nil {
		panic(err)
	}
	defer f.Close()

	adj, err := format.ParseText(f, '\t', false)
	if err != nil {
		panic(err)
	}

	result := graph.PageRank(adj, *dampingFactor, *tolerance, *maxIterations)

	if *jsonOut != "" {
		output := struct {
			Iterations int                             `json:"iterations"`
			Ranks      map[graph.NodeID]float64        `json:"ranks"`
			Graph      map[graph.NodeID][]graph.NodeID `json:"graph"`
		}{
			Iterations: result.Iterations,
			Ranks:      result.Ranks,
			Graph:      make(map[graph.NodeID][]graph.NodeID),
		}
		for id, node := range adj {
			output.Graph[id] = node.OutLinks
		}

		jsonFile, err := os.Create(*jsonOut)
		if err != nil {
			panic(err)
		}
		defer jsonFile.Close()

		enc := json.NewEncoder(jsonFile)
		enc.SetIndent("", "  ")
		enc.Encode(output)
		return
	}

	fmt.Printf("Converged after %d iterations\n", result.Iterations)
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
