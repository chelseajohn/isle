#include "core.hpp"

#include "math.hpp"
#include "lattice.hpp"
#include "hubbardFermiMatrix.hpp"
#include "action.hpp"
#include "pardiso.hpp"

PYBIND11_MODULE(cnxx, mod) {
    mod.doc() = "Python bindings for cnxx";

    bind::bindTensors(mod);
    bind::bindLattice(mod);
    bind::bindHubbardFermiMatrix(mod);
    bind::bindActions(mod);
    bind::bindPARDISO(mod);
}
