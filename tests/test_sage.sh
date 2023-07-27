#!/bin/bash
cwd=$(pwd)
datadir=test_data/

# The bash way of figuring out the absolute path to this file
# (irrespective of cwd). parent_path should be $SAGEROOT/tests
parent_path=$( cd "$(dirname "${BASH_SOURCE[0]}")" ; pwd -P )

# Create the directories to host all the data.
mkdir -p "$parent_path"/$datadir
if [[ $? != 0 ]]; then
    echo "Could not create directory $parent_path/$datadir"
    echo "Failed."
    echo "If the fix to this isn't obvious, please feel free to open an issue on our GitHub page."
    echo "https://github.com/sage-home/sage-model/issues/new"
    exit 1
fi

cd "$parent_path"/$datadir
if [[ $? != 0 ]]; then
    echo "Could not cd into directory $parent_path/$datadir"
    echo "Failed"
    exit 1
fi

# If there isn't a Mini-Millennium tree, then download them.
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

    # Now that we have confirmed we have either `wget` or `curl`, proceed to downloading the trees and data.
    wget "https://www.dropbox.com/s/l5ukpo7ar3rgxo4/mini-millennium-treefiles.tar?dl=0" -O "mini-millennium-treefiles.tar"
    if [[ $? != 0 ]]; then
        echo "Could not download tree files from the Manodeep Sinha's Dropbox...aborting tests."
        echo "Failed."
        echo "If the fix to this isn't obvious, please feel free to open an issue on our GitHub page."
        echo "https://github.com/sage-home/sage-model/issues/new"
        exit 1
    fi

    tar xvf mini-millennium-treefiles.tar
    if [[ $? != 0 ]]; then
        echo "Could not untar the mini-millennium tree files...aborting tests."
        echo "Failed."
        echo "If the fix to this isn't obvious, please feel free to open an issue on our GitHub page."
        echo "https://github.com/sage-home/sage-model/issues/new"
        exit 1
    fi

    # If there aren't trees, there's no way there is the 'correct' data files.
    wget https://www.dropbox.com/s/mxvivrg19eu4v1f/mini-millennium-sage-correct-output.tar?dl=0 -O "mini-millennium-sage-correct-output.tar"
    if [[ $? != 0 ]]; then
        echo "Could not download correct model output from the Manodeep Sinha's Dropbox...aborting tests."
        echo "Failed."
        echo "If the fix to this isn't obvious, please feel free to open an issue on our GitHub page."
        echo "https://github.com/sage-home/sage-model/issues/new"
        exit 1
    fi

    # If we used `curl`, remove the `wget` alias.
    if [[ $clear_alias == 1 ]]; then
        unalias wget
    fi

    tar -xvf mini-millennium-sage-correct-output.tar
    if [[ $? != 0 ]]; then
        echo "Could not untar the correct model output...aborting tests."
        echo "Failed."
        echo "If the fix to this isn't obvious, please feel free to open an issue on our GitHub page."
        echo "https://github.com/sage-home/sage-model/issues/new"
        exit 1
    fi

fi

rm -f test_sage_z*

# cd back into the sage root directory and then run sage
cd ../../

# The 'MPI_RUN_COMMAND' environment variable allows us to run in mpi.
# When running 'sagediff.py', we need knowledge of the number of processors SAGE ran on.
# This command queries 'MPI_RUN_COMMAND' and gets the last column (== the number of processors).
NUM_SAGE_PROCS=$(echo ${MPI_RUN_COMMAND} | awk '{print $NF}')

# If we're running on serial, MPI_RUN_COMMAND shouldn't be set.  Hence set the number of processors to 1.
if [[ -z "${NUM_SAGE_PROCS}" ]]; then
   NUM_SAGE_PROCS=1
fi

# Execute SAGE (potentially in parallel).
tmpfile="$(mktemp)"
sed '/^OutputFormat /s/.*$/OutputFormat        sage_binary/' "$parent_path"/$datadir/mini-millennium.par > ${tmpfile}

