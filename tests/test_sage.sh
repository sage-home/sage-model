#!/bin/bash
#
# test_sage.sh - Test script for SAGE (Semi-Analytical Galaxy Evolution) model
#
# This script:
# 1. Sets up test data directory and downloads Mini-Millennium merger trees if needed
# 2. Sets up a Python environment with the required dependencies (numpy, h5py)
# 3. Optionally compiles SAGE before testing (with --compile flag)
# 4. Runs SAGE with binary output format and compares to reference outputs
# 5. Runs SAGE with HDF5 output format and compares to reference outputs
# 
# Usage: 
#   ./test_sage.sh               # Run tests with default settings (verbose and complie on)
#   ./test_sage.sh --noverbose   # Run tests without verbose output
#   ./test_sage.sh --nocompile   # Don't compile SAGE before running tests
#   ./test_sage.sh --nocompile --noverbose  # Don't compile or use verbose
# 
# --nocompile requires a compiled SAGE executable in the parent directory
# When comparing outputs, we use sagediff.py to ensure all galaxy properties match

# Save current working directory to return at the end
ORIG_DIR=$(pwd)
TEST_DATA_DIR="test_data/"
TEST_RESULTS_DIR="test_results/"
ENV_DIR="test_env"

# Function to show help message
show_help() {
    echo "Usage: $0 [OPTIONS]"
    echo
    echo "Options:"
    echo "  --help        Show this help message"
    echo "  --noverbose   Run tests without verbose output"
    echo "  --nocompile   Don't compile SAGE before running tests"
    echo
    echo "Examples:"
    echo "  $0                        # Run tests with default settings (verbose and complie on)"
    echo "  $0 --noverbose            # Run tests without verbose output"
    echo "  $0 --nocompile            # Don't compile SAGE before running tests"
    echo "  $0 --nocompile --noverbose  # Don't compile or use verbose"
    echo
}

# Process command line arguments
VERBOSE_MODE=1
COMPILE_MODE=1
for arg in "$@"; do
    case $arg in
        --help)
            show_help
            exit 0
            ;;
        --noverbose)
            VERBOSE_MODE=0
            shift
            ;;
        --nocompile)
            COMPILE_MODE=0
            shift
            ;;
        *)
            echo "Unknown option: $arg"
            echo "Usage: $0 [--help] [--noverbose] [--nocompile]"
            echo "Try '$0 --help' for more information"
            exit 1
            ;;
    esac
done

# Determine the absolute path to the test directory
# (irrespective of current working directory)
TESTS_DIR=$( cd "$(dirname "${BASH_SOURCE[0]}")" ; pwd -P )
TEST_DATA_PATH="${TESTS_DIR}/${TEST_DATA_DIR}"
TEST_RESULTS_PATH="${TESTS_DIR}/${TEST_RESULTS_DIR}"
ENV_PATH="${TESTS_DIR}/${ENV_DIR}"
SAGE_ROOT_DIR="${TESTS_DIR}/../"

# Function to display error and exit
error_exit() {
    echo "ERROR: $1"
    echo "Failed."
    echo "If the fix to this isn't obvious, please open an issue at:"
    echo "https://github.com/sage-home/sage-model/issues/new"
    exit 1
}

# Function to compile SAGE
compile_sage() {
    echo "---- COMPILING SAGE ----"
    cd "${SAGE_ROOT_DIR}" || error_exit "Could not change to SAGE root directory: ${SAGE_ROOT_DIR}"
    
    if [ $VERBOSE_MODE -eq 1 ]; then
        echo "Compiling SAGE in verbose mode (MAKE-VERBOSE set)"
        make clean && make MAKE-VERBOSE=yes
    else
        echo "Compiling SAGE"
        make clean && make
    fi
    
    if [ $? -ne 0 ]; then
        error_exit "Failed to compile SAGE"
    fi
    
    echo "SAGE compilation successful"
    echo
}

