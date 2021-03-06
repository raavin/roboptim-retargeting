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

#define BOOST_TEST_MODULE trc

#include <fstream>

#include <boost/test/unit_test.hpp>
#include <boost/test/output_test_stream.hpp>

#include <libmocap/marker-trajectory-factory.hh>
#include <libmocap/marker-trajectory.hh>


#include <roboptim/retargeting/function/libmocap-marker-trajectory.hh>
#include <roboptim/retargeting/io/trc.hh>
#include <roboptim/retargeting/utility.hh>

using namespace roboptim;
using namespace roboptim::retargeting;

using boost::test_tools::output_test_stream;

static void readWrite (const std::string& file, const std::string& output)
{
  // Load data
  libmocap::MarkerTrajectoryFactory factory;
  libmocap::MarkerTrajectory markers =
    factory.load (file);

  // Building the trajectory.
  LibmocapMarkerTrajectory trajectory (markers);

  // Build the mapping
  MarkerMappingShPtr mapping =
    buildMarkerMappingFromMotion (markers);
  writeTRC (output, trajectory, safeGet (mapping));
}

BOOST_AUTO_TEST_CASE (simple)
{
  // Loading the motion.
  std::string file = DATA_DIR;
  file += "/human.trc";

  readWrite (file, "/tmp/test.trc");
  readWrite ("/tmp/test.trc", "/tmp/test2.trc");

  // Check that the files are identical.
  //
  // With zsh, one could run:
  // md5sum =(tail -n+2 /tmp/test.trc) =(tail -n+2 /tmp/test2.trc)
  std::string str1;
  std::ifstream file1 ("/tmp/test.trc");
  std::getline (file1, str1);

  std::string str2;
  std::ifstream file2 ("/tmp/test2.trc");
  std::getline (file2, str2);

  while (std::getline (file1, str1) && std::getline (file2, str2))
    BOOST_CHECK_EQUAL (str1, str2);

  BOOST_CHECK (!std::getline (file1, str1) && !std::getline (file2, str2));
}
