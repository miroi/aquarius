#ifndef _AQUARIUS_TENSOR_DIVISIBLE_TENSOR_HPP_
#define _AQUARIUS_TENSOR_DIVISIBLE_TENSOR_HPP_

#include "tensor/tensor.hpp"

namespace aquarius
{
namespace tensor
{

class InvertedTensor;
class TensorDiv;

class InvertedTensor
{
    private:
        const InvertedTensor& operator=(const InvertedTensor& other);

    public:
        const TensorImplementation<>& tensor;
        Scalar factor;
        bool conj_;

        InvertedTensor(const TensorImplementation<>& tensor,
                       const Scalar& factor, bool conj=false)
        : tensor(tensor), factor(factor), conj_(conj) {}

        /**********************************************************************
         *
         * Unary negation, conjugation
         *
         *********************************************************************/

        InvertedTensor operator-() const
        {
            InvertedTensor ret(*this);
            ret.factor = -ret.factor;
            return ret;
        }

        InvertedTensor conj() const
        {
            InvertedTensor ret(*this);
            ret.conj_ = !ret.conj_;
            return ret;
        }

        friend InvertedTensor conj(const InvertedTensor& tm)
        {
            return tm.conj();
        }

        /**********************************************************************
         *
         * Operations with scalars
         *
         *********************************************************************/

        InvertedTensor operator*(const Scalar& factor) const
        {
            InvertedTensor ret(*this);
            ret.factor *= factor;
            return ret;
        }

        InvertedTensor operator/(const Scalar& factor) const
        {
            InvertedTensor ret(*this);
            ret.factor /= factor;
            return ret;
        }

        friend InvertedTensor operator*(const Scalar& factor, const InvertedTensor& other)
        {
            return other*factor;
        }
};

class TensorDiv
{
    private:
        const TensorDiv& operator=(const TensorDiv& other);

    public:
        const TensorImplementation<>& A;
        const TensorImplementation<>& B;
        bool conja;
        bool conjb;
        Scalar factor;

        TensorDiv(const TensorImplementation<>& A,
                  const TensorImplementation<>& B,
                  const Scalar& factor, bool conja=false, bool conjb=false)
        : A(A), B(B),
          conja(conja), conjb(conjb),
          factor(factor) {}

        /**********************************************************************
         *
         * Unary negation, conjugation
         *
         *********************************************************************/

        TensorDiv operator-() const
        {
            TensorDiv ret(*this);
            ret.factor = -ret.factor;
            return ret;
        }

        TensorDiv conj() const
        {
            TensorDiv ret(*this);
            ret.conja = !ret.conja;
            ret.conjb = !ret.conjb;
            return ret;
        }

        friend TensorDiv conj(const TensorDiv& other)
        {
            return other.conj();
        }

        /**********************************************************************
         *
         * Operations with scalars
         *
         *********************************************************************/

        TensorDiv operator*(const Scalar& factor) const
        {
            TensorDiv ret(*this);
            ret.factor *= factor;
            return ret;
        }

        TensorDiv operator/(const Scalar& factor) const
        {
            TensorDiv ret(*this);
            ret.factor /= factor;
            return ret;
        }

        friend TensorDiv operator*(const Scalar& factor, const TensorDiv& other)
        {
            return other*factor;
        }
};

template <> class TensorInitializer<DIVISIBLE> : public Destructible {};

TENSOR_INTERFACE(DIVISIBLE)
{
    public:
        /*
         * this = beta*this + alpha*A/B
         */
        virtual void div(const Scalar& alpha, bool conja, const TensorImplementation<>& A,
                                              bool conjb, const TensorImplementation<>& B,
                         const Scalar& beta) = 0;

        /*
         * this = beta*this + alpha/A
         */
        virtual void invert(const Scalar& alpha, bool conja, const TensorImplementation<>& A,
                            const Scalar& beta) = 0;
};

template <capability_type C_>
InvertedTensor operator/(const Scalar& factor, const ConstTensor<C_>& other)
{
    static_assert(!ARE_DISTINCT(C_,DIVISIBLE), "The operand must be DIVISIBLE.");
    return InvertedTensor(other, factor);
}

TENSOR_WRAPPER(DIVISIBLE)
{
    public:
        /**********************************************************************
         *
         * Intermediate operations
         *
         *********************************************************************/

        template <capability_type C_>
        TensorDiv operator/(const ConstTensor<C_>& other) const
        {
            static_assert(!ARE_DISTINCT(C_,DIVISIBLE), "The operands must be DIVISIBLE.");
            return TensorDiv(this->impl(), other.impl(), 1);
        }

        /**********************************************************************
         *
         * Binary operations (multiplication and division)
         *
         *********************************************************************/

        Tensor<C>& operator=(const TensorDiv& other)
        {
            static_assert(!(C&CONST_), "The LHS must not be const.");
            this->template impl<DIVISIBLE>().div(other.factor, other.conja, other.A,
                                                               other.conjb, other.B, 0);
            return *this;
        }

        Tensor<C>& operator+=(const TensorDiv& other)
        {
            static_assert(!(C&CONST_), "The LHS must not be const.");
            this->template impl<DIVISIBLE>().div(other.factor, other.conja, other.A,
                                                               other.conjb, other.B, 1);
            return *this;
        }

        Tensor<C>& operator-=(const TensorDiv& other)
        {
            static_assert(!(C&CONST_), "The LHS must not be const.");
            this->template impl<DIVISIBLE>().div(-other.factor, other.conja, other.A,
                                                                other.conjb, other.B, 1);
            return *this;
        }

        /**********************************************************************
         *
         * Unary operations (assignment, summation, scaling, and inversion)
         *
         *********************************************************************/

        template <capability_type C_>
        Tensor<C>& operator/=(const ConstTensor<C_>& other)
        {
            static_assert(!(C&CONST_), "The LHS must not be const.");
            static_assert(!ARE_DISTINCT(C_,DIVISIBLE), "The operands must be DIVISIBLE.");
            this->template impl<DIVISIBLE>().div(1, false, this->impl(),
                                                    false, other.impl(), 0);
            return *this;
        }

        Tensor<C>& operator=(const InvertedTensor& other)
        {
            static_assert(!(C&CONST_), "The LHS must not be const.");
            this->template impl<DIVISIBLE>().invert(other.factor, other.conj_, other.tensor, 0);
            return *this;
        }

        Tensor<C>& operator+=(const InvertedTensor& other)
        {
            static_assert(!(C&CONST_), "The LHS must not be const.");
            this->template impl<DIVISIBLE>().invert(other.factor, other.conj_, other.tensor, 1);
            return *this;
        }

        Tensor<C>& operator-=(const InvertedTensor& other)
        {
            static_assert(!(C&CONST_), "The LHS must not be const.");
            this->template impl<DIVISIBLE>().invert(-other.factor, other.conj_, other.tensor, 0);
            return *this;
        }

        Tensor<C>& operator*=(const InvertedTensor& other)
        {
            static_assert(!(C&CONST_), "The LHS must not be const.");
            this->template impl<DIVISIBLE>().div(other.factor,       false, this->impl(),
                                                               other.conj_, other.tensor, 0);
            return *this;
        }

        Tensor<C>& operator/=(const InvertedTensor& other)
        {
            static_assert(!(C&CONST_), "The LHS must not be const.");
            this->template impl<DIVISIBLE>().mult(1/other.factor,      false, this->impl(),
                                                        other.conj_, other.tensor, 0);
            return *this;
        }
};

}
}

#endif