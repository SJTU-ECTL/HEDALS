#include "cmdline.hpp"
#include "header.h"
#include "my_abc.h"
#include "dals.h"


using namespace abc;
using namespace boost;
using namespace cmdline;
using namespace std;


parser CommPars(int argc, char * argv[]) {
    parser option;
    option.add <string> ("accCirc", 'i', "path to accurate circuit", true);
    option.add <string> ("appCirc", '\0', "path to approximate circuit", false);
    option.add <string> ("standCell", 'l', "path to standard cell library", false, "input/standard-cell/mcnc.genlib");
    option.add <string> ("outpPath", 'o', "path to approximate circuits", false, "tmp");
    option.add <string> ("lacType", '\0', "lac type: CONST, SASIMI, RAC", false, "CONST");
    option.add <string> ("metrType", '\0', "error metric type: ER, MED, MHD", false, "ER");
    option.add <string> ("distrType", '\0', "error distribution type: UNIF, ENUM", false, "UNIF");
    option.add <string> ("mapType", '\0', "mapping type: SCL, LUT", false, "SCL");
    option.add <unsigned> ("sourceSeed", 's', "seed for randomness", false, 0);
    option.add <int> ("nFrame", 'f', "#Monte Carlo samples, nFrame should be an integer multiple of 64", false, 100032);
    option.add <int> ("mode", 'm', "mode selection, 0 or 1\n\t\t\t0: priority cut\n\t\t\t1: maximum flow", false, 0);
    option.add <int> ("usePostProc", '\0', "whether to use post processing for further reducing the area", false, 0);
    option.add <int> ("cutSizeLim", '\0', "size limit of priority cuts", false, 16);
    option.add <double> ("errUppBound", 'e', "error upper bound", false, 0.15);
    option.parse_check(argc, argv);
    return option;
}


void DALS(Abc_Ntk_t * pNtk, unsigned sourceSeed, int nFrame, int mode, int usePostProc, int cutSizeLim, double errUppBound, const string & outpPath, const string & lacTypeStr, const string & metrTypeStr, const string & distrTypeStr, const string & mapTypeStr) {
    LAC_TYPE lacType;
    if (lacTypeStr == "CONST")
        lacType = LAC_TYPE::CONS;
    else if (lacTypeStr == "SASIMI")
        lacType = LAC_TYPE::SASIMI;
    else if (lacTypeStr == "RAC")
        lacType = LAC_TYPE::RAC;
    else
        assert(0);
    METR_TYPE metrType;
    if (metrTypeStr == "ER")
        metrType = METR_TYPE::ER;
    else if (metrTypeStr == "MED")
        metrType = METR_TYPE::MED;
    else if (metrTypeStr == "MHD")
        metrType = METR_TYPE::MHD;
    else
        assert(0);
    DISTR_TYPE distrType;
    if (distrTypeStr == "UNIF")
        distrType = DISTR_TYPE::UNIF;
    else if (distrTypeStr == "ENUM") {
        distrType = DISTR_TYPE::ENUM;
        #ifdef DEBUG
        assert(Abc_NtkPiNum(pNtk) < 20);
        #endif
        nFrame = 1 << Abc_NtkPiNum(pNtk);
        cout << "nFrame for enumeration = " << nFrame << endl;
    }
    else
        assert(0);
    MAP_TYPE mapType;
    if (mapTypeStr == "SCL")
        mapType = MAP_TYPE::SCL;
    else if (mapTypeStr == "LUT")
        mapType = MAP_TYPE::LUT;
    else   
        assert(0);
    DALSMan dalsMan(pNtk, sourceSeed, nFrame, mode, usePostProc, cutSizeLim, errUppBound, lacType, metrType, distrType, mapType);
    dalsMan.Run(outpPath);
}


int main(int argc, char * argv[]) {
    GlobStartAbc();

    parser option = CommPars(argc, argv);
    string accCirc = option.get <string> ("accCirc");
    string appCirc = option.get <string> ("appCirc");
    string standCell = option.get <string> ("standCell");
    string outpPath = option.get <string> ("outpPath");
    string lacType = option.get <string> ("lacType");
    string metrType = option.get <string> ("metrType");
    string distrType = option.get <string> ("distrType");
    string mapType = option.get <string> ("mapType");
    unsigned sourceSeed = option.get <unsigned> ("sourceSeed");
    int nFrame = option.get <int> ("nFrame");
    int mode = option.get <int> ("mode");
    int usePostProc = option.get <int> ("usePostProc");
    int cutSizeLim = option.get <int> ("cutSizeLim");
    double errUppBound = option.get <double> ("errUppBound");

    FixPath(outpPath);
    CreatePath(outpPath);
    if (sourceSeed == 0) {
        random::mt19937 rng(time(0));
        boost::uniform_int <> unif(INT_MIN, INT_MAX);
        sourceSeed = static_cast <unsigned> (unif(rng));
    }

    AbcMan abcMan;
    if (standCell != "")
        abcMan.ReadStandCell(standCell);
    abcMan.ReadNet(accCirc);
    abcMan.PrintStat();

    if (appCirc != "") {
        NetMan accNet(abcMan.GetNet(), true);
        accNet.Comm("st; resyn2; ps; resyn2; ps; if -K 6; ps;");
        abcMan.ReadNet(appCirc);
        abcMan.PrintStat();
        NetMan appNet(abcMan.GetNet(), true);
        appNet.Comm("st; resyn2; ps; resyn2; ps; if -K 6; ps;");
        // cout << "ER = " << CalcErr(accNet, appNet, sourceSeed, nFrame, METR_TYPE::ER, DISTR_TYPE::UNIF) << endl;
        double med = CalcErr(accNet, appNet, sourceSeed, nFrame, METR_TYPE::MED, DISTR_TYPE::UNIF);
        // double med = CalcErr(accNet, appNet, sourceSeed, nFrame, METR_TYPE::MED, DISTR_TYPE::ENUM);
        cout << "MED = " << med << endl;
        cout << "NMED = " << med / (double)((1ll << accNet.GetPoNum()) - 1) << endl;
    }
    else
        DALS(abcMan.GetNet(), sourceSeed, nFrame, mode, usePostProc, cutSizeLim, errUppBound, outpPath, lacType, metrType, distrType, mapType);

    GlobStopAbc();
    return 0;
}
