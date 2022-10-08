#include "dals.h"


using namespace abc;
using std::cout;
using std::endl;
using std::max;
using std::min;
using std::numeric_limits;
using std::ostringstream;
using std::pair;
using std::set;
using std::shared_ptr;
using std::sort;
using std::string;
using std::unordered_map;
using std::vector;


DALSMan::DALSMan(abc::Abc_Ntk_t* pNtk, unsigned source_seed, int n_frame, int _mode, int use_post_proc, int cut_size_lim, double err_upp_bound, LAC_TYPE lac_type, METR_TYPE metr_type, DISTR_TYPE distr_type, MAP_TYPE map_type):
    accNet(NetMan(pNtk, true)), sourceSeed(source_seed), nFrame(n_frame), mode(_mode), usePostProc(use_post_proc), cutSizeLim(cut_size_lim), errUppBound(err_upp_bound), lacType(lac_type), metrType(metr_type), distrType(distr_type), mapType(map_type) {
    randGen.seed(sourceSeed);
    cout << "source seed = " << sourceSeed << endl;
    seed = NewSeed();
}


DALSMan::~DALSMan() {
}


void DALSMan::Run(const string & outpPath) {
    auto currNet = accNet;
    int round = 0;
    double err = 0.0;

    cout << "network representation: " << accNet.GetNetType() << endl;
    cout << "#simulation patterns = " << nFrame << endl;
    cout << "size limit of priority cut " << cutSizeLim << endl;
    cout << "initial error = " << err << endl;
    oriArea = accNet.GetArea();
    Eval(currNet, outpPath, err, round);

    boost::timer::cpu_timer timer;
    NetMan backNet;
    double backErr = 0.0;
    while (DoubleLessEqual(err, errUppBound)) {
        cout << "----------------- round " << ++round << "----------------- " << endl;
        backNet = currNet, backErr = err;
        if (mode == 0)
            err = SimplByCutEnum(currNet, outpPath, round);
        else if (mode == 1)
            err = SimplByMaxFlowWithDeltErr(currNet, outpPath, round);
        else
            assert(0);
        cout << "runtime = " << timer.format(3, "%t sec") << endl;
        cout << endl;
    }
    if (!usePostProc)
        return;
    currNet = backNet, err = backErr;
    while (DoubleLessEqual(err, errUppBound)) {
        cout << "----------------- post-processing round " << ++round << "----------------- " << endl;
        err = SimplByGreedy(currNet, outpPath, round);
        cout << "runtime = " << timer.format(3, "%t sec") << endl;
        cout << endl;
    }
}


double DALSMan::SimplByCutEnum(NetMan & net, const std::string & outpPath, int round) {
    // initialize
    cout << "*****SimplByCutEnum*****" << endl;
    double backErr = CalcErr(accNet, net, seed, nFrame, metrType, distrType);
    cout << "base error = " << backErr << endl;
    net.CreateConst(true);

    // get critical graph
    auto critGraph = GetCritGraph(net);
    if (critGraph.size() == 0 || net.GetLev() <= 1) {
        cout << "Early termination due to small network level" << endl;
        // return (errUppBound + EPSILON * 2);
        return numeric_limits<double>::max();
    }

    // generate LACs
    LACMan lacMan;
    if (lacType == LAC_TYPE::CONS)
        lacMan.GenConstLACs(net, critGraph);
    else if (lacType == LAC_TYPE::SASIMI)
        lacMan.GenSasimiLACsAll(net, critGraph);
    else if (lacType == LAC_TYPE::RAC)
        lacMan.GenRacLACsNew(net, critGraph, seed);
    else   
        assert(0);
    cout << "#lacs = " << lacMan.GetLacNum() << endl;

    // estimate error
    VECBEEMan vecbeeMan(accNet, net, seed, nFrame, metrType, lacType, distrType);
    vecbeeMan.BatchErrEstNew(lacMan, critGraph);

    // find target LACs
    int bestNodeId = lacMan.GetBestId();
    assert(bestNodeId != -1);
    IntVec targNodes;
    bool negCap = false;
    if ((lacMan.GetCandLac(bestNodeId)->GetErr() - backErr) < 0) {
        cout << "deal with negative capacity: " << bestNodeId << ", " << lacMan.GetCandLac(bestNodeId)->GetErr() << endl;
        targNodes.emplace_back(bestNodeId);
        negCap = true;
    }
    else {
        int N = net.GetIdMaxPlus1();
        targNodes = GetMinCut(lacMan, net, critGraph, backErr, 0, N, N);
    }

    Cut bestCut;
    if (!negCap) {
        // get reference error
        DblVec errs(net.GetIdMaxPlus1(), 0);
        for (int id: critGraph) {
            if (!net.IsNode(id) || net.IsConst(id))
                continue;
            errs[id] = lacMan.GetCandLac(id)->GetErr();
        }
        double estErr = -numeric_limits<double>::max();
        for (int id: targNodes)
            estErr = max(errs[id], estErr);
        assert(estErr != -numeric_limits<double>::max());
        vector < std::shared_ptr <LAC> > testLacs;
        for (const auto & cutNode: targNodes)
            testLacs.emplace_back(lacMan.GetCandLac(cutNode));
        auto replTraces = TempApplyLacs(net, testLacs, lacType, false);
        double refErr = CalcErr(accNet, net, seed, nFrame, metrType, distrType);
        cout << "error obtained by maximum flow = " << refErr << endl;
        refErr = max(estErr, refErr);
        cout << "reference error = " << refErr << endl;
        Recov(net, replTraces, false);
        // net.Show(critGraph, "tmp/test.dot", targNodes, errs, refErr);

        // try cut enumeration
        CutMan cutMan(cutSizeLim, net);
        bestCut = cutMan.GenCutsPrunedProPlus(errs, refErr, errUppBound, accNet, lacMan, lacType, seed, nFrame, metrType, distrType, Cut(targNodes.begin(), targNodes.end()));
    }
    else
        bestCut = Cut(targNodes.begin(), targNodes.end());

    // apply best LACs
    vector < std::shared_ptr <LAC> > bestLacs;
    for (const auto & cutNode: bestCut)
        bestLacs.emplace_back(lacMan.GetCandLac(cutNode));
    ApplyLacs(net, bestLacs);
    double err = CalcErr(accNet, net, seed, nFrame, metrType, distrType);
    cout << "current error = " << err << endl;

    // measure, synthesis & mapping, output
    if (DoubleLessEqual(err, errUppBound)) {
        ExactSimpl(net);
        Eval(net, outpPath, err, round);
    }

    // return error
    return err;
}


