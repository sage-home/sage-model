from pathlib import Path

from sage_analysis.galaxy_analysis import GalaxyAnalysis
from sage_analysis.default_analysis_arguments import default_plot_toggles

from sage_analysis.plot_helper import PlotHelper
plot_helper = PlotHelper(output_path="../output/millennium/plots/")

if __name__ == "__main__":

    millennium_par_fname = Path(__file__).parent.joinpath("../input/millennium.par")
    print(millennium_par_fname)
    par_fnames = [millennium_par_fname]

    plot_toggles = default_plot_toggles.copy()  # Copy to ensure ``default_plot_toggles`` aren't overwritten.

    plot_toggles["SMF_history"] = True
    plot_toggles["SMD_history"] = True
    plot_toggles["SFRD_history"] = True

    galaxy_analysis = GalaxyAnalysis(
        par_fnames,
        plot_toggles=plot_toggles,
        history_redshifts={
            "SMF_history": [0.0, 0.5, 1.0, 2.0, 3.0],
            "SMD_history": "All",
            "SFRD_history": "All",
        },
    )
    galaxy_analysis.analyze_galaxies()
    galaxy_analysis.generate_plots(plot_helper=plot_helper)
