/* Copyright (c) 2013, Devin Matthews
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

#include "cholesky.hpp"

#include <cassert>

using namespace std;
using namespace MPI;
using namespace aquarius::slide;
using namespace aquarius::input;
using namespace aquarius::tensor;

namespace aquarius
{
namespace scf
{

CholeskyIntegrals::CholeskyIntegrals(tCTF_World<double>& ctf, const Config& config, const Molecule& molecule)
: Distributed<double>(ctf),
  rank(0),
  molecule(molecule),
  shells(molecule.getShellsBegin(),molecule.getShellsEnd()),
  delta(config.get<double>("delta")),
  cond(config.get<double>("cond_max"))
{
    nfunc = 0;
    for (int i = 0;i < shells.size();i++) nfunc += shells[i].getNFunc()*shells[i].getNContr();
    ndiag = nfunc * (nfunc + 1) / 2;
    nblock = shells.size() * (shells.size() + 1) / 2;
    decompose();
}

CholeskyIntegrals::~CholeskyIntegrals()
{
    delete D;
    delete L;
}

void CholeskyIntegrals::decompose()
{
    int nproc, pid;

    context = new Context();

    pid = comm.Get_rank();
    nproc = comm.Get_size();

    diag_elem_t* diag = new diag_elem_t[ndiag];
    for (int j = 0, elem = 0;j < shells.size();j++)
    {
        for (int i = 0;i <= j;i++)
        {
            elem += getDiagonalBlock(shells[i], shells[j], diag+elem);
        }
    }

    int* block_start = new int[nblock];
    int* block_size = new int[nblock];
    sortBlocks(diag, block_start, block_size);

    int nblock_local = 0;
    for (int elem = 0;;)
    {
        int block = pid+nblock_local*nproc;

        if (block >= nblock) break;

        copy(diag+block_start[block], diag+block_start[block]+block_size[block], diag+elem);
        block_start[nblock_local] = elem;
        block_size[nblock_local] = block_size[block];
        elem += block_size[nblock_local];

        nblock_local++;
    }

    double** block_data = new double*[nblock_local];
    int max_block_size = 0;
    for (int block = 0;block < nblock_local;block++)
    {
        max_block_size = max(max_block_size, block_size[block]);
        block_data[block] = new double[block_size[block]*ndiag];
        fill(block_data[block], block_data[block]+block_size[block]*ndiag, 0.0);
    }

    comm.Allreduce(IN_PLACE, &max_block_size, 1, INT, MAX);

    //for (l = 0;l < ndiag;l++)
    //{
    //    if (diag[l].elem < delta * delta / crit) diag[l].elem = 0;
    //}

    double* D = new double[ndiag];
    double* tmp_block_data = new double[ndiag*max_block_size];
    diag_elem_t* tmp_diag = new diag_elem_t[max_block_size];
    for (rank = 0;;)
    {
        int converged = 1;
        double local_max = -2;
        int max_block = 0;
        for (int block = 0;block < nblock_local;block++)
        {
            double max_elem;
            //printf("checking block: %d\n", block);
            converged = isBlockConverged(diag+block_start[block], block_size[block], max_elem) && converged;
            if (max_elem > local_max)
            {
                local_max = max_elem;
                max_block = block;
            }
        }

        //printf("max block: %d\n", max_block);
        //printf("max elem: %e\n", local_max);

        comm.Allreduce(IN_PLACE, &converged, 1, INT, BAND);
        if (converged) break;

        double* maxes = new double[nproc];
        comm.Allgather(&local_max, 1, DOUBLE, maxes, 1, DOUBLE);

        int pmax = 0;
        double global_max = 0;
        for (int p = 0;p < nproc;p++)
        {
            if (maxes[p] > global_max)
            {
                global_max = maxes[p];
                pmax = p;
            }
        }

        delete[] maxes;

        int old_rank = rank;
        int nactive;

        if (pmax == pid)
        {
            cout << "Decomposing block " << pid+max_block*nproc << endl;

            decomposeBlock(block_size[max_block], block_data[max_block], D, diag+block_start[max_block]);

            nactive = collectActiveRows(block_size[max_block], block_data[max_block], diag+block_start[max_block],
                                        tmp_block_data, tmp_diag);
        }

        comm.Bcast(&rank, 1, INT, pmax);
        if (rank == old_rank) continue;

        comm.Bcast(&nactive, 1, INT, pmax);
        comm.Bcast(tmp_block_data, nactive*ndiag, DOUBLE, pmax);
        comm.Bcast(tmp_diag, nactive*sizeof(diag_elem_t), BYTE, pmax);
        comm.Bcast(D+old_rank, rank-old_rank, DOUBLE, pmax);

        for (int next_block = 0;next_block < nblock_local;next_block++)
        {
            if (next_block == max_block && pid == pmax) continue;

            updateBlock(old_rank, block_size[next_block], block_data[next_block], diag+block_start[next_block],
                                  nactive,                tmp_block_data,         tmp_diag,                     D);
        }
    }

    ASSERT(rank > 0, "rank is 0");

    PRINT("rank: full, partial: %d %d\n\n", ndiag, rank);

    for (int block = 0;block < nblock_local;block++)
        resortBlock(block_size[block], block_data[block], diag+block_start[block], tmp_block_data);

    delete[] tmp_block_data;
    delete[] tmp_diag;

    int sizer[] = {rank};
    int shapeN[] = {NS};
    this->D = new DistTensor<double>(ctf, 1, sizer, shapeN, false);
    int sizeffr[] = {nfunc,nfunc,rank};
    int shapeSNN[] = {SY,NS,NS};
    this->L = new DistTensor<double>(ctf, 3, sizeffr, shapeSNN, false);

    if (pid == 0)
    {
        kv_pair* pairs = new kv_pair[rank];
        for (int i = 0;i < rank;i++)
        {
            pairs[i].k = i;
            pairs[i].d = D[i];
        }
        this->D->writeRemoteData(rank, pairs);
        delete[] pairs;
    }
    else
    {
        this->D->writeRemoteData(0, NULL);
    }

    vector<kv_pair> pairs;
    for (int block = 0;block < nblock_local;block++)
    {
        int i = diag[block_start[block]].shelli;
        int j = diag[block_start[block]].shellj;

        //printf("\nblock %d\n", block);
        //print_matrix(block_data[block], block_size[block], rank);

        int elem = 0;
        for (int n = 0;n < shells[j].getNFunc()*shells[j].getNContr();n++)
        {
            for (int m = 0;m < shells[i].getNFunc()*shells[i].getNContr();m++)
            {
                if (i == j && m > n) break;

                int o = m + shells[i].getIdx()[0];
                int p = n + shells[j].getIdx()[0];
                //key idx = max(o,p)*(max(o,p)+1)/2 + min(o,p);
                key idx = max(o,p)*nfunc + min(o,p);
                //printf("%d %d %d %d\n", i, j, shells[i].getIdx()[0], shells[j].getIdx()[0]);
                for (int r = 0;r < rank;r++)
                {
                    //printf("%d %d %d %d %d %d %d %d %d %d\n", pid, idx, o, p, i, j, m, n, r, block);
                    pairs.push_back(kv_pair(idx, block_data[block][elem*rank+r]));
                    //idx += ndiag;
                    idx += nfunc*nfunc;
                }

                elem++;
            }
        }
    }
    this->L->writeRemoteData(pairs.size(), pairs.data());

    //cout << "D:" << endl;
    //this->D->print(cout);
    //cout << endl;

    //cout << "L:" << endl;
    //this->L->print(cout);
    //cout << endl;

    for (int i = 0;i < nblock_local;i++) delete[] block_data[i];
    delete[] D;
    delete[] diag;
    delete[] block_data;
    delete[] block_size;
    delete[] block_start;

    delete context;
}

void CholeskyIntegrals::resortBlock(const int block_size, double* L, diag_elem_t* diag, double* tmp)
{
    for (int elem = 0;elem < block_size;elem++)
    {
        dcopy(rank, L+elem*ndiag, 1, tmp+diag[elem].idx*rank, 1);
    }

    dcopy(rank*block_size, tmp, 1, L, 1);
}

int CholeskyIntegrals::collectActiveRows(const int block_size, const double* L, diag_elem_t* diag,
                                            double* L_active, diag_elem_t* diag_active)
{
    int cur = 0;

    for (int elem = 0;elem < block_size;elem++)
    {
        if (diag[elem].status == ACTIVE)
        {
            //printf("finishing row: %d\n", elem);
            diag[elem].status = DONE;
            diag_active[cur] = diag[elem];
            dcopy(rank, L+elem*ndiag, 1, L_active+cur*ndiag, 1);
            cur++;
        }
    }

    return cur;
}

bool CholeskyIntegrals::isBlockConverged(const diag_elem_t* diag, int block_size, double& max_elem)
{
    bool converged = true;
    max_elem = -1;

    for (int elem = 0;elem < block_size;elem++)
    {
        if (abs(diag[elem].elem) > delta && diag[elem].status == TODO)
        {
            max_elem = max(max_elem, fabs(diag[elem].elem));
            //printf("row %d isn't done yet: %e %e\n", elem, max_elem, fabs(diag[elem].elem));
            converged &= false;
        }
    }

    return converged;
}

void CholeskyIntegrals::sortBlocks(diag_elem_t* diag, int* block_start, int* block_size)
{
    sort(diag, diag+ndiag, sort_by_integral);

    int block = 0;
    for (int cur = 0;cur < ndiag;cur++)
    {
        ASSERT(block < nblock, "too many blocks");

        block_start[block] = cur;

        for (int end = cur+1;end < ndiag;end++)
        {
            if (diag[cur].shelli == diag[end].shelli &&
                diag[cur].shellj == diag[end].shellj)
            {
                rotate(diag+(++cur), diag+end, diag+end+1);
            }
        }

        block_size[block] = (cur+1)-block_start[block];
        block++;
    }

    ASSERT(block == nblock, "too few blocks");
}

void CholeskyIntegrals::getShellOffsets(const Context& context,
                                        const Shell& a, const Shell& b, const Shell& c, const Shell& d,
                                        size_t& controffa, size_t& funcoffa, size_t& controffb, size_t& funcoffb,
                                        size_t& controffc, size_t& funcoffc, size_t& controffd, size_t& funcoffd)
{
    controffa = 0;
    controffb = 0;
    controffc = 0;
    controffd = 0;
    funcoffa = 0;
    funcoffb = 0;
    funcoffc = 0;
    funcoffd = 0;

    size_t ncab = a.getNContr()*b.getNContr();
    size_t nccd = c.getNContr()*d.getNContr();
    size_t nfab = a.getNFunc()*b.getNFunc();
    size_t nfcd = c.getNFunc()*d.getNFunc();

    if (&context.getA() == &a || &context.getA() == &b)
    {
        if (&context.getA() == &a)
        {
            controffa += 1;
            controffb += a.getNContr();
            funcoffa += ncab*nccd;
            funcoffb += ncab*nccd*a.getNFunc();
        }
        else
        {
            controffa += b.getNContr();
            controffb += 1;
            funcoffa += ncab*nccd*b.getNFunc();
            funcoffb += ncab*nccd;
        }

        if (&context.getC() == &c)
        {
            controffc += ncab;
            controffd += ncab*c.getNContr();
            funcoffc += ncab*nccd*nfab;
            funcoffd += ncab*nccd*nfab*c.getNFunc();
        }
        else
        {
            controffc += ncab*d.getNContr();
            controffd += ncab;
            funcoffc += ncab*nccd*nfab*d.getNFunc();
            funcoffd += ncab*nccd*nfab;
        }
    }
    else
    {
        if (&context.getA() == &c)
        {
            controffc += 1;
            controffd += c.getNContr();
            funcoffc += ncab*nccd;
            funcoffd += ncab*nccd*c.getNFunc();
        }
        else
        {
            controffc += d.getNContr();
            controffd += 1;
            funcoffc += ncab*nccd*d.getNFunc();
            funcoffd += ncab*nccd;
        }

        if (&context.getC() == &a)
        {
            controffa += nccd;
            controffb += nccd*a.getNContr();
            funcoffa += ncab*nccd*nfcd;
            funcoffb += ncab*nccd*nfcd*a.getNFunc();
        }
        else
        {
            controffa += nccd*b.getNContr();
            controffb += nccd;
            funcoffa += ncab*nccd*nfcd*b.getNFunc();
            funcoffb += ncab*nccd*nfcd;
        }
    }
}

int CholeskyIntegrals::getDiagonalBlock(const Shell& a, const Shell& b, diag_elem_t* diag)
{
    context->calcERI(1.0, 0.0, a, b, a, b);
    double* intbuf = context->getIntegrals();

    size_t controffi;
    size_t funcoffi;
    size_t controffj;
    size_t funcoffj;

    getShellOffsets(*context, a, b, a, b,
                    controffi, funcoffi, controffj, funcoffj,
                    controffi, funcoffi, controffj, funcoffj);

    int elem = 0;
    for (int n = 0;n < b.getNFunc();n++)
    {
        for (int p = 0;p < b.getNContr();p++)
        {
            for (int m = 0;m < a.getNFunc();m++)
            {
                for (int o = 0;o < a.getNContr();o++)
                {
                    if (&a == &b && (m*a.getNContr()+o) > (n*b.getNContr()+p)) continue;

                    diag[elem].funci = m;
                    diag[elem].contri = o;
                    diag[elem].funcj = n;
                    diag[elem].contrj = p;
                    diag[elem].elem = intbuf[m*funcoffi+o*controffi+
                                             n*funcoffj+p*controffj];
                    diag[elem].shelli = (int)(&a-&shells[0]);
                    diag[elem].shellj = (int)(&b-&shells[0]);
                    diag[elem].idx = elem;
                    diag[elem].status = TODO;

                    //crit = max(crit, diag[elem].elem);
                    elem++;
                }
            }
        }
    }

    return elem;
}

/*
 * perform a modified Cholesky decomposition (LDL^T) on a matrix subblock
 *
 * block_size       - size of the block to update
 * L                - Cholesky vectors for the block, dimensions L[block_size][>rank]
 * D                - the diagonal factor of the decomposition, dimensions D[>rank]
 * diag             - diagonal elements of the residual matrix for the block and other accounting information,
 *                    dimensions diag[block_size]
 *
 * return value     - rank after update
 */
