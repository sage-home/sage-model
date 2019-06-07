# -*- coding: utf-8 -*-

"""
Sage_Analysis Setup
==========
Contains the setup script required for installing the *Sage_Analysis* package.
This can be ran directly by using::

    pip install .

or anything equivalent.

"""

# Built-in imports
from codecs import open

# Package imports
from setuptools import find_packages, setup


# %% SETUP DEFINITION
# Get the long description from the README file
with open('README.rst', 'r') as f:
    long_description = f.read()

# Get the requirements list by reading the file and splitting it up
with open('requirements.txt', 'r') as f:
    requirements = f.read().splitlines()

# Get the version from the __version__.py file
# This is done in this way to make sure it is stored in a single place and
# does not require the package to be installed already.
version = None
with open('sage_analysis/__version__.py', 'r') as f:
    exec(f.read())

# Setup function declaration
# See https://setuptools.readthedocs.io/en/latest/setuptools.html
setup(name='sage_analysis',      # Distribution name of package (e.g., used on PyPI)
      version=version,      # Version of this package (see PEP 440)
      author="Jacob Seiler",
      author_email="jseiler@swin.edu.au",
      maintainer="jseiler",   # PyPI username of maintainer(s)
      description=("Sage_Analysis: A Python package for reading SAGE data."),
      long_description=long_description,        # Use the README description
      url="https://github.com/sage-home/sage-model",
      license='MIT',        # License of this package
      # List of classifiers (https://pypi.org/pypi?%3Aaction=list_classifiers)
      classifiers=[
          'Development Status :: 5 - Production/Stable',
          'Intended Audience :: Developers',
          'License :: OSI Approved :: MIT License',
          'Natural Language :: English',
          'Operating System :: MacOS',
          'Operating System :: Microsoft :: Windows',
          'Operating System :: Unix',
          'Programming Language :: Python',
          'Programming Language :: Python :: 3',
          'Programming Language :: Python :: 3.5',
          'Programming Language :: Python :: 3.6',
          'Programming Language :: Python :: 3.7',
          'Topic :: Software Development'
          ],
      keywords=('sage'),    # List of keywords
      # String containing the Python version requirements (see PEP 440)
      python_requires='!=3.0.*, !=3.1.*, !=3.2.*, !=3.3.*, !=3.4.*, <4',
      packages=find_packages(),
      # Registered namespace vs. local directory
      package_dir={'sage_analysis': "sage_analysis"},
      include_package_data=True,        # Include non-Python files
      install_requires=requirements,    # Parse in list of requirements
      zip_safe=False,                   # Do not zip the installed package
      )
