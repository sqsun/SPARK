#include <fstream>
#include <RcppArmadillo.h>
// [[Rcpp::depends(RcppArmadillo)]]
#include <R.h>
#include <Rmath.h>
#include <cmath>
#include <stdio.h>
#include <stdlib.h>
#include <cstring>
//#include <omp.h>


using namespace std;
using namespace arma;
using namespace Rcpp;


//' Do inverse of sysmetric matrix 
//' @param Min A sysmetric matrix
//' 
//' @return A list
//' 
//' @export
// [[Rcpp::export]]
SEXP SysMatEigen(SEXP Min) {
try {
		arma::fmat M = as<fmat>(Min);
		arma::fvec eigval = zeros<fvec>( M.n_rows );
		arma::fmat eigvec = zeros<fmat>( size(M) );
		eig_sym(eigval, eigvec, M, "dc");
		const uvec idx = find(eigval < 1e-8 );
		arma::fvec tmp_value = ones<arma::fvec>(idx.n_elem);
		eigval.elem( idx ) = tmp_value * 1e-8;
		arma::fmat M1 = eigvec.each_row() % eigval.t();
		M = M1 * eigvec.t();
		// return values
		return List::create(Named("eigval") = eigval, Named("eigvec") = eigvec, Named("kernel_mat") = M);
		//return List::create(Named("eigval") = eigval, Named("eigvec") = eigvec);
	}
	catch (std::exception &ex)
	{
		forward_exception_to_r(ex);
	}
	catch (...)
	{
		::Rf_error("C++ exception (unknown reason)...");
	}
	return R_NilValue;
}// end func


//' Do inverse of sparse sysmetric matrix 
//' @param Min A sysmetric matrix
//' @param num_topin The number of top eigen values
//' 
//' @return A list
//' 
//' @export
// [[Rcpp::export]]
SEXP SparseSysMatEigen(SEXP Min, SEXP num_topin) {
try {
		arma::sp_mat M = as<sp_mat>(Min);
		int num_top = Rcpp::as<int>(num_topin);
		arma::vec eigval = zeros<vec>( num_top );
		arma::mat eigvec = zeros<mat>(M.n_rows, num_top );
		eigs_sym(eigval, eigvec, M, num_top);
		const uvec idx = find(eigval < 1e-8 );
		arma::vec tmp_value = ones<arma::vec>(idx.n_elem);
		eigval.elem( idx ) = tmp_value * 1e-8;
		arma::mat M1 = eigvec.each_row() % eigval.t();
		M = M1 * eigvec.t();
		// return values
		return List::create(Named("eigval") = eigval, Named("eigvec") = eigvec, Named("kernel_mat") = M);
		//return List::create(Named("eigval") = eigval, Named("eigvec") = eigvec);
	}
	catch (std::exception &ex)
	{
		forward_exception_to_r(ex);
	}
	catch (...)
	{
		::Rf_error("C++ exception (unknown reason)...");
	}
	return R_NilValue;
}// end func


