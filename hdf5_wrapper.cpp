#include <hdf5_wrapper.h>

using namespace H5;
using namespace std;

HDF5Wrapper::HDF5Wrapper(string filename, string dataset_name) {
  try {
    // open file, deleting content if it exists
    // this program only shows you how to append to
    // an already open file, it does not show how
    // to append to a file on disk.
    // For that, see H5Open()
    file = H5File(filename.c_str(), H5F_ACC_TRUNC);
    // 1-D array
    ndims = 1;
    // starting array length is 0, each write () is an append
    dims = 0;

    // create a 'chunked' dataset
    hsize_t max_dims = H5S_UNLIMITED, chunk_dims = 10;
    dataspace = new DataSpace(ndims, &dims, &max_dims);
    DSetCreatPropList prop;
    prop.setChunk(ndims, &chunk_dims);
    dataset = new DataSet(file.createDataSet(dataset_name.c_str(),
          PredType::NATIVE_DOUBLE, *dataspace, prop));
  } catch(FileIException error) {
    error.printError();
  } catch(DataSetIException error) {
    error.printError();
  } catch(DataSpaceIException error) {
    error.printError();
  }
}

int HDF5Wrapper::write(vector<double> data) {
  try {
    // offset is the position where new data will be placed
    // for appending, offset = end of current data
    hsize_t offset = dims;
    hsize_t ext_dims = data.size();
    dims = dims + data.size();
    // extend
    dataset->extend(&dims);
    // write new data
    DataSpace *filespace = new DataSpace(dataset->getSpace());
    filespace->selectHyperslab(H5S_SELECT_SET, &ext_dims, &offset);
    DataSpace *memspace = new DataSpace(ndims, &ext_dims, NULL);
    dataset->write(&data[0], PredType::NATIVE_DOUBLE, *memspace, *filespace);
    delete filespace;
    delete memspace;
  } catch(FileIException error) {
    error.printError();
    return -1;
  } catch(DataSetIException error) {
    error.printError();
    return -1;
  } catch(DataSpaceIException error) {
    error.printError();
    return -1;
  }
  return 0;
}

HDF5Wrapper::~HDF5Wrapper() {
  try {
    delete dataspace;
    delete dataset;
    file.close();
  } catch(FileIException error) {
    error.printError();
  } catch(DataSetIException error) {
    error.printError();
  } catch(DataSpaceIException error) {
    error.printError();
  }
}
