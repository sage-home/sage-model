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

**For Python Analysis and Plotting:**

Python 3.6+ is required for the analysis and plotting tools. The following system packages are recommended:

- **Ubuntu/Debian**: ``sudo apt-get install python3-dev python3-venv libhdf5-dev libopenmpi-dev``
- **macOS (Homebrew)**: ``brew install hdf5 open-mpi``
- **CentOS/RHEL**: ``sudo yum install python3-devel hdf5-devel openmpi-devel``

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

This builds SAGE with all properties included (full-physics mode). For optimised builds:

.. code::

    $ make physics-free     # Core properties only (fastest, minimal memory)
    $ make full-physics     # All properties (default, maximum compatibility)
    $ make custom-physics CONFIG=input/config.json  # Properties for specific modules

Use ``make help`` for detailed build target information. Different property configurations require recompilation.

``SAGE`` is MPI compatible which can be enabled setting ``USE-MPI = yes`` in
the ``Makefile``.  To run in parallel, ensure that you have a installed an MPI distribution (OpenMPI, MPICH, Intel MPI etc).
When compiling with MPI support, the ``Makefile`` expects that the MPI compiler is called ``mpicc`` and is configured appropriately.

Addtionally, ``SAGE`` can be configured to read trees in `HDF5 <https://support.hdfgroup.org/HDF5/>`_ format by setting
``USE-HDF5 = yes`` in the ``Makefile``. If the input trees are in HDF5 format, or you wish to output the catalogs in HDF5 (rather than the default binary format), then please compile with the ``USE-HDF5 = yes`` option.

Setting up Python Environment
=============================

``SAGE`` includes Python tools for analysis, plotting, and testing. To use these tools, set up a Python virtual environment:

.. code::

    $ python3 -m venv sage_venv
    $ source sage_venv/bin/activate
    $ pip install -r requirements.txt

**What this installs:**

- **Core libraries**: NumPy, Matplotlib, h5py for scientific computing and plotting
- **Interface**: CFFI for Python-C integration
- **Configuration**: PyYAML for property system generation
- **User interface**: tqdm for progress bars
- **Optional**: MPI support and external analysis packages

**Activating the environment:**

.. code::

    $ source sage_venv/bin/activate    # Activate environment
    $ deactivate                       # Deactivate when done

The virtual environment needs to be activated before running Python scripts in the ``plotting/``, ``tests/``, or root directories.

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

Plotting the output (basic method)
==================================

After setting up the Python environment (see above), you can use the basic plotting scripts in the plotting directory:

.. code::

    $ source sage_venv/bin/activate     # Activate Python environment
    $ cd plotting/
    $ python3 allresults-local.py      # z=0 results
    $ python3 allresults-history.py    # Higher redshift results

Near the top of both scripts, there is a "USER OPTIONS" section where you can modify the simulation and plotting details for your own needs. 
These scripts can be used as a template to read the hdf5 ``SAGE`` model output and to make your own custom figures.


Plotting the output (sage-analysis package)
===========================================

We have a separate `sage-analysis <https://github.com/sage-home/sage-analysis/>`_ python package for plotting ``SAGE`` output. Please refer to the `sage_analysis
documentation <https://sage-analysis.readthedocs.io/en/latest/user/analyzing_sage.html>`_ for more details. 


Installing ``sage-analysis`` (requires python version >= 3.6)
--------------------------------------------------------------

You can install the external sage-analysis package in your existing Python environment:

.. code::

    $ source sage_venv/bin/activate   # Use the SAGE Python environment
    $ pip install sage-analysis       # Install from PyPI

Alternatively, to install from source:

.. code::

    $ cd ../    # <- Change to the location where you want to clone the sage-analysis repo
    $ git clone https://github.com/sage-home/sage-analysis.git
    $ cd sage-analysis  
    $ source ../sage-model/sage_venv/bin/activate    # Use the SAGE Python environment
    $ python3 -m pip install -e .                    # Install the sage-analysis python package
    $ cd ../sage-model 

Assuming that the `sage-analysis` repo was installed successfully, you are now ready to plot the output from ``SAGE``.

Plotting
--------

The ``plotting`` directory contains an ``example.py`` script that can be run to plot the basic output from ``SAGE``.

.. code::

    $ source sage_venv/bin/activate   # Activate Python environment
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