//' Variance component estimation with covariates using Average Information algorithm
//' @param Yin Working vector
//' @param Xin Covariate matrix
//' @param Din Weight for each gene
//' @param tauin Initial value for variance component
//' @param fixtauin Variance component to be optimized
//' @param tolin Tolerance
//' 
//' @return A list
//' 
//' @export
// [[Rcpp::export]]
SEXP CovariatesAI(SEXP Yin, SEXP Xin, SEXP Din, SEXP tauin, SEXP fixtauin, SEXP tolin) { /*Average Information*/
	try	{
		arma::fvec Y = as<fvec>(Yin);
		arma::fmat X = as<fmat>(Xin);
		arma::fvec D = as<fvec>(Din);
		arma::fvec tau = as<fvec>(tauin);
		const uvec fixtau = as<uvec>(fixtauin);
		const int num_cov_mat2 = sum(fixtau == 0);
		const double tol = Rcpp::as<double>(tolin);
		uvec ZERO = (tau < tol);

		const int num_cell = X.n_rows;
		const int num_cvt = X.n_cols; // if number of column X isnot equal to 1
		arma::fvec Hinv(num_cell);
		arma::fvec one_vec = ones<fvec>(num_cell);

		Hinv = tau(0) * (1.0 / (D + 1e-5));
		Hinv += tau(1) * one_vec;
		Hinv = 1.0 / (Hinv + 1e-5);
		arma::fvec HinvY = Hinv % Y;
		arma::fmat HinvX = X.each_col() % Hinv;
		arma::fmat XtHinvX = X.t() * HinvX;
		arma::fmat XtHinvX_inv = inv_sympd(XtHinvX);

		arma::fmat P = diagmat(Hinv) - HinvX * XtHinvX_inv * HinvX.t();

		arma::fvec alpha = XtHinvX_inv * HinvX.t() * Y;
		arma::fvec eta = Y - tau(0) * (HinvY - HinvX * alpha) / D;
		arma::fvec PY = P * Y;

		if (num_cov_mat2 > 0) {
			const uvec idxtau = find(fixtau == 0);
			arma::fmat AImat(num_cov_mat2, num_cov_mat2); //average information matrix
														 //arma::vec PY = P * Y;
			arma::fvec score(num_cov_mat2), PAPY;
			for (size_t i = 0; i < num_cov_mat2; i++) {
				PAPY = P * PY;
				score(i) = dot(Y, PAPY) - sum(P.diag());
				for (size_t j = 0; j <= i; j++)	{
					AImat(i, j) = dot(PY, PAPY);
					if (j != i)	{
						AImat(j, i) = AImat(i, j);
					} // end fi
				}	 //end for j
			}		  // end for i

			arma::fvec Dtau = solve(AImat, score);
			arma::fvec tau0 = tau;

			tau.elem(idxtau) = tau0.elem(idxtau) + Dtau;

			tau.elem(find(ZERO % (tau < tol))).zeros();
			double step = 1.0;
			while (any(tau < 0.0)) {
				step *= 0.5;
				tau.elem(idxtau) = tau0.elem(idxtau) + step * Dtau;
				tau.elem(find(ZERO % (tau < tol))).zeros();
			}
			tau.elem(find(tau < tol)).zeros();
		} // end fi
		// boundary tau 0<= tau <=10
		// tau.elem(find(tau >10.0)).ones();
		// return values
		return List::create(Named("tau") = tau, Named("P") = P, Named("cov") = XtHinvX_inv,	Named("alpha") = alpha, Named("Py") = PY, Named("eta") = eta);
	}
	catch (std::exception &ex)
	{
		forward_exception_to_r(ex);
	}
	catch (...)
	{
		::Rf_error("C++ exception (unknown reason)...");
	}
	return R_NilValue;
} // end funcs


//' Variance component estimation without covariates using Average Information algorithm, float format
//' @param Yin Working vector
//' @param Xin Covariate matrix
//' @param Din Weight for each gene
//' @param tauin Initial value for variance component
//' @param fixtauin Variance component to be optimized
//' @param tolin Tolerance
//' 
//' @return A list
//' 
//' 
//' @export
// [[Rcpp::export]]
SEXP noCovariatesAI(SEXP Yin, SEXP Xin, SEXP Din, SEXP tauin, SEXP fixtauin, SEXP tolin) { /*Average Information*/
	try	{
		arma::fvec Y = as<fvec>(Yin);
		arma::fmat X = as<fmat>(Xin);
		arma::fvec D = as<fvec>(Din);
		arma::fvec tau = as<fvec>(tauin);
		const uvec fixtau = as<uvec>(fixtauin);
		const int num_cov_mat2 = sum(fixtau == 0);
		const double tol = Rcpp::as<double>(tolin);
		uvec ZERO = (tau < tol);

		const int num_cell = X.n_rows;
		const int num_cvt = X.n_cols; // only suitable for intercept case

		arma::fvec Hinv(num_cell);
		arma::fvec one_vec = ones<fvec>(num_cell);


		Hinv = tau(0) * (1.0 / (D + 1e-5));
		Hinv += tau(1) * one_vec;
		Hinv = 1.0 / (Hinv + 1e-5);

		arma::fvec HinvY = Hinv % Y;

		arma::fvec HinvX = Hinv;
		double XtHinvX = sum(HinvX);
		double XtHinvX_inv = 1.0 / XtHinvX;
		arma::fvec P_diag = Hinv - (HinvX % HinvX) * XtHinvX_inv;
		double alpha = XtHinvX_inv * dot(HinvX, Y);
		arma::fvec eta = Y - tau(0) * (HinvY - HinvX * alpha) / D;

		arma::fvec PY = HinvY - HinvX * XtHinvX_inv * (HinvX.t() * Y);


		if (num_cov_mat2 > 0) {
			const uvec idxtau = find(fixtau == 0);
			arma::fmat AImat(num_cov_mat2, num_cov_mat2); //average information matrix
														 //arma::vec PY = P * Y;
			arma::fvec score(num_cov_mat2);
			for (size_t i = 0; i < num_cov_mat2; i++) {
				
				arma::fvec PAPY = Hinv % PY - HinvX * XtHinvX_inv * (HinvX.t() * PY);
	
				score(i) = dot(Y, PAPY) - sum(P_diag);
				for (size_t j = 0; j <= i; j++)	{
					AImat(i, j) = dot(PY, PAPY);
					if (j != i)	{
						AImat(j, i) = AImat(i, j);
					} // end fi
				}	 //end for j
			}		  // end for i

			arma::fvec Dtau = solve(AImat, score);
			arma::fvec tau0 = tau;

			tau.elem(idxtau) = tau0.elem(idxtau) + Dtau;
			tau.elem(find(ZERO % (tau < tol))).zeros();
			double step = 1.0;
			while (any(tau < 0.0)) {
				step *= 0.5;
				tau.elem(idxtau) = tau0.elem(idxtau) + step * Dtau;
				tau.elem(find(ZERO % (tau < tol))).zeros();
			} // end while
			tau.elem(find(tau < tol)).zeros();
		} // end fi
		// boundary tau 0<= tau <=10
		//tau.elem(find(tau >10.0)).ones();
		// return values

		return List::create(Named("tau") = tau, Named("Py") = PY, Named("cov") = XtHinvX_inv,	Named("alpha") = alpha, Named("eta") = eta);
	} // end try
	catch (std::exception &ex)
	{
		forward_exception_to_r(ex);
	}
	catch (...)
	{
		::Rf_error("C++ exception (unknown reason)...");
	}
	return R_NilValue;
} // end funcs




