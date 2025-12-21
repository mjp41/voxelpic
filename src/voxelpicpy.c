#define PY_SSIZE_T_CLEAN
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include "voxelpic.h"
#include <Python.h>
#include <numpy/arrayobject.h>

typedef struct voxelpic_state {
  voxelpicOcTree *octree;
  voxelpicLevel *level;
} VPICState;

static PyObject *encode(PyObject *module, PyObject *args) {
  PyObject *positions_array_obj = NULL;
  PyObject *colors_array_obj = NULL;
  int depth = 9;
  PyObject *image_array_obj = Py_None;

  // Parse the Python argument to get the NumPy array object
  if (!PyArg_ParseTuple(args, "OO|iO", &positions_array_obj, &colors_array_obj,
                        &depth, &image_array_obj)) {
    return NULL;
  }

  PyArrayObject *positions_array = (PyArrayObject *)PyArray_FROM_OTF(
      positions_array_obj, NPY_FLOAT32, NPY_ARRAY_C_CONTIGUOUS);
  if (positions_array == NULL) {
    return NULL;
  }

  if (PyArray_NDIM(positions_array) != 2) {
    Py_DECREF(positions_array);
    PyErr_SetString(PyExc_ValueError, "Invalid positions array: ndim != 2");
    return NULL;
  }

  if (PyArray_DIM(positions_array, 1) != 4) {
    Py_DECREF(positions_array);
    PyErr_SetString(PyExc_ValueError,
                    "Invalid position array: positions must be Nx4");
    return NULL;
  }

  PyArrayObject *colors_array = (PyArrayObject *)PyArray_FROM_OTF(
      colors_array_obj, NPY_UINT8, NPY_ARRAY_C_CONTIGUOUS);
  if (colors_array == NULL) {
    return NULL;
  }

  if (PyArray_NDIM(colors_array) != 2) {
    Py_DECREF(positions_array);
    Py_DECREF(colors_array);
    PyErr_SetString(PyExc_ValueError, "Invalid colors array: ndim != 2");
    return NULL;
  }

  if (PyArray_DIM(colors_array, 1) != 4) {
    Py_DECREF(positions_array);
    Py_DECREF(colors_array);
    PyErr_SetString(PyExc_ValueError,
                    "Invalid position array: colors must be Nx4");
    return NULL;
  }

  if (PyArray_DIM(positions_array, 0) != PyArray_DIM(colors_array, 0)) {
    Py_DECREF(positions_array);
    Py_DECREF(colors_array);
    PyErr_SetString(PyExc_ValueError,
                    "Dimension mismatch: len(positions) != len(colors");
    return NULL;
  }

  voxelpicPointCloud cloud;
  cloud.size = PyArray_DIM(positions_array, 0);
  cloud.capacity = cloud.size;
  cloud.colors = (voxelpicColor *)PyArray_DATA(colors_array);
  cloud.positions = (voxelpicVec4 *)PyArray_DATA(positions_array);

  VPICState *state = (VPICState *)PyModule_GetState(module);

  voxelpicEnum rc = voxelpicOcTreeBuild(state->octree, &cloud);
  Py_DECREF(positions_array);
  Py_DECREF(colors_array);

  if (rc) {
    PyErr_SetString(PyExc_RuntimeError, voxelpicError(rc));
    return NULL;
  }

  voxelpicLevel *level = NULL;
  rc = voxelpicOcTreeLevel(state->octree, depth, &level);
  if (rc) {
    PyErr_SetString(PyExc_RuntimeError, voxelpicError(rc));
    return NULL;
  }

  size_t width, height;
  rc = voxelpicLevelImageSize(level, &width, &height);
  if (rc) {
    PyErr_SetString(PyExc_RuntimeError, voxelpicError(rc));
    return NULL;
  }

  PyArrayObject *image_array = NULL;
  if (Py_IsNone(image_array_obj)) {
    npy_intp image_dims[3];
    image_dims[0] = (npy_intp)height;
    image_dims[1] = (npy_intp)width;
    image_dims[2] = 4;

    PyArray_Descr *descr = PyArray_DescrFromType(NPY_UINT8);
    if (!descr) {
      PyErr_SetString(PyExc_ValueError,
                      "Could not get descriptor for NPY_UINT8");
      return NULL;
    }
    image_array = (PyArrayObject *)PyArray_NewFromDescr(
        &PyArray_Type, descr, 3, image_dims, NULL, NULL, 0, NULL);
    if (image_array == NULL) {
      return NULL;
    }
  } else {
    image_array = (PyArrayObject *)PyArray_FROM_OTF(image_array_obj, NPY_UINT8,
                                                    NPY_ARRAY_C_CONTIGUOUS);
    if (image_array == NULL) {
      return NULL;
    }

    if (PyArray_NDIM(image_array) != 3) {
      Py_DECREF(image_array);
      PyErr_SetString(PyExc_ValueError,
                      "Invalid output image: must be Height x Width x 4");
      return NULL;
    }

    if (PyArray_DIM(image_array, 0) != (npy_intp)height) {
      Py_DECREF(image_array);
      PyErr_Format(PyExc_ValueError, "Invalid output image: dim 0 != %zu",
                   height);
      return NULL;
    }

    if (PyArray_DIM(image_array, 1) != (npy_intp)width) {
      Py_DECREF(image_array);
      PyErr_Format(PyExc_ValueError, "Invalid output image: dim 1 != %zu",
                   width);
      return NULL;
    }

    if (PyArray_DIM(image_array, 2) != 4) {
      Py_DECREF(image_array);
      PyErr_SetString(PyExc_ValueError, "Invalid output image: dim 2 != 4");
      return NULL;
    }
  }

  voxelpicImage image;
  image.width = width;
  image.height = height;
  image.pixels = (voxelpicColor *)PyArray_DATA(image_array);

  rc = voxelpicLevelEncode(level, &image);

  if (rc) {
    PyErr_SetString(PyExc_RuntimeError, voxelpicError(rc));
    return NULL;
  }

  return (PyObject *)image_array;
}

