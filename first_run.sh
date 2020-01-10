#!/bin/bash

echo "Welcome the Semi Analytic Galaxy Evolution model!"
echo "Always keep up to date by visiting our main Github page."
echo "https://github.com/sage-home"
echo ""

# We're going to execute things assuming we're in the root directory.
# Hence let's first ensure that we're actually there.
parent_path=$( cd "$(dirname "${BASH_SOURCE[0]}")" ; pwd -P )
current_dir_end=${PWD##*/}  # This is the current directory without the base-name.
if [[ ${current_dir_end} != *"sage-model"* ]]; then
   echo "This setup script should be run from inside the 'sage-model' directory."
   echo "Please 'cd' there first and execute again."
   exit 1
fi

echo "Making the 'output/millennium' directory to hold the model output."

# By using `-p`, `mkdir` won't fail if the directory already exists.
mkdir -p output/millennium
echo "Done."
echo ""

echo "Creating the 'input/millennium/trees' directory."
mkdir -p input/millennium/trees
echo "Done."
echo ""

cd input/millennium/trees

if [ ! -f trees_063.7 ]; then

    # To download the trees, we use either `wget` or `curl`. By default, we want to use `wget`.
    # However, if it isn't present, we will use `curl` with a few parameter flags.
    echo "First checking if either 'wget' or 'curl' are present in order to download trees."

    clear_alias=0
    command -v wget

    if [[ $? != 0 ]]; then
        echo "'wget' is not available. Checking if 'curl' can be used."
        command -v curl

        if [[ $? != 0 ]]; then
            echo "Neither 'wget' nor 'curl' are available to download the Mini-Millennium trees."
            echo "Please install one of these to download the trees."
            exit 1
        fi

        echo "Using 'curl' to download trees."

        # `curl` is available. Alias it to `wget` with some options.
        alias wget="curl -L -O -C -"

        # We will need to clear this alias up later.
        clear_alias=1
    else
        echo "'wget' is present. Using it!"
    fi

    # Now that we have confirmed we have either `wget` or `curl`, proceed to downloading the trees.
    echo "Fetching Mini-Millennium trees."
    wget "https://www.dropbox.com/s/l5ukpo7ar3rgxo4/mini-millennium-treefiles.tar?dl=0" -O "mini-millennium-treefiles.tar"
    if [[ $? != 0 ]]; then
        echo "Could not download tree files from the Manodeep Sinha's Dropbox."
        echo "Please open an issue on the 'sage-model' repository and we will assist ASAP :)"
        exit 1
    fi
    echo "Done."
    echo ""

    # If we used `curl`, remove the `wget` alias.
    if [[ $clear_alias == 1 ]]; then
        unalias wget
    fi

    tar -xvf mini-millennium-treefiles.tar
    if [[ $? != 0 ]]; then
        echo "Could not untar the Mini-Millennium tree files."
        echo "Please open an issue on the 'sage-model' repository and we will assist ASAP :)"
        exit 1
    fi
    echo "Done."
    echo ""

    rm -f mini-millennium-treefiles.tar
    echo "Mini-Millennium trees successfully gathered and placed into '${PWD}'"

else
    echo "Mini-Millennium trees already present in 'input/millennium/trees'."
fi

# Now to properly do the plotting, we require the OutputDir, SimulationDir and FileWithSnapList in the SAGE parameter file to be an absolute path.
echo "Appending the 'OutputDir', 'SimulationDir' and 'FileWithSnapList' paths in the SAGE parameter file with your current directory '${parent_path}'"
echo ""

# Move back to the directoy with the input file.
cd ../..

# Generate the new name and replace.
new_OutputDir='OutputDir   '"$parent_path"'/output/millennium/'
new_SimulationDir='SimulationDir   '"$parent_path"'/input/millennium/trees/'
new_FileWithSnapList='FileWithSnapList '"$parent_path"'/input/millennium/trees/millennium.a_list'

# The '%' after the '-Ei' flags are required for MacOS to properly perform the sed command.
# I'm baffled why it's necessary but hey, it now works on Mac and doesn't affect linux *shrug*.
sed -Ei"%" "s|^(OutputDir[[:blank:]]*).*|$new_OutputDir|g" millennium.par
sed -Ei"%" "s|^(SimulationDir[[:blank:]]*).*|$new_SimulationDir|g" millennium.par
sed -Ei"%" "s|^(FileWithSnapList[[:blank:]]*).*|$new_FileWithSnapList|g" millennium.par

# For some odd reason, the '%' flag at the start makes a file named 'millennium.par%'. Remove this.
rm -f millennium.par%

echo "SAGE should be compiled with the 'make' command."
echo "Once compiled, it can be ran by executing './sage input/millennium.par'"
echo "Afterwards, 'cd' into the 'analysis' directory and make some gorgeous plots using 'python allresults.py'"
echo ""

echo "If you have any questions, queuries or comments, please feel free to open an Issue on GitHub!"
echo "https://github.com/sage-home/sage-model/issues/new"
echo ""
echo "Have fun!"
