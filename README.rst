|GitHub CI| |DOCS| 
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

Getting started
===============

Pre-requisites
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

``SAGE`` uses a modern CMake build system with a convenient wrapper script for familiar root-directory workflow:

**Quick Start (Recommended)**

.. code::

    $ ./build.sh          # Automatically sets up and builds everything

This creates the ``build/sage`` executable. For first-time users, start with the setup script:

.. code::

    $ ./first_run.sh      # Download test data and setup directories (first time only)
    $ ./build.sh          # Build SAGE

**Build Commands**

- ``./build.sh`` - Build SAGE (equivalent to traditional ``make``)
- ``./build.sh clean`` - Clean build artifacts  
- ``./build.sh tests`` - Run complete test suite
- ``./build.sh unit_tests`` - Run fast unit tests only
- ``./build.sh debug`` - Configure debug build with memory checking
- ``./build.sh release`` - Configure optimized release build
- ``./build.sh help`` - Show all available commands

**Advanced: Direct CMake Usage**

For users who prefer working directly with CMake:

.. code::

    $ mkdir build && cd build
    $ cmake ..
    $ make -j$(nproc)

**Configuration Options:**

- ``SAGE_USE_MPI=ON/OFF`` - Enable MPI support (default: OFF)
- ``SAGE_USE_HDF5=ON/OFF`` - Enable HDF5 support (default: ON)  
- ``CMAKE_BUILD_TYPE=Release/Debug`` - Build type (default: Debug)

Example with MPI enabled:

.. code::

    $ ./build.sh configure    # Or: cd build && cmake -DSAGE_USE_MPI=ON ..
    $ ./build.sh              # Build with MPI support

Running the code
================

If this the first time running the code, we recommend executing
``first_run.sh``.  This script will initialize the directories for the default
parameter file and download the Mini-millennium dark matter halo trees:

.. code::

    $ ./first_run.sh

After this, the model can be run using:

.. code::

    $ ./build/sage input/millennium.par

or in parallel as:

.. code::

    $ mpirun -np <NUMBER_PROCESSORS> ./build/sage input/millennium.par

Plotting the output
====================

``SAGE`` includes a comprehensive plotting tool called ``sage-plot`` that generates publication-quality figures from model output. The tool creates both snapshot plots (galaxy properties at z=0) and evolution plots (cosmic evolution across redshifts).

Python Environment Setup  
-------------------------

First, set up the Python environment with required dependencies:

.. code::

    $ python3 -m venv sage_venv
    $ source sage_venv/bin/activate
    $ pip install -r requirements.txt

Basic Plotting Usage
--------------------

Generate all plots (both snapshot and evolution) with a single command:

.. code::

    $ source sage_venv/bin/activate
    $ python plotting/sage-plot/sage-plot.py --param-file=input/millennium.par

This creates comprehensive plots in ``output/millennium/plots/`` including:

**Snapshot Plots** (galaxy properties at z=0):
- Stellar mass function, baryonic mass function, gas mass function
- Star formation rates, gas fractions, metallicities  
- Black hole-bulge relations, halo occupation distributions
- Spatial and kinematic distributions

**Evolution Plots** (cosmic evolution):
- Stellar mass function evolution across redshifts
- Cosmic star formation rate density evolution
- Stellar mass density evolution

Advanced Usage
--------------

.. code::

    # Generate only snapshot plots
    $ python plotting/sage-plot/sage-plot.py --param-file=input/millennium.par --snapshot-plots
    
    # Generate only evolution plots  
    $ python plotting/sage-plot/sage-plot.py --param-file=input/millennium.par --evolution-plots
    
    # Generate specific plots
    $ python plotting/sage-plot/sage-plot.py --param-file=input/millennium.par --plots=stellar_mass_function,gas_fraction
    
    # Specify output format and file range
    $ python plotting/sage-plot/sage-plot.py --param-file=input/millennium.par --format=.pdf --first-file=0 --last-file=7

The plotting tool works from any directory and automatically resolves paths relative to the SAGE root directory.

Legacy Plotting Scripts  
-----------------------

For users who prefer the original approach, basic plotting scripts are available in ``plotting/legacy-plot-examples/``:

.. code::

    $ cd plotting/legacy-plot-examples/
    $ python3 allresults-local.py    # z=0 results
    $ python3 allresults-history.py  # evolution results

These scripts contain a "USER OPTIONS" section for customization and can serve as templates for custom analysis.


Citation
=========

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

Author
=======

Questions and comments can be sent to Darren Croton: dcroton@astro.swin.edu.au.

Maintainers
============

- Jacob Seiler (@jacobseiler)
- Manodeep Sinha (@manodeep)
- Darren Croton (@darrencroton)

.. |GitHub CI| image:: https://github.com/sage-home/sage-model/actions/workflows/ci.yml/badge.svg
   :target: https://github.com/sage-home/sage-model/actions
   :alt: GitHub Actions Status
   
.. |DOCS| image:: https://img.shields.io/readthedocs/sage-model/latest.svg?logo=read%20the%20docs&logoColor=white&label=Docs
    :alt: RTD Badge
    :target: https://sage-model.readthedocs.io/en/latest/index.html
