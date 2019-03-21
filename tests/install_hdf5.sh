# Just a small script to install the parallel HDF5 library.

if [ -d $HDF5_DIR/lib ]; then
    echo "Parallel HDF5 library already installed in cache directory ${HDF5_DIR}."
else
    echo "Parallel HDF5 library not installed in cache directory ${HDF5_DIR}."
    echo "Downloading and building."

    wget https://support.hdfgroup.org/ftp/HDF5/releases/hdf5-1.8/hdf5-1.8.19/src/hdf5-1.8.19.tar.bz2
    tar -xvjf hdf5-1.8.19.tar.bz2
    cd hdf5-1.8.19
    ./configure --prefix=$HDF5_DIR --enable-shared --enable-parallel CC=mpicc
    make
    make install
fi