double DALSMan::SimplByMaxFlowWithDeltErr(NetMan & net, const string & outpPath, int round) {
    // initialize
    cout << "*****SimplByMaxFlowWithDeltErr*****" << endl;
    double backErr = CalcErr(accNet, net, seed, nFrame, metrType, distrType);
    cout << "base error = " << backErr << endl;
    net.CreateConst(true);

    // get critical graph
    auto critGraph = GetCritGraph(net);
    if (critGraph.size() == 0 || net.GetLev() <= 1) {
        cout << "Early termination due to small network level" << endl;
        return numeric_limits<double>::max();
    }

    // generate LACs
    LACMan lacMan;
    if (lacType == LAC_TYPE::CONS)
        lacMan.GenConstLACs(net, critGraph);
    else if (lacType == LAC_TYPE::SASIMI)
        lacMan.GenSasimiLACsAll(net, critGraph);
    else if (lacType == LAC_TYPE::RAC)
        lacMan.GenRacLACsNew(net, critGraph, seed);
    else   
        assert(0);
    cout << "#lacs = " << lacMan.GetLacNum() << endl;

    // estimate error
    VECBEEMan vecbeeMan(accNet, net, seed, nFrame, metrType, lacType, distrType);
    vecbeeMan.BatchErrEstNew(lacMan, critGraph);

    // find target LACs
    lacMan.GenCandLacs(critGraph);
    int bestNodeId = lacMan.GetBestId();
    assert(bestNodeId != -1);
    IntVec targNodes;
    if ((lacMan.GetCandLac(bestNodeId)->GetErr() - backErr) < 0) {
        cout << "deal with negative capacity: " << bestNodeId << ", " << lacMan.GetCandLac(bestNodeId)->GetErr() << endl;
        targNodes.emplace_back(bestNodeId);
    }
    else {
        int N = net.GetIdMaxPlus1();
        targNodes = GetMinCut(lacMan, net, critGraph, backErr, 0, N, N);
    }
    vector < std::shared_ptr <LAC> > bestLacs;
    for (const auto & cutNode: targNodes)
        bestLacs.emplace_back(lacMan.GetCandLac(cutNode));

    // apply target LACs
    ApplyLacs(net, bestLacs);
    double err = CalcErr(accNet, net, seed, nFrame, metrType, distrType);
    cout << "current error = " << err << endl;

    // measure, synthesis & mapping, output
    if (DoubleLessEqual(err, errUppBound)) {
        ExactSimpl(net);
        Eval(net, outpPath, err, round);
    }

    // return error
    return err;
}


double DALSMan::SimplByGreedy(NetMan & net, const string & outpPath, int round) {
    // initialize
    cout << "*****SimplByGreedy *****" << endl;
    double backErr = CalcErr(accNet, net, seed, nFrame, metrType, distrType);
    cout << "base error = " << backErr << endl;
    net.CreateConst(true);

    // collect nodes in the whole graph
    auto wholeGraph = net.CalcTopoOrdOfIds();
    if (wholeGraph.size() == 0 || net.GetLev() <= 1) {
        cout << "Early termination due to small network level" << endl;
        return numeric_limits<double>::max();
    }

    // generate LACs
    LACMan lacMan;
    if (lacType == LAC_TYPE::CONS)
        lacMan.GenConstLACs(net, wholeGraph);
    else if (lacType == LAC_TYPE::SASIMI)
        lacMan.GenSasimiLACsAll(net, wholeGraph);
    else if (lacType == LAC_TYPE::RAC)
        lacMan.GenRacLACsNew(net, wholeGraph, seed);
    else   
        assert(0);
    cout << "#lacs = " << lacMan.GetLacNum() << endl;

    // estimate error
    VECBEEMan vecbeeMan(accNet, net, seed, nFrame, metrType, lacType, distrType);
    vecbeeMan.BatchErrEstNew(lacMan, wholeGraph);

    // find target LACs
    lacMan.GenCandLacs(wholeGraph);
    int bestNodeId = lacMan.GetBestId();
    assert(bestNodeId != -1);
    IntVec targNodes;
    cout << "deal with LAC: " << bestNodeId << ", " << lacMan.GetCandLac(bestNodeId)->GetErr() << endl;
    targNodes.emplace_back(bestNodeId);
    vector < std::shared_ptr <LAC> > bestLacs;
    for (const auto & cutNode: targNodes)
        bestLacs.emplace_back(lacMan.GetCandLac(cutNode));

    // apply target LACs
    ApplyLacs(net, bestLacs);
    double err = CalcErr(accNet, net, seed, nFrame, metrType, distrType);
    cout << "current error = " << err << endl;

    // measure, synthesis & mapping, output
    if (DoubleLessEqual(err, errUppBound)) {
        ExactSimpl(net);
        Eval(net, outpPath, err, round);
    }

    // return error
    return err;
}


unsigned DALSMan::NewSeed() {
    boost::uniform_int <> unDistr(numeric_limits <int>::min(), numeric_limits <int>::max());
    unsigned _seed = static_cast <unsigned> (unDistr(randGen));
    cout << "new seed = " << _seed << endl;
    return _seed;
}