//' Compute the testing quantities without covariates, float format
//' @param yin Working vector
//' @param Pyin The vector P*y
//' @param cov_matin Kernel matrix to be tested
//' @param Din Weight for each gene
//' @param tauin Initial value for variance component
//' 
//' @return A list
//' 
//' 
//' @export
// [[Rcpp::export]]
SEXP ComputeTestQuantRcpp_nocov(SEXP yin, SEXP Pyin, SEXP cov_matin, SEXP Din, SEXP tauin)
{
	try
	{
		arma::fvec y = as<fvec>(yin);
		arma::fvec Py = as<fvec>(Pyin);
		arma::fmat cov_mat = as<fmat>(cov_matin);
		arma::fvec D = as<fvec>(Din);
		arma::fvec tau = as<fvec>(tauin);

		const int num_cell = y.n_elem;
		arma::fvec Hinv(num_cell);
		arma::fvec one_vec = ones<fvec>(num_cell);

		Hinv = tau(0) * (1.0 / (D + 1e-5));
		Hinv += tau(1) * one_vec;
		Hinv = 1.0 / (Hinv + 1e-5); // Hinv is a diagonal matrix
		arma::fvec Hinvy = Hinv % y;

		arma::fvec HinvX = Hinv;
		double XtHinvX = sum(HinvX);
	
		arma::fmat P = - arma::kron(HinvX, HinvX.t())/XtHinvX;
		P.diag() = P.diag() + Hinv;

		arma::frowvec PKp2 = HinvX.t()*cov_mat;

		arma::fmat PK = cov_mat.each_col() % HinvX - arma::kron(HinvX, PKp2)/XtHinvX;

		double trace_PKP = accu(PK % P);
		double newInfoM_p1 = 0.5 * trace(PK * PK);
		double newInfoM = newInfoM_p1 - 0.5 * trace_PKP*trace_PKP/accu(P % P);
		double ee = trace(PK) / 2.0;
		double kk = newInfoM / (2.0 * ee);
		double df = 2.0 * ee * ee / newInfoM;

		arma::fvec PKPy = PK * Py;

		double S0 = 0.5 * dot(y, PKPy);
		//cout<<"S0 = " << S0 <<endl;
		// return values
		return List::create(Named("S0") = S0, Named("ee") = ee, Named("infoMp1") = newInfoM_p1, Named("df") = df, Named("kk") = kk);
	} // end try
	catch (std::exception &ex)
	{
		forward_exception_to_r(ex);
	}
	catch (...)
	{
		::Rf_error("C++ exception (unknown reason)...");
	}
	return R_NilValue;
} // end funcs



