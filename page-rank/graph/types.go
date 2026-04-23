package graph

type NodeID string

type Node struct {
	ID       NodeID
	Rank     float64
	OutLinks []NodeID
}

type AdjacencyList map[NodeID]*Node

func (a *AdjacencyList) UpdateRanks(newRanks map[NodeID]float64) {
	for nodeID, newRank := range newRanks {
		if node, ok := (*a)[nodeID]; ok {
			node.Rank = newRank
		}
	}
}

// CalculateContributions returns how much rank this node gives to its neighbors.
func (n *Node) CalculateContributions() map[NodeID]float64 {
	if len(n.OutLinks) == 0 {
		return nil
	}
	contribs := make(map[NodeID]float64)
	c := n.Rank / float64(len(n.OutLinks))
	for _, target := range n.OutLinks {
		contribs[target] += c
	}
	return contribs
}

// IsDangling returns true if the node has no outgoing links.
func (n *Node) IsDangling() bool {
	return len(n.OutLinks) == 0
}