# Function to set up Python environment with required dependencies
setup_python_environment() {
    echo "---- SETTING UP PYTHON ENVIRONMENT ----"
    
    # Check if the environment directory already exists
    if [ -d "$ENV_PATH" ] && [ -f "$ENV_PATH/bin/python" ]; then
        echo "Python environment already exists at $ENV_PATH"
        
        # Check if the environment has numpy and h5py
        echo "Checking if required dependencies are installed..."
        
        # Activate the environment first
        source "$ENV_PATH/bin/activate" || error_exit "Failed to activate Python environment"
        
        # Check for numpy
        if ! python -c "import numpy" &> /dev/null; then
            echo "numpy not found, installing..."
            pip install numpy || error_exit "Failed to install numpy"
        else
            echo "numpy found"
        fi
        
        # Check for h5py (required for HDF5 tests)
        if ! python -c "import h5py" &> /dev/null; then
            echo "h5py not found, installing..."
            pip install h5py || echo "Warning: Failed to install h5py. HDF5 tests will be skipped."
        else
            echo "h5py found"
        fi
        
        # Deactivate to ensure a clean slate before the real activation
        deactivate 2>/dev/null || true
    else
        echo "Creating Python environment at $ENV_PATH"
        
        # Check which virtual environment tool is available
        if command -v python3 -m venv &> /dev/null; then
            echo "Using python3 venv module"
            python3 -m venv "$ENV_PATH" || error_exit "Failed to create Python environment"
        elif command -v virtualenv &> /dev/null; then
            echo "Using virtualenv"
            virtualenv "$ENV_PATH" || error_exit "Failed to create Python environment"
        else
            error_exit "Neither python3 venv nor virtualenv available. Please install one to create a Python environment."
        fi
    fi
    
    # Activate the environment
    echo "Activating Python environment"
    source "$ENV_PATH/bin/activate" || error_exit "Failed to activate Python environment"
    
    # Upgrade pip (in a newly created environment)
    if [ ! -f "$ENV_PATH/.pip_upgraded" ]; then
        echo "Upgrading pip"
        pip install --upgrade pip || echo "Warning: Failed to upgrade pip, continuing anyway"
        touch "$ENV_PATH/.pip_upgraded"
    fi
    
    # Install required dependencies if this is a new environment
    if [ ! -f "$ENV_PATH/.dependencies_installed" ]; then
        echo "Installing required dependencies"
        pip install numpy || error_exit "Failed to install numpy"
        
        # Install h5py (optional, with a warning if it fails)
        pip install h5py || echo "Warning: Failed to install h5py. HDF5 tests will be skipped."
        
        # Mark dependencies as installed
        touch "$ENV_PATH/.dependencies_installed"
    fi
    
    echo "Python environment setup complete"
    echo
}

# Create the test data directory if it doesn't exist
mkdir -p "$TEST_DATA_PATH" || error_exit "Could not create directory $TEST_DATA_PATH"

# Set up the Python environment
setup_python_environment

# Compile SAGE if requested
if [ $COMPILE_MODE -eq 1 ]; then
    compile_sage
fi

# Change to the test data directory
cd "$TEST_DATA_PATH" || error_exit "Could not change to directory $TEST_DATA_PATH"

# Download Mini-Millennium merger trees if they don't exist
# We check for the last tree file (trees_063.7) as indicator that all files are present
if [ ! -f trees_063.7 ]; then
    echo "Mini-Millennium merger trees not found. Preparing to download..."
    
    # Find which download tool is available (wget or curl)
    echo "Checking if either 'wget' or 'curl' are available for downloading trees."
    DOWNLOADER=""
    
    if command -v wget &> /dev/null; then
        echo "'wget' is available. Using it for downloads."
        DOWNLOADER="wget"
    elif command -v curl &> /dev/null; then
        echo "'curl' is available. Using it for downloads."
        DOWNLOADER="curl -L -o"
    else
        error_exit "Neither 'wget' nor 'curl' are available. Please install one to download the test data."
    fi
    
    # Download the Mini-Millennium tree files
    echo "Downloading Mini-Millennium merger tree files..."
    if [ "$DOWNLOADER" = "wget" ]; then
        wget "https://www.dropbox.com/s/l5ukpo7ar3rgxo4/mini-millennium-treefiles.tar?dl=0" -O "mini-millennium-treefiles.tar" || \
            error_exit "Could not download tree files."
    else
        curl -L "https://www.dropbox.com/s/l5ukpo7ar3rgxo4/mini-millennium-treefiles.tar?dl=0" -o "mini-millennium-treefiles.tar" || \
            error_exit "Could not download tree files."
    fi
    
    # Extract the tree files
    echo "Extracting Mini-Millennium merger tree files..."
    tar xf mini-millennium-treefiles.tar || error_exit "Could not extract tree files."
    
    # Clean up the tar file
    rm -f mini-millennium-treefiles.tar