IntVec DALSMan::GetCritGraph(NetMan & net) {
    IntVec critGraph;
    double maxDelay = net.GetDelay();
    if (DoubleEqual(maxDelay, 0.0, DELAY_TOL))
        return IntVec ();
    net.SetNetNotTrav();
    for (int i = 0; i < net.GetPoNum(); ++i) {
        auto pPo = net.GetPo(i);
        net.SetObjTrav(pPo);
        auto pDriv = net.GetFanin(pPo, 0);
        if (DoubleEqual(net.GetObjArrTime(pDriv), maxDelay, DELAY_TOL))
            GetCritGraphRec(net, pDriv, critGraph);
    }

    // if (net.GetNetType() == NET_TYPE::SOP) {
    //     int maxLevel = net.GetLev();
    //     if (maxLevel == 0)
    //         return IntVec ();
    //     net.SetNetNotTrav();
    //     for (int i = 0; i < net.GetPoNum(); ++i) {
    //         auto pPo = net.GetPo(i);
    //         net.SetObjTrav(pPo);
    //         auto pDriv = net.GetFanin(pPo, 0);
    //         if (net.GetObjLev(pDriv) == maxLevel)
    //             GetCritGraphForSOPRec(net, pDriv, critGraph);
    //     }
    // }
    // else if (net.GetNetType() == NET_TYPE::GATE) {
    //     double maxDelay = net.GetDelay();
    //     if (DoubleEqual(maxDelay, 0.0, DELAY_TOL))
    //         return IntVec ();
    //     net.SetNetNotTrav();
    //     for (int i = 0; i < net.GetPoNum(); ++i) {
    //         auto pPo = net.GetPo(i);
    //         net.SetObjTrav(pPo);
    //         auto pDriv = net.GetFanin(pPo, 0);
    //         if (DoubleEqual(net.GetObjArrTime(pDriv), maxDelay, DELAY_TOL))
    //             GetCritGraphForGateRec(net, pDriv, critGraph);
    //     }
    // }
    // else
    //     assert(0);
    return critGraph;
}


// void DALSMan::GetCritGraphForSOPRec(NetMan & net, AbcObj * pObj, IntVec & critGraph) {
//     if (net.GetObjTrav(pObj))
//         return;
//     net.SetObjTrav(pObj);
//     int currLevel = net.GetObjLev(pObj);
//     for (int i = 0; i < net.GetFaninNum(pObj); ++i) {
//         auto pFanin = net.GetFanin(pObj, i);
//         if (net.GetObjLev(pFanin) == currLevel - 1)
//             GetCritGraphForSOPRec(net, pFanin, critGraph) ;
//     }
//     critGraph.emplace_back(net.GetId(pObj));
// }


void DALSMan::GetCritGraphRec(NetMan & net, AbcObj * pObj, IntVec & critGraph) {
    #ifdef DEBUG
    assert(!net.IsObjPo(pObj));
    #endif
    if (net.GetObjTrav(pObj))
        return;
    net.SetObjTrav(pObj);
    // cout << "travel " << pObj << endl;
    if (net.IsObjPi(pObj)) {
        critGraph.emplace_back(net.GetId(pObj));
        return;
    }
    double targArrTime = net.GetMostCritArrTimeOfFanins(pObj);
    for (int i = 0; i < net.GetFaninNum(pObj); ++i) {
        auto pFanin = net.GetFanin(pObj, i);
        // cout << "try " << pObj << "'s fanin " << pFanin << "," << net.GetObjArrTime(pFanin) << endl;
        if (DoubleEqual(net.GetObjArrTime(pFanin), targArrTime, DELAY_TOL))
            GetCritGraphRec(net, pFanin, critGraph) ;
    }
    critGraph.emplace_back(net.GetId(pObj));
}


IntVec DALSMan::GetTFIInCritGraph(NetMan & net, set <int> & critGraph, int nodeId) {
    IntVec ans;
    ans.reserve(critGraph.size());
    net.GetLev();
    net.SetNetNotTrav();
    GetTFIInCritGraphRec(net, critGraph, nodeId, ans);
    return ans;
}


void DALSMan::GetTFIInCritGraphRec(NetMan & net, set <int> & critGraph, int nodeId, IntVec & tfi) {
    if (critGraph.count(nodeId) == 0 || net.GetObjTrav(nodeId))
        return;
    net.SetObjTrav(nodeId);
    for (int iFanin = 0; iFanin < net.GetFaninNum(nodeId); ++ iFanin) {
        auto faninId = net.GetFaninId(nodeId, iFanin);
        if (net.GetObjLev(nodeId) == net.GetObjLev(faninId) + 1)
            GetTFIInCritGraphRec(net, critGraph, faninId, tfi);
    }
    tfi.emplace_back(nodeId);
}


IntVec DALSMan::GetTFOInCritGraph(NetMan & net, set <int> & critGraph, int nodeId) {
    IntVec ans;
    ans.reserve(critGraph.size());
    net.GetLev();
    net.SetNetNotTrav();
    GetTFOInCritGraphRec(net, critGraph, nodeId, ans);
    return ans;
}


void DALSMan::GetTFOInCritGraphRec(NetMan & net, set <int> & critGraph, int nodeId, IntVec & tfo) {
    if (critGraph.count(nodeId) == 0 || net.IsObjPo(nodeId) || net.GetObjTrav(nodeId))
        return;
    net.SetObjTrav(nodeId);
    for (int iFanout = 0; iFanout < net.GetFanoutNum(nodeId); ++ iFanout) {
        auto fanoutId = net.GetFanoutId(nodeId, iFanout);
        if (net.GetObjLev(nodeId) == net.GetObjLev(fanoutId) - 1)
            GetTFOInCritGraphRec(net, critGraph, fanoutId, tfo);
    }
    tfo.emplace_back(nodeId);
}


