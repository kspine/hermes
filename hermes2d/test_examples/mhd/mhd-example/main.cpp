#include "util.h"
#include "numerical_flux.h"
#include "weak_formulation.h"
#include "hermes2d.h"

using namespace Hermes;
using namespace Hermes::Hermes2D;
using namespace Hermes::Hermes2D::Views;

// Initial polynomial degree.
const int P_INIT = 1;
// Number of initial uniform mesh refinements.
const int INIT_REF_NUM = 3;
// Time step length
const double time_step_length = 1e-3;
const double time_interval_length = 20.;

// Equation parameters.
// Exterior pressure (dimensionless).
const double P_EXT = 2.5;
// Inlet density (dimensionless).   
const double RHO_EXT = 1.0;
// Inlet x-velocity (dimensionless).
const double V1_EXT = 1.25;
// Inlet y-velocity (dimensionless).
const double V2_EXT = 0.0;
// Kappa.
const double KAPPA = 1.4;
///TODO Zde bude treba doplnit zbyle slozky stavoveho vektoru na vstupu.

// Mesh filename.
const std::string MESH_FILENAME = "channel.mesh";

// Boundary markers.
const std::string BDY_INLET = "4";
const std::string BDY_OUTLET = "2";
const std::string BDY_SOLID_WALL_BOTTOM = "1";
const std::string BDY_SOLID_WALL_TOP = "3";

int main(int argc, char* argv[])
{
  // Load the mesh.
  MeshSharedPtr mesh(new Mesh);
  MeshReaderH2D mloader;
  mloader.load(MESH_FILENAME, mesh);

  // Perform initial mesh refinements.
  for (int i = 0; i < INIT_REF_NUM; i++)
    mesh->refine_all_elements(0, true);

  // Initialize the spaces with default shapesets.
  SpaceSharedPtr<double> space_rho(new L2Space<double>(mesh, P_INIT));
  SpaceSharedPtr<double> space_rho_v_x(new L2Space<double>(mesh, P_INIT));
  SpaceSharedPtr<double> space_rho_v_y(new L2Space<double>(mesh, P_INIT));
  SpaceSharedPtr<double> space_e(new L2Space<double>(mesh, P_INIT));
  ///TODO Zde bude treba doplnit zbyle 4 prostory
  Hermes::vector<SpaceSharedPtr<double> > spaces(space_rho, space_rho_v_x, space_rho_v_y, space_e);

  // Initialize solutions.
  MeshFunctionSharedPtr<double> sln_rho(new Solution<double>(mesh));
  MeshFunctionSharedPtr<double> sln_rho_v_x(new Solution<double>(mesh));
  MeshFunctionSharedPtr<double> sln_rho_v_y(new Solution<double>(mesh));
  MeshFunctionSharedPtr<double> sln_e(new Solution<double>(mesh));
  ///TODO Zde bude treba doplnit zbyle 4 reseni
  Hermes::vector<MeshFunctionSharedPtr<double> > slns(sln_rho, sln_rho_v_x, sln_rho_v_y, sln_e);

  // Set initial conditions.
  // These are also used for storing the previous iteration solution - which is at time = 0 the initial condition.
  MeshFunctionSharedPtr<double> prev_rho(new ConstantSolution<double>(mesh, RHO_EXT));
  MeshFunctionSharedPtr<double> prev_rho_v_x(new ConstantSolution<double>(mesh, RHO_EXT * V1_EXT));
  MeshFunctionSharedPtr<double> prev_rho_v_y(new ConstantSolution<double>(mesh, RHO_EXT * V2_EXT));
  MeshFunctionSharedPtr<double> prev_e(new ConstantSolution<double>(mesh, QuantityCalculator::calc_energy(RHO_EXT, RHO_EXT * V1_EXT, RHO_EXT * V2_EXT, P_EXT, KAPPA)));
  Hermes::vector<MeshFunctionSharedPtr<double> > prev_slns(prev_rho, prev_rho_v_x, prev_rho_v_y, prev_e);
  ///TODO Zde bude treba doplnit zbyle 4 pocatecni podminky.

  MeshFunctionSharedPtr<double>  Mach_number(new MachNumberFilter(prev_slns, KAPPA));
  MeshFunctionSharedPtr<double>  pressure(new PressureFilter(prev_slns, KAPPA));

  ScalarView pressure_view("Pressure", new WinGeom(0, 0, 600, 300));
  ScalarView Mach_number_view("Mach number", new WinGeom(650, 0, 600, 300));
  ScalarView eview("Error - density", new WinGeom(0, 330, 600, 300));
  ScalarView eview1("Error - momentum", new WinGeom(0, 660, 600, 300));
  OrderView order_view("Orders", new WinGeom(650, 330, 600, 300));

  // Initialize markers for the weak formulation.
  Hermes::vector<std::string> solid_wall_markers(BDY_SOLID_WALL_BOTTOM, BDY_SOLID_WALL_TOP);
  Hermes::vector<std::string> inlet_markers;
  inlet_markers.push_back(BDY_INLET);
  Hermes::vector<std::string> outlet_markers;
  outlet_markers.push_back(BDY_OUTLET);

  // Initialize the numerical flux.
  UpWindNumericalFlux num_flux(KAPPA);

  // Initialize the weak formulation
  MHDWeakForm wf(KAPPA, RHO_EXT, V1_EXT, V2_EXT, P_EXT, inlet_markers, outlet_markers, &num_flux, prev_rho, prev_rho_v_x, prev_rho_v_y, prev_e);
  wf.set_current_time_step(time_step_length);

  LinearSolver<double> solver(&wf, spaces);

#pragma region 5. Time stepping loop.
  int iteration = 0;
  for (double t = 0.0; t < time_interval_length; t += time_step_length)
  {
    // Info.
    Hermes::Mixins::Loggable::Static::info("---- Time step %d, time %3.5f.", iteration++, t);

    try
    {
      // Solve.
      solver.solve();

      // Get the solution vector.
      Solution<double>::vector_to_solutions(solver.get_sln_vector(), spaces, prev_slns);
    }
    catch (std::exception& e)
    {
      std::cout << e.what();
    }

    Mach_number->reinit();
    pressure->reinit();
    pressure_view.show(pressure);
    Mach_number_view.show(Mach_number);
    order_view.show(space_rho);
  }

  // Done.
  return 0;
}