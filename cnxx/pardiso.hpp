#ifndef PARDISO_HPP
#define PARDISO_HPP

/** \file
 * \brief Wrapper around PARDISO sparse solver.
 *
 * Including this file causes a compile time error unless either the macro `PARDISO`
 * or `MKL_PARDISO` is defined. Those macros toggle which version of PARDISO is
 * used as a backend.
 *
 * \todo Implement MKL PARDISO support.
 */

// check that macros are set properly
#if !defined(PARDISO) && !defined(MKL_PARDISO)
  #error "PARDISO is not enabled, cannot use pardiso.hpp"
#endif
#if defined(PARDISO) && defined(MKL_PARDISO)
  #error "Both macros for PARDISO are defined. Select a flavor, either PARDISO or MKL_PARDISO"
#endif

// TODO
#if defined(MKL_PARDISO)
  #error "MKL PARDISO is not yet supported"
#endif


#include <type_traits>
#include <memory>

#include "math.hpp"
#include "hubbardFermiMatrix.hpp"


/// Prototype for init function in PARDISO library.
extern "C" void pardisoinit(void *pt[64], int *mtype, int *solver, int iparm[64],
                            double dparm[64], int *error);

/// Prototype for solver execution function in PARDISO library.
extern "C" void pardiso(void *pt[64], int *maxfct, int *mnum, int *mtype,
                        int *phase, int *n, void *a, int ia[], int ja[],
                        int perm[], int *nrhs, int iparm[64], int *msglvl,
                        void *b, void *x, int *error, double dparm[64]);


/// Wrapper around PARDISO sparse solver.
namespace Pardiso {
    /// Solver kind used by PARDISO.
    enum class Solver {
        DIRECT = 0,    ///< Sparse direct solver.
        ITERATIVE = 1  ///< Multi-recursive iterative solver.
    };

    /// Matrix type for PARDISO excluding datatype.
    /**
     * Only encodes the symmetry type of matrices. The datatype is determined based
     * on the template parameter of Pardiso::State.
     *
     * Note that `SYM_HERM_INDEF`, `DIAGONAL`, and `BUNCH_KAUF` all represent the
     * same matrix type in PARDISO; they can be used interchangeably.
     */
    enum class MType {
        STRUCT_SYM,       ///< Structurally symmetric.
        SYM_HERM_POS_DEF, ///< Symmetric or hermitian positive definite.
        SYM_HERM_INDEF,   ///< Symmetric or hermitian undefinite.
        DIAGONAL,         ///< Diagonal.
        BUNCH_KAUF,       ///< Bunch-Kaufman pivoting.
        COMPL_SYM,        ///< Complex symmetric (not allowed with real matrices).
        NON_SYM           ///< Nonsymmetric.
    };

    /// A single PARDISO phase.
    /**
     * Cleaning up memory is done implicitly by Pardiso::State.
     *
     * \see pardisoPhase to build the combination of start / end phase.
     */
    enum class Phase {
        ANALYSIS = 1,      ///< Analysis.
        FACTORIZATION = 2, ///< Numerical factorization.
        SOLVE = 3,         ///< Solve / iterative refinement.
        SEL_INV = -22      ///< Selected inversion.
    };

    /// Construct phase parameter for calls to PARDISO from a start and end phase.
    inline int pardisoPhase(const Phase start, const Phase end) noexcept {
        if (start == Phase::SEL_INV)
            return static_cast<int>(start);

        return 10*static_cast<int>(start) + static_cast<int>(end);
    }

    /// Indices into integer parameters iparm of PARDISO.
    enum class IParm {
        USE_DEFAULT = 0,  ///< Fill iparm with default values (only for `pardisoinit`).
        NUM_PROC = 2      ///< Number of OpenMP threads.
    };

    /// Indices into double parameters dparm of PARDISO.
    enum class DParm {
        RESIDUAL = 33  ///< Relative residual after Krylov-Subspace convergence.
    };

