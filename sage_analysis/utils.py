def generate_func_dict(plot_toggles, module_name, function_prefix, keyword_args={}):
    """
    Generates a dictionary where the keys are the function name and the value is a list
    containing the function itself (0th element) and keyword arguments as a dictionary
    (1st element). All functions in the returned dictionary are expected to have the same
    call signature for non-keyword arguments. Functions are only added when the
    ``plot_toggles`` value is non-zero.

    Functions are required to be named ``<module_prefix><function_prefix><plot_toggle_key>``
    For example, the default calculation function are kept in the ``model.py`` module and
    are named ``calc_<toggle>``.  E.g., ``sage_analysis.model.calc_SMF()``,
    ``sage_analysis.model.calc_BTF()``, ``sage_analysis.model.calc_sSFR()`` etc.

    Parameters
    ----------

    plot_toggles: dict, [string, int]
        Dictionary specifying the name of each property/plot and whether the values
        will be generated + plotted. A value of 1 denotes plotting, whilst a value of
        0 denotes not plotting.  Entries with a value of 1 will be added to the function
        dictionary.

    module_prefix: string
        Name of the module where the functions are located. If the functions are located
        in this module, pass an empty string "".

    function_prefix: string
        Prefix that is added to the start of each function.

    keyword_args: dict [string, dict[string, variable]], optional
        Allows the adding of keyword aguments to the functions associated with the
        specified plot toggle. The name of each keyword argument and associated value is
        specified in the inner dictionary.

    Returns
    -------

    func_dict: dict [string, list(function, dict[string, variable])]
        The key of this dictionary is the name of the function.  The value is a list with
        the 0th element being the function and the 1st element being a dictionary of
        additional keyword arguments to be passed to the function. The inner dictionary is
        keyed by the keyword argument names with the value specifying the keyword argument
        value.

    Examples
    --------
    >>> import sage_analysis.example_calcs
    >>> import sage_analysis.example_plots
    >>> plot_toggles = {"SMF": 1}
    >>> module_name = "sage_analysis.example_calcs"
    >>> function_prefix = "calc_"
    >>> generate_func_dict(plot_toggles, module_name, function_prefix) #doctest: +ELLIPSIS
    {'calc_SMF': [<function calc_SMF at 0x...>, {}]}
    >>> module_name = "sage_analysis.example_plots"
    >>> function_prefix = "plot_"
    >>> generate_func_dict(plot_toggles, module_name, function_prefix) #doctest: +ELLIPSIS
    {'plot_SMF': [<function plot_SMF at 0x...>, {}]}

    >>> import sage_analysis.example_plots
    >>> plot_toggles = {"SMF": 1}
    >>> module_name = "sage_analysis.example_plots"
    >>> function_prefix = "plot_"
    >>> keyword_args = {"SMF": {"plot_sub_populations": True}}
    >>> generate_func_dict(plot_toggles, module_name, function_prefix, keyword_args) #doctest: +ELLIPSIS
    {'plot_SMF': [<function plot_SMF at 0x...>, {'plot_sub_populations': True}]}

    >>> import sage_analysis.example_plots
    >>> plot_toggles = {"SMF": 1, "quiescent": 1}
    >>> module_name = "sage_analysis.example_plots"
    >>> function_prefix = "plot_"
    >>> keyword_args = {"SMF": {"plot_sub_populations": True},
    ...                 "quiescent": {"plot_output_format": ".pdf", "plot_sub_populations": True}}
    >>> generate_func_dict(plot_toggles, module_name, function_prefix, keyword_args) #doctest: +ELLIPSIS
    {'plot_SMF': [<function plot_SMF at 0x...>, {'plot_sub_populations': True}], \
'plot_quiescent': [<function plot_quiescent at 0x...>, {'plot_output_format': '.pdf', \
'plot_sub_populations': True}]}
    """

    # If the functions are defined in this module, then `module_name` is empty. Need to
    # treat this differently.
    import sys
    if module_name == "":

        # Get the name of this module.
        module = sys.modules[__name__]

    else:

        # Otherwise, check if the specified module is present.
        try:
            module = sys.modules[module_name]
        except KeyError:
            msg = "Module {0} has not been imported.\nPerhaps you need to create an empty " \
                  "`__init__.py` file to ensure your package can be imported.".format(module_name)
            raise KeyError(msg)

    # Only populate those methods that have been marked in the `plot_toggles`
    # dictionary.
    func_dict = {}
    for toggle in plot_toggles.keys():
        if plot_toggles[toggle]:

            func_name = "{0}{1}".format(function_prefix, toggle)

            # Be careful.  Maybe the func for a specified `plot_toggle` value wasn't
            # added to the module.
            try:
                func = getattr(module, func_name)
            except AttributeError:
                msg = "Tried to get the func named '{0}' corresponding to " \
                      "'plot_toggle' value '{1}'.  However, no func named '{0}' " \
                      "could be found in '{2}' module.".format(func_name,
                      toggle, module_prefix)
                raise AttributeError(msg)

            # We may have specified some keyword arguments for this plot toggle. Check.
            try:
                key_args = keyword_args[toggle]
            except KeyError:
                # No extra arguments for this.
                key_args = {}

            func_dict[func_name] = [func, key_args]

    return func_dict