fi

# Create test results directory if it doesn't exist
mkdir -p "$TEST_RESULTS_PATH"

# Clean any existing test output files before running
rm -f "$TEST_RESULTS_PATH"/test-sage-output*

# Function to run SAGE and validate outputs
run_sage_test() {
    local format=$1  # Either "sage_binary" or "sage_hdf5"
    local format_name=$2  # Human-readable name (binary or HDF5)
    
    echo "==== CHECKING SAGE ${format_name} OUTPUT ===="
    echo
    echo "---- RUNNING SAGE IN ${format_name} MODE ----"
    
    # Return to SAGE root directory to run the executable
    cd "${TESTS_DIR}/../" || error_exit "Could not change to SAGE root directory"
    
    # Determine the number of processors for MPI
    # If MPI_RUN_COMMAND is set, extract the processor count from it
    # Otherwise, default to 1 (serial execution)
    NUM_SAGE_PROCS=$(echo ${MPI_RUN_COMMAND} | awk '{print $NF}')
    if [[ -z "${NUM_SAGE_PROCS}" ]]; then
        NUM_SAGE_PROCS=1
    fi
    
    # Create a temporary parameter file with the correct output format
    PARAM_FILE=$(mktemp)
    sed "/^OutputFormat /s/.*$/OutputFormat        ${format}/" "${TEST_DATA_PATH}/test-mini-millennium.par" > "${PARAM_FILE}"
    
    # Run SAGE with the parameter file (potentially using MPI)
    ${MPI_RUN_COMMAND} ./sage "${PARAM_FILE}"
    if [[ $? != 0 ]]; then
        echo "SAGE exited abnormally when running in ${format_name} mode."
        echo "Here is the input file that was used:"
        cat "${PARAM_FILE}"
        rm -f "${PARAM_FILE}"  # Clean up temporary file
        error_exit "SAGE execution failed."
    fi
    
    # Clean up the temporary parameter file
    rm -f "${PARAM_FILE}"
    
    echo
    echo "---- COMPARING AGAINST BENCHMARK OUTPUT FILES ----"
    echo
    
    # Change to the test data directory for comparison
    cd "${TEST_DATA_PATH}" || error_exit "Could not change to test data directory"
    
    # Perform appropriate comparison based on output format
    if [[ "${format}" == "sage_binary" ]]; then
        compare_binary_outputs "${NUM_SAGE_PROCS}"
    else
        compare_hdf5_output
    fi
}

