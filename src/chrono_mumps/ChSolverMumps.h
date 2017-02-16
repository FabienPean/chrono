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

#ifndef CHSOLVERMUMPS_H
#define CHSOLVERMUMPS_H

///////////////////////////////////////////////////
//
//   ChSolverMumps.h
//
//   Use this header if you want to exploit Intel�
//	 MUMPS Library
//   from Chrono::Engine programs.
//
//   HEADER file for CHRONO,
//   Multibody dynamics engine
//
// ------------------------------------------------
//             www.deltaknowledge.com
// ------------------------------------------------
///////////////////////////////////////////////////



#include "chrono_mumps/ChApiMumps.h"
#include "core/ChMatrix.h"
#include "chrono/solver/ChSystemDescriptor.h"
#include "chrono/solver/ChSolver.h"
#include "chrono_mumps/ChMumpsEngine.h"
#include <core/ChTimer.h>

//#include <process.h>



namespace chrono {


    /// Class that wraps the Intel MUMPS 'PARDISO' parallel direct solver.
    /// It can solve linear systems. It cannot solve VI and complementarity problems.
    
   class ChApiMumps ChSolverMumps : public ChSolver {

   public:

	   ChSolverMumps(){}
	   virtual ~ChSolverMumps(){}

       /// Reset timers for internal phases in Solve and Setup.
       void ResetTimers() {
           m_timer_setup_assembly.reset();
           m_timer_setup_solvercall.reset();
           m_timer_solve_assembly.reset();
           m_timer_solve_solvercall.reset();
       }
       
       /// Enable/disable locking the sparsity pattern (default: false).
       /// If \a val is set to true, then the sparsity pattern of the problem matrix is assumed
       /// to be unchanged from call to call.
       void SetSparsityPatternLock(bool val)
       {
           m_lock = val;
           m_mat.SetSparsityPatternLock(m_lock);
       }

       /// Call an update of the sparsiy pattern on the underlying matrix.
       /// It is used to inform the solver (and the underlying matrices) that the sparsity pattern is changed.
       /// It is suggested to call this function just after the construction of the solver.
       void ForceSparsityPatternUpdate(bool val = true)
       {
           m_force_sparsity_pattern_update = val;
       }

       void SetNullPivotDetection(bool val, double threshold = 0)
       {
           m_null_pivot_detection = val;
           m_engine.SetNullPivotDetection(val, threshold);
       }

       /// Get cumulative time for assembly operations in Solve phase.
       double GetTimeSolve_Assembly() const { return m_timer_solve_assembly(); }
       /// Get cumulative time for Pardiso calls in Solve phase.
       double GetTimeSolve_SolverCall() const { return m_timer_solve_solvercall(); }
       /// Get cumulative time for assembly operations in Setup phase.
       double GetTimeSetup_Assembly() const { return m_timer_setup_assembly(); }
       /// Get cumulative time for Pardiso calls in Setup phase.
       double GetTimeSetup_SolverCall() const { return m_timer_setup_solvercall(); }

       bool Setup(ChSystemDescriptor& sysd) override;

       /// Solve using MUMPS
       double Solve(ChSystemDescriptor& sysd) override;  ///< system description with constraints and variables

       bool SolveRequiresMatrix() const override { return false; }

     private:

       ChMumpsEngine m_engine;                         ///< interface to Mumps solver
       ChCOOMatrix m_mat = ChCOOMatrix(1, 1, true);    ///< problem matrix
       ChMatrixDynamic<double> m_rhs_sol;              ///< right-hand side vector (will be overridden by solution)
       ChMatrixDynamic<double> m_rhs_bkp;                  ///< solution vector

       int m_dim = 0;                      ///< problem size
       int m_nnz = 0;                      ///< user-supplied estimate of NNZ
       int m_solve_call = 0;               ///< counter for calls to Solve
       int m_setup_call = 0;               ///< counter for calls to Setup

       bool m_lock = false;              ///< is the matrix sparsity pattern locked?
       bool m_force_sparsity_pattern_update = false; ///< is the sparsity pattern changed compared to last call?
       bool m_use_perm = false;          ///< enable use of the permutation vector?
       bool m_use_rhs_sparsity = false;  ///< leverage right-hand side sparsity?
       bool m_null_pivot_detection = false;  ///< leverage right-hand side sparsity?

       ChTimer<> m_timer_setup_assembly;  ///< timer for matrix assembly
       ChTimer<> m_timer_setup_solvercall;   ///< timer for factorization
       ChTimer<> m_timer_solve_assembly;  ///< timer for RHS assembly
       ChTimer<> m_timer_solve_solvercall;   ///< timer for solution

   };

}  // END_OF_NAMESPACE____

#endif