static PyObject *decode(PyObject *module, PyObject *args) {
  PyObject *image_array_obj = NULL;
  PyObject *positions_array_obj = Py_None;
  PyObject *colors_array_obj = Py_None;
  int truncate = false;

  if (!PyArg_ParseTuple(args, "O|pOO", &image_array_obj, &truncate,
                        &positions_array_obj, &colors_array_obj)) {
    return NULL;
  }

  PyArrayObject *image_array = (PyArrayObject *)PyArray_FROM_OTF(
      image_array_obj, NPY_UINT8, NPY_ARRAY_C_CONTIGUOUS);
  if (image_array == NULL) {
    return NULL;
  }

  if (PyArray_NDIM(image_array) != 3) {
    Py_DECREF(image_array);
    PyErr_SetString(PyExc_ValueError,
                    "Invalid input image: must be Height x Width x 4");
    return NULL;
  }

  if (PyArray_DIM(image_array, 2) != 4) {
    Py_DECREF(image_array);
    PyErr_SetString(PyExc_ValueError, "Invalid input image: dim 2 != 4");
    return NULL;
  }

  voxelpicImage image;
  image.height = PyArray_DIM(image_array, 0);
  image.width = PyArray_DIM(image_array, 1);
  image.pixels = (voxelpicColor *)PyArray_DATA(image_array);

  VPICState *state = (VPICState *)PyModule_GetState(module);

  voxelpicEnum rc = voxelpicLevelDecode(&image, state->level);
  Py_DECREF(image_array);

  if (rc) {
    PyErr_SetString(PyExc_RuntimeError, voxelpicError(rc));
    return NULL;
  }

  size_t count = voxelpicLevelSize(state->level);

  PyArrayObject *positions_array = NULL;
  if (Py_IsNone(positions_array_obj)) {
    npy_intp positions_dims[2];
    positions_dims[0] = (npy_intp)count;
    positions_dims[1] = 4;

    PyArray_Descr *descr = PyArray_DescrFromType(NPY_FLOAT32);
    if (!descr) {
      PyErr_SetString(PyExc_ValueError,
                      "Could not get descriptor for NPY_UINT8");
      return NULL;
    }
    positions_array = (PyArrayObject *)PyArray_NewFromDescr(
        &PyArray_Type, descr, 2, positions_dims, NULL, NULL, 0, NULL);
    if (positions_array == NULL) {
      return NULL;
    }
  } else {
    positions_array = (PyArrayObject *)PyArray_FROM_OTF(
        positions_array_obj, NPY_FLOAT32, NPY_ARRAY_C_CONTIGUOUS);
    if (positions_array == NULL) {
      return NULL;
    }

    if (PyArray_NDIM(positions_array) != 2) {
      Py_DECREF(positions_array);
      PyErr_SetString(PyExc_ValueError,
                      "Invalid output positions shape: must be N x 4");
      return NULL;
    }

    if (PyArray_DIM(positions_array, 0) < (npy_intp)count && !truncate) {
      Py_DECREF(positions_array);
      PyErr_Format(PyExc_ValueError,
                   "Invalid output positions shape: dim 0 < %zu (truncate=%d)",
                   count, truncate);
      return NULL;
    }

    if (PyArray_DIM(positions_array, 1) != 4) {
      Py_DECREF(positions_array);
      PyErr_SetString(PyExc_ValueError,
                      "Invalid output positions shape: dim 2 != 4");
      return NULL;
    }
  }

  PyArrayObject *colors_array = NULL;
  if (Py_IsNone(colors_array_obj)) {
    npy_intp colors_dims[2];
    colors_dims[0] = (npy_intp)count;
    colors_dims[1] = 4;

    PyArray_Descr *descr = PyArray_DescrFromType(NPY_UINT8);
    if (!descr) {
      PyErr_SetString(PyExc_ValueError,
                      "Could not get descriptor for NPY_UINT8");
      return NULL;
    }
    colors_array = (PyArrayObject *)PyArray_NewFromDescr(
        &PyArray_Type, descr, 2, colors_dims, NULL, NULL, 0, NULL);
    if (colors_array == NULL) {
      return NULL;
    }
  } else {
    colors_array = (PyArrayObject *)PyArray_FROM_OTF(
        colors_array_obj, NPY_UINT8, NPY_ARRAY_C_CONTIGUOUS);
    if (colors_array == NULL) {
      return NULL;
    }

    if (PyArray_NDIM(colors_array) != 2) {
      Py_DECREF(positions_array);
      Py_DECREF(colors_array);
      PyErr_SetString(PyExc_ValueError,
                      "Invalid output colors shape: must be N x 4");
      return NULL;
    }

    if (PyArray_DIM(colors_array, 0) < (npy_intp)count && !truncate) {
      Py_DECREF(positions_array);
      Py_DECREF(colors_array);
      PyErr_Format(PyExc_ValueError,
                   "Invalid output colors shape: dim 0 < %zu (truncate=%d)",
                   count, truncate);
      return NULL;
    }

    if (PyArray_DIM(colors_array, 1) != 4) {
      Py_DECREF(positions_array);
      Py_DECREF(colors_array);
      PyErr_SetString(PyExc_ValueError,
                      "Invalid output colors shape: dim 2 != 4");
      return NULL;
    }
  }

  voxelpicPointCloud cloud;
  cloud.capacity = count;
  cloud.size = count;
  cloud.positions = (voxelpicVec4 *)PyArray_DATA(positions_array);
  cloud.colors = (voxelpicColor *)PyArray_DATA(colors_array);

  size_t depth = voxelpicLevelDepth(state->level);

  rc = voxelpicLevelToCloud(state->level, &cloud, truncate);

  if (rc) {
    PyErr_SetString(PyExc_RuntimeError, voxelpicError(rc));
    return NULL;
  }

  return PyTuple_Pack(4, (PyObject *)positions_array, (PyObject *)colors_array,
                      PyLong_FromSize_t(count), PyLong_FromSize_t(depth));
}

