#define HERMES_REPORT_ALL
#define HERMES_REPORT_FILE "application.log"
#include "hermes2d.h"

using namespace RefinementSelectors;

//  This benchmark problem with known exact solution describes an electromagnetic wave that hits
//  a screen under the angle of 45 degrees, causing a very strong singularity at the tip of the 
//  screen. Convergence graphs saved (both exact error and error estimate, and both wrt. dof number.
//  and cpu time).
//
//  PDE: time-harmonic Maxwell's equations.
//
//  Known exact solution, see the function exact().
//
//  Domain: square domain cut from the midpoint of the left edge to the center (center
//          point of left edge duplicated).
//
//  Meshes: you can either use "screen-quad.mesh" (quadrilateral mesh) or
//          "screen-tri.mesh" (triangular mesh). See the command mesh.load(...) below.
//
//  BC: tangential component of solution taken from known exact solution (essential BC),
//      see function essential_bc_values() below.
//
// The following parameters can be changed:

const int INIT_REF_NUM = 1;                       // Number of initial uniform mesh refinements.
const int P_INIT = 1;                             // Initial polynomial degree. NOTE: The meaning is different from
                                                  // standard continuous elements in the space H1. Here, P_INIT refers
                                                  // to the maximum poly order of the tangential component, and polynomials
                                                  // of degree P_INIT + 1 are present in element interiors. P_INIT = 0
                                                  // is for Whitney elements.
const double THRESHOLD = 0.5;                     // This is a quantitative parameter of the adapt(...) function and
                                                  // it has different meanings for various adaptive strategies (see below).
const int STRATEGY = 1;                           // Adaptive strategy:
                                                  // STRATEGY = 0 ... refine elements until sqrt(THRESHOLD) times total
                                                  //   error is processed. If more elements have similar errors, refine
                                                  //   all to keep the mesh symmetric.
                                                  // STRATEGY = 1 ... refine all elements whose error is larger
                                                  //   than THRESHOLD times maximum element error.
                                                  // STRATEGY = 2 ... refine all elements whose error is larger
                                                  //   than THRESHOLD.
                                                  // More adaptive strategies can be created in adapt_ortho_h1.cpp.
const CandList CAND_LIST = H2D_HP_ANISO_H;        // Predefined list of element refinement candidates. Possible values are
                                                  // H2D_P_ISO, H2D_P_ANISO, H2D_H_ISO, H2D_H_ANISO, H2D_HP_ISO,
                                                  // H2D_HP_ANISO_H, H2D_HP_ANISO_P, H2D_HP_ANISO.
                                                  // See User Documentation for details.
const int MESH_REGULARITY = -1;                   // Maximum allowed level of hanging nodes:
                                                  // MESH_REGULARITY = -1 ... arbitrary level hangning nodes (default),
                                                  // MESH_REGULARITY = 1 ... at most one-level hanging nodes,
                                                  // MESH_REGULARITY = 2 ... at most two-level hanging nodes, etc.
                                                  // Note that regular meshes are not supported, this is due to
                                                  // their notoriously bad performance.
const double CONV_EXP = 1.0;                      // Default value is 1.0. This parameter influences the selection of
                                                  // cancidates in hp-adaptivity. See get_optimal_refinement() for details.
const double ERR_STOP = 1.0;                      // Stopping criterion for adaptivity (rel. error tolerance between the
                                                  // reference mesh and coarse mesh solution in percent).
const int NDOF_STOP = 50000;                      // Adaptivity process stops when the number of degrees of freedom grows
                                                  // over this limit. This is to prevent h-adaptivity to go on forever.
MatrixSolverType matrix_solver = SOLVER_UMFPACK;  // Possibilities: SOLVER_AMESOS, SOLVER_AZTECOO, SOLVER_MUMPS,
                                                  // SOLVER_PETSC, SOLVER_SUPERLU, SOLVER_UMFPACK.

// Problem parameters.
const double e_0  = 8.8541878176 * 1e-12;
const double mu_0 = 1.256 * 1e-6;
const double k = 1.0;

// Exact solution.
#include "exact_solution.cpp"

const int BDY_BOTTOM = 1;
const int BDY_RIGHT = 2;
const int BDY_TOP = 3;
const int BDY_LEFT = 4;

// Unit tangential vectors to the boundary. 
double2 tau[5] = { { 0, 0}, { 1, 0 },  { 0, 1 }, { -1, 0 }, { 0, -1 } };

// Essential boundary condition values.
scalar essential_bc_values_top(double x, double y)
{
  scalar dx, dy;
  return exact0(x, y, dx, dy)*tau[3][0] + exact1(x, y, dx, dy)*tau[3][1];
}
scalar essential_bc_values_right(double x, double y)
{
  scalar dx, dy;
  return exact0(x, y, dx, dy)*tau[2][0] + exact1(x, y, dx, dy)*tau[2][1];
}
scalar essential_bc_values_bottom(double x, double y)
{
  scalar dx, dy;
  return exact0(x, y, dx, dy)*tau[1][0] + exact1(x, y, dx, dy)*tau[1][1];
}
scalar essential_bc_values_left(double x, double y)
{
  scalar dx, dy;
  return exact0(x, y, dx, dy)*tau[4][0] + exact1(x, y, dx, dy)*tau[4][1];
}

// Weak forms.
template<typename Real, typename Scalar>
Scalar bilinear_form(int n, double *wt, Func<Scalar> *u_ext[], Func<Real> *u, Func<Real> *v, Geom<Real> *e, ExtData<Scalar> *ext)
{
  return int_curl_e_curl_f<Real, Scalar>(n, wt, u, v) - int_e_f<Real, Scalar>(n, wt, u, v);
}

