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

#ifndef ROBOPTIM_RETARGETING_FUNCTION_TORQUE_METAPOD_HH
# define ROBOPTIM_RETARGETING_FUNCTION_TORQUE_METAPOD_HH
# include <roboptim/core/function.hh>

# include <boost/fusion/include/at_c.hpp>
# include <metapod/tools/joint.hh>
# include <metapod/algos/rnea.hh>
# include <metapod/tools/print.hh>

# include <roboptim/core/finite-difference-gradient.hh>

# include <roboptim/retargeting/function/torque.hh>

namespace roboptim
{
  namespace retargeting
  {
    /// \brief Torque position computed through Metapod
    ///
    /// You *have* to make sure that your model root joint is a
    /// floating joint here.
    ///
    /// \tparam T Function traits type
    /// \tparam R Robot type
    template <typename T, typename R>
    class TorqueMetapod : public Torque<T>
    {
    public:
      ROBOPTIM_DIFFERENTIABLE_FUNCTION_FWD_TYPEDEFS_ (Torque<T>);
      typedef R robot_t;

      explicit TorqueMetapod ()
	: Torque<T> (robot_t::NBDOF, "metapod"),
	  robot_ ()
      {}

      virtual ~TorqueMetapod ()
      {}

      // see https://github.com/laas/metapod/issues/63
      void
      impl_compute
      (result_t& result, const argument_t& x)
	const
      {
	metapod::rnea<robot_t, true>::run
	  (robot_, this->q (x), this->dq (x), this->ddq (x));
	metapod::getTorques (robot_, torques_);
	result = torques_;
      }

      void
      impl_gradient (gradient_t& gradient,
		     const argument_t& x,
		     size_type i)
	const
      {
#ifndef ROBOPTIM_DO_NOT_CHECK_ALLOCATION
	Eigen::internal::set_is_malloc_allowed (true);
#endif //! ROBOPTIM_DO_NOT_CHECK_ALLOCATION

	roboptim::GenericFiniteDifferenceGradient<
	  T,
	  finiteDifferenceGradientPolicies::Simple<T> >
	  fdg (*this);
	fdg.gradient (gradient, x, i);
      }

    private:
      mutable robot_t robot_;
      mutable typename robot_t::confVector torques_;
    };
  } // end of namespace retargeting.
} // end of namespace roboptim.

#endif //! ROBOPTIM_RETARGETING_FUNCTION_TORQUE_HH
