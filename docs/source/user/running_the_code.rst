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