IntVec DALSMan::GetMinCut(const LACMan & lacMan, const NetMan & net, const IntVec & critGraph, double baseErr, int source, int sink, int N) {
    // init
    // const int maxLev = net.GetLev();
    double maxDelay = net.GetDelay();
    const double LARGE = numeric_limits <double>::max() / 100.0;
    const double SMALL = numeric_limits <double>::min();
    Dinic dinic(N * 2);
    // set <int> critGraphSet(critGraph.begin(), critGraph.end());

    // build graph
    for (const auto & u: critGraph) {
        assert(!net.IsObjPo(u));
        // int lev = net.GetObjLev(u);
        double arrTime = net.GetObjArrTime(u);
        // add super source
        if (net.IsObjPi(u) || net.IsConst(u))
            dinic.AddEdge(source, u, LARGE);
        else {
            auto minErr = lacMan.GetCandLac(u)->GetErr() - baseErr;
            // split nodes
            if (minErr == 0)
                dinic.AddEdge(u, u + N, SMALL);
            else if (minErr > 0)
                dinic.AddEdge(u, u + N, minErr);
            else
                assert(0);
            // PO driver, add super sink
            // if (lev == maxLev)
            if (arrTime == maxDelay) {
                dinic.AddEdge(u + N, sink, LARGE);
                // if (print) cout << u << "," << u + N << "->" << sink << ":" << LARGE << endl;
            }
        }
    }
    for (const auto & u: critGraph) {
        double targArrTime = net.GetMostCritArrTimeOfFanins(u);
        // add fanins
        for (int i = 0; i < net.GetFaninNum(u); ++i) {
            int faninId = net.GetFaninId(u, i);
            // if (critGraphSet.count(faninId) && net.GetObjLev(faninId) + 1 == net.GetObjLev(u))
            if (DoubleEqual(net.GetObjArrTime(faninId), targArrTime, DELAY_TOL)) {
                if (net.IsObjPi(faninId) || net.IsConst(faninId))
                    dinic.AddEdge(faninId, u, LARGE);
                else
                    dinic.AddEdge(faninId + N, u, LARGE);
            }
        }
    }

    // compute minimum cut
    auto minCut = dinic.MinCut(source, sink);
    IntVec cut;
    cut.reserve(minCut.size());
    for (auto & edge: minCut)
        cut.emplace_back(edge.u);
    return cut;
}


// IntVec DALSMan::GetMinCutWithRealErr(const LACMan & lacMan, const NetMan & net, const IntVec & critGraph, int source, int sink, int N) {
//     // init
//     const int maxLev = net.GetLev();
//     const double LARGE = numeric_limits <double>::max() / 100.0;
//     const double SMALL = numeric_limits <double>::min();
//     Dinic dinic(N * 2);
//     set <int> critGraphSet(critGraph.begin(), critGraph.end());

//     // build graph
//     for (const auto & u: critGraph) {
//         assert(!net.IsObjPo(u));
//         int lev = net.GetObjLev(u);
//         // add super source
//         if (net.IsObjPi(u) || net.IsConst(u))
//             dinic.AddEdge(source, u, LARGE);
//         else {
//             auto minErr = lacMan.GetCandLac(u)->GetErr();
//             // split nodes
//             if (minErr == 0)
//                 dinic.AddEdge(u, u + N, SMALL);
//             else if (minErr > 0)
//                 dinic.AddEdge(u, u + N, minErr);
//             else
//                 assert(0);
//             // PO driver, add super sink
//             if (lev == maxLev) {
//                 dinic.AddEdge(u + N, sink, LARGE);
//                 // if (print) cout << u << "," << u + N << "->" << sink << ":" << LARGE << endl;
//             }
//         }
//     }
//     for (const auto & u: critGraph) {
//         // add fanins
//         for (int i = 0; i < net.GetFaninNum(u); ++i) {
//             int faninId = net.GetFaninId(u, i);
//             if (critGraphSet.count(faninId) && net.GetObjLev(faninId) + 1 == net.GetObjLev(u)) {
//                 if (net.IsObjPi(faninId) || net.IsConst(faninId))
//                     dinic.AddEdge(faninId, u, LARGE);
//                 else
//                     dinic.AddEdge(faninId + N, u, LARGE);
//             }
//         }
//     }

//     // compute minimum cut
//     auto minCut = dinic.MinCut(source, sink);
//     IntVec cut;
//     cut.reserve(minCut.size());
//     for (auto & edge: minCut)
//         cut.emplace_back(edge.u);
//     return cut;
// }


void DALSMan::ApplyLacs(NetMan & net, vector < std::shared_ptr <LAC> > & lacs, bool isVerb) {
    #ifdef DEBUG
    assert(lacs.size());
    #endif

    // transfer fanouts
    if (lacType == LAC_TYPE::CONS) {
        auto consts = net.GetConstId();
        assert(consts.first != -1 && consts.second != -1);
        for (auto & pLac: lacs) {
            auto pConstLac = dynamic_pointer_cast <ConstLAC> (pLac);
            auto targId = pConstLac->GetTargId();
            auto err = pConstLac->GetErr();
            if (isVerb) {
                auto pos = net.GetFanoutsThatArePos(targId);
                cout << net.GetName(targId);
                if (pos.size()) {
                    cout << "(driver of ";
                    PrintVect(pos);
                    cout << ")";
                }
                cout << " is replaced by " << (pConstLac->IsConst0()? "const0": "const1") << " with error " << err << endl;
            }
            if (pConstLac->IsConst0())
                net.TransfFanout(targId, consts.first);
            else
                net.TransfFanout(targId, consts.second);
        }
    }
    else if (lacType == LAC_TYPE::SASIMI) {
        if (net.GetNetType() == NET_TYPE::SOP)
            net.GetLev();
        else if (net.GetNetType() == NET_TYPE::GATE)
            net.GetDelay();
        else
            assert(0);
        for (const auto & pLac: lacs) {
            auto pSasimiLac = dynamic_pointer_cast <SasimiLAC> (pLac);
            auto targId = pSasimiLac->GetTargId();
            auto subId = pSasimiLac->GetSubId();
            auto isInv = pSasimiLac->GetIsInv();
            auto err = pSasimiLac->GetErr();
            if (isVerb) {
                cout << "replace " << net.GetObj(targId);
                if (net.GetNetType() == NET_TYPE::SOP)
                    cout << "(l=" << net.GetObjLev(targId) << ")";
                else if (net.GetNetType() == NET_TYPE::GATE)
                    cout << "(at=" << net.GetObjArrTime(targId) << ")";
                else
                    assert(0);
                cout << " by " << net.GetObj(subId);
                if (net.GetNetType() == NET_TYPE::SOP)
                    cout << "(l=" << net.GetObjLev(subId) << ")";
                else if (net.GetNetType() == NET_TYPE::GATE)
                    cout << "(at=" << net.GetObjArrTime(subId) << ")";
                else
                    assert(0);
                cout << "+ " << (isInv? "inv": "buf") << " with error " << err << endl;
            }
            if (!isInv)
                net.TransfFanout(targId, subId);
            else {
                int newInvId = net.CreateInv(subId);
                net.TransfFanout(targId, newInvId);
            }
        }
    }
    else if (lacType == LAC_TYPE::RAC) {
        if (net.GetNetType() == NET_TYPE::SOP)
            net.GetLev();
        else
            assert(0);
        for (const auto & pLac: lacs) {
            auto pRacLac = dynamic_pointer_cast <RacLAC> (pLac);
            auto targId = pRacLac->GetTargId();
            auto faninIds = pRacLac->GetDivIds();
            auto sop = pRacLac->GetSop();
            auto err = pRacLac->GetErr();
            if (isVerb) {
                cout << "replace " << net.GetObj(targId);
                if (net.GetNetType() == NET_TYPE::SOP) {
                    cout << "(l=" << net.GetObjLev(targId) << ") with old fanins (";
                    for (int i = 0; i < net.GetFaninNum(targId); ++i) {
                        cout << net.GetFaninId(targId, i) << "(l=" << net.GetObjLev(net.GetFanin(targId, i)) << "),";
                    }
                    cout << ")";
                }
                else
                    assert(0);
                cout << " by ";
                if (net.GetNetType() == NET_TYPE::SOP) {
                    cout << "(";
                    for (const auto & faninId: faninIds)
                        cout << faninId << "(l=" << net.GetObjLev(faninId) << "),";
                    cout << ")";
                }
                else
                    assert(0);
                cout << " with error " << err << ", function:" << endl;
                cout << sop;
            }
            auto newNodeId = net.CreateNode(faninIds, sop);
            net.TransfFanout(targId, newNodeId);
        }
    }
    else
        assert(0);

    // remove redundant nodes
    net.CleanUp();
}