int main(int argc, char* argv[])
{
  // Load the mesh.
  Mesh mesh;
  H2DReader mloader;
  mloader.load("screen-quad.mesh", &mesh);    // quadrilaterals
  // mloader.load("screen-tri.mesh", &mesh);  // triangles

  // Perform initial mesh refinements.
  for (int i = 0; i < INIT_REF_NUM; i++)  mesh.refine_all_elements();

  // Enter boundary markers.
  BCTypes bc_types;
  bc_types.add_bc_dirichlet(Hermes::vector<int>(BDY_BOTTOM, BDY_RIGHT, BDY_TOP, BDY_LEFT));

  // Enter Dirichlet boudnary values.
  BCValues bc_values;
  bc_values.add_function(BDY_TOP, essential_bc_values_top);
  bc_values.add_function(BDY_RIGHT, essential_bc_values_right);
  bc_values.add_function(BDY_BOTTOM, essential_bc_values_bottom);
  bc_values.add_function(BDY_LEFT, essential_bc_values_left);

  // Create an Hcurl space with default shapeset.
  HcurlSpace space(&mesh, &bc_types, &bc_values, P_INIT);

  // Initialize the weak formulation.
  WeakForm wf;
  wf.add_matrix_form(callback(bilinear_form), HERMES_SYM);

  // Initialize refinement selector.
  HcurlProjBasedSelector selector(CAND_LIST, CONV_EXP, H2DRS_DEFAULT_ORDER);

  // Set exact solution.
  ExactSolution exact_sln(&mesh, exact);

  // Initialize views.
  VectorView v_view("Solution", new WinGeom(0, 0, 440, 350));
  v_view.set_min_max_range(0.0, 10.0);
  OrderView  o_view("Polynomial orders", new WinGeom(450, 0, 400, 350));

  // DOF and CPU convergence graphs.
  SimpleGraph graph_dof, graph_cpu, graph_dof_exact, graph_cpu_exact;

  // Time measurement.
  TimePeriod cpu_time;
  cpu_time.tick();

  // Adaptivity loop:
  int as = 1;
  bool done = false;
  do
  {
    info("---- Adaptivity step %d:", as);

    // Construct globally refined reference mesh and setup reference space.
    Space* ref_space = construct_refined_space(&space);

    // Assemble the reference problem.
    info("Solving on reference mesh.");
    bool is_linear = true;
    DiscreteProblem* dp = new DiscreteProblem(&wf, ref_space, is_linear);
    SparseMatrix* matrix = create_matrix(matrix_solver);
    Vector* rhs = create_vector(matrix_solver);
    Solver* solver = create_linear_solver(matrix_solver, matrix, rhs);
    dp->assemble(matrix, rhs);

    // Time measurement.
    cpu_time.tick();

    // Solve the linear system of the reference problem. If successful, obtain the solution.
    Solution ref_sln;
    if(solver->solve()) Solution::vector_to_solution(solver->get_solution(), ref_space, &ref_sln);
    else error ("Matrix solver failed.\n");

    // Time measurement.
    cpu_time.tick();

    // Project the fine mesh solution onto the coarse mesh.
    Solution sln;
    info("Projecting reference solution on coarse mesh.");
    OGProjection::project_global(&space, &ref_sln, &sln, matrix_solver);

    // View the coarse mesh solution and polynomial orders.
    v_view.show(&sln);
    o_view.show(&space);

    // Calculate element errors and total error estimate.
    info("Calculating error estimate and exact error.");
    Adapt* adaptivity = new Adapt(&space);
    double err_est_rel = adaptivity->calc_err_est(&sln, &ref_sln) * 100;

    // Calculate exact error.   
    bool solutions_for_adapt = false;
    double err_exact_rel = adaptivity->calc_err_exact(&sln, &exact_sln, solutions_for_adapt) * 100;

    // Report results.
    info("ndof_coarse: %d, ndof_fine: %d", Space::get_num_dofs(&space), Space::get_num_dofs(ref_space));
    info("err_est_rel: %g%%, err_exact_rel: %g%%", err_est_rel, err_exact_rel);

    // Time measurement.
    cpu_time.tick();

    // Add entry to DOF and CPU convergence graphs.
    graph_dof.add_values(Space::get_num_dofs(&space), err_est_rel);
    graph_dof.save("conv_dof_est.dat");
    graph_cpu.add_values(cpu_time.accumulated(), err_est_rel);
    graph_cpu.save("conv_cpu_est.dat");
    graph_dof_exact.add_values(Space::get_num_dofs(&space), err_exact_rel);
    graph_dof_exact.save("conv_dof_exact.dat");
    graph_cpu_exact.add_values(cpu_time.accumulated(), err_exact_rel);
    graph_cpu_exact.save("conv_cpu_exact.dat");

    // If err_est too large, adapt the mesh.
    if (err_est_rel < ERR_STOP) done = true;
    else
    {
      info("Adapting coarse mesh.");
      done = adaptivity->adapt(&selector, THRESHOLD, STRATEGY, MESH_REGULARITY);

      // Increase the counter of performed adaptivity steps.
      if (done == false)  as++;
    }
    if (Space::get_num_dofs(&space) >= NDOF_STOP) done = true;

    // Clean up.
    delete solver;
    delete matrix;
    delete rhs;
    delete adaptivity;
    if(done == false) delete ref_space->get_mesh();
    delete ref_space;
    delete dp;

  }
  while (done == false);

  verbose("Total running time: %g s", cpu_time.accumulated());

  // Wait for all views to be closed.
  View::wait();
  return 0;
}

