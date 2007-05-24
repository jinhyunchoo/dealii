//----------------------------  hp_constraints_common.cc  ---------------------------
//    $Id: hp_constraints_common.cc 12732 2006-03-28 23:15:45Z wolf $
//    Version: $Name$ 
//
//    Copyright (C) 2006, 2007 by the deal.II authors
//
//    This file is subject to QPL and may not be  distributed
//    without copyright and license information. Please refer
//    to the file deal.II/doc/license.html for the  text  and
//    further information on this license.
//
//----------------------------  hp_constraints_common.cc  ---------------------------


// common framework to check hp constraints

#include "../tests.h"
#include <base/function.h>
#include <base/function_lib.h>
#include <base/logstream.h>
#include <base/quadrature_lib.h>
#include <lac/vector.h>

#include <grid/tria.h>
#include <hp/dof_handler.h>
#include <dofs/dof_constraints.h>
#include <grid/grid_generator.h>
#include <grid/grid_refinement.h>
#include <grid/tria_accessor.h>
#include <grid/tria_iterator.h>
#include <grid/tria_boundary_lib.h>
#include <dofs/dof_accessor.h>
#include <dofs/dof_tools.h>
#include <numerics/vectors.h>
#include <fe/fe_abf.h>
#include <fe/fe_dgp.h>
#include <fe/fe_dgp_monomial.h>
#include <fe/fe_dgp_nonparametric.h>
#include <fe/fe_dgq.h>
#include <fe/fe_nedelec.h>
#include <fe/fe_q.h>
#include <fe/fe_q_hierarchical.h>
#include <fe/fe_raviart_thomas.h>
#include <fe/fe_system.h>
#include <fe/q_collection.h>

#include <fstream>
#include <vector>


template <int dim>
void test ();




template <int dim>
void do_check (const Triangulation<dim> &triangulation,
	       const hp::FECollection<dim> &fe)
{
  hp::DoFHandler<dim>        dof_handler(triangulation);

				   // distribute fe_indices randomly
  for (typename hp::DoFHandler<dim>::active_cell_iterator
	 cell = dof_handler.begin_active();
       cell != dof_handler.end(); ++cell)
    cell->set_active_fe_index (rand() % fe.size());
  dof_handler.distribute_dofs (fe);

  deallog << "n_dofs=" << dof_handler.n_dofs() << std::endl;

  ConstraintMatrix constraints;
  DoFTools::make_hanging_node_constraints (dof_handler,
					   constraints);
  constraints.close ();

  constraints.print (deallog.get_file_stream());
}



// check on a uniformly refined mesh
template <int dim>
void test_no_hanging_nodes (const hp::FECollection<dim> &fe)
{
  Triangulation<dim>     triangulation;
  GridGenerator::hyper_cube (triangulation);
  triangulation.refine_global (3);

  do_check (triangulation, fe);
}



// same test as above, but this time with a mesh that has hanging nodes
template <int dim>
void test_with_hanging_nodes (const hp::FECollection<dim> &fe)
{
  Triangulation<dim>     triangulation;
  GridGenerator::hyper_cube (triangulation);
  triangulation.refine_global (1);
  triangulation.begin_active()->set_refine_flag ();
  triangulation.execute_coarsening_and_refinement ();
  triangulation.refine_global (1);
  
  do_check (triangulation, fe);
}



// test with a 3d grid that has cells with face_orientation==false and hanging
// nodes. this trips up all sorts of pieces of code, for example there was a
// crash when computing hanging node constraints on such faces (see
// bits/face_orientation_crash), and it triggers all sorts of other
// assumptions that may be hidden in places
//
// the mesh we use is the 7 cells of the hyperball mesh in 3d, with each of
// the cells refined in turn. that then makes 7 meshes with 14 active cells
// each. this also cycles through all possibilities of coarser or finer cell
// having face_orientation==false
template <int dim>
void test_with_wrong_face_orientation (const hp::FECollection<dim> &fe)
{
  if (dim != 3)
    return;
  
  for (unsigned int i=0; i<7; ++i)
    {
      Triangulation<dim>     triangulation;
      GridGenerator::hyper_ball (triangulation);
      typename Triangulation<dim>::active_cell_iterator
	cell = triangulation.begin_active();
      std::advance (cell, i);
      cell->set_refine_flag ();
      triangulation.execute_coarsening_and_refinement ();
  
      do_check (triangulation, fe);
    }
}




