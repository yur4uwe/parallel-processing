package main

import (
	"bufio"
	"fmt"
	"os"
	"page-rank/graph"
	"strconv"
	"strings"
)

func main() {
	scanner := bufio.NewScanner(os.Stdin)
	for scanner.Scan() {
		line := scanner.Text()
		if strings.TrimSpace(line) == "" {
			continue
		}

		parts := strings.Split(line, "\t")
		if len(parts) < 2 {
			continue
		}

		nodeID := graph.NodeID(parts[0])
		rank, _ := strconv.ParseFloat(parts[1], 64)
		var links []graph.NodeID
		if len(parts) > 2 && parts[2] != "" {
			for l := range strings.SplitSeq(parts[2], ",") {
				links = append(links, graph.NodeID(strings.TrimSpace(l)))
			}
		}

		node := &graph.Node{
			ID:       nodeID,
			Rank:     rank,
			OutLinks: links,
		}

		// 1. Pass Structure
		linksStr := strings.Join(toStrings(node.OutLinks), ",")
		fmt.Printf("%s\tSTRUCT\t%s\n", node.ID, linksStr)

		// 2. Distribute Rank using serial logic
		if node.IsDangling() {
			fmt.Printf("__DANGLING__\tVOTE\t%.10f\n", node.Rank)
		} else {
			contribs := node.CalculateContributions()
			for target, val := range contribs {
				fmt.Printf("%s\tVOTE\t%.10f\n", target, val)
			}
		}
	}
}

func toStrings(ids []graph.NodeID) []string {
	res := make([]string, len(ids))
	for i, id := range ids {
		res[i] = string(id)
	}
	return res
}
