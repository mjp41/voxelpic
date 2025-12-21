from setuptools import Extension, setup

import numpy

NUMPY_INCLUDE = numpy.get_include()

setup(
    ext_modules=[
        Extension(
            name="_voxelpic",
            sources=["src/voxelpicpy.c", "src/voxelpic.c"],
            include_dirs=["include/voxelpic", NUMPY_INCLUDE],
            library_dirs=["C:\\Source\\Python-3.12.12\\PCbuild\\amd64"]
        ),
    ]
)
