#include "error.h"


using namespace std;
using namespace abc;
using namespace boost;


ErrMan::ErrMan(NetMan & netMan0, NetMan & netMan1, unsigned _seed, int n_frame, DISTR_TYPE distr_type):
    smlt0(Simulator(netMan0, _seed, n_frame)), smlt1(Simulator(netMan1, _seed, n_frame)), seed(_seed), nFrame(n_frame), distrType(distr_type) {
    #ifdef DEBUG
    assert(IsPIOSame(smlt0, smlt1));
    #endif
    if (distrType == DISTR_TYPE::UNIF) {
        smlt0.InpUnifFast();
        smlt1.InpUnifFast();
        // smlt0.InpUnif();
        // smlt1.InpUnif();
    }
    else if (distrType == DISTR_TYPE::ENUM) {
        smlt0.InpEnum();
        smlt1.InpEnum();
    }
    else
        assert(0);
}


double ErrMan::CalcErrRate() {
    smlt0.Sim();
    smlt1.Sim();
    return GetErrRate();
}


double ErrMan::CalcMeanErrDist() {
    smlt0.Sim();
    smlt1.Sim();
    return GetMeanErrDist();
}


double ErrMan::CalcMeanHammDist() {
    smlt0.Sim();
    smlt1.Sim();
    return GetMeanHammDist();
}


VECBEEMan::VECBEEMan(NetMan& accNet, NetMan& appNet, unsigned _seed, int n_frame, METR_TYPE metr_type, LAC_TYPE lac_type, DISTR_TYPE distr_type):
    accSmlt(accNet, _seed, n_frame), appSmlt(appNet, _seed, n_frame), seed(_seed), nFrame(n_frame), metrType(metr_type), lacType(lac_type), distrType(distr_type) {
    assert(IsPIOSame(accNet, appNet));
    FindAppDisjCut(appNet);
    if (distrType == DISTR_TYPE::UNIF) {
        accSmlt.InpUnifFast();
        appSmlt.InpUnifFast();
    }
    else if (distrType == DISTR_TYPE::ENUM) {
        accSmlt.InpEnum();
        appSmlt.InpEnum();
    }
    else
        assert(0);
    accSmlt.Sim();
    appSmlt.Sim();
}


// void VECBEEMan::BatchErrEst(NetMan & accNet, NetMan & appNet, LACMan & lacMan) {
//     #ifdef DEBUG
//     assert(IsPIOSame(accNet, appNet));
//     #endif
//     auto topoNodes = appNet.CalcTopoOrd();
//     // FindDisjCut(appNet, topoNodes);
//     FindAppDisjCut(appNet);
//     Simulator accSmlt(accNet, seed, nFrame);
//     Simulator appSmlt(appNet, seed, nFrame);
//     if (distrType == DISTR_TYPE::UNIF) {
//         accSmlt.InpUnifFast();
//         appSmlt.InpUnifFast();
//     }
//     else if (distrType == DISTR_TYPE::ENUM) {
//         accSmlt.InpEnum();
//         appSmlt.InpEnum();
//     }
//     else
//         assert(0);
//     accSmlt.Sim();
//     appSmlt.Sim();
//     CalcBoolDiffCut2Node(appSmlt, topoNodes);
//     CalcBoolDiffPo2Node(appSmlt, topoNodes);
//     CalcLACErrs(accSmlt, appSmlt, lacMan);
// }


void VECBEEMan::BatchErrEstNew(LACMan& lacMan, IntVec& critGraph) {
    // static int totTime = 0;
    // clock_t st = clock();
    auto topoNodes = appSmlt.CalcTopoOrd();
    CalcBoolDiffCut2Node(topoNodes);
    CalcBoolDiffPo2Node(topoNodes);
    CalcLACErrs(lacMan);
    lacMan.GenCandLacs(critGraph);
    // CalcChanges(lacMan, critGraph);
    // totTime += clock() - st;
    // cout << "total VECBEE time = " << totTime / 1000000.0 << "s" << endl;
}