void DALSMan::ExactSimpl(NetMan & net) {
    cout << "before exact simplification: "; net.PrintStat();
    if (net.GetNetType() == NET_TYPE::SOP) {
        net.Comm("resyn2; ifraig; logic; sop;");
        net.MergeConst();
    }
    else if (net.GetNetType() == NET_TYPE::GATE) {
        assert(net.UseModernSCL());
        auto newNet = net;
        newNet.Comm("topo");
        double oldDelay = newNet.GetDelay(), oldArea = newNet.GetArea();
        newNet.Comm("st; resyn2; ifraig; dch; map; topo;");
        double newDelay = newNet.GetDelay(), newArea = newNet.GetArea();
        if (DoubleLessEqual(newArea, oriArea)) {
            if (DoubleLess(newDelay, oldDelay, DELAY_TOL) ||
                (DoubleEqual(newDelay, oldDelay, DELAY_TOL) && DoubleLess(newArea, oldArea, AREA_TOL))
            ) {
                net = newNet;
            }
        }
        net.Comm("topo");
    }
    cout << "after exact simplification: "; net.PrintStat();
}


void DALSMan::Eval(NetMan & net, const string & outpPath, double err, int round) {
    if (DoubleGreat(err, errUppBound))
        return;
    if (net.GetNetType() == NET_TYPE::SOP) {
        // measure and output SOP
        ostringstream oss("");
        oss << outpPath << round << "_" << net.GetNet()->pName << "_err_" << err << "_size_" << net.GetArea() << "_depth_" << net.GetDelay() << ".blif";
        net.WriteNet(oss.str(), true);

        // synthesis & mapping
        auto newNet = net;
        if (mapType == MAP_TYPE::SCL) {
            newNet.Comm("st; resyn2; ifraig; dch; map;");
            assert(newNet.GetNetType() == NET_TYPE::GATE);
            cout << "after mapping: "; newNet.PrintStat();
        }
        else
            assert(0);

        // measure and output
        oss.str("");
        oss << outpPath << round << "_" << newNet.GetNet()->pName << "_err_" << err << "_area_" << newNet.GetArea() << "_delay_" << newNet.GetDelay() << ".blif";
        newNet.WriteNet(oss.str(), true);
    }
    else if (net.GetNetType() == NET_TYPE::GATE) {
        // measure and output original gate-netlist
        ostringstream oss("");
        oss << outpPath << round << "_" << net.GetNet()->pName << "_err_" << err << "_area_" << net.GetArea() << "_delay_" << net.GetDelay() << ".blif";
        net.WriteNet(oss.str(), true);
        net.PrintStat();
    }
    else
        assert(0);
}


int GetCutGain(NetMan&net, const Cut& cut, const LACMan& lacMan, LAC_TYPE lacType) {
    // auto tempNet = net;
    // vector <std::shared_ptr <LAC>> applLacs;
    // for (int appl: cut) {
    //     auto applLac = lacMan.GetCandLac(appl);
    //     assert(applLac != nullptr);
    //     applLacs.emplace_back(applLac);
    // }
    // tempNet.Print(true);
    // TempApplyLacs(tempNet, applLacs, lacType, true);
    // tempNet.CleanUp();
    // tempNet.MergeConst();
    // return tempNet.GetNodeNum();
    assert(cut.size() > 0);
    vector < std::shared_ptr <LAC> > applLacs;
    for (const auto & appl: cut) {
        auto applLac = lacMan.GetCandLac(appl);
        assert(applLac != nullptr);
        applLacs.emplace_back(applLac);
    }
    auto replTraces = TempApplyLacs(net, applLacs, lacType, false);
    auto tempNet = net;
    for (auto it = replTraces.rbegin(); it != replTraces.rend(); ++it)
        net.Recov(*it, false);
    tempNet.CleanUp();
    tempNet.MergeConst();
    return tempNet.GetNodeNum();
}