static PyObject *image_shape(PyObject *module, PyObject *args) {
  unsigned int depth;

  if (!PyArg_ParseTuple(args, "I", &depth)) {
    return NULL;
  }

  size_t width, height;
  voxelpicEnum rc = voxelpicDepthImageSize(depth, &width, &height);

  if (rc) {
    PyErr_SetString(PyExc_RuntimeError, voxelpicError(rc));
  }

  return Py_BuildValue("(II)", height, width);
}

static PyObject *voxel_size(PyObject *module, PyObject *args) {
  unsigned int depth;

  if (!PyArg_ParseTuple(args, "I", &depth)) {
    return NULL;
  }

  float voxel_size = voxelpicDepthVoxelSize(depth);

  return Py_BuildValue("f", voxel_size);
}

static PyMethodDef voxelpic_methods[] = {
    {"encode", encode, METH_VARARGS, "encode"},
    {"decode", decode, METH_VARARGS, "decode"},
    {"image_shape", image_shape, METH_VARARGS, "image_shape"},
    {"voxel_size", voxel_size, METH_VARARGS, "voxel_size"},
    {NULL} /* Sentinel */
};

static int voxelpic_exec(PyObject *module) {
  VPICState *state = (VPICState *)PyModule_GetState(module);
  if (state == NULL) {
    return -1;
  }

  state->octree = voxelpicOcTreeNew(1, 10);
  if (state->octree == NULL) {
    return -1;
  }

  state->level = voxelpicLevelNew(0);
  if (state->level == NULL) {
    voxelpicOcTreeFree(state->octree);
    return -1;
  }

  import_array1(-1);

  return 0;
}

void voxelpic_free(PyObject *module) {
  VPICState *state = (VPICState *)PyModule_GetState(module);
  voxelpicOcTreeFree(state->octree);
  voxelpicLevelFree(state->level);
}

#ifdef Py_mod_exec
static PyModuleDef_Slot voxelpic_slots[] = {
    {Py_mod_exec, (void *)voxelpic_exec},
#if PY_VERSION_HEX >= 0x030C0000
    {Py_mod_multiple_interpreters, Py_MOD_PER_INTERPRETER_GIL_SUPPORTED},
#endif
    {0, NULL},
};
#endif

static PyModuleDef voxelpicmoduledef = {
    PyModuleDef_HEAD_INIT,
    .m_name = "_voxelpic",
    .m_doc = "voxelpic is a Python module which transcodes point clouds to and "
             "from images",
    .m_methods = voxelpic_methods,
    .m_free = (freefunc)voxelpic_free,
#ifdef Py_mod_exec
    .m_slots = voxelpic_slots,
#endif
    .m_size = sizeof(VPICState)};

PyMODINIT_FUNC PyInit__voxelpic(void) {
#ifdef Py_mod_exec
  return PyModuleDef_Init(&voxelpicmoduledef);
#else
  PyObject *module;
  module = PyModule_Create(&voxelpicmoduledef);
  if (module == NULL)
    return NULL;

  if (voxelpic_exec(module) != 0) {
    Py_DECREF(module);
    return NULL;
  }

  return module;
#endif
}