#!/bin/bash
cwd=`pwd`
datadir=test_data/
# the bash way of figuring out the absolute path to this file
# (irrespective of cwd). parent_path should be $SAGEROOT/tests
parent_path=$( cd "$(dirname "${BASH_SOURCE[0]}")" ; pwd -P )
echo "Printing current directory"
pwd

mkdir -p "$parent_path"/$datadir
if [[ $? != 0 ]]; then
    echo "Could not create directory $parent_path/$datadir"
    echo "Failed"
    exit 1
fi

cd "$parent_path"/$datadir
if [[ $? != 0 ]]; then
    echo "Could not cd into directory $parent_path/$datadir"
    echo "Failed"
    exit 1
fi

if [ ! -f trees_063.7 ]; then
    wget "https://www.dropbox.com/s/l5ukpo7ar3rgxo4/mini-millennium-treefiles.tar?dl=0"  -O "mini-millennium-treefiles.tar"
    if [[ $? != 0 ]]; then
        echo "Could not download tree files from the Manodeep Sinha's Dropbox...aborting tests"
        echo "Failed"
        exit 1
    fi

    tar xvf mini-millennium-treefiles.tar
    if [[ $? != 0 ]]; then
        echo "Could not untar the mini-millennium tree files...aborting tests"
        echo "Failed"
        exit 1
    fi

    wget "https://www.dropbox.com/s/i6y03mbqz664dka/mini-millennium-sage-correct-output.tar?dl=0" -O "mini-millennium-sage-correct-output.tar"
    if [[ $? != 0 ]]; then
        echo "Could not download correct model output from the Manodeep Sinha's Dropbox...aborting tests"
        echo "Failed"
        exit 1
    fi

    tar -xvf mini-millennium-sage-correct-output.tar
    if [[ $? != 0 ]]; then
        echo "Could not untar the correct model output...aborting tests"
        echo "Failed"
        exit 1
    fi

fi

rm -f test_sage_z*

# cd back into the sage root directory and then run sage
echo "Printing current directory before I jump up two directories"
pwd
cd ../../
echo "Printing current directory"
pwd
./sage "$parent_path"/$datadir/mini-millennium.par
if [[ $? != 0 ]]; then
    echo "sage exited abnormally...aborting tests"
    echo "Failed"
    exit 1
fi

# now cd into the output directory for this sage-run
cd "$parent_path"/$datadir

# These commands create arrays containing the file names. Used because we're going to iterate over both files simultaneously.
test_files=($(ls -d test_sage_z*))
correct_files=($(ls -d correct-mini-millennium-output_z*))

if [[ $? == 0 ]]; then
    npassed=0
    nbitwise=0
    nfiles=0
    nfailed=0
    for f in ${test_files[@]}; do
        ((nfiles++))
        diff -q ${test_files[${nfiles}-1]} ${correct_files[${nfiles}-1]} 
        if [[ $? == 0 ]]; then
            ((npassed++))
            ((nbitwise++))
        else
            python "$parent_path"/sagediff.py ${test_files[${nfiles}-1]} ${correct_files[${nfiles}-1]}         
            if [[ $? == 0 ]]; then 
                ((npassed++))
            else
                ((nfailed++))
            fi
        fi
    done
else
    # even the simple ls model_z* failed
    # which means the code didnt produce the output files
    # everything failed
    npassed=0
    # use the knowledge that there should have been 64
    # files for mini-millennium test case
    # This will need to be changed once the files get combined -- MS: 10/08/2018
    nfiles=64
    nfailed=$nfiles
fi
echo "Passed: $npassed. Bitwise identical: $nbitwise"
echo "Failed: $nfailed"
# restore the original working dir
cd "$cwd"
exit $nfailed