double CalcCutErr(NetMan& accNet, NetMan& net, const Cut& cut, const LACMan& lacMan, LAC_TYPE lacType, unsigned seed, int nFrame, METR_TYPE metrType, DISTR_TYPE distrType) {
    assert(cut.size() > 0);
    vector < std::shared_ptr <LAC> > applLacs;
    for (const auto & appl: cut) {
        auto applLac = lacMan.GetCandLac(appl);
        assert(applLac != nullptr);
        applLacs.emplace_back(applLac);
    }
    auto replTraces = TempApplyLacs(net, applLacs, lacType, false);
    assert(distrType == DISTR_TYPE::UNIF);
    static shared_ptr<Simulator> pAccSmlt = nullptr;
    if (pAccSmlt == nullptr) {
        pAccSmlt = std::make_shared<Simulator> (accNet, seed, nFrame);
        pAccSmlt->InpUnifFast();
        pAccSmlt->Sim();
    }
    Simulator appSmlt(net, seed, nFrame);
    appSmlt.InpUnifFast();
    appSmlt.Sim();
    double newErr = 0.0;
    if (metrType == METR_TYPE::ER)
        newErr = pAccSmlt->GetErrRate(appSmlt);
    else if (metrType == METR_TYPE::MED)
        newErr = pAccSmlt->GetMeanErrDist(appSmlt);
    else if (metrType == METR_TYPE::MHD)
        newErr = pAccSmlt->GetMeanHammDist(appSmlt);
    else
        assert(0);
    for (auto it = replTraces.rbegin(); it != replTraces.rend(); ++it)
        net.Recov(*it, false);
    return newErr;
}


// double CalcCutErrNew(const Cut& cut, VECBEEMan& vecbeeMan) {
//     // static int totTime1 = 0; static int totTime2 = 0; static int cnt = 0;
//     // clock_t st = clock();

//     assert(cut.size() > 0);
//     Simulator& accSmlt = vecbeeMan.GetAccSmlt();
//     Simulator& appSmlt = vecbeeMan.GetAppSmlt();
//     int nFrame = vecbeeMan.GetFrameNum();
//     int nPo = appSmlt.GetPoNum();
//     auto metrType = vecbeeMan.GetMetrType();
//     auto& poChangeWithNode = vecbeeMan.GetPoChangeWithNode();
//     for (int i = 0; i < nPo; ++i) {
//         auto poId = appSmlt.GetPoId(i);
//         BitVec isChanged(nFrame, 0);
//         for (int nodeId: cut)
//             isChanged |= poChangeWithNode[i][nodeId];
//         appSmlt.SetTempDat(poId, *appSmlt.GetDat(poId) ^ isChanged);
//     }

//     // totTime1 += clock() - st; ++cnt;
//     // cout << "average runtime1 = " << totTime1 / (double)cnt << endl;

//     double ans = 0.0;
//     if (metrType == METR_TYPE::ER) {
//         BitVec diff(nFrame, 0);
//         for (int i = 0; i < nPo; ++i)
//             diff |= (*accSmlt.GetDat(accSmlt.GetPoId(i)) ^ *appSmlt.GetTempDat(appSmlt.GetPoId(i)));
//         ans = diff.count() / (double)(nFrame);
//     }
//     else if (metrType == METR_TYPE::MED) {
//         const ll warnPoint = numeric_limits <ll>::max() * 0.9;
//         static LLVec accOut(nFrame, 0);
//         static int accOutCtrl = 0;
//         ll sed = 0;
//         if (!accOutCtrl) {
//             for (int iPatt = 0; iPatt < nFrame; ++iPatt) {
//                 accOut[iPatt] = accSmlt.GetOutputFast(iPatt);
//                 ll appOut = appSmlt.GetTempOutputFast(iPatt);
//                 sed += abs(accOut[iPatt] - appOut);
//                 if (sed > warnPoint)
//                     assert(0);
//             }
//             accOutCtrl = 1;
//         }
//         else {
//             for (int iPatt = 0; iPatt < nFrame; ++iPatt) {
//                 ll appOut = appSmlt.GetTempOutputFast(iPatt);
//                 sed += abs(accOut[iPatt] - appOut);
//                 if (sed > warnPoint)
//                     assert(0);
//             }
//         }
//         ans = sed / (double)(nFrame);
//     }
//     else
//         assert(0);

//     // totTime2 += clock() - st;
//     // cout << "average runtime2 = " << totTime2 / (double)cnt << endl;

//     return ans;
// }


vector<IntVec> TempApplyLacs(NetMan & net, vector < std::shared_ptr <LAC> > & lacs, LAC_TYPE lacType, bool isVerb) {
    assert(lacs.size());
    vector<IntVec> replTraces;
    for (const auto & pLac: lacs) {
        AbcObj * pTS = nullptr, * pSS = nullptr;
        pTS = net.GetObj(pLac->GetTargId());
        if (lacType == LAC_TYPE::CONS) {
            auto constLac = *dynamic_pointer_cast <ConstLAC> (pLac);
            auto consts = net.CreateConst();
            assert(consts.first != -1 && consts.second != -1);
            pSS = constLac.IsConst0()? net.GetObj(consts.first): net.GetObj(consts.second);
            if (isVerb) cout << pTS << " is replaced by const" << !constLac.IsConst0() << endl;
        }
        else if (lacType == LAC_TYPE::SASIMI) {
            auto sasimiLac = *dynamic_pointer_cast <SasimiLAC> (pLac);
            auto pSub = net.GetObj(sasimiLac.GetSubId());
            pSS = sasimiLac.GetIsInv()? net.CreateInv(pSub): pSub;
            if (isVerb) cout << pTS << " is replaced by " << (sasimiLac.GetIsInv()? "\\bar ": "") << pSS << endl;
        }
        else if (lacType == LAC_TYPE::RAC) {
            auto racLac = *dynamic_pointer_cast <RacLAC> (pLac);
            auto newNodeId = net.CreateNode(racLac.GetDivIds(), racLac.GetSop());
            pSS = net.GetObj(newNodeId);
            if (isVerb) {cout << pTS << " is rewritten, "; net.PrintObj(pSS, true);}
        }
        else
            assert(0);
        auto replTrace = net.TempRepl(pTS, pSS);
        replTraces.emplace_back(replTrace);
    }
    return replTraces;
}


