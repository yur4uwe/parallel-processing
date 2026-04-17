import sys
import json
from typing import TypedDict, cast
import networkx as nx
import matplotlib.pyplot as plt


class PageRankData(TypedDict):
    iterations: int
    ranks: dict[str, float]
    graph: dict[str, list[str] | None]


if len(sys.argv) < 2:
    print("Usage: python visualize.py <results.json>")
    sys.exit(1)

with open(sys.argv[1], "r") as f:
    data = cast(PageRankData, json.load(f))

ranks = data["ranks"]
graph_data = data["graph"]

G = nx.DiGraph()
for node, edges in graph_data.items():
    G.add_node(node, rank=ranks.get(node, 0))

    if edges is None:
        continue

    for target in edges:
        G.add_edge(node, target)

# Use PageRank values for node sizes
# Scale them up for visibility
node_sizes = [ranks.get(node, 0) * 5000 for node in G.nodes()]

# Use PageRank values for node colors
node_colors = [ranks.get(node, 0) for node in G.nodes()]

fig, ax = plt.subplots(figsize=(10, 8))
pos = nx.spring_layout(G)

nodes = nx.draw_networkx_nodes(
    G,
    pos,
    node_size=node_sizes,
    node_color=node_colors,
    cmap=plt.cm.viridis,
    alpha=0.8,
    ax=ax,
)
nx.draw_networkx_edges(
    G,
    pos,
    arrowstyle="->",
    arrowsize=20,
    edge_color="gray",
    width=1.5,
    alpha=0.5,
    ax=ax,
    node_size=node_sizes,
)
nx.draw_networkx_labels(G, pos, font_size=12, font_family="sans-serif", ax=ax)

plt.title(f"PageRank Visualization (Converged in {data['iterations']} iterations)")
plt.colorbar(nodes, ax=ax, label="PageRank")
plt.axis("off")

output_file = "pagerank_visualization.png"
plt.savefig(output_file)
print(f"Visualization saved to {output_file}")
plt.show()
