#ifndef _AQUARIUS_CC_LAMBDACCSDTQ_1B_HPP_
#define _AQUARIUS_CC_LAMBDACCSDTQ_1B_HPP_

#include "../../frameworks/convergence/diis.hpp"
#include "../../frameworks/operator/deexcitationoperator.hpp"
#include "../../frameworks/operator/excitationoperator.hpp"
#include "../../frameworks/operator/st2eoperator.hpp"
#include "../../frameworks/task/task.hpp"
#include "../../frameworks/util/global.hpp"
#include "../../frameworks/util/iterative.hpp"

namespace aquarius
{
namespace cc
{

/*
 * Solve the left-hand coupled cluster eigenvalue equation:
 *
 *               _
 * <0|L|Phi><Phi|H    |Phi> = 0
 *                open
 *
 *       _    -T   T       T
 * where X = e  X e  = (X e )
 *                           c
 */
template <typename U>
class LambdaCCSDTQ_1b : public Iterative<U>
{
    protected:
        convergence::DIIS<op::DeexcitationOperator<U,3>> diis;

    public:
        LambdaCCSDTQ_1b(const string& name, input::Config& config);

        bool run(task::TaskDAG& dag, const Arena& arena);

        void iterate(const Arena& arena);
};

}
}

#endif