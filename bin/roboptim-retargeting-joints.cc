// Copyright (C) 2014 by Thomas Moulard, AIST, CNRS.
//
// This file is part of the roboptim.
//
// roboptim is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// roboptim is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with roboptim.  If not, see <http://www.gnu.org/licenses/>.

#include <fstream>
#include <stdexcept>
#include <string>

#include <boost/program_options.hpp>

#include <yaml-cpp/yaml.h>

#include <roboptim/core/problem.hh>
#include <roboptim/core/result.hh>
#include <roboptim/core/result-with-warnings.hh>
#include <roboptim/core/solver-factory.hh>
#include <roboptim/core/solver.hh>

#include <roboptim/retargeting/problem/joint-problem-builder.hh>
#include <roboptim/retargeting/io/choreonoid-body-motion.hh>

#include "path.hh"

static bool parseOptions
(roboptim::retargeting::JointProblemOptions& options,
 int argc, const char* argv[])
{
  namespace po = boost::program_options;
  po::options_description desc ("Options");
  desc.add_options ()
    ("help,h", "Print help messages")

    ("output-file,o",
     po::value<std::string>
     (&options.outputFile)->required (),
     "output joint trajectory (Choreonoid YAML file)")

    ("joint-trajectory,j",
     po::value<std::string>
     (&options.jointsTrajectory)->required (),
     "articular trajectory (Choreonoid YAML file)")
    ("trajectory-type,t",
     po::value<std::string>
     (&options.trajectoryType)->default_value ("discrete"),
     "Trajectory type (discrete)")
    ("robot-model,r",
     po::value<std::string> (&options.robotModel)->required (),
     "Robot Model (Choreonoid YAML file)")
    ("morphing,M",
     po::value<std::string> (&options.morphing)->required (),
     "Morphing data (YAML file)")
    ("plugin,p",
     po::value<std::string> (&options.plugin)->default_value ("cfsqp"),
     "RobOptim plug-in to be used")
    ("cost,c",
     po::value<std::string> (&options.cost)->default_value ("lde"),
     "What cost function should be used?")
    ("constraint,C",
     po::value<std::vector<std::string> > (&options.constraints),
     "Which constraints should be used?")
    ("disable-joint,d",
     po::value<std::vector<std::string> > (&options.disabledJoints),
     "Exclude a joint from the optimization process")

    ("start-frame",
     po::value<int> (&options.startFrame)->default_value (0),
     "Starting frame (previous frames will be dropped)")
    ("length",
     po::value<int> (&options.length)->default_value (-1),
     "How many frames will be considered? (-1 means all)")
    ;

  po::variables_map vm;
  po::store
    (po::command_line_parser (argc, argv)
     .options (desc)
     .run (),
     vm);

  if (vm.count ("help"))
    {
      std::cout << desc << "\n";

      std::cout << "Available functions:\n";

      std::vector<std::string> functions =
	roboptim::retargeting::JointFunctionFactory::listFunctions ();
      std::vector<std::string>::const_iterator it;
      for (it = functions.begin (); it != functions.end (); ++it)
	std::cout << "\t - " << *it << "\n";

      return false;
    }

  po::notify (vm);

  roboptim::retargeting::resolvePath (options.jointsTrajectory);
  roboptim::retargeting::resolvePath (options.robotModel);
  roboptim::retargeting::resolvePath (options.morphing);
  return true;
}

int safeMain (int argc, const char* argv[])
{
  typedef roboptim::retargeting::denseProblem_t problem_t;
  typedef roboptim::Solver<
    roboptim::GenericDifferentiableFunction<roboptim::EigenMatrixDense>,
    boost::mpl::vector<
      roboptim::GenericLinearFunction<roboptim::EigenMatrixDense>,
      roboptim::GenericDifferentiableFunction<roboptim::EigenMatrixDense>
      >
    >
    solver_t;

  roboptim::retargeting::JointProblemOptions options;

  if (!parseOptions (options, argc, argv))
    return 0;

  // Build problem.
  roboptim::retargeting::JointProblemBuilder<problem_t>
    builder (options);

  boost::shared_ptr<problem_t> problem;
  roboptim::retargeting::JointFunctionData data;
  builder (problem, data);

  if (!problem)
    throw std::runtime_error ("failed to build problem");

  roboptim::SolverFactory<solver_t>
    factory (options.plugin, *problem);
  solver_t& solver = factory ();

  // Set solver parameters.
  solver.parameters ()["max-iterations"].value = 1000;

  solver.parameters ()["ipopt.output_file"].value =
    "/tmp/ipopt.log";
  solver.parameters ()["ipopt.print_level"].value = 5;
  solver.parameters ()["ipopt.expect_infeasible_problem"].value = "no";
  solver.parameters ()["ipopt.nlp_scaling_method"].value = "none";
  solver.parameters ()["ipopt.tol"].value = 1e-3;
  solver.parameters ()["ipopt.dual_inf_tol"].value = 1.;
  solver.parameters ()["ipopt.constr_viol_tol"].value = 1e-3;

  // first-order
  solver.parameters ()["ipopt.derivative_test"].value = "first-order";
  solver.parameters ()["nag.verify-level"].value = 0;

  std::cout << solver << std::endl;

  const solver_t::result_t& result = solver.minimum ();

  boost::shared_ptr<roboptim::Trajectory<3> > finalTrajectoryFiltered =
    boost::shared_ptr<roboptim::Trajectory<3> >
    (data.filteredTrajectory->clone ());

  if (result.which () == solver_t::SOLVER_VALUE_WARNINGS)
    {
      std::cout << "Optimization finished. Warnings have been issued\n";
      roboptim::ResultWithWarnings result_ =
        boost::get<roboptim::ResultWithWarnings> (result);
      std::cerr << result << std::endl;
      finalTrajectoryFiltered->setParameters (result_.x);
    }
  else if (result.which () == solver_t::SOLVER_VALUE)
    {
      std::cout << "Optimization finished successfully.\n";
      roboptim::Result result_ =
        boost::get<roboptim::Result> (result);
      std::cerr << result << std::endl;
      finalTrajectoryFiltered->setParameters (result_.x);
    }
  else
    {
      throw std::runtime_error ("Optimization failed");
    }


  // Re-expend trajectory.
  boost::shared_ptr<roboptim::Trajectory<3> > finalTrajectory =
    boost::shared_ptr<roboptim::Trajectory<3> >
    (data.trajectory->clone ());

  roboptim::Function::vector_t finalTrajectoryParameters =
    finalTrajectory->parameters ();
  for (roboptim::Function::vector_t::Index frameId = 0;
       frameId < data.nFrames (); ++frameId)
    {
      roboptim::Function::vector_t::Index jointIdReduced = 0;
      for (roboptim::Function::vector_t::Index jointId = 0;
	   jointId < data.nDofsFull (); ++jointId)
        {
          if (data.disabledJointsConfiguration
	      [static_cast<std::size_t> (jointId)])
	    finalTrajectoryParameters[frameId * data.nDofsFull () + jointId] =
              finalTrajectoryFiltered->parameters ()
	      [frameId * data.nDofsFiltered () + jointIdReduced++];
        }
    }
  finalTrajectory->setParameters (finalTrajectoryParameters);

  roboptim::retargeting::writeBodyMotion
    (options.outputFile, finalTrajectory);
  return 0;
}

int main (int argc, const char* argv[])
{
  try
    {
      return safeMain (argc, argv);
    }
  catch (const std::exception& e)
    {
      std::cerr << e.what () << std::endl;
      return 1;
    }
}

