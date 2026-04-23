package main

import (
	"bufio"
	"fmt"
	"os"
	"page-rank/format"
	"page-rank/graph"
	"strings"
)

func main() {
	scanner := bufio.NewScanner(os.Stdin)
	for scanner.Scan() {
		line := scanner.Text()
		if strings.TrimSpace(line) == "" {
			continue
		}

		node, err := format.ParseLine(line, '\t')
		if err != nil {
			fmt.Printf("Error: %s\n", err)
			continue
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
