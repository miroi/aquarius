/* Copyright (c) 2014, Devin Matthews
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following
 * conditions are met:
 *      * Redistributions of source code must retain the above copyright
 *        notice, this list of conditions and the following disclaimer.
 *      * Redistributions in binary form must reproduce the above copyright
 *        notice, this list of conditions and the following disclaimer in the
 *        documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL DEVIN MATTHEWS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE. */

#include "ccsdt_q_n.hpp"

using namespace std;
using namespace aquarius::op;
using namespace aquarius::cc;
using namespace aquarius::input;
using namespace aquarius::tensor;
using namespace aquarius::task;
using namespace aquarius::time;
using namespace aquarius::symmetry;

template <typename U>
CCSDT_Q_N<U>::CCSDT_Q_N(const string& name, const Config& config)
: Task("ccsdt(q-n)", name)
{
    vector<Requirement> reqs;
    reqs.push_back(Requirement("moints", "H"));
    reqs.push_back(Requirement("ccsdt.Hbar", "Hbar"));
    reqs.push_back(Requirement("ccsdt.T", "T"));
    reqs.push_back(Requirement("ccsdt.L", "L"));
    this->addProduct(Product("double", "E(2)", reqs));
    this->addProduct(Product("double", "E(3)", reqs));
    this->addProduct(Product("double", "E(4)", reqs));
}

