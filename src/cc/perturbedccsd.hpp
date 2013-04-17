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

#ifndef _AQUARIUS_CC_PERTURBEDCCSD_HPP_
#define _AQUARIUS_CC_PERTURBEDCCSD_HPP_

#include "operator/2eoperator.hpp"

#include "ccsd.hpp"

namespace aquarius
{
namespace cc
{

/*
 * Solve the frequency-dependent amplitude response equations:
 *
 *      _                A                _
 * <Phi|H  - w|Phi><Phi|T (w)|0> = - <Phi|A|0> = - <Phi|X|0>
 *       open
 *
 *       _    -T   T       T
 * where X = e  X e  = (X e )
 *                           c
 */
template <typename U>
class PerturbedCCSD : public Iterative, public op::ExcitationOperator<U,2>
{
    protected:
        const op::STTwoElectronOperator<U,2>& H;
        U omega;
        op::ExcitationOperator<U,2>& TA;
        op::ExcitationOperator<U,2> D, Z, X;
        convergence::DIIS< op::ExcitationOperator<U,2> > diis;

    public:
        PerturbedCCSD(const input::Config& config, const op::STTwoElectronOperator<U,2>& H,
                      const op::ExponentialOperator<U,2>& T, const OneElectronOperator<U>& A, const U omega=0)
        : Iterative(config), op::ExcitationOperator<U,2>(ccsd.uhf),
          H(H), omega(omega), TA(*this), D(this->uhf), Z(this->uhf), X(this->uhf),
          diis(config.get("diis"))
        {
            D(0) = 1;
            D(1)["ai"]  = H.getIJ()["ii"];
            D(1)["ai"] -= H.getAB()["aa"];
            D(2)["abij"]  = H.getIJ()["ii"];
            D(2)["abij"] += H.getIJ()["jj"];
            D(2)["abij"] -= H.getAB()["aa"];
            D(2)["abij"] -= H.getAB()["bb"];

            D += omega;
            D = 1/D;

            op::STExcitationOperator<U,2>::transform(A, T, X);
            X(0) = 0;

            TA = X*D;
        }

        PerturbedCCSD(const input::Config& config, const op::TwoElectronOperator& H,
                      const CCSD<U>& T, const TwoElectronOperator<U>& A, const U omega=0)
        : Iterative(config), op::ExcitationOperator<U,2>(ccsd.uhf),
          H(H), omega(omega), TA(*this), D(this->uhf), Z(this->uhf), X(this->uhf),
          diis(config.get("diis"))
        {
            D(0) = 1;
            D(1)["ai"]  = H.getIJ()["ii"];
            D(1)["ai"] -= H.getAB()["aa"];
            D(2)["abij"]  = H.getIJ()["ii"];
            D(2)["abij"] += H.getIJ()["jj"];
            D(2)["abij"] -= H.getAB()["aa"];
            D(2)["abij"] -= H.getAB()["bb"];

            D += omega;
            D = 1/D;

            op::STExcitationOperator<U,2>::transform(A, T, X);
            X(0) = 0;

            TA = X*D;
        }

        void _iterate()
        {
            Z = X;
            H.contract(TA, Z);

             Z *=  D;
            // Z -= TA;
            TA +=  Z;

            conv =               Z(1)(0).reduce(CTF_OP_MAXABS);
            conv = std::max(conv,Z(1)(1).reduce(CTF_OP_MAXABS));
            conv = std::max(conv,Z(2)(0).reduce(CTF_OP_MAXABS));
            conv = std::max(conv,Z(2)(1).reduce(CTF_OP_MAXABS));
            conv = std::max(conv,Z(2)(2).reduce(CTF_OP_MAXABS));

            diis.extrapolate(TA, Z);
        }
};

}
}

#endif