void VECBEEMan::FindDisjCut(NetMan & net, vector <Abc_Obj_t *> & topoNodes) {
    #ifdef DEBUG
    assert(disjCuts.empty());
    assert(cutNtks.empty());
    assert(topoNodes.size());
    assert(topoNodes[0]->pNtk == net.GetNet());
    #endif

    // init
    cutNtks.resize(net.GetIdMaxPlus1());
    disjCuts.resize(net.GetIdMaxPlus1());
    poMarks.resize(net.GetIdMaxPlus1(), dynamic_bitset <ull>(net.GetPoNum(), 0));

    // update topo ids
    topoIds.resize(net.GetIdMaxPlus1());
    for (int i = 0; i < topoNodes.size(); ++i)
        topoIds[topoNodes[i]->Id] = i;
    int topoId = -1;
    for (int i = 0; i < net.GetPiNum(); ++i)
        topoIds[net.GetPiId(i)] = topoId--;
    topoId = topoNodes.size();
    for (int i = 0; i < net.GetPoNum(); ++i)
        topoIds[net.GetPoId(i)] = topoId++;

    // determine the POs that each node will affect
    for (int i = 0; i < net.GetPoNum(); ++i)
        poMarks[net.GetPoId(i)].set(i);
    for (auto it = topoNodes.rbegin(); it != topoNodes.rend(); ++it) {
        auto pObj = *it;
        if (pObj == nullptr)
            continue;
        int i = net.GetId(pObj);
        for (int j = 0; j < net.GetFanoutNum(pObj); ++j)
            poMarks[i] |= poMarks[net.GetFanoutId(pObj, j)];
    }

    // collect disjoint cuts and the corresponding cut networks
    for (const auto & pObj: topoNodes) {
        int i = net.GetId(pObj);
        assert(net.IsNode(pObj));
        Abc_NtkIncrementTravId(net.GetNet());
        FindDisjCutOfNode(pObj, disjCuts[i]);
        for (const auto & node: topoNodes) {
            if (Abc_NodeIsTravIdCurrent(node))
                cutNtks[i].emplace_back(node);
        }
        for (int j = 0; j < net.GetPoNum(); ++j) {
            auto pPo = net.GetPo(j);
            if (Abc_NodeIsTravIdCurrent(pPo))
                cutNtks[i].emplace_back(pPo);
        }
        // cout << pObj << ":" << endl;
        // PrintVect(vector <Abc_Obj_t *> (disjCuts[i].begin(), disjCuts[i].end()), "\n");
        // PrintVect(cutNtks[i], "\n");
    }
}


void VECBEEMan::FindAppDisjCut(NetMan & net) {
    #ifdef DEBUG
    assert(disjCuts.empty());
    assert(cutNtks.empty());
    #endif

    // init
    cutNtks.resize(net.GetIdMaxPlus1());
    disjCuts.resize(net.GetIdMaxPlus1());

    // collect disjoint cuts and the corresponding cut networks
    for (int iNode = 0; iNode < net.GetIdMaxPlus1(); ++iNode) {
        if (!net.IsNode(iNode))
            continue;
        if (net.IsConst(iNode))
            continue;
        for (int iFanout = 0; iFanout < net.GetFanoutNum(iNode); ++iFanout) {
            auto pFanout = net.GetFanout(iNode, iFanout);
            disjCuts[iNode].emplace_back(pFanout);
            cutNtks[iNode].emplace_back(pFanout);
        }
        // cout << net.GetObj(iNode) << ":" << endl;
        // PrintVect(vector <Abc_Obj_t *> (disjCuts[iNode].begin(), disjCuts[iNode].end()), "\n");
        // PrintVect(cutNtks[iNode], "\n");
    }
}


void VECBEEMan::FindDisjCutOfNode(Abc_Obj_t * pObj, list <Abc_Obj_t *> & disjCut) {
    disjCut.clear();
    ExpandCut(pObj, disjCut);
    Abc_Obj_t * pObjExpd = nullptr;
    while ((pObjExpd = ExpandWhich(disjCut)) != nullptr) {
        ExpandCut(pObjExpd, disjCut);
    }
}