    /// Check PARDISO error flag and throw exception if an error occured.
    /**
     * \param error PARDISO error code.
     * \throws `std::runtime_error` if `error != 0`. The exception message contains a brief
     *         description of the error.
     */
    inline void handleError(const int error) {
        switch (error) {
        case 0:
            return;
        case -1:
            throw std::runtime_error("PARDISO Error -1: Input inconsistent.");
        case -2:
            throw std::runtime_error("PARDISO Error -2: Not enough memory");
        case -3:
            throw std::runtime_error("PARDISO Error -3: Reordering problem");
        case -4:
            throw std::runtime_error("PARDISO Error -4: Zero pivot, numerical fact. or iterative refinement problem");
        case -5:
            throw std::runtime_error("PARDISO Error -5: Unclassified (internal) error");
        case -6:
            throw std::runtime_error("PARDISO Error -6: Preordering failed");
        case -7:
            throw std::runtime_error("PARDISO Error -7: Diagonal matrix problem");
        case -8:
            throw std::runtime_error("PARDISO Error -8: 32-bit integer overflow problem");
        case -10:
            throw std::runtime_error("PARDISO Error -10: No license file pardiso.lic found");
        case -11:
            throw std::runtime_error("PARDISO Error -11: License is expired");
        case -12:
            throw std::runtime_error("PARDISO Error -12: Wrong username or hostname");
        case -100:
            throw std::runtime_error("PARDISO Error -100: Reached maximum number of Krylov-subspace iteration");
        case -101:
            throw std::runtime_error("PARDISO Error -101: No sufficient convergence in Krylov-subspace iteration within 25 iterations");
        case -102:
            throw std::runtime_error("PARDISO Error -102: Error in Krylov-subspace iteration");
        case -103:
            throw std::runtime_error("PARDISO Error -103: Break-down in Krylov-subspace iteration");
        default:
            throw std::runtime_error("Unknown PARDISO Error");
        }
    }


    /// Holds a sparse matrix in CRS format.
    /**
     * This class helps cosntruct matrices in the CRS (compressed row storage) format
     * required by PARDISO. Indices are stored as 1-based internally (<TT>FORTRAN</TT>).
     * However the interface requires 0-based indices suitable for <TT>C++</TT>.
     * The only exception are `getia()` and `getja()` which give direct access to the
     * internal memory.
     *
     * <B>Usage</B><BR>
     * You need the specify elements in order of ascending row and column as shown in the
     * following with an imaginary matrix class that iterates over rows is ascending order.
     * \code
     Pardiso::Matrix<T> pmat;
     for (auto row : matrix.rows()) {
         for (int column = 0; column < row.nonZeros(); ++column) {
             pmat.add(column, row.at(column));
         }
         pmat.finishRow();
     }
     \endcode
     * Note that you have to call Matrix::finishRow() when moving on the the next row
     * as well as at the end after inserting all elements.
     */
    template <typename ET>
    struct Matrix {
        using elementType = ET;  ///< Type of matrix elements.

        std::vector<elementType> a;  ///< Array of matrix elements.
        std::vector<int> ja;  ///< Array of column indices (1-based).
        std::vector<int> ia;  ///< Array of row indices (1-based).


        /// Construct without reserving memory.
        Matrix() {
            ia.push_back(1);
        }

        /// Reserve memory for known number of elements.
        /**
         * \param n (Expected) number of non-zeros of the matrix. Memory for `n`
         *          elements is reserved.
         */
        explicit Matrix(const std::size_t n) {
            reserve(n);
            ia.push_back(1);
        }

        /// Reserve memory for known number of elements and rows.
        /**
         * \param n (Expected) number of non-zeros of the matrix. Memory for `n`
         *          elements is reserved.
         * \param nrow (Expected) number of rows. Memory for `nrow` rows is reseved.
         */
        Matrix(const std::size_t n, const std::size_t nrow) {
            reserve(n, nrow);
            ia.push_back(1);
        }

        /// Copy.
        Matrix(const Matrix &) = default;
        /// Move.
        Matrix(Matrix &&) = default;
        /// Copy assign.
        Matrix &operator=(const Matrix &) = default;
        /// Move assign.
        Matrix &operator=(Matrix &&) = default;
        ~Matrix() = default;

        /// Reserve memory for known number of elements.
        /**
         * \param n (Expected) number of non-zeros of the matrix. Memory for `n`
         *          elements is reserved.
         */
        void reserve(const std::size_t n) {
            a.reserve(n);
            ja.reserve(n);
        }

        /// Reserve memory for known number of elements and rows.
        /**
         * \param n (Expected) number of non-zeros of the matrix. Memory for `n`
         *          elements is reserved.
         * \param nrow (Expected) number of rows. Memory for `nrow` rows is reseved.
         */
        void reserve(const std::size_t n, const std::size_t nrow) {
            a.reserve(n);
            ja.reserve(n);
            ia.reserve(nrow+1);
        }

        /// Remove all stored elements; start a new matrix.
        void clear() {
            a.clear();
            ja.clear();
            ia.clear();
            ia.push_back(1);
        }

