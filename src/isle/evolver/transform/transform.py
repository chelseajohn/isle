r"""!\file
\ingroup evolvers
Base class for evolvers.
"""

from abc import ABCMeta, abstractmethod


class Transform(metaclass=ABCMeta):
    r"""! \ingroup evolvers
    Abstract base class for evolver transforms.
    """

    @abstractmethod
    def forward(self, phi):
        r"""!
        Transform a configuration from proposal to MC manifold.
        \param phi Configuration on proposal manifold.
        \returns In order:
          - Configuration on MC manifold.
          - Value of action at new configuration.
          - \f$\log \det J\f$ where \f$J\f$ is the Jacobian of the transformation.
        """

    @abstractmethod
    def backward(self, phi):
        r"""!
        Transform a configuration from MC to proposal manifold.
        \param phi Configuration on MC manifold.
        \returns In order:
          - Configuration on proposal manifold.
          - \f$\log \det J\f$ where \f$J\f$ is the Jacobian of the *forward* transformation.
        """

    @abstractmethod
    def save(self, h5group, manager):
        r"""!
        Save the transform to HDF5.
        Has to be the inverse of Transform.fromH5().
        \param h5group HDF5 group to save to.
        \param manager EvolverManager whose purview to save the transform in.
        """

    @classmethod
    @abstractmethod
    def fromH5(cls, h5group, manager, action, lattice, rng):
        r"""!
        Construct a trasnform from HDF5.
        Create and initialize a new instance from parameters stored via Transform.save().
        \param h5group HDF5 group to load parameters from.
        \param manager EvolverManager responsible for the HDF5 file.
        \param action Action to use.
        \param lattice Lattice the simulation runs on.
        \param rng Central random number generator for the run.
        \returns A newly constructed transform.
        """