void VECBEEMan::ExpandCut(Abc_Obj_t * pObj, list <Abc_Obj_t *> & disjCut) {
    abc::Abc_Obj_t * pFanout = nullptr;
    int i = 0;
    Abc_ObjForEachFanout(pObj, pFanout, i) {
        if (poMarks[pFanout->Id].none())
            continue;
        if (!abc::Abc_NodeIsTravIdCurrent(pFanout)) {
            if (abc::Abc_ObjFanoutNum(pFanout) || abc::Abc_ObjIsPo(pFanout)) {
                abc::Abc_NodeSetTravIdCurrent(pFanout);
                disjCut.emplace_back(pFanout);
            }
        }
    } 
}


Abc_Obj_t * VECBEEMan::ExpandWhich(list <Abc_Obj_t *> & disjCut) {
    for (auto ppAbcObj1 = disjCut.begin(); ppAbcObj1 != disjCut.end(); ++ppAbcObj1) {
        auto ppAbcObj2 = ppAbcObj1;
        for (++ppAbcObj2; ppAbcObj2 != disjCut.end(); ++ppAbcObj2) {
            #ifdef DEBUG
            assert(poMarks[(*ppAbcObj1)->Id].size() == poMarks[(*ppAbcObj2)->Id].size());
            assert((*ppAbcObj1)->Id != (*ppAbcObj2)->Id);
            if (topoIds[(*ppAbcObj1)->Id] == topoIds[(*ppAbcObj2)->Id]) {
                cout << topoIds[(*ppAbcObj1)->Id] << "," << topoIds[(*ppAbcObj2)->Id] << "," << *ppAbcObj1 << "," << *ppAbcObj2 << endl;
                assert(0);
            }
            #endif
            auto isJoint = poMarks[(*ppAbcObj1)->Id] & poMarks[(*ppAbcObj2)->Id];
            if (isJoint.any()) {
                abc::Abc_Obj_t * pRet = nullptr;
                if (topoIds[(*ppAbcObj1)->Id] < topoIds[(*ppAbcObj2)->Id]) {
                    pRet = *ppAbcObj1;
                    disjCut.erase(ppAbcObj1);
                }
                else {
                    pRet = *ppAbcObj2;
                    disjCut.erase(ppAbcObj2);
                }
                return pRet;
            }
        }
    }
    return nullptr;
}


void VECBEEMan::CalcBoolDiffCut2Node(ObjVec& topoNodes) {
    // cout << "calculating boolean difference of cuts with regard to nodes" << endl;
    #ifdef DEBUG
    assert(topoNodes.size());
    assert(topoNodes[0]->pNtk == appSmlt.GetNet());
    #endif
    // timer::progress_display pd(topoNodes.size());
    bdCut2Nodes.resize(appSmlt.GetIdMaxPlus1());
    for (const auto & pObj: topoNodes) {
        int i = appSmlt.GetId(pObj);
        if (!appSmlt.IsNode(pObj) || appSmlt.IsConst(pObj)) {
            // ++pd;
            continue;
        }
        // cout << "cut to node " << pObj << endl;
        appSmlt.CalcLocBoolDiff(pObj, disjCuts[i], cutNtks[i], bdCut2Nodes[i]);
        // ++pd;
    }
}


void VECBEEMan::CalcBoolDiffPo2Node(ObjVec& topoNodes) {
    // cout << "calculating boolean difference of POs with regard to nodes" << endl;
    #ifdef DEBUG
    assert(topoNodes.size());
    assert(topoNodes[0]->pNtk == appSmlt.GetNet());
    #endif
    int nPo = appSmlt.GetPoNum();
    bdPo2Nodes.resize(nPo);
    // timer::progress_display pd(nPo);
    for (int o = 0; o < nPo; ++o) {
        // init boolean difference;
        auto & bdPo2Node = bdPo2Nodes[o];
        bdPo2Node.resize(appSmlt.GetIdMaxPlus1(), dynamic_bitset <ull> (nFrame, 0));
        // for each PO, update boolean difference
        for (int n = 0; n < appSmlt.GetPoNum(); ++n) {
            auto pNodeN = appSmlt.GetPo(n);
            auto nId = appSmlt.GetId(pNodeN);
            if (n == o)
                bdPo2Node[nId].set(); 
            else
                bdPo2Node[nId].reset(); 
        }
        // for each node, update boolean difference
        for (auto it = topoNodes.rbegin(); it != topoNodes.rend(); ++it) {
            auto pNodeN = *it;
            if (!appSmlt.IsNode(pNodeN))
                continue;
            int n = appSmlt.GetId(pNodeN);
            bdPo2Node[n].reset();
            int i = 0;
            for (auto pCut: disjCuts[n]) {
                bdPo2Node[n] |= bdPo2Node[pCut->Id] & bdCut2Nodes[n][i];
                ++i;
            } 
        }
        // ++pd;
    }
}


