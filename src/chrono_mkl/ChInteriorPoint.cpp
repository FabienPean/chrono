﻿#include "ChInteriorPoint.h"
#include <algorithm>

namespace chrono
{

	// | M  -Cq'|*|q|- | f|= |0|
    // | Cq  -E | |l|  |-b|  |c| 

	void ChInteriorPoint::Initialize(ChLcpSystemDescriptor& sysd)
	{
		// Initial resizing;
		if (solver_call == 0)
		{
			// not mandatory, but it speeds up the first build of the matrix, guessing its sparsity; needs to stay BEFORE ConvertToMatrixForm()
			n = sysd.CountActiveVariables();
			m = sysd.CountActiveConstraints(false, true);
	
			reset_dimensions();
		}

		// load system matrix in BigMat and rhs
		switch (qp_solve_type)
		{
		case STANDARD:
			std::cout << std::endl << "Perturbed KKT system cannot be loaded with 'STANDARD' method yet.";
			break;
		case AUGMENTED:
			sysd.ConvertToMatrixForm(&BigMat, nullptr, false, true);
			
			break;
		case NORMAL:
			std::cout << std::endl << "Perturbed KKT system cannot be loaded with 'NORMAL' method yet.";
			sysd.ConvertToMatrixForm(nullptr, nullptr, nullptr, &c, &b, nullptr, false, true);
			break;
			break;
		}

		sysd.ConvertToMatrixForm(nullptr, nullptr, nullptr, &c, &b, nullptr, false, true);
		c.MatrScale(-1);
		b.MatrScale(-1);

		mkl_engine.SetProblem(BigMat, rhs, sol);

		// Starting Point (pag..484-485)
		rpd_pred = y; rpd_pred.MatrScale(lam); // as (16.57 pag.481 suggests)

		{
			ChStreamOutAsciiFile mfileo("rpd_pred2.txt");
			//ChArchiveAsciiDump marchiveout(mfileo);
			mfileo << "inizio!!!! \n";
			mfileo << rpd_pred;
			mfileo << "fine!!!! \n";
			//ChLcpSolver::ArchiveOUT(marchiveout);

			//rpd_pred.StreamOUTdenseMatlabFormat(mfileo)
		}

		KKTsolve(0); // to obtain Dx, Dy, Dlam called "affine"
		y += Dy; // calculate y0
		lam += Dlam; // calculate lam0
		// x0 is equal to x
		
		for (int row_sel = 0; row_sel < m; row_sel++)
			if (abs(y(row_sel)) < 1)
				y(row_sel) = 1;

		for (int row_sel = 0; row_sel < m; row_sel++)
			if (abs(lam(row_sel)) < 1)
				lam(row_sel) = 1;

		switch (qp_solve_type)
		{
		case STANDARD:
			std::cout << std::endl << "rp and rd cannot be updated in 'STANDARD' method yet.";
			break;
		case AUGMENTED:
			// Residual initialization (16.59 pag.482)
			// rp initialization 
			BigMat.MatMultiplyClipped(x, rp, n, n + m - 1, 0, n - 1, 0, 0);  // rp = A*x
			vectm = y; // vectm = y;
			vectm.MatrScale(-1); // vectm = -y;
			vectm.PasteClippedMatrix(&rhs, n, 0, m, 1, 0, 0); // vectm = -y + (-b);
			rp += vectm; // rp = (A*x) + (- y - b);

			// rd initialization
			BigMat.MatMultiplyClipped(x, rd, 0, n - 1, 0, n - 1, 0, 0); // rd = G*x
			rd += c; // rd = G*x + c
			BigMat.MatMultiplyClipped(lam, vectn, 0, n - 1, n, n + m - 1, 0, 0); // vectn = (-A^T)*lam
			rd += vectn; // rd = (G*x + c) + (-A^T*lam)
			break;
		case NORMAL:
			std::cout << std::endl << "rp and rd cannot be updated in 'NORMAL' method yet.";
			break;
		}

		
	}

