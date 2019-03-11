#!/bin/bash

echo "Welcome the Semi Analytic Galaxy Evolution model!"
echo ""

# We're going to execute things assuming we're in the root directory.
# Hence let's first ensure that we're actually there.
current_dir=${PWD##*/}  # This is the current directory without the base-name.
if [[ $current_dir != "sage-model" ]]; then
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
    echo "Fetching Mini-Millennium trees."
    wget "https://www.dropbox.com/s/l5ukpo7ar3rgxo4/mini-millennium-treefiles.tar?dl=0"  -O "mini-millennium-treefiles.tar"
    if [[ $? != 0 ]]; then
        echo "Could not download tree files from the Manodeep Sinha's Dropbox."
        echo "Please open an issue on the 'sage-model' repository and we will assist ASAP :)"
        exit 1
    fi
    echo "Done."
    echo ""

    tar xvf mini-millennium-treefiles.tar
    if [[ $? != 0 ]]; then
        echo "Could not untar the Mini-Millennium tree files."
        echo "Please open an issue on the 'sage-model' repository and we will assist ASAP :)"
        exit 1
    fi
    echo "Done."
    echo ""

    rm mini-millennium-treefiles.tar
    echo "Mini-Millennium trees successfully gathered and placed into '${PWD}'"
else
    echo "Mini-Millennium trees already present in 'input/millennium/trees'."
fi

echo "SAGE should be compiled with the 'make' command."
echo "Once compiled, it can be ran by executing './sage input/millennium.par'"
echo "Afterwards, 'cd' into the 'analysis' directory and make some gorgeous plots using 'python allresults.py'"
echo ""

echo "If you have any questions, queuries or comments, please feel free to open an Issue on GitHub!"
echo "Have fun!"