void Recov(NetMan & net, vector<IntVec> & replTraces, bool isVerb) {
    for (auto it = replTraces.rbegin(); it != replTraces.rend(); ++it)
        net.Recov(*it, isVerb);
}


CutMan::CutMan(int cut_size_lim, NetMan & _net): cutSizeLim(cut_size_lim), net(_net) {
    // init nodes
    critNodes.resize(0);
    double maxDelay = net.GetDelay();
    cout << "maxDelay = " << maxDelay << endl;
    if (DoubleEqual(maxDelay, 0.0, DELAY_TOL))
        return;
    net.SetNetNotTrav();
    unordered_map <int, int> netId2CritId;
    for (int iPo = 0; iPo < net.GetPoNum(); ++iPo) {
        auto pDr = net.GetFanin(net.GetPo(iPo), 0);
        if (DoubleEqual(net.GetObjArrTime(pDr), maxDelay, DELAY_TOL)) {
            BuildRec(pDr, netId2CritId);
        }
    }
    critNodes.emplace_back(-1);
    nSize = critNodes.size();

    // init pis & pos
    for (int iPi = 0; iPi < net.GetPiNum(); ++iPi) {
        int piId = net.GetPiId(iPi);
        if (netId2CritId.count(piId))
            pis.emplace_back(netId2CritId[piId]);
    }
    for (int iPo = 0; iPo < net.GetPoNum(); ++iPo) {
        int drId = net.GetFaninId(net.GetPo(iPo), 0);
        if (DoubleEqual(net.GetObjArrTime(drId), maxDelay, DELAY_TOL) && netId2CritId.count(drId))
            pos.emplace_back(netId2CritId[drId]);
    }

    // init fanins
    critFis.resize(nSize);
    for (int id = 0; id < nSize; ++id) {
        int nodeId = critNodes[id];
        if (nodeId == -1) {
            for (int po: pos)
                critFis[id].emplace_back(po);
            continue;
        }

        int nFanin = net.GetFaninNum(nodeId);
        if (nFanin == 0)
            continue;
        // double targArrTime = net.GetObjArrTime(nodeId) - net.GetObjDelay(nodeId);
        double targArrTime = net.GetMostCritArrTimeOfFanins(nodeId);
        for (int iFanin = 0; iFanin < nFanin; ++iFanin) {
            auto faninNodeId = net.GetFaninId(nodeId, iFanin);
            if (DoubleEqual(net.GetObjArrTime(faninNodeId), targArrTime, DELAY_TOL)) {
                assert(netId2CritId.count(faninNodeId) != 0);
                critFis[id].emplace_back(netId2CritId[faninNodeId]);
            }
        }
    }
    
    // init cuts
    cuts.clear();
}


void CutMan::BuildRec(AbcObj * pObj, unordered_map <int, int> & netId2CritId) {
    if (net.GetObjTrav(pObj))
        return;
    net.SetObjTrav(pObj);
    int nFanin = net.GetFaninNum(pObj);
    if (nFanin > 0) {
        // double targArrTime = net.GetObjArrTime(pObj) - net.GetObjDelay(pObj);
        double targArrTime = net.GetMostCritArrTimeOfFanins(pObj);
        for (int iFanin = 0; iFanin < nFanin; ++iFanin) {
            auto pFanin = net.GetFanin(pObj, iFanin);
            if (DoubleEqual(net.GetObjArrTime(pFanin), targArrTime, DELAY_TOL))
                BuildRec(pFanin, netId2CritId);
        }
    }
    int nodeId = net.GetId(pObj);
    critNodes.emplace_back(nodeId);
    assert(netId2CritId.count(nodeId) == 0);
    netId2CritId[nodeId] = critNodes.size() - 1;
}


void CutMan::Print() const {
    for (int id = 0; id < critNodes.size(); ++id) {
        cout << net.GetObj(critNodes[id]) << ":";
        for (const auto & faninId: critFis[id])
            cout << net.GetObj(critNodes[faninId]) << ",";
        cout << endl;
    }
}


void CutMan::LimNumbOfCuts(CutSet& fcw, double refErr, NetMan& accNet, LACMan& lacMan, LAC_TYPE lacType, unsigned seed, int nFrame, METR_TYPE metrType, DISTR_TYPE distrType) {
    CutSet fcwCopy = fcw;
    double minErr = numeric_limits<double>::max();
    for (auto& cut: fcwCopy) {
        double tempErr = -1.0;
        if (cut2Err.count(cut))
            tempErr = cut2Err[cut];
        else {
            tempErr = CalcCutErr(accNet, net, cut, lacMan, lacType, seed, nFrame, metrType, distrType);
            cut2Err[cut] = tempErr;
        }
        minErr = min(minErr, tempErr);
    }
    for (auto& cut: fcwCopy) {
        assert(cut2Err.count(cut));
        double tempErr = cut2Err[cut];
        if (DoubleGreat(tempErr, refErr) && DoubleGreat(tempErr, minErr))
            fcw.erase(cut);
    }
    if (fcw.size() > cutSizeLim) {
        vector<pair<Cut, double>> cutAndErr;
        cutAndErr.reserve(fcw.size());
        for (auto& cut: fcw)
            cutAndErr.emplace_back(cut, cut2Err[cut]);
        sort(cutAndErr.begin(), cutAndErr.end(),
            [](const auto& a, const auto& b){return a.second < b.second;}
        );
        cutAndErr.resize(cutSizeLim);
        fcw.clear();
        for (auto& _pair: cutAndErr)
            fcw.emplace(_pair.first);
    }
}

