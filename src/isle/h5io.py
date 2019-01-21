"""!
Routines for working with HDF5.
"""

from logging import getLogger
from pathlib import Path

import yaml
import h5py as h5

from . import Vector, isleVersion, pythonVersion, blazeVersion, pybind11Version
from .random import readStateH5

def createH5Group(base, name):
    r"""!
    Create a new HDF5 group if it does not yet exist.
    \param base H5 group in which to create the new group.
    \param name Name of the new group relative to base.
    \returns The (potentially newly created) group.
    """

    if name in base:
        if isinstance(base[name], h5.Group):
            return base[name] # there is already a group with that name
        # something else than a group with that name
        raise ValueError(("Cannot create group '{}', another object with the same"\
                         +" name already exists in '{}/{}'").format(name, base.filename, base.name))
    # does not exists yet
    return base.create_group(name)

def writeMetadata(fname, lattice, params, makeActionSrc):
    """!
    Write metadata to HDF5 file.
    Overwrites any existing datasets.
    """

    with h5.File(str(fname), "a") as outf:
        metaGrp = createH5Group(outf, "meta")
        metaGrp["lattice"] = yaml.dump(lattice)
        metaGrp["params"] = yaml.dump(params)
        metaGrp["action"] = makeActionSrc

        vgrp = createH5Group(metaGrp, "version")
        vgrp["isle"] = str(isleVersion)
        vgrp["python"] = str(pythonVersion)
        vgrp["blaze"] = str(blazeVersion)
        vgrp["pybind11"] = str(pybind11Version)

def readMetadata(fname):
    r"""!
    Read metadata on ensemble from HDF5 file.

    \returns Lattice, parameters, makeAction (source code of function)
    """
    if isinstance(fname, (tuple, list)):
        fname = fname[0]
    with h5.File(str(fname), "r") as inf:
        try:
            metaGrp = inf["meta"]
            lattice = yaml.safe_load(metaGrp["lattice"][()])
            params = yaml.safe_load(metaGrp["params"][()])
            makeActionSrc = metaGrp["action"][()]
            versions = {name: val[()] for name, val in metaGrp["version"].items()}
        except KeyError as exc:
            getLogger(__name__).error("Cannot read metadata from file %s: %s",
                                              str(fname), str(exc))
            raise
    return lattice, params, makeActionSrc, versions

def initializeNewFile(fname, overwrite, lattice, params, makeActionSrc, extraGroups=[]):
    """!
    Prepare the output file by storing program versions, metadata, and creating groups.
    If `overwrite==False` the file must not exist. If it is True, the file is removed if it exists.
    """

    fname = Path(fname)
    if fname.exists():
        if overwrite:
            fname.unlink()
            getLogger(__name__).info("Output file %s exists -- overwriting", fname)
        else:
            getLogger(__name__).error("Output file %s exists and not allowed to overwrite", fname)
            raise RuntimeError("Ouput file exists")

    with h5.File(str(fname), "w-") as h5f:
        for group in extraGroups:
            createH5Group(h5f, group)

    writeMetadata(fname, lattice, params, makeActionSrc)

def writeTrajectory(h5group, label, phi, actVal, trajPoint):
    r"""!
    Write a trajectory (endpoint) to a HDF5 group.
    Creates a new group with name 'label' and stores
    Configuration, action, and whenther the trajectory was accepted.

    \param h5group Base HDF5 group to store trajectory in.
    \param label Name of the subgroup of `h5group` to write to.
                 The subgroup must not already exist.
    \param phi Configuration to save.
    \param actVal Value of the action at configuration `phi`.
    \param trajPoint Point on the trajectory that was accepted.
                     `trajPoint==0` is the start point and values `>0` or `<0` are
                     `trajPoint` MD steps after or before the start point.

    \returns The newly created HDF5 group containing the trajectory.
    """

    grp = h5group.create_group(str(label))
    grp["phi"] = phi
    grp["action"] = actVal
    grp["trajPoint"] = trajPoint
    return grp

def writeCheckpoint(h5group, label, rng, trajGrpName, proposer, proposerManager):
    r"""!
    Write a checkpoint to a HDF5 group.
    Creates a new group with name 'label' and stores RNG state
    and a soft link to the trajectory for this checkpoint.

    \param h5group Base HDF5 group to store trajectory in.
    \param label Name of the subgroup of `h5group` to write to.
                 The subgroup must not already exist.
    \param rng Random number generator whose state to save in the checkpoint.
    \param trajGrpName Name of the HDF5 group containing the trajectory this
                       checkpoint corresponds to.
    \param proposer Proposer used to make the trajectory at this checkpoint.
    \param proposerManager Instance of ProposerManager to handle saving the proposer.

    \returns The newly created HDF5 group containing the checkpoint.
    """

    grp = h5group.create_group(str(label))
    rng.writeH5(grp.create_group("rngState"))
    grp["cfg"] = h5.SoftLink(trajGrpName)
    proposerManager.save(proposer, grp.create_group("proposer"))
    return grp

def loadCheckpoint(h5group, label, proposerManager, action, lattice):
    r"""!
    Load a checkpoint from a HDF5 group.

    \param h5group Base HDF5 group containing checkpoints.
    \param label Name of the subgroup of `h5group` to read from.
    \param proposerManager A ProposerManager to load the proposer
                           including its type.
    \param action Action to construct the proposer with.
    \param lattice Lattice to construct the proposer with.
    \returns (RNG, configuration, proposer)
    """

    grp = h5group[str(label)]
    rng = readStateH5(grp["rngState"])
    phi = Vector(grp["cfg/phi"][()])
    proposer = proposerManager.load(grp["proposer"], action, lattice)
    return rng, phi, proposer
