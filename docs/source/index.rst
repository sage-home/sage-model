.. sage-model documentation master file, created by
   sphinx-quickstart on Sat Jun  1 14:01:23 2019.
   You can adapt this file completely to your liking, but it should at least
   contain the root `toctree` directive.

The Semi-Analytic Galaxy Evolution (**SAGE**)
===========================================

This is the documentation for the Semi-Analytic Galaxy Evolution  model.
**SAGE** was original developed by *Darren Croton* and is a significant update
to the model described in `Croton et al., 2006`_. The updated **SAGE** model is described
`Croton et al., 2016`_. The code is publicly available and can be found on
`Github`_.

.. _Croton et al., 2006: https://arxiv.org/abs/astro-ph/0508046
.. _Croton et al., 2016: https://arxiv.org/abs/1601.04709
.. _Github: https://github.com/sage-home/sage-model

Citation
--------

If you use **SAGE** in a publication, please cite the following:

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

Maintainers
-----------

- Jacob Seiler (`@jacobseiler`_)
- Manodeep Sinha (`@manodeep`_)
- Darren Croton (`@darrencroton`_)

.. _@jacobseiler: https://github.com/jacobseiler
.. _@manodeep: https://github.com/manodeep
.. _@darrencroton: https://github.com/darrencroton

* :ref:`user-docs`
* :ref:`api-docs`

.. _user-docs:

.. toctree::
   :maxdepth: 2
   :caption: User Documentation

   introduction
   user/getting_started.rst
   user/plotting.rst
   user/subclass.rst
   user/calc.rst

.. _api-docs:

.. toctree::
   :maxdepth: 2
   :caption: API Reference

   api/modules
