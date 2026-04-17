package graph

import (
	"math"
	"testing"
)

func TestPageRankConvergence(t *testing.T) {
	// Simple graph: A -> B, B -> A
	adj := AdjacencyList{
		"A": &Node{ID: "A", Rank: 0.5, OutLinks: []NodeID{"B"}},
		"B": &Node{ID: "B", Rank: 0.5, OutLinks: []NodeID{"A"}},
	}

	dampingFactor := 0.85
	tolerance := 0.0001
	maxIterations := 100

	result := PageRank(adj, dampingFactor, tolerance, maxIterations)

	if result.Iterations == 0 {
		t.Error("expected iterations > 0")
	}

	// In a symmetric graph, ranks should be equal
	if math.Abs(result.Ranks["A"]-result.Ranks["B"]) > tolerance {
		t.Errorf("expected equal ranks for symmetric graph, got A:%f, B:%f", result.Ranks["A"], result.Ranks["B"])
	}

	// Sum of ranks should be 1.0
	sum := 0.0
	for _, rank := range result.Ranks {
		sum += rank
	}
	if math.Abs(sum-1.0) > 0.0001 {
		t.Errorf("expected sum of ranks to be 1.0, got %f", sum)
	}
}

func TestPageRankDanglingNode(t *testing.T) {
	// Graph with dangling node: A -> B, B (no links)
	adj := AdjacencyList{
		"A": &Node{ID: "A", Rank: 0.5, OutLinks: []NodeID{"B"}},
		"B": &Node{ID: "B", Rank: 0.5, OutLinks: []NodeID{}},
	}

	result := PageRank(adj, 0.85, 0.0001, 100)

	sum := 0.0
	for _, rank := range result.Ranks {
		sum += rank
	}
	if math.Abs(sum-1.0) > 0.0001 {
		t.Errorf("expected sum of ranks to be 1.0 with dangling node, got %f", sum)
	}
}