void CholeskyIntegrals::decomposeBlock(int block_size, double* L_, double* D, diag_elem_t* diag)
{
    //printf("starting block: %d %f %d\n", block_size, diag[0].elem, rank);

    double (*L)[ndiag] = (double(*)[ndiag])L_;

    bool found = false;
    double max_elem = 0;
    for (int elem = 0;elem < block_size;elem++)
    {
        if (diag[elem].status != TODO) continue;
        max_elem = max(max_elem, abs(diag[elem].elem));
        if (abs(diag[elem].elem) > delta) found = true;
    }

    double crit = max(delta, max_elem/cond);

    if (!found) return;

    context->calcERI(1.0, 0.0, shells[diag[0].shelli], shells[diag[0].shellj], shells[diag[0].shelli], shells[diag[0].shellj]);
    double* intbuf = context->getIntegrals();

    size_t controffi;
    size_t funcoffi;
    size_t controffj;
    size_t funcoffj;
    size_t controffk;
    size_t funcoffk;
    size_t controffl;
    size_t funcoffl;

    getShellOffsets(*context,
                    shells[diag[0].shelli], shells[diag[0].shellj],
                    shells[diag[0].shelli], shells[diag[0].shellj],
                    controffi, funcoffi, controffj, funcoffj,
                    controffk, funcoffk, controffl, funcoffl);

    for (int elem = 0;elem < block_size;elem++)
    {
        //printf("activating row: %d\n", elem);
        if (abs(diag[elem].elem) < crit || diag[elem].status == DONE) continue;

        diag[elem].status = ACTIVE;

        //printf("updating L[%d][%d] (out of %d %d, addr=0x%x)\n", rank, elem, ndiag, block_size, L_);
        L[elem][rank] = 1.0;
        D[rank] = diag[elem].elem;

        #pragma omp parallel for schedule(dynamic) default(shared)
        for (int row = 0;row < block_size;row++)
        {
            if (diag[row].status != TODO) continue;

            //printf("updating L[%d][%d]\n", rank, row);
            L[row][rank] = intbuf[diag[elem].contri*controffi+diag[elem].funci*funcoffi+
                                  diag[elem].contrj*controffj+diag[elem].funcj*funcoffj+
                                  diag[ row].contri*controffk+diag[ row].funci*funcoffk+
                                  diag[ row].contrj*controffl+diag[ row].funcj*funcoffl];
            for (int col = 0;col < rank;col++)
                L[row][rank] -= D[col] * L[row][col] * L[elem][col];
            L[row][rank] /= D[rank];
            diag[row].elem -= D[rank] * L[row][rank] * L[row][rank];
        }

        rank++;
    }
}