	void ChInteriorPoint::InteriorPointIterate()
	{
		// Prediction phase
		rpd_pred = y; rpd_pred.MatrScale(lam); // rpd_pred = y°lam; as (16.57 pag.481 suggests)
		KKTsolve(0); // to obtain Dx, Dy, Dlam called "affine" aka "prediction"

		// from 16.60 pag.482 from 14.32 pag.408 (remember that y>=0!)
		double alfa_pred_prim = findNewtonStepLength(y, Dy);
		double alfa_pred_dual = findNewtonStepLength(lam, Dlam);

		if (EQUAL_STEP_LENGTH)
		{
			double alfa_pred = std::min(alfa_pred_prim, alfa_pred_dual);
			alfa_pred_prim = alfa_pred;
			alfa_pred_dual = alfa_pred;
		}

		  y_pred = Dy;     y_pred.MatrScale(alfa_pred_prim);   y_pred += y;
		lam_pred = Dlam; lam_pred.MatrScale(alfa_pred_dual); lam_pred += lam;

		if (ONLY_PREDICT)
		{
			x_pred = Dx;		  x_pred.MatrScale(alfa_pred_prim);		x_pred += x;	  x = x_pred;
			y = y_pred;
			lam = lam_pred;
			
			rp.MatrScale(1 - alfa_pred_prim);
			
			BigMat.MatMultiplyClipped(Dx, vectn, 0, n-1, 0, n-1, 0, 0); // vectn = G * Dx
			vectn.MatrScale(alfa_pred_prim - alfa_pred_dual); // vectn = (alfa_pred_prim - alfa_pred_dual) * (G * Dx)
			rd.MatrScale(1 - alfa_pred_dual);
			rd += vectn;

			return;
		}
		

		// Corrector phase
		double mu_pred = y_pred.MatrDot(&y_pred, &lam_pred)/n; // from 14.33 pag.408 //TODO: why MatrDot is a member?
		double mu = y.MatrDot(&y, &lam)/n; // from 14.6 pag.395
		double sigma = (mu_pred / mu)*(mu_pred / mu)*(mu_pred / mu); // from 14.34 pag.408

		rpd_corr = Dlam; rpd_corr.MatrScale(Dy); rpd_corr.MatrAdd(-sigma*mu); rpd_corr += rpd_pred;
		KKTsolve(sigma*mu);

		double eta = (ADAPTIVE_ETA) ? (exp(-mu*n)*0.1) + 0.9 : 0.95; // exponential descent of eta

		double alfa_corr_prim = findNewtonStepLength(y, Dy, eta);
		double alfa_corr_dual = findNewtonStepLength(lam, Dlam, eta);

		if (EQUAL_STEP_LENGTH)
		{
			double alfa_corr = std::min(alfa_corr_prim, alfa_corr_dual);
			alfa_corr_prim = alfa_corr;
			alfa_corr_dual = alfa_corr;
		}

		x_corr = Dx;		  x_corr.MatrScale(alfa_corr_prim);		  x_corr += x;		  x = x_corr;
		y_corr = Dy;		  y_corr.MatrScale(alfa_corr_prim);		  y_corr += y;		  y = y_corr;
		lam_corr = Dlam;	lam_corr.MatrScale(alfa_corr_dual);		lam_corr += lam;	lam = lam_corr;


		// Update for new cycle
		rp.MatrScale(1 - alfa_corr_prim);

		BigMat.MatMultiplyClipped(Dx, vectn, 0, n - 1, 0, n - 1, 0, 0); // vectn = G*Dx
		vectn.MatrScale(alfa_corr_prim - alfa_corr_dual); // vectn = (alfa_pred_prim - alfa_pred_dual) * (G * Dx)
		rd.MatrScale(1 - alfa_corr_dual);
		rd += vectn;
	}

