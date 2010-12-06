cdef extern from "../../hermes_common/matrix.h":

    cdef cppclass Matrix:
        pass

    cdef cppclass SparseMatrix(Matrix):
        pass

    cdef cppclass Vector:
        pass

cdef extern from "../../hermes_common/solver/umfpack_solver.h":

    cdef cppclass UMFPackMatrix(SparseMatrix):
        pass

    cdef cppclass UMFPackVector(Vector):
        pass

