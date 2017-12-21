/** \file
 * \brief Wraps a round a math library and provides abstracted types and functions.
 *
 * The types do not provide a distinction between space and spacetime vectors / matrices.
 * Spacetime vectors are assumed to be encoded as a single vector with index
 * \f$(it) \equiv i n_{t} + t\f$, where \f$i\f$ is a space index, \f$t\f$ a time index, and
 * \f$n_{t}\f$ the number of time slices.
 */

#ifndef MATH_HPP
#define MATH_HPP

#include <type_traits>

#include <blaze/Math.h>

#include "core.hpp"

template <typename ET>
using Vector = blaze::DynamicVector<ET>;

template <typename ET>
using Vec3 = blaze::StaticVector<ET, 3>;

template <typename ET>
using Matrix = blaze::DynamicMatrix<ET>;

template <typename ET>
using SparseMatrix = blaze::CompressedMatrix<ET>;

template <typename ET>
using IdMatrix = blaze::IdentityMatrix<ET>;

template <typename ET>
using SymmetricMatrix = blaze::SymmetricMatrix<blaze::DynamicMatrix<ET>>;

/// Multiply a space matrix with a space time vector.
/**
 * Let \f$v, u\f$ be vectors in spacetime and \f$M\f$ a matrix in space.
 * Furthermore, let \f$(it)\f$ denote a spacetime index comprised of the spatial index
 * \f$i\f$ and time index \f$t\f$.<BR>
 * This function computes
 * \f[
 *   u_{(it)} = M_{i,j} v_{(jt)}
 * \f]
 *
 * \tparam MT Arbitrary matrix type.
 * \tparam VT Dense vector type.
 * \param spaceMatrix \f$M\f$
 * \param spacetimeVector \f$v\f$
 * \returns \f$u\f$
 *
 * \throws std::runtime_error
 *  - `spaceMatrix` is not a square matrix.
 *  - Length of `spacetimeVector` is not a multiple of the dimension of `spaceMatrix`.
 *
 *  Does not throw if macro `NDEBUG` is defined.
 */
/*
 * Works by wrapping input and output vectors in a blaze::CustomMatrix
 * to treat a spacetime vector v_{(it)} as a matrix vm_{i,t}.
 * Then m*v can be performed as m*vm.
 */
template <typename MT, typename VT,
          typename = std::enable_if_t<blaze::IsDenseVector<VT>::value, VT>>
auto spaceMatSpacetimeVec(const MT &spaceMatrix,
                          const VT &spacetimeVector) noexcept(ndebug) {

    // get lattice size
    const auto nx = spaceMatrix.rows();
    const auto nt = spacetimeVector.size() / nx;

#ifndef NDEBUG
    if (nx != spaceMatrix.columns())
        throw std::runtime_error("Matrix is not square");
    if (spacetimeVector.size() % nx != 0)
        throw std::runtime_error("Matrix and vector size do not match");
#endif

    // return type, same as VT with adjusted element type
    using RT = typename VT::template Rebind<
        decltype(typename MT::ElementType{} * typename VT::ElementType{})
        >::Other;

    // space time matrix type for input vector
    using STMV = blaze::CustomMatrix<std::add_const_t<typename VT::ElementType>,
                                     blaze::unaligned, blaze::unpadded>;
    // dpace time matrix type for returned vector
    using STMR = blaze::CustomMatrix<typename RT::ElementType,
                                     blaze::unaligned, blaze::unpadded>;

    // do computation
    RT result(spacetimeVector.size());
    STMR{&result[0], nx, nt} = spaceMatrix * STMV{&spacetimeVector[0], nx, nt};
    return result;
}

#endif  // ndef MATH_HPP
