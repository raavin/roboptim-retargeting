// Copyright (C) 2013 by Thomas Moulard, AIST, CNRS, INRIA.
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

#ifndef ROBOPTIM_RETARGETING_CHOREONOID_BODY_MOTION_HH
# define ROBOPTIM_RETARGETING_CHOREONOID_BODY_MOTION_HH
# include <boost/array.hpp>
# include <boost/shared_ptr.hpp>
# include <roboptim/trajectory/vector-interpolation.hh>

# include <cnoid/BodyMotion>

namespace roboptim
{
  namespace retargeting
  {
    /// \brief Discrete trajectory built from a cnoid::BodyMotion
    ///        object
    class ChoreonoidBodyTrajectory :
      public VectorInterpolation
    {
    public:
      ROBOPTIM_TWICE_DIFFERENTIABLE_FUNCTION_FWD_TYPEDEFS
      (VectorInterpolation);
      ROBOPTIM_IMPLEMENT_CLONE (ChoreonoidBodyTrajectory);

      explicit ChoreonoidBodyTrajectory (cnoid::BodyMotionPtr bodyMotion,
					 bool addFreeFloating) throw ()
	: VectorInterpolation
	  (computeParametersFromBodyMotion (bodyMotion, addFreeFloating),
	   (addFreeFloating ? 6 : 0) + bodyMotion->numJoints (),
	   1. / bodyMotion->frameRate ()),
	  bodyMotion_ (bodyMotion)
      {
	std::cout << "PARAMETERS SIZE: " << this->parameters ().size () << std::endl;
	std::cout << "OUTPUT SIZE: " << this->outputSize () << std::endl;
      }

      virtual ~ChoreonoidBodyTrajectory () throw ()
      {}

    private:
      static vector_t computeParametersFromBodyMotion
      (cnoid::BodyMotionPtr bodyMotion, bool addFreeFloating)
      {
	std::size_t freeFloatingOffset = 0;
	if (addFreeFloating)
	  freeFloatingOffset = 6;
	vector_t x
	  (bodyMotion->getNumFrames ()
	   * (freeFloatingOffset + bodyMotion->numJoints ()));
	for (int frameId = 0; frameId < bodyMotion->getNumFrames (); ++frameId)
	  {
	    if (addFreeFloating)
	      {
		x.segment
		  (frameId * (freeFloatingOffset + bodyMotion->numJoints ()),
		   3) = bodyMotion->linkPosSeq ()->frame (frameId)[0].translation ();

		Eigen::AngleAxisd angleAxis =
		  angleAxis.fromRotationMatrix
		  (bodyMotion->linkPosSeq ()->frame
		   (frameId)[0].rotation ().toRotationMatrix ());
		x.segment
		  (frameId * (freeFloatingOffset + bodyMotion->numJoints ()) + 3,
		   3) = angleAxis.angle () * angleAxis.axis ();

	      }

	    for (int dofId = 0; dofId < bodyMotion->numJoints (); ++dofId)
	      x[frameId * (freeFloatingOffset + bodyMotion->numJoints ())
		+ freeFloatingOffset + dofId] =
		bodyMotion->jointPosSeq ()->frame (frameId)[dofId];
	  }
	return x;
      }

      cnoid::BodyMotionPtr bodyMotion_;
    };
  } // end of namespace retargeting.
} // end of namespace roboptim.

#endif //! ROBOPTIM_RETARGETING_CHOREONOID_BODY_MOTION_HH