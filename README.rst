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
- ``./build.sh test`` - Run complete test suite
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

Plotting the output (basic method)
==================================

If you already have Python 3 installed, you can switch to the plotting directory, where you will find two scripts, 
``allresults-local.py`` (for z=0 results) and ``allresults-history.py`` (for higher redshift results). 
If you're following the above, these scripts can run as-is to produce a series of figures you can use to check the model output.

.. code::

    $ cd plotting/
    $ python3 allresults-local.py
    $ python3 allresults-history.py

Near the top of both scripts, there is a "USER OPTIONS" section where you can modify the simulation and plotting details for your own needs. 
These scripts can be used as a template to read the hdf5 ``SAGE`` model output and to make your own custom figures.


Plotting the output (sage-analysis package)
===========================================

We have a separate `sage-analysis <https://github.com/sage-home/sage-analysis/>`_ python package for plotting ``SAGE`` output. Please refer to the `sage_analysis
documentation <https://sage-analysis.readthedocs.io/en/latest/user/analyzing_sage.html>`_ for more details. 


Installing ``sage-analysis`` (requires python version >= 3.6)
--------------------------------------------------------------

.. code::

    $ cd ../    # <- Change to the location where you want to clone the sage-analysis repo
    $ git clone https://github.com/sage-home/sage-analysis.git
    $ cd sage-analysis  

You may need to first create a Python virtual environment in your sage-analysis directory and source it:

.. code::

    $ python3 -m venv .sage_venv
    $ source .sage_venv/bin/activate

Then finish installing sage-analysis:

.. code::

    $ python3 -m pip install -e .    # Install the sage-analysis python package
    $ cd ../sage-model 

Assuming that the `sage-analysis` repo was installed successfully, you are now ready to plot the output from ``SAGE``.

Plotting
--------

The ``plotting`` directory contains an ``example.py`` script that can be run to plot the basic output from ``SAGE``.

.. code::

    $ cd plotting/
    $ python3 example.py

This will create a number of plots in the ``plotting/plots/`` directory. Please refer to the `sage_analysis
documentation <https://sage-analysis.readthedocs.io/en/latest/user/analyzing_sage.html>`_ for a thorough guide on how
to tweak the plotting script to suit your needs.


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