Cut CutMan::GenCutsPrunedProPlus(const DblVec& errs, double refErr, double errUppBound, NetMan& accNet, LACMan& lacMan, LAC_TYPE lacType, unsigned seed, int nFrame, METR_TYPE metrType, DISTR_TYPE distrType, const Cut& refCut) {
    vector<CutSet> cutsOfNodes(nSize);
    // phase 1 --- cut enumeration
    // fc(w) = (u1+fc(u1))(u2+fc(u2))(u3+fc(u3)) = s1 s2 s3
    boost::timer::progress_display progrDispl(critNodes.size());
    cout << "enumerating cuts..." << endl;
    for (int w = 0; w < critNodes.size(); ++w) {
        CutSet& fcw = cutsOfNodes[w];
        fcw.clear();
        for (int iFanin = 0; iFanin < critFis[w].size(); ++iFanin) {
            int u = critFis[w][iFanin];
            if (net.IsObjPi(critNodes[u]) || net.IsConst(critNodes[u]))
                continue;
            CutSet& fcu = cutsOfNodes[u];
            if (fcw.empty()) {
                fcw = fcu;
                if (DoubleLessEqual(errs[critNodes[u]], refErr))
                    fcw.emplace(Cut({critNodes[u]}));
            }
            else
                JoinCutSet(fcw, fcu, refErr, critNodes[u], errs);
            if (critNodes[w] == -1) // avoid too much fanins on the sink node
                LimNumbOfCuts(fcw, refErr, accNet, lacMan, lacType, seed, nFrame, metrType, distrType);
        }
        if (critNodes[w] != -1) // avoid repetitive computation
            LimNumbOfCuts(fcw, refErr, accNet, lacMan, lacType, seed, nFrame, metrType, distrType);
        ++progrDispl;
    }

    // phase 2 --- get best global cut
    Cut bestCut;
    DblPair bestErrAndGain = DblPair(numeric_limits<double>::max(), numeric_limits<double>::max());
    for (const auto& cut: cutsOfNodes.back()) {
        // if (!cut2Err.count(cut))
        //     PrintCut(cut); cout << endl;
        double tempErr = GetCutErr(cut);
        if (tempErr < bestErrAndGain.first) {
            bestErrAndGain = DblPair(tempErr, GetCutGain(net, cut, lacMan, lacType));
            bestCut = cut;
            cout << "global cut "; PrintSet(cut, ":"); PrintPair(bestErrAndGain, "\n");
        }
        else if (tempErr == bestErrAndGain.first) {
            int gain = GetCutGain(net, cut, lacMan, lacType);
            if (gain < bestErrAndGain.second) {
                bestErrAndGain.second = gain;
                bestCut = cut;
                cout << "global cut "; PrintSet(cut, ":"); PrintPair(bestErrAndGain, "\n");
            }
        }
        // PrintSet(cut, ":"); PrintPair(bestErrAndGain, "\n");
        cut2Err.erase(cut);
    }
    if (bestCut.empty()) {
        bestCut = refCut;
        bestErrAndGain = DblPair(refErr, GetCutGain(net, bestCut, lacMan, lacType));
        cout << "global cut "; PrintSet(bestCut, ":"); PrintPair(bestErrAndGain, "\n");
    }
    assert(bestErrAndGain.first != numeric_limits<double>::max() && bestErrAndGain.second != numeric_limits<double>::max());
    if (DoubleLessEqual(bestErrAndGain.first, errUppBound))
        return bestCut;

    // phase 3 --- get best local cut
    for (int w = 0; w < critNodes.size(); ++w) {
        if (critNodes[w] != -1 && DoubleLessEqual(errs[critNodes[w]], refErr)) {
            if (!net.IsObjPi(critNodes[w]) && !net.IsConst(critNodes[w])) {
                Cut resCut({critNodes[w]});
                if (!cut2Err.count(resCut))
                    cut2Err[resCut] = CalcCutErr(accNet, net, resCut, lacMan, lacType, seed, nFrame, metrType, distrType);
            }
        }
    }
    bestErrAndGain = DblPair(numeric_limits<double>::max(), numeric_limits<double>::max());
    bestCut.clear();
    for (const auto& [cut, tempErr]: cut2Err)  {
        bool fUpd = (tempErr < bestErrAndGain.first) ||
            (tempErr == bestErrAndGain.first && GetCutGain(net, cut, lacMan, lacType) < bestErrAndGain.second);
        if (fUpd) {
            bestErrAndGain = DblPair(tempErr, GetCutGain(net, cut, lacMan, lacType));
            bestCut = cut;
            cout << "local cut "; PrintSet(cut, ":"); PrintPair(bestErrAndGain, "\n");
        }
    }
    if (bestCut.empty())
        bestCut = refCut;
    return bestCut;
}


void CutMan::MergeCut(const set <int> & cut0, const set <int> & cut1, set <int> & resCut) {
    assert(cut0.size() && cut1.size());
    resCut = cut0;
    for (const auto & cutNode: cut1)
        resCut.emplace(cutNode);
}


void CutMan::JoinCutSet(CutSet& fcw, const CutSet& fcu, double refErr, int critNode, const DblVec& errs) {
    // fcw = fcw \Join fcu
    // A \Join B = \{a \cup b | \forall a \in A, b \in b\}
    CutSet fcwCopy = fcw;
    fcw.clear();
    for (const Cut &cut0: fcwCopy) {
        Cut resCut;
        for (const Cut &cut1: fcu) {
            MergeCut(cut0, cut1, resCut);
            fcw.emplace(resCut);
        }
        if (DoubleLessEqual(errs[critNode], refErr)) {
            MergeCut(cut0, Cut({critNode}), resCut);
            fcw.emplace(resCut);
        }
    }
}


void CutMan::GetTopoRec(int targNode, std::unordered_map <int, double> & nodeFlows, std::unordered_set <int> & visNodes, IntVec & topoNodes) {
    if (visNodes.count(targNode))
        return;
    visNodes.emplace(targNode);
    for (int fanin: critFis[targNode])
        GetTopoRec(fanin, nodeFlows, visNodes, topoNodes);
    topoNodes.emplace_back(targNode);
}


void CutMan::PrintCut(const Cut& cut) const {
    cout << "(";
    for (const auto & cutNode: cut) {
        cout << cutNode << ",";
    }
    // cout << ")=" << cut2Err.at(cut) << ",";
    cout << ")";
}


void CutMan::PrintCutSet(const CutSet& cutSet) const {
    for (const auto& cut: cutSet)
        PrintCut(cut);
    cout << endl;
}