//' Compute the testing quantities with covariates, float format
//' @param yin Working vector
//' @param Pyin The vector P*y
//' @param Xin Covariate matrix, including the intercept
//' @param cov_matin Kernel matrix to be tested
//' @param Din Weight for each gene
//' @param tauin Initial value for variance component
//' 
//' @return A list
//' 
//' 
//' @export
// [[Rcpp::export]]
SEXP ComputeTestQuantRcpp_cov(SEXP yin, SEXP Pyin, SEXP Xin, SEXP cov_matin, SEXP Din, SEXP tauin)
{
  try
  {
    arma::fvec y = as<fvec>(yin);
    arma::fvec Py = as<fvec>(Pyin);
    arma::fmat cov_mat = as<fmat>(cov_matin);
    arma::fmat X = as<fmat>(Xin);
    arma::fvec D = as<fvec>(Din);
    arma::fvec tau = as<fvec>(tauin);
    
    const int num_cell = y.n_elem;
    arma::fvec Hinv(num_cell);
    arma::fvec one_vec = ones<fvec>(num_cell);
    
    Hinv = tau(0) * (1.0 / (D + 1e-5));
    Hinv += tau(1) * one_vec;
    Hinv = 1.0 / (Hinv + 1e-5); // Hinv is a diagonal matrix
    arma::fvec Hinvy = Hinv % y;
    arma::fmat HinvX = X.each_col() % Hinv;
    arma::fmat XtHinvX = X.t() * HinvX;
    arma::fmat XtHinvX_inv = inv_sympd(XtHinvX);
    arma::fmat P = diagmat(Hinv) - HinvX * XtHinvX_inv * HinvX.t();

    // modified by sun, 2019-4-13 16:25:06
    arma::fmat PK = P*cov_mat;
    double trace_PKP = accu(PK % P);

    // modified by sun, 2019-4-9 12:26:03
    double newInfoM_p1 = 0.5 * trace(PK * PK);
    double newInfoM = newInfoM_p1 - 0.5 * trace_PKP*trace_PKP/accu(P % P);
    double ee = trace(PK) / 2.0;
    double kk = newInfoM / (2.0 * ee);
    double df = 2.0 * ee * ee / newInfoM;
    arma::fvec PKPy = PK * Py;

    double S0 = 0.5 * dot(y, PKPy);
    double ll = 0.0;
    
    // return values
    return List::create(Named("S0") = S0, Named("ee") = ee, Named("infoMp1") = newInfoM_p1, Named("df") = df, Named("kk") = kk);
  } // end try
  catch (std::exception &ex)
  {
    forward_exception_to_r(ex);
  }
  catch (...)
  {
    ::Rf_error("C++ exception (unknown reason)...");
  }
  return R_NilValue;
} // end funcs


//' Compute the testing quantities for linear mixed model, float format
//' @param yin Working vector
//' @param Pyin The vector P*y
//' @param Xin Covariate matrix, including the intercept
//' @param cov_matin Kernel matrix to be tested
//' @param Din Weight for each gene
//' @param tauin Initial value for variance component
//' 
//' @return A list
//' 
//' 
//' @export
// [[Rcpp::export]]
SEXP ComputeTestQuantRcpp_Gaussian(SEXP yin, SEXP Pyin, SEXP Xin, SEXP cov_matin, SEXP Din, SEXP tauin) {
  try {
    arma::fvec y = as<fvec>(yin);
    arma::fvec Py = as<fvec>(Pyin);
    arma::fmat cov_mat = as<fmat>(cov_matin);
    arma::fmat X = as<fmat>(Xin);
    arma::fvec D = as<fvec>(Din);
    arma::fvec tau = as<fvec>(tauin);
    
    const int num_cell = y.n_elem;
    arma::fvec Hinv(num_cell);
    arma::fvec one_vec = ones<fvec>(num_cell);
    
    Hinv = tau(0) * (1.0 / (D + 1e-5));
    Hinv += tau(1) * one_vec;
    Hinv = 1.0 / (Hinv + 1e-5); // Hinv is a diagonal matrix
    arma::fvec Hinvy = Hinv % y;
    arma::fmat HinvX = X.each_col() % Hinv;
    arma::fmat XtHinvX = X.t() * HinvX;
    arma::fmat XtHinvX_inv = inv_sympd(XtHinvX);
    arma::fmat P = diagmat(Hinv) - HinvX * XtHinvX_inv * HinvX.t();

    // modified by sun, 2019-8-13 11:17:35
	// compute score, i.e., first derivatives
	arma::fvec score = zeros<fvec>(2);
    arma::fmat PK = P*cov_mat; 
	arma::fvec PKPy = PK * Py;
	score(0) = 0.5 * (dot(y, PKPy) - trace(PK));
	score(1) = 0.5 * (dot(Py, Py) - trace(P));
   //double score = 0.5 * (dot(y, PKPy) - trace(PK));

    // modified by sun, 2019-8-13 11:17:58
	// compute variance, i.e., second derivatives
	arma::fmat Var = zeros<fmat>(2, 2);
	Var(0, 0) = 0.5*dot(y, PK*PKPy);
	Var(0, 1) = 0.5*dot(y, PK*P*Py);
	Var(1, 0) = 0.5*dot(y, P*PK*Py);
	Var(1, 1) = 0.5*dot(y, P*P*Py);

	//Var(0, 0) = dot(y, PK*PKPy) - 0.5 * trace(cov_mat*P*cov_mat*P);
	//Var(0, 1) = dot(y, PK*P*Py) - 0.5 * trace(cov_mat*P*P);
	//Var(1, 0) = dot(y, P*PK*Py) - 0.5 * trace(P*cov_mat*P);
	//Var(1, 1) = dot(y, P*P*Py) - 0.5 * trace(P*P);
	//double Var = 0.5 * dot(y, PK*PKPy);
	arma::fmat Var_inv = pinv(Var);
	//double Var =  dot(y, PK*PKPy) - 0.5 * trace(cov_mat*P*cov_mat*P);

	// score statistics
    double Ts = dot(score, Var_inv*score);
 
    // return values
    return List::create(Named("Ts") = Ts, Named("score") = score, Named("Var") = Var);
  } // end try
  catch (std::exception &ex)
  {
    forward_exception_to_r(ex);
  }
  catch (...)
  {
    ::Rf_error("C++ exception (unknown reason)...");
  }
  return R_NilValue;
} // end funcs


