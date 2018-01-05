#include "lattice.hpp"

#include "../lattice.hpp"

namespace bind {
    void bindLattice(py::module &mod) {
        py::class_<Lattice>{mod, "Lattice"}
            .def(py::init<std::size_t, std::size_t>())
            .def("setNeighbor", &Lattice::setNeighbor)
            .def("getNeighbor", [](const Lattice &self,
                                   const std::size_t i, const std::size_t j) {
                     if (self.hopping().find(i, j) != self.hopping().end(i))
                         return self.hopping()(i, j);
                     else
                         throw std::invalid_argument("No matrix element at given indices");
                })
            .def("distance", (double (Lattice::*)(std::size_t, std::size_t) const)&Lattice::distance)
            .def("distance", (void (Lattice::*)(std::size_t, std::size_t, double))&Lattice::distance)
            .def("nt", &Lattice::nt)
            .def("nx", &Lattice::nx)
            .def("lattSize", &Lattice::lattSize)
            ;
    }
}
