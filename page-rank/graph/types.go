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
		(*a)[nodeID].Rank = newRank
	}
}
