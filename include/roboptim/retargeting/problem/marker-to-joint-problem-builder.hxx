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
#ifndef ROBOPTIM_RETARGETING_PROBLEM_MARKER_TO_JOINT_PROBLEM_BUILDER_HXX
# define ROBOPTIM_RETARGETING_PROBLEM_MARKER_TO_JOINT_PROBLEM_BUILDER_HXX
# include <algorithm>

# include <boost/format.hpp>
# include <boost/make_shared.hpp>
# include <boost/optional.hpp>

# include <cnoid/BodyIMesh>
# include <cnoid/BodyLoader>
# include <cnoid/BodyMotion>

# include <roboptim/core/problem.hh>
# include <roboptim/core/filter/bind.hh>

# include <roboptim/trajectory/state-function.hh>
# include <roboptim/trajectory/vector-interpolation.hh>

# include <roboptim/retargeting/function/choreonoid-body-trajectory.hh>

# include <roboptim/retargeting/problem/marker-to-joint-function-factory.hh>


// for trajectory conversion
# include <roboptim/retargeting/problem/marker-problem-builder.hh>
// for trajectory filtering
# include <roboptim/retargeting/problem/joint-problem-builder.hh>


namespace roboptim
{
  namespace retargeting
  {
    // Warning: be particularly cautious regarding the loading order
    // as data is inter-dependent.
    void
    buildJointDataFromOptions (MarkerToJointFunctionData& data,
			       const MarkerToJointProblemOptions& options)
    {
      cnoid::BodyLoader loader;

      data.markerSet =
	libmocap::MarkerSetFactory ().load (options.markerSet);
      data.markersTrajectory =
	libmocap::MarkerTrajectoryFactory ().load (options.markersTrajectory);
      data.robotModel = loader.load (options.robotModel);

      if (options.trajectoryType == "discrete")
	data.inputTrajectory =
	  convertToTrajectory<VectorInterpolation> (data.markersTrajectory);
      else
	throw std::runtime_error ("invalid trajectory type");

      Function::size_type nFrames =
	static_cast<Function::size_type> (data.markersTrajectory.numFrames ());
      Function::size_type dt =
	static_cast<Function::size_type>
	(1. / data.markersTrajectory.dataRate ());
      Function::size_type nDofsFull =
	static_cast<Function::size_type> (6 + data.robotModel->numJoints ());
      Function::size_type nDofsReduced =
	static_cast<Function::size_type> (nDofsFull - 0); //FIXME:

      data.outputTrajectoryReduced =
	boost::make_shared<VectorInterpolation>
	(Function::vector_t (nDofsReduced * nFrames),
	 nDofsReduced, dt);
      data.outputTrajectory =
	boost::make_shared<VectorInterpolation>
	(Function::vector_t (nDofsFull), nDofsReduced, dt);

      //FIXME: first frame value of outputTrajectory will be used for
      //joints defaults value, we probably want to set it to
      //half-sitting to avoid issues.
      data.disabledJointsConfiguration =
	disabledJointsConfiguration
	(options.disabledJoints, data.outputTrajectory, data.robotModel);
    }


    template <typename T>
    MarkerToJointProblemBuilder<T>::MarkerToJointProblemBuilder
    (const MarkerToJointProblemOptions& options)
      : options_ (options)
    {}

    template <typename T>
    MarkerToJointProblemBuilder<T>::~MarkerToJointProblemBuilder ()
    {}

    template <typename T>
    void
    MarkerToJointProblemBuilder<T>::operator ()
      (boost::shared_ptr<T>& problem, MarkerToJointFunctionData& data)
    {
      buildJointDataFromOptions (data, options_);
      MarkerToJointFunctionFactory factory (data);
      data.cost =
	factory.buildFunction<DifferentiableFunction> (options_.cost);
      problem = boost::make_shared<T> (*data.cost);

      Function::vector_t::Index nMarkers =
	static_cast<Function::vector_t::Index>
	(data.markersTrajectory.numMarkers ());
      Function::vector_t::Index length =
	static_cast<Function::vector_t::Index>
	(nMarkers * 3);

      // for first frame, start from half-sitting
      if (options_.frameId == 0)
	{
	  //FIXME: zero for now
	  problem->startingPoint () = Function::vector_t (length);
	}
      // for other frames, start from previous frame
      else
	{
	  Function::vector_t::Index start =
	    static_cast<Function::vector_t::Index>
	    ((options_.frameId - 1) * nMarkers * 3);

	  problem->startingPoint () =
	    data.outputTrajectoryReduced->parameters ().segment
	    (start, length);
	}

    }
  } // end of namespace retargeting.
} // end of namespace roboptim.

#endif //! ROBOPTIM_RETARGETING_PROBLEM_MARKER_TO_JOINT_PROBLEM_BUILDER_HXX