${MPI_RUN_COMMAND} ./sage "${tmpfile}"
if [[ $? != 0 ]]; then
    echo "sage exited abnormally...aborting tests."
    echo "Failed."
    echo "If the fix to this isn't obvious, please feel free to open an issue on our GitHub page."
    echo "https://github.com/sage-home/sage-model/issues/new"
    exit 1
fi

# Now cd into the output directory for this run.
pushd "$parent_path"/$datadir

# These commands create arrays containing the file names. Used because we're going to iterate over both files simultaneously.
# We will iterate over 'correct_files' and hence we want the first entries 8 entries of 'test_files' to be all the different redshifts
# with file extension '_0'.  This is what the `sort` command does.
correct_files=($(ls -d correct-mini-millennium-output_z*))
test_files=($(ls -d test_sage_z* | sort -k 1.18))
if [[ $? == 0 ]]; then
    npassed=0
    nbitwise=0
    nfiles=0
    nfailed=0
    for f in ${correct_files[@]}; do
        ((nfiles++))

        # First check if the correct and tests files are bitwise identical.
        diff -q ${test_files[${nfiles}-1]} ${correct_files[${nfiles}-1]} 2>&1 1>/dev/null
        if [[ $? == 0 ]]; then
            ((npassed++))
            ((nbitwise++))
        else
            # If they're not identical, manually check all the fields for differences.
            # The two `1` at the end here denotes that the 'correct' SAGE files are in one file.
            python "$parent_path"/sagediff.py ${correct_files[${nfiles}-1]} ${test_files[${nfiles}-1]} binary-binary 1 $NUM_SAGE_PROCS
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
    echo "If the fix to this isn't obvious, please feel free to open an issue on our GitHub page."
    echo "https://github.com/sage-home/sage-model/issues/new"
    exit 1
fi

# Now that we've checked the binary output, also check the HDF5 output format.
# First replace the "OutputFormat" argument with HDF5 in the parameter file.
popd
tmpfile="$(mktemp)"
sed '/^OutputFormat /s/.*$/OutputFormat        sage_hdf5/' "$parent_path"/$datadir/mini-millennium.par > ${tmpfile}

# Run SAGE on this new parameter file.
${MPI_RUN_COMMAND} ./sage "${tmpfile}"
if [[ $? != 0 ]]; then
    echo "sage exited abnormally when running on the HDF5 output format."
    echo "Here is the input file for this run."
    cat $tmpfile
    echo "If the fix to this isn't obvious, please feel free to open an issue on our GitHub page."
    echo "https://github.com/sage-home/sage-model/issues/new"
    exit 1
fi

rm -f ${tmpfile}

# now cd into the output directory for this sage-run
cd "$parent_path"/$datadir

# For the binary output, there are multiple redshift files.  However for HDF5, there is a single file.
correct_files=($(ls -d correct-mini-millennium-output_z*))
test_file=$(ls test_sage.hdf5)

# Now run the comparison between each correct binary file and the single HDF5 file.
if [[ $? == 0 ]]; then
    npassed=0
    nfiles=0
    nfailed=0
    for f in ${correct_files[@]}; do
        ((nfiles++))
        # The two `1` at the end here denotes that the 'correct' SAGE files are in one file and
        # the SAGE output we're testing was written to a single file.
        python "$parent_path"/sagediff.py ${correct_files[${nfiles}-1]} ${test_file} binary-hdf5 1 1
        if [[ $? == 0 ]]; then
            ((npassed++))
        else
            ((nfailed++))
        fi
    done
else
    # Even the simple ls failed which means the code didnt produce the output file
    npassed=0
    # Use the knowledge that there is 8 SAGE output files we were trying to check against.
    nfiles=8
    nfailed=$nfiles
fi
echo "Passed: $npassed."
echo "Failed: $nfailed."

# restore the original working dir
cd "$cwd"
exit $nfailed