template <typename U>
void CCSDT_Q_N<U>::run(task::TaskDAG& dag, const Arena& arena)
{
    const TwoElectronOperator<U>& H = this->template get<TwoElectronOperator<U>>("H");
    const STTwoElectronOperator<U>& Hbar = this->template get<STTwoElectronOperator<U>>("Hbar");

    const Space& occ = H.occ;
    const Space& vrt = H.vrt;
    const PointGroup& group = occ.group;

    Denominator<U> D(H);
    const ExcitationOperator  <U,3>& T = this->template get<ExcitationOperator  <U,3>>("T");
    const DeexcitationOperator<U,3>& L = this->template get<DeexcitationOperator<U,3>>("L");

    SpinorbitalTensor<U> FME(Hbar.getIA());
    SpinorbitalTensor<U> FAE(Hbar.getAB());
    SpinorbitalTensor<U> FMI(Hbar.getIJ());
    FME -= H.getIA();
    FAE -= H.getAB();
    FMI -= H.getIJ();

    const SpinorbitalTensor<U>& WMNEF = Hbar.getIJAB();
    const SpinorbitalTensor<U>& WAMEF = Hbar.getAIBC();
    const SpinorbitalTensor<U>& WABEJ = Hbar.getABCI();
    const SpinorbitalTensor<U>& WABEF = Hbar.getABCD();
    const SpinorbitalTensor<U>& WMNIJ = Hbar.getIJKL();
    const SpinorbitalTensor<U>& WMNEJ = Hbar.getIJAK();
    const SpinorbitalTensor<U>& WAMIJ = Hbar.getAIJK();
    const SpinorbitalTensor<U>& WAMEI = Hbar.getAIBJ();

    SpinorbitalTensor<U> WABCEJK("W(abc,ejk)", arena, group, {vrt,occ}, {3,0}, {1,2});
    SpinorbitalTensor<U> WABMIJK("W(abm,ijk)", arena, group, {vrt,occ}, {2,1}, {0,3});
    SpinorbitalTensor<U> WAMNIJK("W(amn,ijk)", arena, group, {vrt,occ}, {1,2}, {0,3});
    SpinorbitalTensor<U> WABMEJI("W(abm,ejk)", arena, group, {vrt,occ}, {2,1}, {1,2});

    ExcitationOperator<U,4> T_1("T^(1)", arena, occ, vrt);
    ExcitationOperator<U,4> T_2("T^(2)", arena, occ, vrt);
    ExcitationOperator<U,4> T_3("T^(3)", arena, occ, vrt);

    DeexcitationOperator<U,4> L_1("L^(1)", arena, occ, vrt);
    DeexcitationOperator<U,4> L_2("L^(2)", arena, occ, vrt);
    DeexcitationOperator<U,4> L_3("L^(3)", arena, occ, vrt);

    ExcitationOperator<U,4> DT_1("DT^(1)", arena, occ, vrt);
    ExcitationOperator<U,4> DT_2("DT^(2)", arena, occ, vrt);
    ExcitationOperator<U,4> DT_3("DT^(3)", arena, occ, vrt);

    DeexcitationOperator<U,4> DL_1("DL^(1)", arena, occ, vrt);
    DeexcitationOperator<U,4> DL_2("DL^(2)", arena, occ, vrt);
    DeexcitationOperator<U,4> DL_3("DL^(3)", arena, occ, vrt);

    SpinorbitalTensor<U> WABCEJK_1(WABCEJK);
    SpinorbitalTensor<U> WABMIJK_1(WABMIJK);

    SpinorbitalTensor<U> FMI_2(FMI);
    SpinorbitalTensor<U> FAE_2(FAE);
    SpinorbitalTensor<U> WAMEI_2(WAMEI);
    SpinorbitalTensor<U> WMNIJ_2(WMNIJ);
    SpinorbitalTensor<U> WABEF_2(WABEF);
    SpinorbitalTensor<U> WABEJ_2(WABEJ);
    SpinorbitalTensor<U> WAMIJ_2(WAMIJ);
    SpinorbitalTensor<U> WABCEJK_2(WABCEJK);
    SpinorbitalTensor<U> WABMIJK_2(WABMIJK);
    SpinorbitalTensor<U> WAMNIJK_2(WAMNIJK);

    SpinorbitalTensor<U> DAI_1(T(1));
    SpinorbitalTensor<U> GIJAK_1(WMNEJ);
    SpinorbitalTensor<U> GAIBC_1(WAMEF);
    SpinorbitalTensor<U> GAIJK_1(WAMIJ);
    SpinorbitalTensor<U> GABCI_1(WABEJ);
    SpinorbitalTensor<U> GABCD_1(WABEF);
    SpinorbitalTensor<U> GAIBJ_1(WAMEI);
    SpinorbitalTensor<U> GIJKL_1(WMNIJ);
    SpinorbitalTensor<U> GAIJBCD_1("G(aij,bcd)", arena, group, {vrt,occ}, {1,2}, {3,0});
    SpinorbitalTensor<U> GIJKABL_1("G(ijk,abl)", arena, group, {vrt,occ}, {0,3}, {2,1});
    SpinorbitalTensor<U> GIJKALM_1("G(ijk,alm)", arena, group, {vrt,occ}, {0,3}, {1,2});

    SpinorbitalTensor<U> GAIJBCD_2("G(aij,bcd)", arena, group, {vrt,occ}, {1,2}, {3,0});
    SpinorbitalTensor<U> GIJKABL_2("G(ijk,abl)", arena, group, {vrt,occ}, {0,3}, {2,1});

    printf("WABEJ: %15.12f\n", WABEJ({1,0},{0,1}).norm(2));
    printf("WAMIJ: %15.12f\n", WAMIJ({1,0},{0,1}).norm(2));
    printf("T2: %15.12f\n", T(2)({1,0},{0,1}).norm(2));
    printf("L2: %15.12f\n", L(2)({0,1},{1,0}).norm(2));
    printf("T3: %15.12f\n", scalar(T(3)*T(3)));

    /***************************************************************************
     *
     * T^(1)
     *
     **************************************************************************/

    //WABCEJK[  "abcejk"]  =       WABEF[  "abef"]*T(2)[    "fcjk"];

    //WABMIJK[  "abmijk"]  =       WAMEI[  "amek"]*T(2)[    "ebij"];
    //WABMIJK[  "abmijk"] -=       WMNIJ[  "nmjk"]*T(2)[    "abin"];
    //WABMIJK[  "abmijk"] +=   0.5*WAMEF[  "bmef"]*T(3)[  "aefijk"];

    //WAMNIJK[  "amnijk"]  =       WMNEJ[  "mnek"]*T(2)[    "aeij"];
    //WAMNIJK[  "amnijk"] +=   0.5*WMNEF[  "mnef"]*T(3)[  "aefijk"];

    //WABMEJI[  "abmeji"]  =       WAMEF[  "amef"]*T(2)[    "bfji"];
    //WABMEJI[  "abmeji"] -=       WMNEJ[  "nmei"]*T(2)[    "abnj"];
    //WABMEJI[  "abmeji"] +=   0.5*WMNEF[  "mnef"]*T(3)[  "abfnji"];

    //DT_1(4)["abcdijkl"]  =     WABCEJK["abcejk"]*T(2)[    "edil"];
    //DT_1(4)["abcdijkl"] -=     WABMIJK["abmijk"]*T(2)[    "cdml"];
    //DT_1(4)["abcdijkl"] +=       WABEJ[  "abej"]*T(3)[  "ecdikl"];
    DT_1(4)["abcdijkl"] -=       WAMIJ[  "amij"]*T(3)[  "bcdmkl"];
    //DT_1(4)["abcdijkl"] += 0.5*WAMNIJK["amnijk"]*T(3)[  "bcdmnl"];
    //DT_1(4)["abcdijkl"] -=     WABMEJI["abmeji"]*T(3)[  "ecdmkl"];

    T_1 = DT_1;
    T_1.weight(D);

    /***************************************************************************
     *
     * T^(2)
     *
     **************************************************************************

    WABCEJK_1[  "abcejk"]  =  -0.5*WMNEF[  "mnef"]*T_1(4)["abcfmjkn"];
    WABMIJK_1[  "abmijk"]  =   0.5*WMNEF[  "mnef"]*T_1(4)["abefijkn"];

      DT_2(2)[    "abij"] +=  0.25*WMNEF[  "mnef"]*T_1(4)["abefijmn"];

      DT_2(3)[  "abcijk"] +=         FME[    "me"]*T_1(4)["abceijkm"];
      DT_2(3)[  "abcijk"] +=   0.5*WAMEF[  "amef"]*T_1(4)["efbcimjk"];
      DT_2(3)[  "abcijk"] -=   0.5*WMNEJ[  "mnek"]*T_1(4)["abecijmn"];

      DT_2(4)["abcdijkl"]  =   WABCEJK_1["abcejk"]*  T(2)[    "edil"];
      DT_2(4)["abcdijkl"] -=   WABMIJK_1["abmijk"]*  T(2)[    "cdml"];
      DT_2(4)["abcdijkl"] +=         FAE[    "ae"]*T_1(4)["ebcdijkl"];
      DT_2(4)["abcdijkl"] -=         FMI[    "mi"]*T_1(4)["abcdmjkl"];
      DT_2(4)["abcdijkl"] +=   0.5*WABEF[  "abef"]*T_1(4)["efcdijkl"];
      DT_2(4)["abcdijkl"] +=   0.5*WMNIJ[  "mnij"]*T_1(4)["abcdmnkl"];
      DT_2(4)["abcdijkl"] +=       WAMEI[  "amei"]*T_1(4)["ebcdjmkl"];

    T_2 = DT_2;
    T_2.weight(D);

    /***************************************************************************
     *
     * T^(3)
     *
     **************************************************************************

      WABCEJK[  "abcejk"] -=         FME[    "me"]*  T(3)[  "abcmjk"];
      WABCEJK[  "abcejk"] -=       WAMEI[  "amej"]*  T(2)[    "bcmk"];
      WABCEJK[  "abcejk"] +=   0.5*WMNEJ[  "mnej"]*  T(3)[  "abcmnk"];
      WABCEJK[  "abcejk"] +=       WAMEF[  "amef"]*  T(3)[  "fbcmjk"];

      WABMIJK[  "abmijk"] +=         FME[    "me"]*  T(3)[  "abeijk"];
      WABMIJK[  "abmijk"] +=       WMNEJ[  "nmek"]*  T(3)[  "abeijn"];

        FMI_2[      "mi"]  =   0.5*WMNEF[  "mnef"]*T_2(2)[    "efin"];
        FAE_2[      "ae"]  =  -0.5*WMNEF[  "mnef"]*T_2(2)[    "afmn"];

      WAMIJ_2[    "amij"]  =       WMNEJ[  "nmej"]*T_2(2)[    "aein"];
      WAMIJ_2[    "amij"] +=   0.5*WAMEF[  "amef"]*T_2(2)[    "efij"];
      WAMIJ_2[    "amij"] +=   0.5*WMNEF[  "mnef"]*T_2(3)[  "aefijn"];

      WABEJ_2[    "abej"]  =       WAMEF[  "amef"]*T_2(2)[    "fbmj"];
      WABEJ_2[    "abej"] +=   0.5*WMNEJ[  "mnej"]*T_2(2)[    "abmn"];
      WABEJ_2[    "abej"] -=   0.5*WMNEF[  "mnef"]*T_2(3)[  "afbmnj"];

      WAMEI_2[    "amei"]  =       WMNEF[  "mnef"]*T_2(2)[    "afni"];

      WMNIJ_2[    "mnij"]  =   0.5*WMNEF[  "mnef"]*T_2(2)[    "efij"];

      WABEF_2[    "abef"]  =   0.5*WMNEF[  "mnef"]*T_2(2)[    "abmn"];

    WABCEJK_2[  "abcejk"]  =    -WAMEI_2[  "amej"]*  T(2)[    "bcmk"];
    WABCEJK_2[  "abcejk"] +=     WABEF_2[  "abef"]*  T(2)[    "fcjk"];
    WABCEJK_2[  "abcejk"] -=   0.5*WMNEF[  "mnef"]*T_2(4)["abcfmjkn"];

    WABMIJK_2[  "abmijk"]  =     WAMEI_2[  "amek"]*  T(2)[    "ebij"];
    WABMIJK_2[  "abmijk"] -=     WMNIJ_2[  "nmjk"]*  T(2)[    "abmn"];
    WABMIJK_2[  "abmijk"] +=   0.5*WAMEF[  "bmef"]*T_2(3)[  "aefijk"];
    WABMIJK_2[  "abmijk"] +=   0.5*WMNEF[  "mnef"]*T_2(4)["abefijkn"];

    WAMNIJK_2[  "amnijk"]  =   0.5*WMNEF[  "mnef"]*T_2(3)[  "aefijk"];

      DT_3(1)[      "ai"] +=         FME[    "me"]*T_2(2)[    "aeim"];
      DT_3(1)[      "ai"] +=   0.5*WAMEF[  "amef"]*T_2(2)[    "efim"];
      DT_3(1)[      "ai"] -=   0.5*WMNEJ[  "mnei"]*T_2(2)[    "eamn"];
      DT_3(1)[      "ai"] +=  0.25*WMNEF[  "mnef"]*T_2(3)[  "aefimn"];

      DT_3(2)[    "abij"]  =       FAE_2[    "af"]*  T(2)[    "fbij"];
      DT_3(2)[    "abij"] -=       FMI_2[    "ni"]*  T(2)[    "abnj"];
      DT_3(2)[    "abij"] +=         FAE[    "af"]*T_2(2)[    "fbij"];
      DT_3(2)[    "abij"] -=         FMI[    "ni"]*T_2(2)[    "abnj"];
      DT_3(2)[    "abij"] +=   0.5*WABEF[  "abef"]*T_2(2)[    "efij"];
      DT_3(2)[    "abij"] +=   0.5*WMNIJ[  "mnij"]*T_2(2)[    "abmn"];
      DT_3(2)[    "abij"] +=       WAMEI[  "amei"]*T_2(2)[    "ebjm"];
      DT_3(2)[    "abij"] +=   0.5*WAMEF[  "bmef"]*T_2(3)[  "aefijm"];
      DT_3(2)[    "abij"] -=   0.5*WMNEJ[  "mnej"]*T_2(3)[  "abeinm"];
      DT_3(2)[    "abij"] +=         FME[    "me"]*T_2(3)[  "abeijm"];
      DT_3(2)[    "abij"] +=  0.25*WMNEF[  "mnef"]*T_2(4)["abefijmn"];

      DT_3(3)[  "abcijk"]  =     WABEJ_2[  "bcek"]*  T(2)[    "aeij"];
      DT_3(3)[  "abcijk"] -=     WAMIJ_2[  "bmjk"]*  T(2)[    "acim"];
      DT_3(3)[  "abcijk"] +=       WABEJ[  "bcek"]*T_2(2)[    "aeij"];
      DT_3(3)[  "abcijk"] -=       WAMIJ[  "bmjk"]*T_2(2)[    "acim"];
      DT_3(3)[  "abcijk"] +=         FAE[    "ce"]*T_2(3)[  "abeijk"];
      DT_3(3)[  "abcijk"] -=         FMI[    "mk"]*T_2(3)[  "abcijm"];
      DT_3(3)[  "abcijk"] +=   0.5*WABEF[  "abef"]*T_2(3)[  "efcijk"];
      DT_3(3)[  "abcijk"] +=   0.5*WMNIJ[  "mnij"]*T_2(3)[  "abcmnk"];
      DT_3(3)[  "abcijk"] +=       WAMEI[  "amei"]*T_2(3)[  "ebcjmk"];
      DT_3(3)[  "abcijk"] +=         FME[    "me"]*T_2(4)["abceijkm"];
      DT_3(3)[  "abcijk"] +=   0.5*WAMEF[  "amef"]*T_2(4)["efbcimjk"];
      DT_3(3)[  "abcijk"] -=   0.5*WMNEJ[  "mnek"]*T_2(4)["abecijmn"];

      DT_3(4)["abcdijkl"]  =   WABCEJK_2["abcejk"]*  T(2)[    "edil"];
      DT_3(4)["abcdijkl"] -=   WABMIJK_2["abmijk"]*  T(2)[    "cdml"];
      DT_3(4)["abcdijkl"]  =     WABCEJK["abcejk"]*T_2(2)[    "edil"];
      DT_3(4)["abcdijkl"] -=     WABMIJK["abmijk"]*T_2(2)[    "cdml"];
      DT_3(4)["abcdijkl"] +=       WABEJ[  "abej"]*T_2(3)[  "ecdikl"];
      DT_3(4)["abcdijkl"] -=       WAMIJ[  "amij"]*T_2(3)[  "bcdmkl"];
      DT_3(4)["abcdijkl"] += 0.5*WAMNIJK["amnijk"]*T_2(3)[  "bcdmnl"];
      DT_3(4)["abcdijkl"] -=     WABMEJI["abmeji"]*T_2(3)[  "ecdmkl"];
      DT_3(4)["abcdijkl"] +=         FAE[    "ae"]*T_2(4)["ebcdijkl"];
      DT_3(4)["abcdijkl"] -=         FMI[    "mi"]*T_2(4)["abcdmjkl"];
      DT_3(4)["abcdijkl"] +=   0.5*WABEF[  "abef"]*T_2(4)["efcdijkl"];
      DT_3(4)["abcdijkl"] +=   0.5*WMNIJ[  "mnij"]*T_2(4)["abcdmnkl"];
      DT_3(4)["abcdijkl"] +=       WAMEI[  "amei"]*T_2(4)["ebcdjmkl"];

    T_3 = DT_3;
    T_3.weight(D);

    /***************************************************************************
     *
     * L^(1)
     *
     **************************************************************************/

    DL_1(4)["ijklabcd"]  = WMNEF["ijab"]*L(2)[  "klcd"];
    //DL_1(4)["ijklabcd"] +=   FME[  "ia"]*L(3)["jklbcd"];
    //DL_1(4)["ijklabcd"] += WAMEF["elcd"]*L(3)["ijkabe"];
    //DL_1(4)["ijklabcd"] -= WMNEJ["ijam"]*L(3)["mklbcd"];

    L_1 = DL_1;
    L_1.weight(D);

    /***************************************************************************
     *
     * L^(2)
     *
     **************************************************************************

      GIJAK_1[    "ijak"]  =  (1.0/12.0)*   T(3)[  "efgkmn"]*   L_1(4)["ijmnaefg"];
      GAIBC_1[    "aibc"]  = -(1.0/12.0)*   T(3)[  "aefmno"]*   L_1(4)["minobcef"];

        DAI_1[      "ai"]  =  (1.0/36.0)* T_1(4)["efgamnoi"]*     L(3)[  "mnoefg"];
        DAI_1[      "ai"] +=  (1.0/ 2.0)*   T(2)[    "efim"]*  GAIBC_1[    "amef"];
        DAI_1[      "ai"] -=  (1.0/ 2.0)*   T(2)[    "eamn"]*  GIJAK_1[    "mnei"];

      GAIJK_1[    "aijk"]  =  (1.0/12.0)* T_1(4)["aefgjkmn"]*     L(3)[  "imnefg"];
      GAIJK_1[    "aijk"] +=  (1.0/ 6.0)*   T(3)[  "efgjkm"]*GAIJBCD_1[  "aimefg"];
      GAIJK_1[    "aijk"] +=  (1.0/ 4.0)*   T(3)[  "aefjmn"]*GIJKABL_1[  "mniefk"];

      GABCI_1[    "abci"]  = -(1.0/12.0)* T_1(4)["abefmino"]*     L(3)[  "mnocef"];
      GABCI_1[    "abci"] +=  (1.0/ 6.0)*   T(3)[  "eabmno"]*GIJKABL_1[  "mnoeci"];
      GABCI_1[    "abci"] +=  (1.0/ 4.0)*   T(3)[  "efbmni"]*GAIJBCD_1[  "amncef"];

    GIJKABL_1[  "ijkabl"]  =  (1.0/ 2.0)*   T(2)[    "eflm"]*   L_1(4)["ijkmabef"];
    GAIJBCD_1[  "aijbcd"]  = -(1.0/ 2.0)*   T(2)[    "aemn"]*   L_1(4)["mijnbcde"];
    GIJKALM_1[  "ijkalm"]  =  (1.0/ 6.0)*   T(3)[  "efglmn"]*   L_1(4)["ijknaefg"];

      GABCD_1[    "abcd"]  = -(1.0/ 2.0)*   T(2)[    "bemn"]*GAIJBCD_1[  "amncde"];
      GAIBJ_1[    "aibj"]  =  (1.0/ 2.0)*   T(2)[    "aenm"]*GIJKABL_1[  "minebj"];
      GIJKL_1[    "ijkl"]  =  (1.0/ 2.0)*   T(2)[    "efmk"]*GIJKABL_1[  "mijefl"];

      DL_2(1)[      "ia"] +=               WMNEF[    "miea"]*    DAI_1[      "em"];
      DL_2(1)[      "ia"] +=               WMNEF[    "imef"]*  GABCI_1[    "efam"];
      DL_2(1)[      "ia"] -=               WMNEF[    "mnea"]*  GAIJK_1[    "eimn"];
      DL_2(1)[      "ia"] -=  (1.0/ 2.0)*  WABEF[    "efga"]*  GAIBC_1[    "gief"];
      DL_2(1)[      "ia"] +=               WAMEI[    "eifm"]*  GAIBC_1[    "fmea"];
      DL_2(1)[      "ia"] -=               WAMEI[    "eman"]*  GIJAK_1[    "inem"];
      DL_2(1)[      "ia"] +=  (1.0/ 2.0)*  WMNIJ[    "imno"]*  GIJAK_1[    "noam"];
      DL_2(1)[      "ia"] -=  (1.0/ 2.0)*  WAMEF[    "gief"]*  GABCD_1[    "efga"];
      DL_2(1)[      "ia"] +=               WAMEF[    "fmea"]*  GAIBJ_1[    "eifm"];
      DL_2(1)[      "ia"] -=               WMNEJ[    "inem"]*  GAIBJ_1[    "eman"];
      DL_2(1)[      "ia"] +=  (1.0/ 2.0)*  WMNEJ[    "noam"]*  GIJKL_1[    "imno"];

      DL_2(2)[    "ijab"] -=               WAMEF[    "fiae"]*  GAIBC_1[    "ejbf"];
      DL_2(2)[    "ijab"] -=               WMNEJ[    "ijem"]*  GAIBC_1[    "emab"];
      DL_2(2)[    "ijab"] -=               WAMEF[    "emab"]*  GIJAK_1[    "ijem"];
      DL_2(2)[    "ijab"] -=               WMNEJ[    "niam"]*  GIJAK_1[    "mjbn"];
      DL_2(2)[    "ijab"] +=  (1.0/ 2.0)*  WMNEF[    "ijef"]*  GABCD_1[    "efab"];
      DL_2(2)[    "ijab"] +=               WMNEF[    "imea"]*  GAIBJ_1[    "ejbm"];
      DL_2(2)[    "ijab"] +=  (1.0/ 2.0)*  WMNEF[    "mnab"]*  GIJKL_1[    "ijmn"];
      DL_2(2)[    "ijab"] +=  (1.0/ 2.0)*WABCEJK[  "efgbmn"]*   L_1(4)["ijmnaefg"];
      DL_2(2)[    "ijab"] -=  (1.0/ 2.0)*WABMIJK[  "efimno"]*   L_1(4)["mnojefab"];

      DL_2(3)[  "ijkabc"] +=               WMNEF[    "ijae"]*  GAIBC_1[    "ekbc"];
      DL_2(3)[  "ijkabc"] -=               WMNEF[    "mkbc"]*  GIJAK_1[    "ijam"];
      DL_2(3)[  "ijkabc"] +=  (1.0/ 2.0)*  WMNEF[    "mnbc"]*GIJKALM_1[  "ijkamn"];
      DL_2(3)[  "ijkabc"] -=               WAMEF[    "embc"]*GIJKABL_1[  "ijkaem"];
      DL_2(3)[  "ijkabc"] +=  (1.0/ 2.0)*  WABEJ[    "efcm"]*   L_1(4)["ijkmabef"];
      DL_2(3)[  "ijkabc"] -=  (1.0/ 2.0)*  WAMIJ[    "eknm"]*   L_1(4)["ijmnabce"];
      DL_2(3)[  "ijkabc"] -=  (1.0/ 4.0)*WABMEJI[  "efkcnm"]*   L_1(4)["ijmnabef"];
      DL_2(3)[  "ijkabc"] +=  (1.0/ 6.0)*WAMNIJK[  "eijmno"]*   L_1(4)["mnokeabc"];

      DL_2(4)["ijklabcd"] +=               WMNEF[    "ijae"]*GAIJBCD_1[  "eklbcd"];
      DL_2(4)["ijklabcd"] -=               WMNEF[    "mlcd"]*GIJKABL_1[  "ijkabm"];
      DL_2(4)["ijklabcd"] +=                 FAE[      "ea"]*   L_1(4)["ijklebcd"];
      DL_2(4)["ijklabcd"] -=                 FMI[      "im"]*   L_1(4)["mjklabcd"];
      DL_2(4)["ijklabcd"] +=  (1.0/ 2.0)*  WABEF[    "efab"]*   L_1(4)["ijklefcd"];
      DL_2(4)["ijklabcd"] +=  (1.0/ 2.0)*  WMNIJ[    "ijmn"]*   L_1(4)["mnklabcd"];
      DL_2(4)["ijklabcd"] +=               WAMEI[    "eiam"]*   L_1(4)["mjklbecd"];

    L_2 = DL_2;
    L_2.weight(D);

    /***************************************************************************
     *
     * L^(3)
     *
     **************************************************************************

    GIJKABL_2[  "ijkabl"]  =  (1.0/ 2.0)*   T(2)["eflm"]*   L_2(4)["ijkmabef"];
    GAIJBCD_2[  "aijbcd"]  = -(1.0/ 2.0)*   T(2)["aemn"]*   L_2(4)["mijnbcde"];

      DL_3(4)["ijklabcd"]  =               WMNEF["ijab"]*     L(2)[    "klcd"];
      DL_3(4)["ijklabcd"] +=                 FME[  "ia"]*     L(3)[  "jklbcd"];
      DL_3(4)["ijklabcd"] +=               WAMEF["ejab"]*     L(3)[  "iklecd"];
      DL_3(4)["ijklabcd"] -=               WMNEJ["ijam"]*     L(3)[  "mklbcd"];
      DL_3(4)["ijklabcd"] +=               WMNEF["ijae"]*GAIJBCD_2[  "eklbcd"];
      DL_3(4)["ijklabcd"] -=               WMNEF["mlcd"]*GIJKABL_2[  "ijkabm"];
      DL_3(4)["ijklabcd"] +=                 FAE[  "ea"]*   L_2(4)["ijklebcd"];
      DL_3(4)["ijklabcd"] -=                 FMI[  "im"]*   L_2(4)["mjklabcd"];
      DL_3(4)["ijklabcd"] +=  (1.0/ 2.0)*  WABEF["efab"]*   L_2(4)["ijklefcd"];
      DL_3(4)["ijklabcd"] +=  (1.0/ 2.0)*  WMNIJ["ijmn"]*   L_2(4)["mnklabcd"];
      DL_3(4)["ijklabcd"] +=               WAMEI["eiam"]*   L_2(4)["mjklbecd"];

    L_3 = DL_3;
    L_3.weight(D);

    /***************************************************************************
     *
     * CCSDT(Q-2)
     *
     **************************************************************************/

    printf("DT4_1: %15.12f\n", scalar(DT_1(4)({0,0},{0,0})*DT_1(4)({0,0},{0,0})));
    printf("DT4_2: %15.12f\n", scalar(DT_1(4)({1,0},{0,1})*DT_1(4)({1,0},{0,1}))*16);
    printf("DT4_3: %15.12f\n", scalar(DT_1(4)({2,0},{0,2})*DT_1(4)({2,0},{0,2}))*36);
    printf("DT4_4: %15.12f\n", scalar(DT_1(4)({3,0},{0,3})*DT_1(4)({3,0},{0,3}))*16);
    printf("DT4_5: %15.12f\n", scalar(DT_1(4)({4,0},{0,4})*DT_1(4)({4,0},{0,4})));

    printf("DT4: %15.12f\n", scalar(DT_1(4)({0,0},{0,0})*DT_1(4)({0,0},{0,0}))+
                             scalar(DT_1(4)({1,0},{0,1})*DT_1(4)({1,0},{0,1}))*16+
                             scalar(DT_1(4)({2,0},{0,2})*DT_1(4)({2,0},{0,2}))*36+
                             scalar(DT_1(4)({3,0},{0,3})*DT_1(4)({3,0},{0,3}))*16+
                             scalar(DT_1(4)({4,0},{0,4})*DT_1(4)({4,0},{0,4})));
    printf("DT4: %15.12f\n", scalar(DT_1(4)*DT_1(4)));
    printf("DL4: %15.12f\n", scalar(DL_1(4)*DL_1(4)));

    U E101 = (1.0/576.0)*scalar(L_1(4)["mnopefgh"]*DT_1(4)["efghmnop"]);
    U E2 = E101;

    /***************************************************************************
     *
     * CCSDT(Q-3)
     *
     **************************************************************************

    U E201 = (1.0/576.0)*scalar(L_2(4)["mnopefgh"]*DT_1(4)["efghmnop"]);
    U E102 = (1.0/576.0)*scalar(L_1(4)["mnopefgh"]*DT_2(4)["efghmnop"]);

    printf("E201: %18.15f\n", E201);
    printf("E102: %18.15f\n", E102);
    printf("\n");

    U E3 = E102;

    /***************************************************************************
     *
     * CCSDT(Q-4)
     *
     **************************************************************************

    U E301 =  (1.0/576.0)*scalar(L_3(4)["mnopefgh"]*DT_1(4)["efghmnop"]);
    U E202 =  (1.0/  1.0)*scalar(L_2(1)[      "me"]*DT_2(1)[      "em"])
             +(1.0/  4.0)*scalar(L_2(2)[    "mnef"]*DT_2(2)[    "efmn"])
             +(1.0/ 36.0)*scalar(L_2(3)[  "mnoefg"]*DT_2(3)[  "efgmno"])
             +(1.0/576.0)*scalar(L_2(4)["mnopefgh"]*DT_2(4)["efghmnop"]);
    U E103 =  (1.0/576.0)*scalar(L_1(4)["mnopefgh"]*DT_3(4)["efghmnop"]);

    printf("E301: %18.15f\n", E301);
    printf("E202: %18.15f\n", E202);
    printf("E103: %18.15f\n", E103);
    printf("\n");

    U E4 = E202;
    */

    printf("CCSDT(Q-2): %18.15f\n", E2);
    //printf("CCSDT(Q-3): %18.15f\n", E3);
    //printf("CCSDT(Q-4): %18.15f\n", E4);

    this->put("E(2)", new U(E2));
    //this->put("E(3)", new U(E3));
    //this->put("E(4)", new U(E4));
}

INSTANTIATE_SPECIALIZATIONS(CCSDT_Q_N);
REGISTER_TASK(CCSDT_Q_N<double>,"ccsdt(q-n)");