	void ChInteriorPoint::KKTsolve(const double perturbation)
	{
		switch (qp_solve_type)
		{
		case STANDARD:
			std::cout << std::endl << "Perturbed KKT system cannot be solved with 'STANDARD' method yet.";
			for (int diag_sel = 0; diag_sel < m; diag_sel++)
			{
				BigMat.SetElement(n + m + diag_sel, n + diag_sel, lam.GetElement(diag_sel, 0)); // write lambda diagonal submatrix
				BigMat.SetElement(n + m + diag_sel, n + m + diag_sel, y.GetElement(diag_sel, 0)); // write y diagonal submatrix
			}
			


			break;
		case AUGMENTED:
			// update lambda°y diagonal submatrix
			for (int diag_sel = 0; diag_sel < m; diag_sel++)
			{
				BigMat.SetElement(n + diag_sel, n + diag_sel, lam.GetElement(diag_sel, 0)*y.GetElement(diag_sel, 0));
			}

			BigMat.Compress();

			for (int row_sel = 0; row_sel < n; row_sel++)
				rhs.SetElement(row_sel, 0, -rd.GetElement(row_sel, 0));
			for (int row_sel = 0; row_sel < m; row_sel++)
				rhs.SetElement(row_sel + n, 0, -rp(row_sel, 0) - y(row_sel,0) + perturbation/lam(row_sel,0) );

			BigMat.ExportToDatFile("", 6);

			
			mkl_engine.PardisoCall(13, 0);

			for (int row_sel = 0; row_sel < n; row_sel++)
				Dx.SetElement(row_sel, 0, sol.GetElement(row_sel, 0));
			for (int row_sel = 0; row_sel < m; row_sel++)
				Dlam.SetElement(row_sel, 0, sol.GetElement(row_sel + n, 0));

			BigMat.MatMultiplyClipped(Dx, Dy, n, n + m - 1, 0, n - 1, 0, 0);  // Dy = A*Dx
			Dy += rp;

			break;
		case NORMAL:
			std::cout << std::endl << "Perturbed KKT system cannot be solved with 'NORMAL' method yet.";
			break;
		}
	}

	double ChInteriorPoint::findNewtonStepLength(ChMatrix<double>& vect, ChMatrix<double>& Dvect, double eta )
	{
		double alpha = 1;
		for (int row_sel = 0; row_sel < vect.GetRows(); row_sel++)
		{
			if (Dvect(row_sel)<0)
			{
				double alfa_temp = -eta * vect(row_sel) / Dvect(row_sel);
				if (alfa_temp < alpha)
					alpha = alfa_temp;
			}
		}

		return (alpha>0) ? alpha : 0;
	}

	void ChInteriorPoint::reset_dimensions()
	{
		// variables: (x,y) primal variables; lam dual
		x.Resize(n, 1);
		y.Resize(m, 1);
		lam.Resize(m, 1);
        
		x_pred.Resize(n, 1);
		y_pred.Resize(m, 1);
		lam_pred.Resize(m, 1);
        
		x_corr.Resize(n, 1);
		y_corr.Resize(m, 1);
		lam_corr.Resize(m, 1);
        
		Dx.Resize(n, 1);
		Dy.Resize(m, 1);
		Dlam.Resize(m, 1);

		// vectors
		b.Resize(m, 1); // rhs of constraints (is -b in chrono)
		c.Resize(n, 1); // part of minimization function (is -f in chrono)
        
		// residuals
		rp.Resize(m, 1); // primal constraint A*x - y - b = 0
		rd.Resize(n, 1); // dual constraint G*x - AT*lam + c = 0
		rpd_pred.Resize(m, 1); // primal-dual constraints used in the predictor phase
		rpd_corr.Resize(m, 1); // primal-dual constraints used in the corrector phase

		// temporary vectors
		vectn.Resize(n, 1);; // temporary variable that has always size (n,1)

		// BigMat and sol
		switch (qp_solve_type)
		{
		case STANDARD:
			BigMat.Reset(2 * m + n, 2 * m + n, static_cast<int>(n*n*SPM_DEF_FULLNESS));
			sol.Resize(2 * m + n, 1);
			rhs.Resize(2 * n + m, 1);
			break;
		case AUGMENTED:
			BigMat.Reset(n + m, n + m, static_cast<int>(n*n*SPM_DEF_FULLNESS));
			sol.Resize(n + m, 1);
			rhs.Resize(n + m, 1);
			break;
		case NORMAL:
			std::cout << std::endl << "Perturbed KKT system cannot be stored with 'NORMAL' method yet.";
			break;
		}
	}


	ChInteriorPoint::ChInteriorPoint(QP_SOLUTION_TECHNIQUE qp_solve_type_selection):
		qp_solve_type(qp_solve_type_selection),
		solver_call(0),
		n(0),
		m(0),
		iteration_count_max(10),
		EQUAL_STEP_LENGTH(false),
		ADAPTIVE_ETA(true),
		ONLY_PREDICT(false)
	{

	}

	double ChInteriorPoint::Solve(ChLcpSystemDescriptor& sysd)
	{
		Initialize(sysd);

		for (int iteration_count = 0; iteration_count < iteration_count_max; iteration_count++)
		{
			InteriorPointIterate();
		}

		return 0;
	}

}
