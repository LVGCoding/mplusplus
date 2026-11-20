#pragma once

// Everything a compiled M++ program includes. The editor precompiles this
// header once (see src/run/Toolchain.cpp), so keep the heavy library headers
// here: they cost nothing per run after the first.

#ifndef BOOST_MATH_STANDALONE
#define BOOST_MATH_STANDALONE 1
#endif

#include <Eigen/Dense>
#include <boost/math/distributions/normal.hpp>

#include "control.hpp"
#include "core.hpp"
#include "data.hpp"
#include "linalg.hpp"
#include "print.hpp"
#include "scalar.hpp"
#include "set.hpp"
