//
// PROJECT CHRONO - http://projectchrono.org
//
// Copyright (c) 2010 Alessandro Tasora
// All rights reserved.
//
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file at the top level of the distribution
// and at http://projectchrono.org/license-chrono.txt.
//


/// Class for Interior-Point methods
/// for QP convex programming


#ifndef CHIPENGINE_H
#define CHIPENGINE_H

///////////////////////////////////////////////////
//
//   ChMklEngine.h
//
//   Use this header if you want to exploit
//   Interior-Point methods
//   from Chrono::Engine programs.
//
//   HEADER file for CHRONO,
//  Multibody dynamics engine
//
// ------------------------------------------------
//             www.deltaknowledge.com
// ------------------------------------------------
///////////////////////////////////////////////////

#include <mkl.h>
#include "chrono_mkl/ChApiMkl.h"
//#include "core/ChMatrixDynamic.h"
#include "chrono_mkl/ChCSR3Matrix.h"
#include "lcp/ChLcpSystemDescriptor.h"
#include "lcp/ChLcpSolver.h"
#include "ChMklEngine.h"

// Interior point methdon based on Numerical Optimization by Nocedal, Wright
// minimize 0.5*xT*G*x + xT*x while Ax>=b (16.54 pag.480)
// WARNING: FOR THE MOMNET THE CONSTRAINTS MUST BE INEQUALITIES

// KKT conditions (16.55 pag.481)
// G*x-AT*lam+c = 0; (dual)
// A*x-y-b = 0; (primal)
// y.*lam = 0 (mixed)
// y>=0
// lam>=0

namespace chrono
{

	class ChApiMkl ChInteriorPoint : public ChLcpSolver
	{
	public:
		enum KKT_SOLUTION_METHOD
		{
			STANDARD,
			AUGMENTED,
			NORMAL
		};

	private:
		size_t m; // size of lam, y, A rows
		size_t n; // size of x, G, A columns
		size_t solver_call;
		size_t iteration_count_max;

		bool EQUAL_STEP_LENGTH;
		bool ADAPTIVE_ETA;
		bool ONLY_PREDICT;
		bool warm_start;

		struct IPresidual_t
		{
			double rp_nnorm;
			double rd_nnorm;
			double rpd_nnorm;

			IPresidual_t() : rp_nnorm(0), rd_nnorm(0), rpd_nnorm(0) {};
			explicit IPresidual_t(double tol) : rp_nnorm(tol), rd_nnorm(tol), rpd_nnorm(tol) {};
		} IPtolerances, IPresiduals;

		double mu_tolerance; // stop iterations if mu under this threshold

		KKT_SOLUTION_METHOD KKT_solve_method;

		// variables: x primal; lam dual, y slack
		ChMatrixDynamic<double> x; // ('q' in chrono)
		ChMatrixDynamic<double> y; // ('c' in chrono)
		ChMatrixDynamic<double> lam; // ('l' in chrono)

		ChMatrixDynamic<double> x_pred;
		ChMatrixDynamic<double> y_pred;
		ChMatrixDynamic<double> lam_pred;

		ChMatrixDynamic<double> x_corr;
		ChMatrixDynamic<double> y_corr;
		ChMatrixDynamic<double> lam_corr;

		ChMatrixDynamic<double> Dx;
		ChMatrixDynamic<double> Dy;
		ChMatrixDynamic<double> Dlam;

		// vectors
		ChMatrixDynamic<double> b; // rhs of constraints (is '-b' in chrono)
		ChMatrixDynamic<double> c; // part of minimization function (is '-f' in chrono)


		// residuals
		ChMatrixDynamic<double> rp; // primal constraint A*x - y - b = 0
		ChMatrixDynamic<double> rd; // dual constraint G*x - AT*lam + c = 0
		ChMatrixDynamic<double> rpd; // primal-dual constraints
		double mu; // complementarity measure


		// problem matrices and vectors
		ChMatrixDynamic<double> rhs;
		ChMatrixDynamic<double> sol;
		ChCSR3Matrix BigMat;
		ChCSR3Matrix SmallMat;
		
		// MKL engine
		ChMklEngine mkl_engine;

		// temporary vectors
		ChMatrixDynamic<double> vectn; // temporary variable that has always size (n,1)
		ChMatrixDynamic<double> vectm; // temporary variable that has always size (m,1)

		// IP specific functions
		void KKTsolve(double sigma = 0.0);
		void initialize(ChLcpSystemDescriptor& sysd);
		void iterate();
		double find_Newton_step_length(ChMatrix<double>& vect, ChMatrix<double>& Dvect, double eta = 1) const;

		// Auxiliary
		void reset_dimensions();
		void fullupdate_residual();
		void make_positive_definite();
		void multiplyA(ChMatrix<double>& vect_in, ChMatrix<double>& vect_out) const;
		void multiplyNegAT(ChMatrix<double>& vect_in, ChMatrix<double>& vect_out) const;
		void multiplyG(ChMatrix<double>& vect_in, ChMatrix<double>& vect_out) const;
		bool check_exit_conditions(bool only_mu = true);
		bool check_feasibility(double tolerance);
		
		// Test
		ChMatrixDynamic<double> sol_chrono;
		void generate_solution();

	public:

		ChInteriorPoint();

		virtual double Solve(ChLcpSystemDescriptor& sysd) override;
		
		// Auxiliary
		void VerifyKKTconditions(bool print = false);
		void SetKKTSolutionMethod(KKT_SOLUTION_METHOD qp_solve_type_selection) { KKT_solve_method = qp_solve_type_selection; }
		void SetMaxIterations(size_t max_iter){ iteration_count_max = max_iter; }
		void SetWarmStart(bool on_off) { warm_start = on_off; }
		void SetTolerances(double rp_tol, double rd_tol, double rpd_tol, double complementarity_tol)
		{
			IPtolerances.rp_nnorm = rp_tol;
			IPtolerances.rd_nnorm = rd_tol;
			IPtolerances.rpd_nnorm = rpd_tol;
			mu_tolerance = complementarity_tol;
		}

		// Test
		void TestAugmentedMatrix();
		void DumpProblem(std::string suffix = "");
		void DumpIPStatus(std::string suffix = "");
	};

} // end of namespace chrono


#endif