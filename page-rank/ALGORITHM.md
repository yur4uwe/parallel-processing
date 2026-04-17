# PageRank Algorithm (Serial Implementation Guide)

This document describes the PageRank algorithm, broken down for a serial implementation in Go. It serves as a stepping stone toward the MapReduce version.

## 1. Mathematical Foundation

The core PageRank formula calculates the rank (importance) of a node based on the nodes that link to it. Instead of complex mathematical notation, here is the formula represented in Python:

```python
def calculate_pagerank(node, damping_factor, incoming_nodes, current_ranks, graph):
    """
    node: The current node we are calculating the rank for
    damping_factor: Usually 0.85
    incoming_nodes: A list of nodes that link to `node`
    current_ranks: A dictionary of the current ranks of all nodes
    graph: A dictionary mapping a node to its outgoing links
    """
    rank_sum = 0
    for in_node in incoming_nodes:
        # Get the number of outgoing links from the incoming node
        outbound_link_count = len(graph[in_node])
        
        # Add the portion of the incoming node's rank
        rank_sum += current_ranks[in_node] / outbound_link_count
        
    # Apply the damping factor formula
    new_rank = (1 - damping_factor) + (damping_factor * rank_sum)
    return new_rank
```

- **damping_factor**: Typically `0.85`. This represents the probability that a hypothetical random surfer clicks a link on the current page. The `(1 - damping_factor)` portion represents the probability they get bored and type a random new URL.

## 2. Data Structures in Go

For your Go implementation, you will need two main data structures:

1.  **Graph Representation (Adjacency List)**: 
    - A Go map where the key is a Node ID (string) and the value is a slice of destination Node IDs it links to.
    - `graph := map[string][]string{"A": {"B", "C"}, "B": {"C"}, "C": {"A"}}`

2.  **Rank Storage**: 
    - A Go map to store the current PageRank of each node.
    - `ranks := map[string]float64{"A": 1.0, "B": 1.0, "C": 1.0}`

## 3. Step-by-Step Algorithm (Pseudocode)

### Step 1: Initialization
```text
N = Total number of unique nodes in the graph
For each node in graph:
    ranks[node] = 1.0 / N  (or simply 1.0)
```

### Step 2: Iterative Processing
PageRank requires multiple passes over the data. Repeat the following process for a fixed number of iterations (e.g., 20-30 times) or until convergence.

```text
damping_factor = 0.85

Loop for maximum iterations:
    new_ranks = Create empty Map[string]float64

    // 1. Apply Damping Base
    For each node in graph:
        new_ranks[node] = 1 - damping_factor

    // 2. Distribute Ranks
    For each node in graph:
        current_rank = ranks[node]
        outgoing_links = graph[node]
        L = length(outgoing_links)

        If L > 0:
            contribution = current_rank / L
            For each destination in outgoing_links:
                new_ranks[destination] += damping_factor * contribution

        Else:
            // Handling Dangling Nodes (Nodes with no outgoing links)
            // Standard practice: distribute evenly to ALL nodes
            contribution = current_rank / Total nodes
            For every node in the entire graph:
                new_ranks[node] += damping_factor * contribution

    // 3. Commit Updates
    ranks = new_ranks

    // Optional: Check for convergence here to break early
```

### Step 3: Convergence Check
Instead of hardcoding iterations, you can stop when the ranks stabilize.
- At the end of each iteration, compare `ranks` and `new_ranks`.
- Calculate the sum of absolute differences: `sum(abs(ranks[node] - new_ranks[node]))` for all nodes.
- If this sum is less than a small threshold (e.g., `0.0001`), the algorithm has converged, and you can stop.