# Function to compare binary outputs with reference files
compare_binary_outputs() {
    local num_procs=$1
    
    # Get lists of correct and test files for comparison
    # Sort by redshift to ensure correct matching
    correct_files=($(ls -d "${TEST_DATA_PATH}"/reference-sage-output_z*))
    # Use sort -k 1.18 to sort by the redshift value that starts at position 18
    test_files=($(ls -d "${TEST_RESULTS_PATH}"/test-sage-output_z* | sort -k 1.18))
    
    if [[ ${#correct_files[@]} -eq 0 || ${#test_files[@]} -eq 0 ]]; then
        echo "No output files found to compare."
        exit 1
    fi
    
    # Run comparisons for each redshift output file
    npassed=0
    nfiles=0
    nfailed=0
    for f in ${correct_files[@]}; do
        ((nfiles++))
        # Extract just the basename for display purposes
        correct_basename=$(basename "${correct_files[${nfiles}-1]}")
        test_basename=$(basename "${test_files[${nfiles}-1]}")
        echo "Comparing ${correct_basename} with ${test_basename}..."
        # Use the Python from our environment
        if [ $VERBOSE_MODE -eq 1 ]; then
            "${ENV_PATH}/bin/python" "${TESTS_DIR}/sagediff.py" -v "${correct_files[${nfiles}-1]}" "${test_files[${nfiles}-1]}" binary-binary 1 ${num_procs}
        else
            "${ENV_PATH}/bin/python" "${TESTS_DIR}/sagediff.py" "${correct_files[${nfiles}-1]}" "${test_files[${nfiles}-1]}" binary-binary 1 ${num_procs}
        fi
        if [[ $? == 0 ]]; then
            ((npassed++))
        else
            ((nfailed++))
        fi
    done
    
    echo
    echo "Binary checks passed: $npassed"
    echo "Binary checks failed: $nfailed"
    echo
    
    # Exit if any comparisons failed
    if [[ $nfailed > 0 ]]; then
        error_exit "The binary comparison check failed."
    fi
}

# Function to compare HDF5 output with reference binary files
compare_hdf5_output() {
    # Get list of correct binary files to compare against
    correct_files=($(ls -d "${TEST_DATA_PATH}"/reference-sage-output_z*))
    # There's only one HDF5 output file
    test_file=$(ls "${TEST_RESULTS_PATH}"/test-sage-output.hdf5)
    
    if [[ ${#correct_files[@]} -eq 0 || -z "${test_file}" ]]; then
        echo "Missing files for comparison."
        exit 1
    fi
    
    # First check if h5py is available in our Python environment
    "${ENV_PATH}/bin/python" -c "import h5py" 2>/dev/null
    if [[ $? != 0 ]]; then
        echo
        echo "Notice: Python h5py module is not available. Skipping HDF5 tests."
        echo "To enable HDF5 testing, please install h5py: pip install h5py"
        echo
        return 0  # Skip HDF5 tests but don't fail
    fi
    
    # Run comparisons for each redshift output
    npassed=0
    nfiles=0
    nfailed=0
    for f in ${correct_files[@]}; do
        ((nfiles++))
        # Extract just the basename for display purposes
        correct_basename=$(basename "${correct_files[${nfiles}-1]}")
        test_basename=$(basename "${test_file}")
        echo "Comparing ${correct_basename} with ${test_basename}..."
        # Use the Python from our environment
        if [ $VERBOSE_MODE -eq 1 ]; then
            "${ENV_PATH}/bin/python" "${TESTS_DIR}/sagediff.py" -v "${correct_files[${nfiles}-1]}" "${test_file}" binary-hdf5 1 1
        else
            "${ENV_PATH}/bin/python" "${TESTS_DIR}/sagediff.py" "${correct_files[${nfiles}-1]}" "${test_file}" binary-hdf5 1 1
        fi
        if [[ $? == 0 ]]; then
            ((npassed++))
        else
            ((nfailed++))
        fi
    done
    
    echo
    echo "HDF5 checks passed: $npassed"
    echo "HDF5 checks failed: $nfailed"
    echo
    
    # Exit if any comparisons failed
    if [[ $nfailed > 0 ]]; then
        error_exit "The HDF5 comparison check failed."
    fi
}

# Run the binary output test
run_sage_test "sage_binary" "BINARY"

# Run the HDF5 output test
run_sage_test "sage_hdf5" "HDF5"

echo "==== ALL CHECKS COMPLETED SUCCESSFULLY ===="

# Deactivate Python environment if it was activated
if [[ -n "$VIRTUAL_ENV" ]]; then
echo
    echo "Deactivating Python environment"
    deactivate
fi

# Return to the original working directory
cd "${ORIG_DIR}"
exit 0