// test with a 2d mesh that forms a square but subdivides it into 3
// elements. this tests the case of the sign_change thingy in
// fe_poly_tensor.cc
template <int dim>
void test_with_2d_deformed_mesh (const hp::FECollection<dim> &fe)
{
  if (dim != 2)
    return;
  
  std::vector<Point<dim> > points_glob;
  std::vector<Point<dim> > points;

  points_glob.push_back (Point<dim> (0.0, 0.0));
  points_glob.push_back (Point<dim> (1.0, 0.0));
  points_glob.push_back (Point<dim> (1.0, 0.5));
  points_glob.push_back (Point<dim> (1.0, 1.0));
  points_glob.push_back (Point<dim> (0.6, 0.5));
  points_glob.push_back (Point<dim> (0.5, 1.0));
  points_glob.push_back (Point<dim> (0.0, 1.0));

  				   // Prepare cell data
  std::vector<CellData<dim> > cells (3);

  cells[0].vertices[0] = 0;
  cells[0].vertices[1] = 1;
  cells[0].vertices[2] = 4;
  cells[0].vertices[3] = 2;
  cells[0].material_id = 0;

  cells[1].vertices[0] = 4;
  cells[1].vertices[1] = 2;
  cells[1].vertices[2] = 5;
  cells[1].vertices[3] = 3;
  cells[1].material_id = 0;

  cells[2].vertices[0] = 0;
  cells[2].vertices[1] = 4;
  cells[2].vertices[2] = 6;
  cells[2].vertices[3] = 5;
  cells[2].material_id = 0;

  Triangulation<dim>     triangulation;
  triangulation.create_triangulation (points_glob, cells, SubCellData());
  
  do_check (triangulation, fe);
}



// same as test_with_2d_deformed_mesh, but refine each element in turn. this
// makes sure we also check the sign_change thingy for refined cells
template <int dim>
void test_with_2d_deformed_refined_mesh (const hp::FECollection<dim> &fe)
{
  if (dim != 2)
    return;

  for (unsigned int i=0; i<3; ++i)
    {
      std::vector<Point<dim> > points_glob;
      std::vector<Point<dim> > points;

      points_glob.push_back (Point<dim> (0.0, 0.0));
      points_glob.push_back (Point<dim> (1.0, 0.0));
      points_glob.push_back (Point<dim> (1.0, 0.5));
      points_glob.push_back (Point<dim> (1.0, 1.0));
      points_glob.push_back (Point<dim> (0.6, 0.5));
      points_glob.push_back (Point<dim> (0.5, 1.0));
      points_glob.push_back (Point<dim> (0.0, 1.0));

				       // Prepare cell data
      std::vector<CellData<dim> > cells (3);

      cells[0].vertices[0] = 0;
      cells[0].vertices[1] = 1;
      cells[0].vertices[2] = 4;
      cells[0].vertices[3] = 2;
      cells[0].material_id = 0;

      cells[1].vertices[0] = 4;
      cells[1].vertices[1] = 2;
      cells[1].vertices[2] = 5;
      cells[1].vertices[3] = 3;
      cells[1].material_id = 0;

      cells[2].vertices[0] = 0;
      cells[2].vertices[1] = 4;
      cells[2].vertices[2] = 6;
      cells[2].vertices[3] = 5;
      cells[2].material_id = 0;

      Triangulation<dim>     triangulation;
      triangulation.create_triangulation (points_glob, cells, SubCellData());

      switch (i)
	{
	  case 0:
		triangulation.begin_active()->set_refine_flag();
		break;
	  case 1:
		(++(triangulation.begin_active()))->set_refine_flag();
		break;
	  case 2:
		(++(++(triangulation.begin_active())))->set_refine_flag();
		break;
	  default:
		Assert (false, ExcNotImplemented());
	}
      triangulation.execute_coarsening_and_refinement ();
      
      do_check (triangulation, fe);
    }
}



