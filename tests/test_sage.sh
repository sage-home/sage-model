#!/bin/bash
cwd=`pwd`
datadir=test_data/
# the bash way of figuring out the absolute path to this file
# (irrespective of cwd). parent_path should be $SAGEROOT/tests
parent_path=$( cd "$(dirname "${BASH_SOURCE[0]}")" ; pwd -P )

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

    wget "https://www.dropbox.com/s/mxvivrg19eu4v1f/mini-millennium-sage-correct-output.tar?dl=0" -O "mini-millennium-sage-correct-output.tar"
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
cd ../../

$BEFORE_SAGE ./sage "$parent_path"/$datadir/mini-millennium.par
if [[ $? != 0 ]]; then
    echo "sage exited abnormally...aborting tests"
    echo "Failed"
    exit 1
fi

# now cd into the output directory for this sage-run
pushd "$parent_path"/$datadir

# These commands create arrays containing the file names. Used because we're going to iterate over both files simultaneously.
correct_files=($(ls -d correct-mini-millennium-output_z*))
test_files=($(ls -d test_sage_z* | sort -k 1.18))

echo "Test files"
echo ${test_files[0]}
echo ${test_files[1]}
echo ${test_files[2]}
echo ${test_files[3]}
echo ${test_files[4]}
echo ${test_files[5]}
echo ${test_files[6]}
echo ${test_files[7]}
echo "Correct files"
echo ${correct_files}
exit 1

if [[ $? == 0 ]]; then
    npassed=0
    nbitwise=0
    nfiles=0
    nfailed=0
    for f in ${correct_files[@]}; do
        ((nfiles++))
        diff -q ${test_files[${nfiles}-1]} ${correct_files[${nfiles}-1]} 
        if [[ $? == 0 ]]; then
            ((npassed++))
            ((nbitwise++))
        else
            python "$parent_path"/sagediff.py ${correct_files[${nfiles}-1]} ${test_files[${nfiles}-1]} binary-binary $AFTER_PYTHON
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
    nfiles=8
    nfailed=$nfiles
fi
echo "Passed: $npassed. Bitwise identical: $nbitwise"
echo "Failed: $nfailed."

if [[ $nfailed > 0 ]]; then
    echo "The binary-binary check failed."
    echo "Uh oh...I'm outta here!"
    exit 1
fi

# Now that we've checked the binary output, also check the HDF5 output format.
# First replace the "OutputFormat" argument with HDF5 in the parameter file.
popd
tmpfile="$(mktemp)"
sed '/^OutputFormat /s/.*$/OutputFormat        sage_hdf5/' "$parent_path"/$datadir/mini-millennium.par > ${tmpfile}

# Run SAGE on this new parameter file.
$BEFORE_SAGE ./sage ${tmpfile}
if [[ $? != 0 ]]; then
    echo "sage exited abnormally...aborting tests"
    echo "Failed"
    cat ${tmpfile}
    exit 1
fi

rm -f ${tmpfile}

# now cd into the output directory for this sage-run
cd "$parent_path"/$datadir

# Update this comment future Jacob.
correct_files=($(ls -d correct-mini-millennium-output_z*))
test_file=`ls test_sage_*.hdf5*`

if [[ $? == 0 ]]; then
    npassed=0
    nfiles=0
    nfailed=0
    for f in ${correct_files[@]}; do
        ((nfiles++))
        python "$parent_path"/sagediff.py ${correct_files[${nfiles}-1]} ${test_file} binary-hdf5 1
        if [[ $? == 0 ]]; then
            ((npassed++))
        else
            ((nfailed++))
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
    nfiles=8
    nfailed=$nfiles
fi
echo "Passed: $npassed."
echo "Failed: $nfailed."

# restore the original working dir
cd "$cwd"
exit $nfailed
