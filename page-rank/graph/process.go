package graph

import "math"

func CheckConvergence(adj AdjacencyList, newRanks map[NodeID]float64, tol float64) bool {
	absError := 0.0
	for nodeID, newRank := range newRanks {
		actualRank := adj[nodeID].Rank
		absError += math.Abs(actualRank - newRank)
	}

	if absError < tol {
		return true
	}

	return false
}

func RankNodes(adj AdjacencyList, dampingFactor float64, tolerance float64) map[NodeID]float64 {
	ranks := make(map[NodeID]float64)
	baseRank := (1.0 - dampingFactor) / float64(len(adj))
	for nodeID := range adj {
		ranks[nodeID] = baseRank
	}

	for _, node := range adj {
		links_count := len(node.OutLinks)

		if links_count > 0 {
			contribution := node.Rank / float64(links_count)
			for _, dest := range node.OutLinks {
				ranks[dest] += dampingFactor * contribution
			}
		} else {
			// Handling Dangling Nodes (Nodes with no outgoing links)
			// Standard practice: distribute evenly to ALL nodes
			contribution := node.Rank / float64(len(adj))
			for _, node := range adj {
				ranks[node.ID] += dampingFactor * contribution
			}
		}
	}

	return ranks
}

type Result struct {
	Ranks      map[NodeID]float64
	Iterations int
}

func PageRank(adj AdjacencyList, dampingFactor float64, tolerance float64, maxIterations int) Result {
	var lastRanks map[NodeID]float64
	iterations := 0
	for range maxIterations {
		iterations++
		lastRanks = RankNodes(adj, dampingFactor, tolerance)

		converges := CheckConvergence(adj, lastRanks, tolerance)

		adj.UpdateRanks(lastRanks)

		if converges {
			break
		}
	}
	return Result{
		Ranks:      lastRanks,
		Iterations: iterations,
	}
}