// test that interpolating a suitable polynomial onto a refined mesh (which
// should yield zero error) and then applying constraints still yields zero
// error. we do so with every pair of finite elements given
template <int dim>
void test_interpolation_base (const hp::FECollection<dim>     &fe,
			      const std::vector<unsigned int> &polynomial_degrees,
			      const bool                       do_refine)
{
				   // create a mesh like this (viewed
				   // from top, if in 3d):
				   // *---*---*
				   // | 0 | 1 |
				   // *---*---*
				   //
				   // then refine cell 1 if do_refine, so that
				   // we get hanging nodes
  Triangulation<dim>     triangulation;
  std::vector<unsigned int> subdivisions (dim, 1);
  subdivisions[0] = 2;
  GridGenerator::subdivided_hyper_rectangle (triangulation, subdivisions,
                                             Point<dim>(),
					     (dim == 3 ?
					      Point<dim>(2,1,1) :
					      (dim == 2 ?
					       Point<dim>(2,1) :
					       Point<dim>(2.))));

  if (do_refine)
    {
      (++triangulation.begin_active())->set_refine_flag ();
      triangulation.execute_coarsening_and_refinement ();
    }
  
  hp::DoFHandler<dim>        dof_handler(triangulation);


				   // for every pair of finite elements,
				   // assign them to each of the two sides of the domain
  for (unsigned int fe1=0; fe1<fe.size(); ++fe1)
    for (unsigned int fe2=0; fe2<fe.size(); ++fe2)
      {
					 // skip elements that don't have
					 // support points defined
	if (! (fe[fe1].has_support_points() &&
	       fe[fe2].has_support_points()))
	  continue;
	
	deallog << "Testing " << fe[fe1].get_name()
		<< " vs. " << fe[fe2].get_name()
		<< std::endl;
	
					 // set fe on coarse cell to 'i', on
					 // all fine cells to 'j'
	typename hp::DoFHandler<dim>::active_cell_iterator
	  cell = dof_handler.begin_active();
	cell->set_active_fe_index (fe1);
	++cell;

	for (; cell != dof_handler.end(); ++cell)
	  cell->set_active_fe_index (fe2);

	dof_handler.distribute_dofs (fe);
  
	ConstraintMatrix constraints;
	DoFTools::make_hanging_node_constraints (dof_handler,
						 constraints);
	constraints.close ();

	Vector<double> interpolant_1 (dof_handler.n_dofs());
	Vector<double> interpolant_2 (dof_handler.n_dofs());
	
	Vector<float>  error (triangulation.n_active_cells());

					 // now consider the dim polynomials
					 // of degree equal to the minimal
					 // degree of the two finite elements
					 // with dependence on only a single
					 // coordinate. also consider the one
					 // that has full degree in all
					 // directions (note that the last
					 // test will fail for P finite
					 // elements, whereas it is ok for the
					 // Q elements; the P elements don't
					 // run this test right now because
					 // they have no support points and
					 // therefore can't use
					 // VectorTools::interpolate...)
	const unsigned int min_degree = std::min (polynomial_degrees[fe1],
						  polynomial_degrees[fe2]);
	for (unsigned int test=0; test<dim+1; ++test)
	  {
	    Tensor<1,dim> exponents;
	    if (test < dim)
	      exponents[test] = min_degree;
	    else
	      {
		for (unsigned int i=0; i<dim; ++i)
		  exponents[i] = min_degree;
	      }

	    const Functions::Monomial<dim> test_function (exponents,
							  fe.n_components());

					     // interpolate the function
	    VectorTools::interpolate (dof_handler,
				      test_function,
				      interpolant_1);

					     // then compute the interpolation error
	    VectorTools::integrate_difference (dof_handler,
					       interpolant_1,
					       test_function,
					       error,
					       hp::QCollection<dim>(QGauss<dim>(min_degree+2)),
					       VectorTools::L2_norm);
	    Assert (error.l2_norm() < 1e-12*interpolant_1.l2_norm(),
		    ExcInternalError());
	    deallog << "  Relative interpolation error before constraints: "
		    << error.l2_norm() / interpolant_1.l2_norm()
		    << std::endl;

					     // copy the interpolant, apply
					     // the constraints, and then
					     // compare the difference. it
					     // should be zero, since our
					     // original polynomial was in the
					     // ansatz space
	    interpolant_2 = interpolant_1;
	    constraints.distribute (interpolant_2);

	    interpolant_2 -= interpolant_1;

	    Assert (interpolant_2.l2_norm() < 1e-12*interpolant_1.l2_norm(),
		    ExcInternalError());

	    deallog << "  Relative difference after constraints: "
		    << interpolant_2.l2_norm() / interpolant_1.l2_norm()
		    << std::endl;
	  }
      }
}


template <int dim>
void test_interpolation (const hp::FECollection<dim>     &fe,
			 const std::vector<unsigned int> &polynomial_degrees)
{
  test_interpolation_base (fe, polynomial_degrees, false);
  test_interpolation_base (fe, polynomial_degrees, true);
}



int main ()
{
  std::ofstream logfile(logname);
  logfile.precision (3);
  
  deallog.attach(logfile);
  deallog.depth_console(0);
  deallog.threshold_double(1.e-10);

  test<1>();
  test<2>();
  test<3>();
}