        /// Number of rows currently stored.
        std::size_t rows() const noexcept {
            return ia.size()-1;
        }

        /// Finalize the current row, prepare for input of next row.
        void finishRow() {
            ia.push_back(static_cast<int>(ja.size())+1);
        }

        /// Add an element to the current row.
        /**
         * \attention Must be called with `column` is ascending order.
         * \param column Index of the column.
         * \param value Element at given index.
         */
        void add(const int column, const elementType value) {
            ja.push_back(column+1);
            a.push_back(value);
        }

        /// Number of elements currently stored.
        int nelem() const noexcept {
            return static_cast<int>(a.size());
        }

        /// Get a pointer to the elements.
        elementType *geta() noexcept {
            return &a[0];
        }
        /// Get a pointer to the elements.
        const elementType *geta() const noexcept {
            return &a[0];
        }

        /// Get a pointer to the column indices (1-based).
        int *getja() noexcept {
            return &ja[0];
        }
        /// Get a pointer to the column indices (1-based).
        const int *getja() const noexcept {
            return &ja[0];
        }

        /// Get a pointer to the row indices (1-based).
        int *getia() noexcept {
            return &ia[0];
        }
        /// Get a pointer to the column indices (1-based).
        const int *getia() const noexcept {
            return &ia[0];
        }
    };  // struct Matrix


    /// Handle to PARDISO's internal state.
    /**
     * Initializes PARDISO on construction and cleans up PARDISO's internal memory
     * when it is destroyed.
     * Since the content of the internal state is unknown, it cannot be copied.
     * In order to avoid shared references, %State can only be moved, not copied.
     *
     * You can set or retrieve all parameters from `iparm` or `dparm` via the
     * overloaded `operator[]`.
     * PARDISO can be executed by calling `operator()` which can handle various
     * inout formats.
     *
     * \tparam ET Element type of matrices and vectors used by this solver.
     *            Must be one either `double` or `std::complex<double>`.
     */
    template <typename ET>
    struct State {
        static_assert(std::is_same<ET, double>::value
                      || std::is_same<ET, std::complex<double>>::value,
                      "PARDISO can only handle double and std::complex<double>.");

        using elementType = ET;  ///< Type of matrix elements.

        /// Selecct a solver and optionally matrix type and message level; initialize PARDISO.
        State(const Solver solver, const MType mtype = MType::NON_SYM,
              const int messageLevel = 0)
            : _msglvl{messageLevel}, _mtype{matrixType(mtype)}, _ownsMemory{false} {

            int error;
            int slvr = static_cast<int>(solver);
            (*this)[IParm::USE_DEFAULT] = 0;  // fill in default values
            (*this)[IParm::NUM_PROC] = 1;
            pardisoinit(_statePtr.get(), &_mtype, &slvr,
                        _iparm.get(), _dparm.get(), &error);
            handleError(error);
        };

        /// Copying is not allowed.
        State(const State &other) = delete;
        /// Copying is not allowed.
        State &operator=(const State &other) = delete;

        /// Move, old instance releases control over PARDISO.
        State(State &&other) noexcept
            : _statePtr{std::move(other._statePtr)},
              _iparm{std::move(other._iparm)},
              _dparm{std::move(other._dparm)},
              _msglvl{other._msglvl},
              _mtype{other._mtype},
              _ownsMemory{std::exchange(other._ownsMemory, false)} { }

        /// Move assign, old instance releases control over PARDISO.
        State &operator=(State &&other) noexcept {
            _statePtr = std::move(other._statePtr);
            _iparm = std::move(other._iparm);
            _dparm = std::move(other._dparm);
            _msglvl = other._msglvl;
            _mtype = other._mtype;
            _ownsMemory = std::exchange(other._ownsMemory, false);
            return *this;
        }

        ~State() {
            clear();
        }

        /// Free all memory allocated by PARDISO; is called by destructor.
        void clear() {
            if (_ownsMemory) {
                int error;
                int phase = -1;
                pardiso(_statePtr.get(), nullptr, nullptr, nullptr,
                        &phase, nullptr, nullptr, nullptr, nullptr,
                        nullptr, nullptr, _iparm.get() , &_msglvl, nullptr,
                        nullptr, &error, _dparm.get());
                handleError(error);
                _ownsMemory = false;
            }            
        }


