Getting Started
===============

Pre-Requisites
--------------

``SAGE`` should compile on most systems out of the box and the only required tool is a `C99  compiler <https://en.wikipedia.org/wiki/C99>`_.
`GSL <http://www.gnu.org/software/gsl/>`_ is recommended but not necessary.

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

Addtionally, ``SAGE`` can be configured to read trees in `HDF5 <https://support.hdfgroup.org/HDF5/>`_ format by setting
``USE-HDF5 = yes`` in the ``Makefile``. If the input trees are in HDF5 format, or you wish to output the catalogs in HDF5 (rather than the default binary format), then please compile with the ``USE-HDF5 = yes`` option.
