Plotting **SAGE** Results
=========================

Installation
------------

We have created a small analysis package (**sage_analysis**) to provide greater flexibility in the
plotting options for **SAGE**.  Indeed, this package is highly customizable and
we recommend using it for a variety of projects! It can be locally installed
via:

.. code::

    $ cd sage-model/
    $ pip install . (--user)

Example Usage
-------------

In the ``plotting``

In the ``analysis`` directory are a number of Python scripts to read and parse
the ``SAGE`` output.  The most important file is ``example.py`` which creates
plots for the default Mini-Millennium galaxies.

.. code::

    $ cd analysis/
    $ python example.py

and will create a number of useful diagnostic plots in the ``analysis/plots``
directory.

We also include the ability to compare the properties of a number of different
models.  See the documenation in the ``__main__`` function call of ``example.py`` to use this functionality.