void VECBEEMan::CalcLACErrs(LACMan& lacMan) {
    #ifdef DEBUG
    assert(IsPIOSame(accSmlt, appSmlt));
    assert((nFrame & 63) == 0);
    if (metrType == METR_TYPE::MED)
        assert(accSmlt.GetPoNum() < 64);
    #endif
    cout << "calculating LAC errors..." << endl;
    timer::progress_display progrDispl(lacMan.GetLacNum());
    for (int i = 0; i < lacMan.GetLacNum(); ++i) {
        auto pLac =  lacMan.GetLac(i);
        auto targId = pLac->GetTargId();
        boost::dynamic_bitset <ull> isChanged(nFrame, 0);
        if (lacType == LAC_TYPE::CONS) {
            auto pSpecLac = dynamic_pointer_cast<ConstLAC>(pLac);
            if (pSpecLac->IsConst0())
                isChanged.reset();
            else
                isChanged.set();
            isChanged = isChanged ^ (*appSmlt.GetDat(targId));
        }
        else if (lacType == LAC_TYPE::SASIMI) {
            auto pSpecLac = dynamic_pointer_cast<SasimiLAC>(pLac);
            auto subId = pSpecLac->GetSubId();
            boost::dynamic_bitset <ull> isChanged(nFrame, 0);
            if (!pSpecLac->GetIsInv())
                isChanged = (*appSmlt.GetDat(targId)) ^ (*appSmlt.GetDat(subId));
            else
                isChanged = ~(*appSmlt.GetDat(targId)) ^ (*appSmlt.GetDat(subId));
        }
        else if (lacType == LAC_TYPE::RAC) {
            auto pSpecLac = dynamic_pointer_cast<RacLAC>(pLac);
            boost::dynamic_bitset<ull> newValue(nFrame, 0);
            GetNewValue(appSmlt, pSpecLac->GetDivIds(), pSpecLac->GetSop(), newValue);
            isChanged = (*appSmlt.GetDat(targId)) ^ newValue;
        }
        else
            assert(0);
        for (int j = 0; j < appSmlt.GetPoNum(); ++j) {
            auto poId = appSmlt.GetPoId(j);
            appSmlt.SetTempDat(poId, *appSmlt.GetDat(poId) ^ (isChanged & bdPo2Nodes[j][targId]));
        }
        if (metrType == METR_TYPE::ER) {
            boost::dynamic_bitset <ull> diff(nFrame, 0);
            for (int j = 0; j < appSmlt.GetPoNum(); ++j)
                diff |= (*accSmlt.GetDat(accSmlt.GetPoId(j)) ^ *appSmlt.GetTempDat(appSmlt.GetPoId(j)));
            pLac->SetErr(diff.count() / (double)(nFrame));
        }
        else if (metrType == METR_TYPE::MED) {
            const ll warnPoint = numeric_limits <ll>::max() * 0.8;
            ll sed = 0;
            for (int iPatt = 0; iPatt < nFrame; ++iPatt) {
                ll accOut = accSmlt.GetOutputFast(iPatt);
                ll appOut = appSmlt.GetTempOutputFast(iPatt);
                sed += abs(accOut - appOut);
                if (sed > warnPoint)
                    assert(0);
            }
            pLac->SetErr(sed / (double)(nFrame));
        }
        else if (metrType == METR_TYPE::MHD) {
            ll sumHammDist = 0;
            for (int j = 0; j < appSmlt.GetPoNum(); ++j) {
                auto diff = (*accSmlt.GetDat(accSmlt.GetPoId(j)) ^ *appSmlt.GetTempDat(appSmlt.GetPoId(j)));
                sumHammDist += diff.count();
            }
            pLac->SetErr(sumHammDist / (double)(nFrame));
        }
        else
            assert(0);
        ++progrDispl;
    }
}


