#pragma once


#include "header.h"


struct Edge {
    int u, v;
    double cap, flow;

    Edge() = default;

    Edge(int u, int v, double cap) : u(u), v(v), cap(cap), flow(0) {}
};

class Dinic {
public:
    explicit Dinic(int N);

    void AddEdge(int u, int v, double cap);

    bool BFS(int S, int T);

    double DFS(int u, int T, double flow = -1);

    double MaxFlow(int S, int T);

    void DFSResidualNetwork(int u);

    std::vector<Edge> MinCut(int S, int T);

private:
    int N;
    std::vector<Edge> E;
    std::vector<std::vector<int>> G;
    std::vector<int> level, pt;
    std::vector<uint8_t> res_visited;
};