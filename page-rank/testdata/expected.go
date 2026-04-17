package testdata

import (
	"page-rank/graph"
	"testing"
)

func CompareAdjacencyLists(t *testing.T, expected, actual graph.AdjacencyList) {
	t.Helper()
	if len(expected) != len(actual) {
		t.Fatalf("expected %d nodes, got %d", len(expected), len(actual))
	}

	for nodeID, expectedNode := range expected {
		actualNode, ok := actual[nodeID]
		if !ok {
			t.Fatalf("expected nodeID %s, got %v", nodeID, actual)
		}

		if expectedNode.Rank != actualNode.Rank {
			t.Fatalf("expected node %s to have rank %f, got %f", nodeID, expectedNode.Rank, actualNode.Rank)
		}

		if len(expectedNode.OutLinks) != len(actualNode.OutLinks) {
			t.Fatalf("expected node %s to have %d outgoing links, got %d\nExpected links: %v\nActual links: %v",
				nodeID, len(expectedNode.OutLinks), len(actualNode.OutLinks),
				expectedNode.OutLinks, actualNode.OutLinks)
		}

		for i, expectedLink := range expectedNode.OutLinks {
			actualLink := actualNode.OutLinks[i]
			if expectedLink != actualLink {
				t.Fatalf("expected node %s to have outgoing link %s, got %s",
					nodeID, expectedLink, actualLink)
			}
		}
	}
}

func GetAdjacencyList(t *testing.T, fileName string) graph.AdjacencyList {
	t.Helper()

	var expected graph.AdjacencyList
	switch fileName {
	case "disconnected.txt":
		expected = ExpectedDisconnected
	case "linear-graph.txt":
		expected = ExpectedLinearGraph
	case "star-graph.txt":
		expected = ExpectedStarGraph
	case "tiny-web.txt":
		expected = ExpectedTinyWeb
	}

	return expected
}

var ExpectedDisconnected = graph.AdjacencyList{
	"A": graph.Node{
		ID:       "A",
		Rank:     1.0,
		OutLinks: []graph.NodeID{"B"},
	},
	"B": graph.Node{
		ID:       "B",
		Rank:     1.0,
		OutLinks: []graph.NodeID{"A"},
	},
	"C": graph.Node{
		ID:       "C",
		Rank:     1.0,
		OutLinks: []graph.NodeID{"D"},
	},
	"D": graph.Node{
		ID:       "D",
		Rank:     1.0,
		OutLinks: []graph.NodeID{"C"},
	},
}

var ExpectedLinearGraph = graph.AdjacencyList{
	"A": graph.Node{
		ID:       "A",
		Rank:     1.0,
		OutLinks: []graph.NodeID{"B"},
	},
	"B": graph.Node{
		ID:       "B",
		Rank:     1.0,
		OutLinks: []graph.NodeID{"C"},
	},
	"C": graph.Node{
		ID:       "C",
		Rank:     1.0,
		OutLinks: []graph.NodeID{"D"},
	},
	"D": graph.Node{
		ID:       "D",
		Rank:     1.0,
		OutLinks: []graph.NodeID{},
	},
}

var ExpectedStarGraph = graph.AdjacencyList{
	"A": graph.Node{
		ID:       "A",
		Rank:     1.0,
		OutLinks: []graph.NodeID{"D"},
	},
	"B": graph.Node{
		ID:       "B",
		Rank:     1.0,
		OutLinks: []graph.NodeID{"D"},
	},
	"C": graph.Node{
		ID:       "C",
		Rank:     1.0,
		OutLinks: []graph.NodeID{"D"},
	},
	"D": graph.Node{
		ID:       "D",
		Rank:     1.0,
		OutLinks: []graph.NodeID{},
	},
}

var ExpectedTinyWeb = graph.AdjacencyList{
	"A": graph.Node{
		ID:       "A",
		Rank:     1.0,
		OutLinks: []graph.NodeID{"B", "C"},
	},
	"B": graph.Node{
		ID:       "B",
		Rank:     1.0,
		OutLinks: []graph.NodeID{"D"},
	},
	"C": graph.Node{
		ID:       "C",
		Rank:     1.0,
		OutLinks: []graph.NodeID{"A", "B", "D"},
	},
	"D": graph.Node{
		ID:       "D",
		Rank:     1.0,
		OutLinks: []graph.NodeID{"C"},
	},
}
