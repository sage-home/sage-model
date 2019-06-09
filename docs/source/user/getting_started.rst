Getting Started
===============

Pre-Requisites
--------------

**SAGE** should compile on most systems out of the box and the only required tool is a `C99  compiler`_.
`GSL`_ is recommended but not necessary.

.. _C99 compiler: https://en.wikipedia.org/wiki/C99
.. _GSL: http://www.gnu.org/software/gsl

Downloading
-----------

``SAGE`` can be installed by cloning the GitHub repository:

.. code::

    $ git clone https://github.com/sage-home/sage-model
    $ cd sage-model/

Building
--------

To create the ``SAGE`` executable, simply run the following command:

.. code::

    $ make

``SAGE`` is MPI compatible which can be enabled setting ``USE-MPI = yes`` in
the ``Makefile``.  To run in parallel, ensure that you have a installed an MPI distribution (OpenMPI, MPICH, Intel MPI etc).
When compiling with MPI support, the ``Makefile`` expects that the MPI compiler is called ``mpicc`` and is configured appropriately.

HDF5 Support
------------

**SAGE** supports reading and writing in `HDF5`_. We recommend writing your output
data in HDF5 for ease-of-use. To easily handle your HDF5 installation, we also
recommend using `Conda`_.

.. code::

    $ conda install -q --yes -c conda-forge hdf5

HDF5 support can be enabled by setting ``USE-HDF5 = yes`` in the ``Makefile``.
Adjust the ``tree_type`` and ``OutputFormat`` in your **SAGE** parameter file
to allow reading/writing of HDF5 files.

.. _Conda: https://docs.conda.io/projects/conda/en/latest/user-guide/getting-started.html

Example Usage
-------------

**SAGE** runs on dark matter halo merger trees constructed in a *vertical* format.
The trees for the Mini-Millennium dark matter simulation (a smaller box size
version of the `Millennium simulation` with identical mass resolution) can be
retrieved by executing the ``first_run.sh`` script from within the
``sage-model`` directory. This will create the necessary file structure and parameter
file required for running **SAGE**.

.. code::

    $ ./first_run.sh

After this, the model can be run using:

.. code::

    $ ./sage input/millennium.par

or in parallel as:

.. code::

    $ mpirun -np <NUMBER_PROCESSORS> ./sage input/millennium.par

.. _Millennium simulation: https://wwwmpa.mpa-garching.mpg.de/galform/virgo/millennium/
