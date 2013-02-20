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

#include "input/config.hpp"
#include "input/molecule.hpp"
#include "scf/aoints.hpp"
#include "scf/cholesky.hpp"
#include "scf/choleskyscf.hpp"
#include "scf/choleskymoints.hpp"
#include "cc/cc.hpp"

#include "fenv.h"

using namespace std;
using namespace elem;
using namespace MPI;
using namespace aquarius::slide;
using namespace aquarius::input;
using namespace aquarius::scf;
using namespace aquarius::cc;

int main(int argc, char **argv)
{
    //feenableexcept(FE_DIVBYZERO);

    MPI_Init(&argc, &argv);
    SLIDE::init();
    elem::Initialize(argc, argv);

    {
        tCTF_World<double> ctf;

        assert(argc > 1);
        Schema schema(TOPDIR "/input_schema");
        Config config(argv[1]);
        schema.apply(config);

        Molecule mol(config);

        CholeskyIntegrals chol(ctf, config.get("cholesky"), mol);
        CholeskyUHF scf(chol, config.get("scf"));

        //chol.test();

        PRINT("UHF-SCF\n\n");
        PRINT("It.         SCF Energy     Residual\n");
        for (int i = 0;scf.iterate();i++)
        {
            PRINT("%3d %18.15f %12.6e\n", i+1, scf.getEnergy(), scf.getConvergence());
        }

        PRINT("\n");
        PRINT("<S^2>     = %f\n", scf.getS2());
        PRINT("<2S+1>    = %f\n", scf.getMultiplicity());
        PRINT("<n_alpha> = %f\n", scf.getAvgNumAlpha());
        PRINT("<n_beta>  = %f\n", scf.getAvgNumBeta());
        PRINT("\n");

        CholeskyMOIntegrals moints(scf);
        CCSD ccsd(config.get("cc"), moints);

        PRINT("UHF-MP2 Energy: %18.15f\n\n", ccsd.getEnergy());

        PRINT("UHF-CCSD\n\n");
        PRINT("It. Correlation Energy     Residual\n");
        for (int i = 0;ccsd.iterate();i++)
        {
            PRINT("%3d %18.15f %12.6e\n", i+1, ccsd.getEnergy(), ccsd.getConvergence());
        }

        PRINT("\nFinal Energy: %18.15f\n", scf.getEnergy()+ccsd.getEnergy());
    }

    elem::Finalize();
    SLIDE::finish();
    MPI_Finalize();
}