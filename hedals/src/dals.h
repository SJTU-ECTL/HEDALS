#pragma once


#include "header.h"
#include "my_abc.h"
#include "simulator.h"
#include "error.h"
#include "lac.h"
#include "dinic.h"


using Cut = std::set<int>;
using CutSet = std::set<Cut>;
using CutVec = std::vector<IntVec>;


class DALSMan {
private:
    NetMan accNet;
    unsigned sourceSeed;
    unsigned seed;
    int nFrame;
    int mode;
    int usePostProc;
    int cutSizeLim;
    double errUppBound;
    LAC_TYPE lacType;
    METR_TYPE metrType;
    DISTR_TYPE distrType;
    MAP_TYPE mapType;
    boost::mt19937 randGen;
    double oriArea;

    DALSMan(const DALSMan &);
    DALSMan(DALSMan &&);
    DALSMan & operator = (const DALSMan &);
    DALSMan & operator = (DALSMan &&);

public:
    explicit DALSMan(abc::Abc_Ntk_t * pNtk, unsigned source_seed, int n_frame, int _mode, int use_post_proc, int cut_size_lim, double err_upp_bound, LAC_TYPE lac_type, METR_TYPE metr_type, DISTR_TYPE distr_type, MAP_TYPE map_type);
    ~DALSMan();
    void Run(const std::string & outpPath);
    double SimplByCutEnum(NetMan & net, const std::string & outpPath, int round);
    double SimplByMaxFlowWithDeltErr(NetMan & net, const std::string & outpPath, int round);
    double SimplByGreedy(NetMan & net, const std::string & outpPath, int round);
    unsigned NewSeed();
    IntVec GetCritGraph(NetMan & net);
    void GetCritGraphRec(NetMan & net, AbcObj * pObj, IntVec & critGraph);
    IntVec GetTFIInCritGraph(NetMan & net, std::set <int> & critGraph, int nodeId);
    void GetTFIInCritGraphRec(NetMan & net, std::set <int> & critGraph, int nodeId, IntVec & tfi);
    IntVec GetTFOInCritGraph(NetMan & net, std::set <int> & critGraph, int nodeId);
    void GetTFOInCritGraphRec(NetMan & net, std::set <int> & critGraph, int nodeId, IntVec & tfo);
    IntVec GetMinCut(const LACMan & lacMan, const NetMan & net, const IntVec & critGraph, double baseErr, int source, int sink, int N);
    void ApplyLacs(NetMan & net, std::vector < std::shared_ptr <LAC> > & lacs, bool isVerb = true);
    void ExactSimpl(NetMan & net);
    void Eval(NetMan & net, const std::string & outpPath, double err, int round);
};


int GetCutGain(NetMan&net, const Cut& cut, const LACMan& lacMan, LAC_TYPE lacType);
double CalcCutErr(NetMan& accNet, NetMan& net, const Cut& cut, const LACMan& lacMan, LAC_TYPE lacType, unsigned seed, int nFrame, METR_TYPE metrType, DISTR_TYPE distrType);
std::vector<IntVec> TempApplyLacs(NetMan& net, std::vector<std::shared_ptr<LAC>>& lacs, LAC_TYPE lacType, bool isVerb = true);
void Recov(NetMan& net, std::vector<IntVec>& replTraces, bool isVerb = false);


class CutMan {
private:
    int nSize;
    int cutSizeLim;
    NetMan & net;
    IntVec critNodes;
    IntVec pis;
    IntVec pos;
    std::vector <IntVec> critFis;
    CutSet cuts;
    std::map<Cut, double> cut2Err;

public:
    CutMan(int cut_size_lim, NetMan & _net);
    ~CutMan() = default;
    CutMan(const CutMan &) = delete;
    CutMan(CutMan &&) = delete;
    CutMan & operator = (const CutMan &) = delete;
    CutMan & operator = (CutMan &&)  = delete;
    void BuildRec(AbcObj * pObj, std::unordered_map <int, int> & netId2CritId);
    void Print() const;
    void LimNumbOfCuts(CutSet& fcw, double refErr, NetMan& accNet, LACMan& lacMan, LAC_TYPE lacType, unsigned seed, int nFrame, METR_TYPE metrType, DISTR_TYPE distrType);
    Cut GenCutsPrunedProPlus(const DblVec& errs, double refErr, double errUppBound, NetMan& accNet, LACMan& lacMan, LAC_TYPE lacType, unsigned seed, int nFrame, METR_TYPE metrType, DISTR_TYPE distrType, const Cut& refCut);
    void MergeCut(const std::set <int> & cut0, const std::set <int> & cut1, std::set <int> & resCut);
    void JoinCutSet(CutSet& fcw, const CutSet& fcu, double refErr, int critNode, const DblVec& errs);
    void JoinCutSetStrict(CutSet& fcw, const CutSet& fcu, double refErr, int critNode, NetMan& accNet, LACMan& lacMan, LAC_TYPE lacType, unsigned seed, int nFrame, METR_TYPE metrType, DISTR_TYPE distrType);
    void GetTopoRec(int targNode, std::unordered_map <int, double> & nodeFlows, std::unordered_set <int> & visNodes, IntVec & topoNodes);
    void PrintCut(const Cut& cut) const;
    void PrintCutSet(const CutSet& cutSet) const;

    inline CutSet& GetCuts() {return cuts;}
    inline IntVec& GetCritNodes() {return critNodes;}
    inline int GetNode(int id) const {return critNodes[id];}
    inline double GetCutErr(const Cut& cut) {assert(cut2Err.count(cut)); return cut2Err[cut];}
};