/*
 * update a block of Cholesky vectors
 *
 * rank             - first rank to update
 * rank         - rank after update
 * block_size_i     - size of the block of Cholesky vectors to update
 * L_i              - Cholesky vectors to update, dimensions L_i[block_size_i][rank]
 * diag_i           - diagonal elements of the residual matrix and other accounting information
 *                    for the block to be updated, dimensions diag_i[block_size_i]
 * block_size_j     - size of the block containing the rows leading to this update
 * L_j              - Cholesky vectors for the rows leading to the update, dimensions L_j[rank-rank][rank]
 *                    (only those vectors actually needed, and not the entire block)
 * diag_j           - diagonal elements of the residual matrix for the block leading to the update,
 *                    dimensions diag_j[block_size_j] (the full block)
 * D                - the diagonal factor of the decomposition, dimensions D[rank]
 */
void CholeskyIntegrals::updateBlock(int old_rank, int block_size_i, double* L_i_, diag_elem_t* diag_i,
                                       int block_size_j, double* L_j_, diag_elem_t* diag_j, double* D)
{
    double (*L_i)[ndiag] = (double(*)[ndiag])L_i_;
    double (*L_j)[ndiag] = (double(*)[ndiag])L_j_;

    bool found = false;
    for (int elem = 0;elem < block_size_i;elem++)
    {
        if (diag_i[elem].status == TODO) found = true;
    }

    if (!found) return;

    //printf("subblock: %d %d\n", l, diag[l].nblock);
    int controff1 = shells[diag_j[0].shelli].getNContr() * shells[diag_j[0].shellj].getNContr();
    int funcoff1 = shells[diag_j[0].shelli].getNFunc() * shells[diag_j[0].shellj].getNFunc();
    int controff2 = shells[diag_i[0].shelli].getNContr() * shells[diag_i[0].shellj].getNContr();

    context->calcERI(1.0, 0.0, shells[diag_j[0].shelli], shells[diag_j[0].shellj],
                              shells[diag_i[0].shelli], shells[diag_i[0].shellj]);
    double* intbuf = context->getIntegrals();

    size_t controffii;
    size_t funcoffii;
    size_t controffij;
    size_t funcoffij;
    size_t controffji;
    size_t funcoffji;
    size_t controffjj;
    size_t funcoffjj;

    getShellOffsets(*context,
                    shells[diag_j[0].shelli], shells[diag_j[0].shellj],
                    shells[diag_i[0].shelli], shells[diag_i[0].shellj],
                    controffji, funcoffji, controffjj, funcoffjj,
                    controffii, funcoffii, controffij, funcoffij);

    #pragma omp parallel for schedule(dynamic) default(shared)
    for (int elem_i = 0;elem_i < block_size_i;elem_i++)
    {
        if (diag_i[elem_i].status != TODO) continue;

        int cur_rank = old_rank;
        for (int elem_j = 0;elem_j < block_size_j;elem_j++)
        {
            //printf("updating L[%d][%d]\n", cur_rank, elem_i);
            L_i[elem_i][cur_rank] = intbuf[diag_j[elem_j].contri*controffji+diag_j[elem_j].funci*funcoffji+
                                           diag_j[elem_j].contrj*controffjj+diag_j[elem_j].funcj*funcoffjj+
                                           diag_i[elem_i].contri*controffii+diag_i[elem_i].funci*funcoffii+
                                           diag_i[elem_i].contrj*controffij+diag_i[elem_i].funcj*funcoffij];
            for (int col = 0;col < cur_rank;col++)
                L_i[elem_i][cur_rank] -= D[col] * L_i[elem_i][col] * L_j[cur_rank-old_rank][col];
            L_i[elem_i][cur_rank] /= D[cur_rank];
            diag_i[elem_i].elem -= D[cur_rank] * L_i[elem_i][cur_rank] * L_i[elem_i][cur_rank];

            cur_rank++;
        }
    }
}

