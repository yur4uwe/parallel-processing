package graph

import "math"

// ComputeNewRank applies the PageRank formula.
// sum: total contributions from neighbors
// danglingMass: total rank from all dangling nodes in the graph
func ComputeNewRank(sum, danglingMass float64, damping float64, totalNodes int) float64 {
	return (1.0-damping)/float64(totalNodes) + damping*(sum+danglingMass/float64(totalNodes))
}

func CheckConvergence(adj AdjacencyList, newRanks map[NodeID]float64, tol float64) bool {
	absError := 0.0
	for nodeID, newRank := range newRanks {
		if node, ok := adj[nodeID]; ok {
			absError += math.Abs(node.Rank - newRank)
		}
	}
	return absError < tol
}

func RankNodes(adj AdjacencyList, dampingFactor float64, tolerance float64) map[NodeID]float64 {
	ranks := make(map[NodeID]float64)
	
	// Calculate total dangling mass first
	danglingMass := 0.0
	for _, node := range adj {
		if node.IsDangling() {
			danglingMass += node.Rank
		}
	}

	// Calculate sums of contributions for each node
	sums := make(map[NodeID]float64)
	for _, node := range adj {
		contribs := node.CalculateContributions()
		for target, val := range contribs {
			sums[target] += val
		}
	}

	// Apply formula to all nodes
	for id := range adj {
		ranks[id] = ComputeNewRank(sums[id], danglingMass, dampingFactor, len(adj))
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
