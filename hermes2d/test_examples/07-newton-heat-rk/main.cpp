#include "definitions.h"

//  This example is a continuation of the example "09-timedep-basic" and it shows how
//  to perform time integration with arbitrary Runge-Kutta methods, using Butcher's
//  tables as input parameters. Currently (as of January 2011) approx. 30 tables are
//  available by default, as can be seen below. They are taken from various sources
//  including J. Butcher's book, journal articles, and the Wikipedia page
//  http://en.wikipedia.org/wiki/List_of_Runge%E2%80%93Kutta_methods. If you know
//  about some other interesting R-K method that we are missing here, please let us
//  know!
//
//  PDE: non-stationary heat transfer equation
//       HEATCAP * RHO * dT/dt - LAMBDA * Laplace T = 0.
//  This equation is, however, written in such a way that the time-derivative
//  is on the left and everything else on the right:
//
//  dT/dt = LAMBDA * Laplace T / (HEATCAP * RHO).
//
//  As opposed to the previous example where the time-discretization was hardwired
//  into the weak formulation, now we only need the weak formulation of the
//  right-hand side.
//
//  Domain: St. Vitus cathedral (file cathedral.mesh).
//
//  IC:  T = TEMP_INIT.
//  BC:  T = TEMP_INIT on the bottom edge ... Dirichlet,
//       LAMBDA * dT/dn = ALPHA*(t_exterior(time) - T) ... Newton, time-dependent.
//
//  Time-stepping: Arbitrary Runge-Kutta methods.
//

//  The following parameters can be changed:
// Polynomial degree of all mesh elements.
const int P_INIT = 1;
// Number of initial uniform mesh refinements.
const int INIT_REF_NUM = 3;
// Number of initial uniform mesh refinements towards the boundary.
const int INIT_REF_NUM_BDY = 2;
// Time step in seconds.
double time_step = 1e+2;
// Stopping criterion for the Newton's method.
const double NEWTON_TOL = 1e-5;
// Damping factor for the Newton's method.
const double NEWTON_DF = .5;
// Maximum allowed number of Newton iterations.
const int NEWTON_MAX_ITER = 100;

ButcherTableType butcher_table_type = Explicit_RK_1;
//ButcherTableType butcher_table_type = Explicit_RK_3;
//ButcherTableType butcher_table_type = Implicit_RK_1;
//ButcherTableType butcher_table_type = Implicit_SDIRK_CASH_3_23_embedded;
//ButcherTableType butcher_table_type = Implicit_SDIRK_CASH_5_34_embedded;

// Problem parameters.
// Temperature of the ground (also initial temperature).
const double TEMP_INIT = 10;
// Heat flux coefficient for Newton's boundary condition.
const double ALPHA = 10;
// Thermal conductivity of the material.
const double LAMBDA = 1e2;
// Heat capacity.
const double HEATCAP = 1e2;
// Material density.
const double RHO = 3000;
// Length of time interval (24 hours) in seconds.
const double T_FINAL = 86400;

int main(int argc, char* argv[])
{
  // Choose a Butcher's table or define your own.
  ButcherTable bt(butcher_table_type);

  // Init mesh.
  MeshSharedPtr mesh(new Mesh);
  MeshReaderH2D mloader;
  mloader.load("cathedral.mesh", mesh);

  // Perform initial mesh refinements.
  for (int i = 0; i < INIT_REF_NUM; i++)
    mesh->refine_all_elements();
  mesh->refine_towards_boundary("Boundary_air", INIT_REF_NUM_BDY);
  mesh->refine_towards_boundary("Boundary_ground", INIT_REF_NUM_BDY);

  // Initialize space & boundary conditions.
  Hermes::Hermes2D::DefaultEssentialBCConst<double> bc_essential("Boundary_ground", TEMP_INIT);
  Hermes::Hermes2D::EssentialBCs<double> bcs(&bc_essential);
  SpaceSharedPtr<double> space(new H1Space<double>(mesh, &bcs, P_INIT));

  // Solution pointer.
  MeshFunctionSharedPtr<double> sln_time_prev(new ConstantSolution<double>(mesh, TEMP_INIT));
  MeshFunctionSharedPtr<double> sln_time_new(new Solution<double>(mesh));
  MeshFunctionSharedPtr<double> time_error_fn(new ZeroSolution<double>(mesh));

  // Weak formulation.
  WeakFormSharedPtr<double> wf(new CustomWeakFormHeatRK("Boundary_air", ALPHA, LAMBDA, HEATCAP, RHO, TEMP_INIT, T_FINAL));

  // Initialize views.
  Hermes::Hermes2D::Views::ScalarView Eview("Time error", new Hermes::Hermes2D::Views::WinGeom(500, 0, 450, 600));
  Hermes::Hermes2D::Views::ScalarView Tview("Temperature", new Hermes::Hermes2D::Views::WinGeom(0, 0, 450, 600));
  Tview.set_min_max_range(0, 20);
  Tview.fix_scale_width(30);

  // Initialize Runge-Kutta time stepping.
  RungeKutta<double> runge_kutta(wf, space, &bt);
  runge_kutta.set_newton_tolerance(NEWTON_TOL);
  runge_kutta.set_newton_damping_coeff(NEWTON_DF);
  runge_kutta.set_verbose_output(true);
  runge_kutta.set_time_step(time_step);
  runge_kutta.set_newton_max_allowed_iterations(NEWTON_MAX_ITER);

  // Time stepping loop:
  // - Initialize the time.
  double current_time = 0;
  do
  {
    runge_kutta.set_time(current_time);
	if (bt.is_embedded())
		runge_kutta.rk_time_step_newton(sln_time_prev, sln_time_new, time_error_fn);
	else
		runge_kutta.rk_time_step_newton(sln_time_prev, sln_time_new);

    // Show the new_ time level solution.
    char title[100];
    sprintf(title, "Time %3.2f s", current_time);
    Tview.set_title(title);
    Tview.show(sln_time_new);

    // Copy solution for the new_ time step.
    sln_time_prev->copy(sln_time_new);

	// Time error
	if (bt.is_embedded())
	{
		Eview.show(time_error_fn);
		DefaultNormCalculator<double, HERMES_H1_NORM> normCalculator(1);
		normCalculator.calculate_norm(time_error_fn);
		double time_error_norm = normCalculator.get_total_norm_squared();
		normCalculator.calculate_norm(sln_time_new);
		double sln_norm = normCalculator.get_total_norm_squared();

		Hermes::Mixins::Loggable::Static::info("\tRelative time error: %g%%.", (time_error_norm / sln_norm) * 100.);
	}

    // Increase current time and time step counter.
    current_time += time_step;
  } while (current_time < T_FINAL);

  // Wait for the view to be closed.
  Hermes::Hermes2D::Views::View::wait();
  return 0;
}