        /// Access an integer parameter.
        int &operator[](const IParm ip) noexcept {
            return _iparm[static_cast<std::size_t>(ip)];
        }
        /// Access an integer parameter.
        const int &operator[](const IParm ip) const noexcept {
            return _iparm[static_cast<std::size_t>(ip)];
        }

        /// Return pointer to array of integer parameters.
        int *iparm() noexcept {
            return _iparm.get();
        }
        /// Return pointer to array of integer parameters.
        const int *iparm() const noexcept {
            return _iparm.get();
        }

        /// Access a double parameter.
        double &operator[](const DParm dp) noexcept {
            return _iparm[static_cast<std::size_t>(dp)];
        }
        /// Access a double parameter.
        const double &operator[](const DParm dp) const noexcept {
            return _iparm[static_cast<std::size_t>(dp)];
        }

        /// Return pointer to array of double parameters.
        int *dparm() noexcept {
            return _dparm.get();
        }
        /// Return pointer to array of double parameters.
        const int *dparm() const noexcept {
            return _dparm.get();
        }

        /// Access PARDISO message level (msglvl).
        int &messageLevel() noexcept {
            return _msglvl;
        }
        /// Access PARDISO message level (msglvl).
        const int &messageLevel() const noexcept {
            return _msglvl;
        }


        /// Perform sparse solve by calling `pardiso`.
        /**
         * Solves a*x = b for x.<BR>
         * Low level interface to PARDISO. Matrix must be specified in CRS
         * (compressed row storage) format.
         * Memory for the output must be allocated by caller.
         *
         * \param n Number of equations (size of x, b).
         * \param a Non zero elements of matrix. Size depends on matrix.
         * \param ia Index to beginning to each row (1 based). Size: `n+1`.
         * \param ja Column indices for each element in `a`. Size depends on matrix.
         * \param b Right hand side vector. Size: `n`.
         * \param x Solution vector. Size: `n`.
         * \param startPhase Phase at which to start computation.
         * \param endPhase Phase at which to end computation.
         *
         * \throws std::runtime_error if PARDISO reports an error.
         */
        void operator()(int n,
                        elementType * const a, int * const ia, int * const ja,
                        elementType * const b, elementType * const x,
                        const Phase startPhase, const Phase endPhase=Phase::SOLVE) {

            int maxfct = 1, mnum = 1;
            int phase = pardisoPhase(startPhase, endPhase);
            int nrhs = 1;
            int error;
            
            pardiso(_statePtr.get(), &maxfct, &mnum, &_mtype, &phase,
                    &n, a, ia, ja, nullptr, &nrhs, _iparm.get(),
                    &_msglvl, b, x, &error, _dparm.get());
            handleError(error);

            _ownsMemory = true;
        }

        /// Perform sparse solve by calling `pardiso`.
        /**
         * Solves a*x = b for x.<BR>
         * Thin wrapper for `std::vector` around overload for arrays.
         * Matrix must be specified in CRS (compressed row storage) format.
         * Size n is derived from right hand side b.
         *
         * \param a Non zero elements of matrix. Size depends on matrix.
         * \param ia Index to beginning to each row (1 based). Size: `n+1`.
         * \param ja Column indices for each element in `a`. Size depends on matrix.
         * \param b Right hand side vector. Size: `n`.
         * \param startPhase Phase at which to start computation.
         * \param endPhase Phase at which to end computation.
         *
         * \return Solution vector x.
         *
         * \throws std::runtime_error if PARDISO reports an error or vector sizes
         *                            do not match and in debug build.
         */
        std::vector<elementType> operator()(std::vector<elementType> &a,
                                            std::vector<int> &ia,
                                            std::vector<int> &ja,
                                            std::vector<elementType> &b,
                                            const Phase startPhase,
                                            const Phase endPhase=Phase::SOLVE) {
#ifndef NDEBUG
            if (b.size() != ia.size()-1)
                throw std::runtime_error("Numbers of rows of matrix and rright hand side do not match");
#endif
            std::vector<elementType> x(b.size());
            (*this)(b.size(), &a[0], &ia[0], &ja[0], &b[0], &x[0], startPhase, endPhase);
            return x;
        }

