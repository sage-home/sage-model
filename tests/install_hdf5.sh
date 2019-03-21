wget https://support.hdfgroup.org/ftp/HDF5/releases/hdf5-1.8/hdf5-1.8.19/src/hdf5-1.8.19.tar.bz2
tar -xvjf hdf5-1.8.19.tar.bz2
cd hdf5-1.8.19
./configure --prefix=$HDF5_PREFIX --enable-shared --enable-parallel CC=mpicc
make
make install
