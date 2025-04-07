#
# ICRAR - International Centre for Radio Astronomy Research
# (c) UWA - The University of Western Australia, 2019
# Copyright by UWA (in the framework of the ICRAR)
#
# Originally contributed by Mawson Sammons
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.
#
"""
Contains the methods used to quantify the "goodness" of fit
between model and observed parameters
"""

import functools
import math
import sys

import numpy as np # type: ignore
import scipy.special # type: ignore


if sys.version_info[0] == 3:
    b2s = lambda b: b.decode('ascii')
else:
    b2s = lambda b: b

def load_space(fname):
    """Loads the search space from a csv file"""
    space = np.genfromtxt(fname, delimiter=',',
              dtype=[('name', 'object'), ('plot_label', 'object'),
                     ('is_log', 'int'),
                     ('lb', 'float'), ('ub', 'float')])
    space['name'] = [b2s(n) for n in space['name']]
    space['plot_label'] = [b2s(n) for n in space['plot_label']]
    return space

def npsum(f):
    """Return the array-wise sum of the returned array"""
    @functools.wraps(f)
    def wrapper(*args):
        return np.sum(f(*args))
    return wrapper

@npsum
def chi2(obs, mod, err):
    error = np.std(obs)
    chi2 = ((mod - obs) / err)**2 #/ (len(mod) - 4.0) # hard-coded for 4 parameters being fit here for a reduced chi-squared.  Note that this array actually has to be summed to get the reduced chi^2 of the fit
    #chi2 = ((mod**2 - obs**2) / error**2) #/ (len(mod) - 4.0) # hard-coded for 4 parameters being fit here for a reduced chi-squared.  Note that this array actually has to be summed to get the reduced chi^2 of the fit
    #chi2 = np.nan_to_num(chi2, nan=0.0)
    #print(chi2)
    return chi2

@npsum
def studentT(obs, mod, err):
    # Ensure errors are valid
    err = np.where(err == 0, np.std(obs), err)
    err = np.maximum(err, 1e-8)

    # Calculate sigma and variance
    sigma = (obs - mod) / err
    var = np.mean(sigma ** 2)
    var = np.maximum(var, 1e-8)  # Just prevent division by zero

    # Calculate degrees of freedom (nu)
    nu = (2 * var) / (var - 1 + 1e-8)

    # Calculate x and ensure validity
    x = (mod - obs) ** 2 / err
    safe_x = np.maximum(1 + x / nu, 1e-8)

    # Calculate t using the Student's T-distribution formula
    t = (
        (scipy.special.gamma((nu + 1) / 2.0))
        / ((nu * math.pi) ** 0.5 * scipy.special.gamma(nu / 2.0))
        * safe_x ** (-1 * (nu + 1) / 2.0)
    )

    # Prevent log of zero
    t = np.maximum(t, 1e-8)

    # Return the negative log of t
    return -1.0 * np.log(t)


stat_tests = {
    'student-t': studentT,
    'chi2': chi2
}