//' Compute the kernel weights using psedu-counts
//' @param Yin Working vector
//' @param num_kernelin The number of kernel matrices,  10
//' @param Phiin The kernel matrices and identity matrix,  10 + 1 
//' 
//' @return A list
//' 
//' @export
// [[Rcpp::export]]
SEXP ComputeWeightsRcpp(SEXP Yin, SEXP num_kernelin, SEXP Phiin) {
	try	{
		arma::fmat Y = as<fmat>(Yin);
		const int num_kernel = Rcpp::as<int>(num_kernelin); // number of kernels inputs
		const Rcpp::List Phi(Phiin); // kernel matrix
	
		const int num_gene = Y.n_cols;
		const int num_cell = Y.n_rows; // 
	
		//arma::fcube PHI(num_cell, num_gene, num_kernel);

		arma::fmat Q = zeros<fmat>(num_kernel, num_kernel);
		for(size_t i=1; i<=num_kernel; i++) {
		    stringstream kinsi;
			kinsi << "kernel" << i;
			for(size_t j=i; j<=num_kernel; j++){
				stringstream kinsj;
				kinsj << "kernel" << j;
				Q(i-1, j-1) = accu(symmatl(as<fmat>(Phi[kinsi.str()])) % symmatl(as<fmat>(Phi[kinsj.str()])) );
				if(i != j){
					Q(j-1, i-1) = Q(i-1, j-1);
				}// end fi
			}// end for
		}// end for
		//cout<<"Q="<<Q<<endl;
		arma::fvec y = zeros<fvec>(num_cell);
		arma::fvec x = zeros<fvec>(num_kernel);
		arma::fmat weights = zeros<fmat>(num_kernel, num_gene);
		//arma::fmat Q_inv = inv_sympd( Q );
		arma::fmat Q_inv = pinv( Q );
		for(size_t ig=0; ig<num_gene; ig++) {
			y = Y.col(ig);
			for(size_t i=1; i<=num_kernel; i++) {
				stringstream kins;
				kins << "kernel" << i;
				x(i-1) =  dot(y, symmatl(as<fmat>(Phi[kins.str()])) * y);
			}// end for
			weights.col(ig) = Q_inv * x;
		}// end for

		
		// boundary tau 0<= tau <=10
		// tau.elem(find(tau >10.0)).ones();
		//arma::fmat AImat_inv = inv_sympd(AImat);
		// return values
		return List::create(Named("weights") = weights.t());
	}
	catch (std::exception &ex)
	{
		forward_exception_to_r(ex);
	}
	catch (...)
	{
		::Rf_error("C++ exception (unknown reason)...");
	}
	return R_NilValue;
} // end funcs

///////////////////////////////////////////////////////////////////////////////////////////
////                             CODE END HERE                                           //
///////////////////////////////////////////////////////////////////////////////////////////