void VECBEEMan::CalcChanges(LACMan& lacMan, IntVec& critGraph) {
    // local changes
    // nodeWrong[nodeId][bitId], whether the "nodeId"-th node changes after applying the candidate LAC on the node under the "bitId"-th input pattern
    std::vector<BitVec> nodeChange(appSmlt.GetIdMaxPlus1(), BitVec(nFrame, 0));
    for (int nodeId: critGraph) {
        if (appSmlt.IsObjPi(nodeId) || appSmlt.IsConst(nodeId))
            continue;
        auto pLac = lacMan.GetCandLac(nodeId);
        assert(pLac != nullptr);
        BitVec& isChanged = nodeChange[nodeId];
        if (lacType == LAC_TYPE::CONS) {
            auto pSpecLac = dynamic_pointer_cast<ConstLAC>(pLac);
            if (pSpecLac->IsConst0())
                isChanged.reset();
            else
                isChanged.set();
            isChanged = isChanged ^ (*appSmlt.GetDat(nodeId));
        }
        else if (lacType == LAC_TYPE::SASIMI) {
            auto pSpecLac = dynamic_pointer_cast<SasimiLAC>(pLac);
            auto subId = pSpecLac->GetSubId();
            boost::dynamic_bitset <ull> isChanged(nFrame, 0);
            if (!pSpecLac->GetIsInv())
                isChanged = (*appSmlt.GetDat(nodeId)) ^ (*appSmlt.GetDat(subId));
            else
                isChanged = ~(*appSmlt.GetDat(nodeId)) ^ (*appSmlt.GetDat(subId));
        }
        else if (lacType == LAC_TYPE::RAC) {
            auto pSpecLac = dynamic_pointer_cast<RacLAC>(pLac);
            boost::dynamic_bitset<ull> newValue(nFrame, 0);
            GetNewValue(appSmlt, pSpecLac->GetDivIds(), pSpecLac->GetSop(), newValue);
            isChanged = (*appSmlt.GetDat(nodeId)) ^ newValue;
        }
        else
            assert(0);
    }

    // global changes
    int nPo = appSmlt.GetPoNum();
    poChangeWithNode.resize(nPo);
    for (int poId = 0; poId < nPo; ++poId) {
        auto& changeWithNode = poChangeWithNode[poId];
        auto& bdPo2Node = bdPo2Nodes[poId];
        changeWithNode.resize(appSmlt.GetIdMaxPlus1());
        for (int nodeId: critGraph) {
            if (appSmlt.IsObjPi(nodeId) || appSmlt.IsConst(nodeId))
                continue;
            changeWithNode[nodeId] = nodeChange[nodeId] & bdPo2Node[nodeId];
        }
    }
}


void VECBEEMan::GetNewValue(Simulator & smlt, const IntVec& faninIds, const std::string & sop, boost::dynamic_bitset <ull> & value) {
    if (sop == " 0\n") {
        value.reset();
        return;
    }
    if (sop == " 1\n") {
        value.set();
        return;
    }
    char * pSop = const_cast <char *> (sop.c_str());
    int nVars = Abc_SopGetVarNum(pSop);
    #ifdef DEBUG
    assert(nVars == faninIds.size());
    #endif

    dynamic_bitset <ull> product(nFrame, 0);
    for (char * pCube = pSop; *pCube; pCube += nVars + 3) {
        bool isFirst = true;
        for (int i = 0; pCube[i] != ' '; i++) {
            int faninId = faninIds[i];
            switch (pCube[i]) {
                case '-':
                    continue;
                    break;
                case '0':
                    if (isFirst) {
                        isFirst = false;
                        product = ~(*smlt.GetDat(faninId));
                    }
                    else
                        product &= ~(*smlt.GetDat(faninId));
                    break;
                case '1':
                    if (isFirst) {
                        isFirst = false;
                        product = (*smlt.GetDat(faninId));
                    }
                    else
                        product &= (*smlt.GetDat(faninId));
                    break;
                default:
                    assert(0);
            }
        }
        if (isFirst) {
            isFirst = false;
            product.set();
        }
        #ifdef DEBUG
        assert(!isFirst);
        #endif
        if (pCube == pSop)
            value = product;
        else
            value |= product;
    }
}