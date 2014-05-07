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
#ifndef ROBOPTIM_RETARGETING_PROBLEM_JOINT_PROBLEM_BUILDER_HXX
# define ROBOPTIM_RETARGETING_PROBLEM_JOINT_PROBLEM_BUILDER_HXX
# include <algorithm>

# include <boost/format.hpp>
# include <boost/make_shared.hpp>
# include <boost/optional.hpp>

# include <cnoid/BodyIMesh>
# include <cnoid/BodyLoader>
# include <cnoid/BodyMotion>

# include <roboptim/core/problem.hh>

# include <roboptim/trajectory/vector-interpolation.hh>

# include <roboptim/retargeting/function/choreonoid-body-trajectory.hh>

# include <roboptim/retargeting/problem/joint-function-factory.hh>


namespace roboptim
{
  namespace retargeting
  {
    /// \brief Store which joints are disabled and what value they
    ///        should be set in this case.
    ///
    /// By default, all joints are enabled.
    ///
    /// The user can also pass a set of joints to be filtered out before
    /// starting the optimization process. The joints are described by
    /// their name.
    ///
    /// Invalid joint names or duplicated joint are illegal.
    ///
    /// This function build the set of enabled joints from this vector
    /// of strings. This is represented by a vector of boolean
    /// value. The index is the joint id (0...6 is free-floating then
    /// the joints configurations).
    ///
    /// \param[in] disabledJoints list of joints to be excluded from the
    ///            optimization process and identified by their name
    /// \param[in] originalTrajectory full joint trajectory
    /// \param[in] robotModel robot model loaded by Choreonoid and
    ///            providing the joint name to index mapping
    /// \return vector of enabled and disabled joints (true means that
    ///         joint is enabled, i.e. to be taken into account during the
    ///         optimization process).
    std::vector<boost::optional<Function::value_type> >
    disabledJointsConfiguration
    (const std::vector<std::string>& disabledJoints,
     boost::shared_ptr<Trajectory<3> > originalTrajectory,
     cnoid::BodyPtr robotModel)
    {
      std::set<std::string> disabledJointsSet
	(disabledJoints.begin (), disabledJoints.end ());
      if (disabledJoints.size () != disabledJointsSet.size ())
	throw std::runtime_error ("disabled joints are not unique");

      if (disabledJointsSet.size () ==
	  static_cast<std::size_t> (robotModel->numJoints ()))
	throw std::runtime_error ("all joints are disabled");

      std::vector<boost::optional<Function::value_type> > configuration
	(6 + static_cast<std::size_t> (robotModel->numJoints ()), true);

      std::set<std::string>::const_iterator it;
      for (it = disabledJointsSet.begin (); it != disabledJointsSet.end (); ++it)
	{
	  cnoid::Link* link = robotModel->link (*it);
	  if (!link)
	    {
	      boost::format fmt ("joint ``%s'' does not exist in robotModel ``%s''");
	      fmt % *it % robotModel->modelName ();
	      throw std::runtime_error (fmt.str ());
	    }

	  // add 6 for free-floating and remove 1 as body are labeled
	  // starting from one in Choreonoid
	  int id = 6 + link->index () - 1;
	  if (id < 0)
	    throw std::runtime_error ("conversion from body name"
				      " to index failed (negative index)");
	  configuration[static_cast<std::size_t> (id)] =
	    originalTrajectory->parameters ()[id];
	}
      return configuration;
    }