        /// Perform sparse solve by calling `pardiso`.
        /**
         * Solves a*x = b for x.<BR>
         * Thin wrapper for Pardiso::Matrix around overload for arrays.
         *
         * \param mat Sparse matrix `a`.
         * \param b Right hand side vector.
         * \param startPhase Phase at which to start computation.
         * \param endPhase Phase at which to end computation.
         *
         * \return Solution vector x.
         *
         * \throws std::runtime_error if PARDISO reports an error or matrix and vector
         *                            sizes do not match and in debug build.
         */
        std::vector<elementType> operator()(Pardiso::Matrix<elementType> &mat,
                                            std::vector<elementType> &b,
                                            const Phase startPhase,
                                            const Phase endPhase=Phase::SOLVE) {
#ifndef NDEBUG
            if (b.size() != mat.rows())
                throw std::runtime_error("Numbers of rows of matrix and rright hand side do not match");
#endif
            std::vector<elementType> x(b.size());
            (*this)(b.size(), mat.geta(), mat.getia(), mat.getja(),
                    &b[0], &x[0], startPhase, endPhase);
            return x;
        }

        /// Perform sparse solve by calling `pardiso`.
        /**
         * Solves a*x = b for x.<BR>
         * Copies the input matrix into CRS format via Pardiso::Matrix and
         * thus has some overhead over a plain call to `pardiso`.
         *
         * \param mat Sparse matrix `a`.
         * \param b Right hand side vector.
         * \param startPhase Phase at which to start computation.
         * \param endPhase Phase at which to end computation.
         *
         * \return Solution vector x.
         *
         * \throws std::runtime_error if PARDISO reports an error or matrix and vector
         *                            sizes do not match and in debug build.
         */        
        Vector<elementType> operator()(const SparseMatrix<elementType> &mat,
                                       Vector<elementType> &b,
                                       const Phase startPhase,
                                       const Phase endPhase=Phase::SOLVE) {
#ifndef NDEBUG
            if (b.size() != mat.rows())
                throw std::runtime_error("Numbers of rows of matrix and rright hand side do not match");
#endif

            // construct matrix in CRS format
            Pardiso::Matrix<elementType> pmat(mat.nonZeros(), mat.rows());
            for (std::size_t row = 0; row < mat.rows(); ++row) {
                for (auto it = mat.begin(row), end = mat.end(row); it != end; ++it)
                    pmat.add(static_cast<int>(it->index()), it->value());
                pmat.finishRow();
            }

            // solve equation
            Vector<elementType> x(b.size());
            (*this)(mat.rows(), pmat.geta(), pmat.getia(), pmat.getja(),
                    &b[0], &x[0], startPhase, endPhase);
            return x;
        }

    private:
        /// Pointer to the internal state of PARDISO.
        std::unique_ptr<void*[]> _statePtr = std::make_unique<void*[]>(64);
        /// Array of integer parameters.
        std::unique_ptr<int[]> _iparm = std::make_unique<int[]>(64);
        /// Array of double parameters.
        std::unique_ptr<double[]> _dparm = std::make_unique<double[]>(64);

        int _msglvl;      ///< Message level (0 or 1).
        int _mtype;       ///< PARDISO matrix type.
        bool _ownsMemory; ///< `true` if PARDISO allocated memory that needs to be freed.

        /// Return the full matrix type based on a symmetry type and datatype of this class.
        /**
         * \param mtype Determines the symmetry type of the matrix. The data type
         *        (real / complex) is determined from `State::elementType`.
         * \throws std::runtime_error if calling with `mtype == MType::COMPL_SYM` and
         *         `State::elementType == double`.
         * \returns Integer encoding PARDISO matrix type.
         */
        int matrixType(const MType mtype) const {
            if (std::is_same<elementType, double>::value) {
                switch (mtype) {
                case MType::NON_SYM:
                    return 11;
                case MType::STRUCT_SYM:
                    return 1;
                case MType::SYM_HERM_POS_DEF:
                    return 2;
                case MType::SYM_HERM_INDEF:
                case MType::DIAGONAL:
                case MType::BUNCH_KAUF:
                    return -2;
                case MType::COMPL_SYM:
                    throw std::runtime_error("Cannot use Pardiso::Mtype::COMPL_SYM with real matrix.");
                }
            } else if (std::is_same<elementType, std::complex<double>>::value) {
                switch (mtype) {
                case MType::NON_SYM:
                    return 13;
                case MType::STRUCT_SYM:
                    return 3;
                case MType::SYM_HERM_POS_DEF:
                    return 4;
                case MType::SYM_HERM_INDEF:
                case MType::DIAGONAL:
                case MType::BUNCH_KAUF:
                    return -4;
                case MType::COMPL_SYM:
                    return 6;
                }                
            }
            return 0;
        }
    };  // struct State
}  // namespace Spardiso

#endif  // ndef PARDISO_HPP