void CholeskyIntegrals::test()
{
    int sizeffr[] = {nfunc,nfunc,rank};
    int shapeSNN[] = {SY,NS,NS};
    int sizeffff[] = {nfunc,nfunc,nfunc,nfunc};
    int shapeNNNN[] = {NS,NS,NS,NS};
    DistTensor<double> LD(ctf, 3, sizeffr, shapeSNN, false);
    DistTensor<double> ints(ctf, 4, sizeffff, shapeNNNN, false);

    context = new Context();

    LD["pqJ"] = (*L)["pqJ"]*(*D)["J"];
    ints["pqrs"] = (*L)["pqJ"]*LD["rsJ"];

    //cout << "ints:" << endl;
    //ints.print(stdout);
    //cout << endl;

    double err = 0;
    for (int i = 0, m = 0;i < shells.size();i++)
    {
        for (int j = 0, n = 0;j <= i;j++)
        {
            for (int k = 0, o = 0;k < shells.size();k++)
            {
                for (int l = 0, p = 0;l <= k;l++)
                {
                    int ni = shells[i].getNFunc()*shells[i].getNContr();
                    int nj = shells[j].getNFunc()*shells[j].getNContr();
                    int nk = shells[k].getNFunc()*shells[k].getNContr();
                    int nl = shells[l].getNFunc()*shells[l].getNContr();

                    int size[] = {ni,nj,nk,nl};
                    DenseTensor local_ints(4, size);

                    kv_pair* pairs = new kv_pair[ni*nj*nk*nl];

                    for (int a = 0, q = 0;a < ni;a++)
                    {
                        for (int b = 0;b < nj;b++)
                        {
                            for (int c = 0;c < nk;c++)
                            {
                                for (int d = 0;d < nl;d++)
                                {
                                    pairs[q++].k = (((d+p)*nfunc+(c+o))*nfunc+(b+n))*nfunc+(a+m);
                                }
                            }
                        }
                    }

                    if (comm.Get_rank() == 0)
                    {
                        ints.getRemoteData(ni*nj*nk*nl, pairs);

                        double* local_data = local_ints.getData();
                        for (int q = 0;q < ni*nj*nk*nl;q++)
                        {
                            int d = pairs[q].k/(nfunc*nfunc*nfunc) - p;
                            pairs[q].k -= (d+p)*nfunc*nfunc*nfunc;
                            int c = pairs[q].k/(nfunc*nfunc) - o;
                            pairs[q].k -= (c+o)*nfunc*nfunc;
                            int b = pairs[q].k/(nfunc) - n;
                            pairs[q].k -= (b+n)*nfunc;
                            int a = pairs[q].k - m;

                            local_data[((d*nk+c)*nj+b)*ni+a] = pairs[q].d;
                        }

                        //printf("%d %d %d %d\n", i, j, k, l);
                        err += testBlock(local_ints, shells[i], shells[j], shells[k], shells[l]);
                    }
                    else
                    {
                        ints.getRemoteData(0, NULL);
                    }

                    p += shells[l].getNFunc()*shells[l].getNContr();
                }
                o += shells[k].getNFunc()*shells[k].getNContr();
            }
            n += shells[j].getNFunc()*shells[j].getNContr();
        }
        m += shells[i].getNFunc()*shells[i].getNContr();
    }

    if (comm.Get_rank() == 0)
    {
        err = sqrt(err / ndiag / ndiag);
        printf("RMS error: %.15e\n\n", err);
    }

    delete context;
}