    /// \brief Copy a trajectory while excluding some joints.
    ///
    /// \param[in] originalTrajectory original, full, trajectory
    /// \param[in] disabledJoints list of joints to be excluded
    /// \param[in] robot robot model
    boost::shared_ptr<Trajectory<3> >
    filterTrajectory (boost::shared_ptr<Trajectory<3> > originalTrajectory,
		      const std::vector<boost::optional<Function::value_type> >&
		      disabledJointsConfiguration)
    {
      typedef Trajectory<3>::vector_t::Index index_t;

      index_t nDofsFull =
	static_cast<index_t> (disabledJointsConfiguration.size ());
      index_t nDofsReduced =
	std::count (disabledJointsConfiguration.begin (),
		    disabledJointsConfiguration.end (),
		    boost::optional<Function::value_type> ());
      index_t nFrames =
	static_cast<index_t> (originalTrajectory->parameters ().size ())
	/ nDofsFull;
      index_t reducedTrajectoryParametersSize = nDofsReduced * nFrames;

      Trajectory<3>::vector_t
	reducedTrajectoryParameters (reducedTrajectoryParametersSize);

      index_t idx = 0;
      index_t idxOriginal = 0;
      while (idx < reducedTrajectoryParametersSize &&
	     idxOriginal < originalTrajectory->parameters ().size ())
	{
	  if (disabledJointsConfiguration[static_cast<std::size_t> (idxOriginal % nDofsFull)])
	    reducedTrajectoryParameters[idx++] = originalTrajectory->parameters ()[idxOriginal];
	  idxOriginal++;
	}

      boost::shared_ptr<Trajectory<3> > reducedTrajectory =
	boost::make_shared<VectorInterpolation>
	(reducedTrajectoryParameters,
	 nDofsReduced, originalTrajectory->length () / nDofsFull);

      return reducedTrajectory;
    }

    // Warning: be particularly cautious regarding the loading order
    // as data is inter-dependent.
    void
    buildJointDataFromOptions (JointFunctionData& data,
			       const JointProblemOptions& options)
    {
      cnoid::BodyLoader loader;

      data.jointsTrajectory = boost::make_shared<cnoid::BodyMotion> ();
      data.jointsTrajectory->loadStandardYAMLformat (options.jointsTrajectory);

      data.robotModel = loader.load (options.robotModel);

      // Create the interaction mesh
      data.interactionMesh = boost::make_shared<cnoid::BodyIMesh> ();
      if (!data.interactionMesh->addBody (data.robotModel, data.jointsTrajectory))
	throw std::runtime_error ("failed to add body to body interaction mesh");
      if (!data.interactionMesh->initialize ())
        throw std::runtime_error ("failed to initialize body interaction mesh");


      // Load the trajectory
      if (options.trajectoryType == "discrete")
	{
	  data.trajectory = boost::make_shared<ChoreonoidBodyTrajectory>
	    (data.jointsTrajectory, true);
	}
      else
      	throw std::runtime_error ("invalid trajectory type");

      // Filter the trajectory (do not re-order as the initial trajectory is needed!)

      data.disabledJointsConfiguration =
	disabledJointsConfiguration
	(options.disabledJoints, data.trajectory, data.robotModel);

      for (Function::vector_t::Index frame = 0; frame < data.nFrames (); ++frame)
	data.disabledJointsTrajectory.insert
	  (data.disabledJointsTrajectory.end (),
	   data.disabledJointsConfiguration.begin (),
	   data.disabledJointsConfiguration.end ());

      data.filteredTrajectory =
	filterTrajectory
	(data.trajectory, data.disabledJointsConfiguration);

    }


    template <typename T>
    JointProblemBuilder<T>::JointProblemBuilder
    (const JointProblemOptions& options)
      : options_ (options)
    {}

    template <typename T>
    JointProblemBuilder<T>::~JointProblemBuilder ()
    {}

    template <typename T>
    std::pair<boost::shared_ptr<T>,
	      boost::shared_ptr<typename T::function_t> >
    JointProblemBuilder<T>::operator () ()
    {
      std::pair<boost::shared_ptr<T>,
		boost::shared_ptr<typename T::function_t > > result;
      JointFunctionData data;
      buildJointDataFromOptions (data, options_);

      JointFunctionFactory factory (data);

      boost::shared_ptr<DifferentiableFunction> cost =
	factory.buildFunction<DifferentiableFunction> (options_.cost);

      boost::shared_ptr<T> problem = boost::make_shared<T> (*cost);

      std::vector<std::string>::const_iterator it;
      for (it = options_.constraints.begin ();
	   it != options_.constraints.end (); ++it)
	{
	  Constraint<DifferentiableFunction> constraint =
	    factory.buildConstraint<DifferentiableFunction> (*it);
	  problem->addConstraint
	    (constraint.function,
	     constraint.intervals,
	     constraint.scales);
	}

      problem->startingPoint () = data.trajectory->parameters ();

      result.first = problem;
      result.second = cost;
      return result;
    }
  } // end of namespace retargeting.
} // end of namespace roboptim.

#endif //! ROBOPTIM_RETARGETING_PROBLEM_JOINT_PROBLEM_BUILDER_HXX