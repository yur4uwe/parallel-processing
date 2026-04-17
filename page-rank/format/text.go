package format

import (
	"bufio"
	"fmt"
	"io"
	"page-rank/graph"
	"strconv"
	"strings"
)

func ParseText(r io.Reader, delimiter rune, skipFirstLine bool) (graph.AdjacencyList, error) {
	scanner := bufio.NewScanner(r)
	if skipFirstLine {
		scanner.Scan()
	}

	list := make(graph.AdjacencyList)
	for scanner.Scan() {
		line := scanner.Text()
		fields := strings.Split(line, string(delimiter))
		if len(fields) != 3 {
			return nil, fmt.Errorf("expected 3 fields, got %d", len(fields))
		}

		nodeID := graph.NodeID(fields[0])
		rank, err := strconv.ParseFloat(fields[1], 64)
		if err != nil {
			return nil, fmt.Errorf("error parsing rank: %v", err)
		}
		var outgoingLinks []graph.NodeID
		if fields[2] != "" {
			parts := strings.Split(fields[2], ",")
			outgoingLinks = make([]graph.NodeID, len(parts))
			for i, link := range parts {
				outgoingLinks[i] = graph.NodeID(link)
			}
		}

		list[nodeID] = &graph.Node{
			ID:       nodeID,
			Rank:     rank,
			OutLinks: outgoingLinks,
		}
	}

	return list, nil
}