double CholeskyIntegrals::testBlock(const DenseTensor& block, const Shell& a, const Shell& b, const Shell& c, const Shell& d)
{
    DenseTensor packed(4, block.getLengths(), false);
    const double* ints = packed.getData();

    packed = block;

    context->calcERI(1.0, 0.0, a, b, c, d);
    double* intbuf = context->getIntegrals();

    size_t controffa;
    size_t funcoffa;
    size_t controffb;
    size_t funcoffb;
    size_t controffc;
    size_t funcoffc;
    size_t controffd;
    size_t funcoffd;

    getShellOffsets(*context, a, b, c, d,
                    controffa, funcoffa, controffb, funcoffb,
                    controffc, funcoffc, controffd, funcoffd);

    double err = 0;
    int q = 0;
    for (int l = 0;l < d.getNFunc();l++)
    {
        for (int p = 0;p < d.getNContr();p++)
        {
            for (int k = 0;k < c.getNFunc();k++)
            {
                for (int o = 0;o < c.getNContr();o++)
                {
                    for (int j = 0;j < b.getNFunc();j++)
                    {
                        for (int n = 0;n < b.getNContr();n++)
                        {
                            for (int i = 0;i < a.getNFunc();i++)
                            {
                                for (int m = 0;m < a.getNContr();m++)
                                {
                                    //if ((&a == &b && (j > i || (j == i && n > m))) ||
                                    //    (&c == &d && (l > k || (l == k && p > o))))
                                    //{
                                    //    q++;
                                    //    continue;
                                    //}

                                    err += pow(ints[q] - intbuf[m*controffa+i*funcoffa+
                                                                n*controffb+j*funcoffb+
                                                                o*controffc+k*funcoffc+
                                                                p*controffd+l*funcoffd], 2);

                                    if (abs(ints[q] - intbuf[m*controffa+i*funcoffa+
                                                             n*controffb+j*funcoffb+
                                                             o*controffc+k*funcoffc+
                                                             p*controffd+l*funcoffd]) > 1e-10)
                                    {
                                    ///*
                                    printf("%d %d %d %d %d %d %d %d: %.12f %.12f\n",
                                           i, j, k, l, m, n, o, p, ints[q],
                                           intbuf[m*controffa+i*funcoffa+
                                                  n*controffb+j*funcoffb+
                                                  o*controffc+k*funcoffc+
                                                  p*controffd+l*funcoffd]);
                                    //*/
                                    }

                                    q++;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    return err;
}

}
}