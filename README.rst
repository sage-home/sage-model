|TRAVIS|
|DOI|

*************************************
Semi-Analytic Galaxy Evolution (SAGE)
*************************************

``SAGE`` is a publicly available code-base for modelling galaxy formation in a
cosmological context. A description of the model and its default calibration
results can be found in `Croton et al. (2016) <https://arxiv.org/abs/1601.04709>`_.
``SAGE`` is a significant update to that previously used in `Croton et al. (2006) <http://arxiv.org/abs/astro-ph/0508046>`_.

``SAGE`` is written in C and was built to be modular and customisable.
It will run on any N-body simulation whose trees are organised in a supported format and contain a minimum set of basic halo properties.
For testing purposes, treefiles for the `mini-Millennium Simulation <http://arxiv.org/abs/astro-ph/0504097>`_ are available
`here <https://data-portal.hpc.swin.edu.au/dataset/mini-millennium-simulation>`_. 

Galaxy formation models built using ``SAGE`` on the Millennium, Bolshoi and simulations can be downloaded at the
`Theoretical Astrophysical Observatory (TAO) <https://tao.asvo.org.au/>`_. You can also find SAGE on `ascl.net <http://ascl.net/1601.006>`_.

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

To create the ``sage`` executable, simply run the following command:

.. code::
    $ make

``SAGE`` is MPI compatible which can be enabled setting ``USE-MPI = yes`` in
the ``Makefile``.  To run in parallel, ensure that you have a parallel compiler
downloaded and installed on your system (insert link to ``MPICC``?).  

Addtionally, ``SAGE`` can be configured to read trees in `HDF5 <https://support.hdfgroup.org/HDF5/>`_ format by setting
``USE-HDF5 = yes`` in the ``Makefile``. Ensure that HDF5 is installed on your system (insert link to ``HDF5``?).

Running the code
================

If this the first time running the code, we recommend executing
``first_run.sh``.  This script will initialize the directories for the default
parameter file and download the Mini-millennium dark matter halo trees:

.. code::

    $ ./first_run.sh

After this, the model can be run using:

.. code::

    $ ./sage input/millennium.par

or in parallel as:

.. code::

    $ mpirun -np <NUMBER_PROCESSORS> ./sage input/millennium.par

Plotting the Output
===================

In the ``analysis`` directory are a number of Python scripts to read and parse
the ``SAGE`` output.  The most important file is ``allresults.py`` which  acts
as the driver file.  It can be executed as:

.. code::

    $ cd analysis/
    $ python allresults.py

and will create a number of useful diagnostic plots in the ``analysis/plots``
directory.

We also include the ability to compare the properties of a number of different
models.  See the documenation in the ``__main__`` function call of ``allresults.py`` to use this functionality.

Citation
====================

If you use SAGE in a publication, please cite the following items:

.. code::

    @ARTICLE{2016ApJS..222...22C,
    author = {{Croton}, D.~J. and {Stevens}, A.~R.~H. and {Tonini}, C. and 
	{Garel}, T. and {Bernyk}, M. and {Bibiano}, A. and {Hodkinson}, L. and 
	{Mutch}, S.~J. and {Poole}, G.~B. and {Shattow}, G.~M.},
    title = "{Semi-Analytic Galaxy Evolution (SAGE): Model Calibration and Basic Results}",
    journal = {\apjs},
    archivePrefix = "arXiv",
    eprint = {1601.04709},
    keywords = {galaxies: active, galaxies: evolution, galaxies: halos, methods: numerical},
    year = 2016,
    month = feb,
    volume = 222,
    eid = {22},
    pages = {22},
    doi = {10.3847/0067-0049/222/2/22},
    adsurl = {http://adsabs.harvard.edu/abs/2016ApJS..222...22C},
    adsnote = {Provided by the SAO/NASA Astrophysics Data System}
    }

Maintainer 
====================

Questions and comments can be sent to Darren Croton: dcroton@astro.swin.edu.au.

.. |TRAVIS| image:: https://travis-ci.org/manodeep/sage.svg?branch=lhvt
    :target: https://travis-ci.org/manodeep/sage

.. |DOI| image:: https://zenodo.org/badge/13542/darrencroton/sage.svg
    :target: https://zenodo.org/badge/latestdoi/13542/darrencroton/sage

