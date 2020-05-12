#include <pybind11/pybind11.h>
// #include <pybind11/factory.h>
#include <pybind11/numpy.h>
#include "qdldl.hpp"

namespace py = pybind11;
using namespace py::literals; // to bring in the `_a` literal


class PySolver{
	public:
		PySolver(py::object A);
		py::array solve(py::array_t<QDLDL_float, py::array::c_style | py::array::forcecast> b_py);
		void update(py::object Anew_py);

	private:
		std::unique_ptr<qdldl::Solver> s;

};


py::array PySolver::solve(
		const py::array_t<QDLDL_float, py::array::c_style | py::array::forcecast> b_py){

	auto b = (QDLDL_float *)b_py.data();

	if ((QDLDL_int)b_py.size() != this->s->nx)
		throw py::value_error("Length of b does not match size of A");

	py::print("Size of b: ", b_py.size());
	py::print("Size of A: ", this->s->nx);

	py::gil_scoped_release release;
	auto x = s->solve(b);
    py::gil_scoped_acquire acquire;

	py::array x_py = py::array(s->nx, x);

	delete [] x;

    return x_py;
}

void PySolver::update(const py::object Anew){

	// Use scipy to convert to upper triangular and get data
	py::object spa = py::module::import("scipy.sparse");
	py::object Anew_triu = spa.attr("triu")(Anew, "format"_a="csc");
	auto Anew_x_py = Anew_triu.attr("data").cast<py::array_t<QDLDL_float>>();

	auto Anew_x = (QDLDL_float *)Anew_x_py.data();

	py::gil_scoped_release release;
	s->update(Anew_x);
    py::gil_scoped_acquire acquire;
}



PySolver::PySolver(py::object A){

	// Use scipy to convert to upper triangular and get data
	py::object spa = py::module::import("scipy.sparse");

	// Check dimensions
	py::tuple dim = A.attr("shape");
	int m = dim[0].cast<int>();
	int n = dim[1].cast<int>();

	if (m != n) throw py::value_error("Matrix A is not square");

	if (!spa.attr("isspmatrix_csc")(A)) A = spa.attr("csc_matrix")(A);

	if (A.attr("nnz").cast<int>() == 0) throw py::value_error("Matrix A is empty");

	py::object A_triu = spa.attr("triu")(A, "format"_a="csc");

	auto Ap_py = A_triu.attr("indptr").cast<py::array_t<QDLDL_int, py::array::c_style>>();
	auto Ai_py = A_triu.attr("indices").cast<py::array_t<QDLDL_int, py::array::c_style>>();
	auto Ax_py = A_triu.attr("data").cast<py::array_t<QDLDL_float, py::array::c_style>>();

	QDLDL_int nx = Ap_py.request().size - 1;
	QDLDL_int * Ap = (QDLDL_int *)Ap_py.data();
	QDLDL_int * Ai = (QDLDL_int *)Ai_py.data();
	QDLDL_float * Ax = (QDLDL_float *)Ax_py.data();

	py::gil_scoped_release release;
	// TODO: Replace this line with the make_unique line below in the future.
	// It needs C++14 but manylinux does not support it yet
	this->s = std::unique_ptr<qdldl::Solver>(new qdldl::Solver(nx, Ap, Ai, Ax));
	// s = std::make_unique<qdldl::Solver>(nx, Ap, Ai, Ax);
	py::gil_scoped_acquire acquire;
}




PYBIND11_MODULE(qdldl, m) {
  m.doc() = "QDLDL wrapper";
  py::class_<PySolver>(m, "Solver")
	  .def(py::init<py::object>())
	  .def("solve", &PySolver::solve)
	  .def("update", &PySolver::update);
}
