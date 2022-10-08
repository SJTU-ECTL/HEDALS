#pragma once


#include "simulator.h"
#include "my_abc.h"
#include "lac.h"


enum class METR_TYPE{
    ER, MED, MRED, MSE, MHD
};


enum class DISTR_TYPE {
    UNIF, ENUM
};


class ErrMan {
private:
    Simulator smlt0;
    Simulator smlt1;
    unsigned seed;
    int nFrame;
    DISTR_TYPE distrType;

public:
    ErrMan(NetMan & netMan0, NetMan & netMan1, unsigned _seed, int n_frame, DISTR_TYPE distr_type);
    ~ErrMan() = default;
    ErrMan(const ErrMan &) = delete;
    ErrMan(ErrMan &&) = delete;
    ErrMan & operator = (const ErrMan &) = delete;
    ErrMan & operator = (ErrMan &&)  = delete;

    double CalcErrRate();
    double CalcMeanErrDist();
    double CalcMeanHammDist();

    inline double GetErrRate() const {return smlt0.GetErrRate(smlt1);}
    inline double GetMeanErrDist() const {return smlt0.GetMeanErrDist(smlt1);}
    inline double GetMeanHammDist() const {return smlt0.GetMeanHammDist(smlt1);}
};


static inline double CalcErr(NetMan & netMan0, NetMan & netMan1, unsigned seed, int nFrame, METR_TYPE metrType, DISTR_TYPE distrType) {
    ErrMan errMan(netMan0, netMan1, seed, nFrame, distrType);
    if (metrType == METR_TYPE::ER)
        return errMan.CalcErrRate();
    else if (metrType == METR_TYPE::MED)
        return errMan.CalcMeanErrDist();
    else if (metrType == METR_TYPE::MHD)
        return errMan.CalcMeanHammDist();
    else {
        assert(0);
        return 0;
    }
}


class VECBEEMan {
private:
    Simulator accSmlt;
    Simulator appSmlt;
    unsigned seed;
    int nFrame;
    METR_TYPE metrType;
    LAC_TYPE lacType;
    DISTR_TYPE distrType;
    std::vector<std::vector<BitVec>> bdPo2Nodes; // bdPo2Nodes[poId][nodeId][bitId], whether the node changing on the "nodeId"-th node can be propagated to the "poId"-th PO under the "bitId"-th input pattern
    std::vector<std::vector<BitVec>> bdCut2Nodes;
    std::vector<ObjList> disjCuts;
    std::vector<ObjVec> cutNtks;
    std::vector<BitVec> poMarks;
    std::vector<std::vector<BitVec>> poChangeWithNode; // poChangeWithNode[poId][nodeId][bitId], whether the candidate LAC on the "nodeId"-th node affects the "poId"-th PO under the "bitId"-th input pattern
    IntVec topoIds;

public:
    // VECBEEMan() = default;
    VECBEEMan(NetMan& accNet, NetMan& appNet, unsigned _seed, int n_frame, METR_TYPE metr_type, LAC_TYPE lac_type, DISTR_TYPE distr_type);
    ~VECBEEMan() = default;
    VECBEEMan(const VECBEEMan &) = delete;
    VECBEEMan(VECBEEMan &&) = delete;
    VECBEEMan & operator = (const VECBEEMan &) = delete;
    VECBEEMan & operator = (VECBEEMan &&)  = delete;

    // void BatchErrEst(NetMan & accNet, NetMan & appNet, LACMan & lacMan);
    void BatchErrEstNew(LACMan& lacMan, IntVec& critGraph);
    void FindDisjCut(NetMan & net, std::vector <abc::Abc_Obj_t *> & topoNodes);
    void FindAppDisjCut(NetMan & net);
    void FindDisjCutOfNode(abc::Abc_Obj_t * pObj, std::list <abc::Abc_Obj_t *> & disjCut);
    void ExpandCut(abc::Abc_Obj_t * pObj, std::list <abc::Abc_Obj_t *> & disjCut);
    abc::Abc_Obj_t * ExpandWhich(std::list <abc::Abc_Obj_t *> & disjCut);
    void CalcBoolDiffCut2Node(ObjVec& topoNodes);
    void CalcBoolDiffPo2Node(ObjVec& topoNodes);
    void CalcLACErrs(LACMan& lacMan);
    void CalcChanges(LACMan& lacMan, IntVec& critGraph);
    void GetNewValue(Simulator & smlt, const IntVec& faninIds, const std::string & sop, boost::dynamic_bitset <ull> & value);

    inline Simulator& GetAccSmlt() {return accSmlt;}
    inline Simulator& GetAppSmlt() {return appSmlt;}
    inline int GetFrameNum() {return nFrame;}
    inline std::vector<std::vector<BitVec>>& GetPoChangeWithNode() {return poChangeWithNode;}
    inline METR_TYPE GetMetrType() {return metrType;}
};