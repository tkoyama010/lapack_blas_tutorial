/*************************************************/
/* LAPACK/BLAS Tutorial                          */
/* Jacobi Iterative Refinement with Sparse BLAS  */
/* Last Update: 2015-04-16 (Thu) T.Kouya         */
/*************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "tkaux.h"

#include "mkl.h"		// Sparse BLAS in Intel Math Kernel
#include "mm/matrix_market_io.h" 	// Matrix Market(MTX) I/O

// relative difference
double reldiff_dvector(double *vec1, double *vec2, int dim)
{
	double *tmp_vec;
	double ret, norm;

	tmp_vec = (double *)calloc(dim, sizeof(double));

	cblas_dcopy(dim, vec1, 1, tmp_vec, 1);
	cblas_daxpy(dim, -1.0, vec2, 1, tmp_vec, 1);
	ret = cblas_dnrm2(dim, tmp_vec, 1);
	norm = cblas_dnrm2(dim, vec1, 1);

	if(norm != 0.0)
		ret /= norm;

	return ret;
}


// BiCGSTAB
int bicgstab_coo(double *answer, double *mat_a_val, int *mat_a_row_index, int *mat_a_col_index, int *mat_a_num_nonzeros, double *vec_b, double reps, double aeps, double history_norm2[], int dim, int maxtimes)
{
	long int i, j, times, return_val;
	double alpha, alpha_num, alpha_den;
	double beta, beta_num;
	double rho, old_rho;
	double omega, omega_den;
	double dtmp, init_resnorm;
	double *vec[9]; /* Temporary Vectors */

//	printf("vec_b: \n"); printf_dvector("%3d %25.17e\n", vec_b, dim, 1);

/* Set initial value */
	for(i = 0; i < 9; i++)
	{
		vec[i] = (double *)calloc(dim, sizeof(double));
		set0_dvector(vec[i], dim, 1);
	}

	/* vec[0] ... approximation of solution */
	/* vec[1] ... residual : b - a * vec[0]  == r */
	/* vec[2] ... (b - a * vec[0])^T  == r^t */
	/* vec[3] ... p */
	/* vec[4] ... p^T */
	/* vec[5] ... v */
	/* vec[6] ... s */
	/* vec[7] ... s^T */
	/* vec[8] ... t */

	cblas_dcopy(dim, vec_b, 1, vec[1], 1); 
	cblas_dcopy(dim, vec_b, 1, vec[2], 1);

	beta_num = cblas_ddot(dim, vec[1], 1, vec[1], 1);
	init_resnorm = sqrt(beta_num);

	old_rho = 0.0;
	rho = 0.0;
	return_val = 0;

/* Main loop */
	for(times = 0; times < maxtimes; times++)
	{
		/* rho */
		rho = cblas_ddot(dim, vec[2], 1, vec[1], 1);

		if(rho == 0.0)
		{
			fprintf(stderr, "Rho is zero!(bicgstab, %ld)\n", times);
			return_val= -1; // Fix!
			break; // Fix!
		}

		if(times == 0)
		{
			/* p := r */
			cblas_dcopy(dim, vec[1], 1, vec[3], 1);
		}
		else
		{
			beta = (rho / old_rho) * (alpha / omega);

			/* p := r + beta * (p - omega * v) */
			//add_cmul_dvector(vec[4], vec[3], -omega, vec[5]);
			//add_cmul_dvector(vec[3], vec[1], beta, vec[4]);

			cblas_dcopy(dim, vec[3], 1, vec[4], 1);
			cblas_daxpy(dim, -omega, vec[5], 1, vec[4], 1);
			cblas_dcopy(dim, vec[1], 1, vec[3], 1);
			cblas_daxpy(dim, beta, vec[4], 1, vec[3], 1);
		}
		/* precondition */
		
		/* v := Apt */
		//mul_drsmatrix_dvec(vec[5], a, vec[3]);
		mkl_cspblas_dcoogemv("N", &dim, mat_a_val, mat_a_row_index, mat_a_col_index, mat_a_num_nonzeros, vec[3], vec[5]);

		alpha_den = cblas_ddot(dim, vec[2], 1, vec[5], 1);
		if(alpha_den == 0.0)
		{
			fprintf(stderr, "Denominator of Alpha is zero!(bicgstab, %ld)\n", times);
			return_val = -2; // Fix!
			break; // Fix!
		}
		alpha = rho / alpha_den;

		/* s = r - alpha v */
		//add_cmul_dvector(vec[6], vec[1], -alpha, vec[5]);
		cblas_dcopy(dim, vec[1], 1, vec[6], 1);
		cblas_daxpy(dim, -alpha, vec[5], 1, vec[6], 1);

		/* Stopping Criteria */
		dtmp = cblas_dnrm2(dim, vec[6], 1);
		if(dtmp <= aeps + reps * init_resnorm)
		{
			/* x = x + alpha pt */
			//add_cmul_dvector(vec[0], vec[0], alpha, vec[3]);
			cblas_daxpy(dim, alpha, vec[3], 1, vec[0], 1);

			cblas_dcopy(dim, vec[0], 1, answer, 1);
			return_val = times; // Fix!
			break; // Fix!
		}

		/* precondition */
		//mul_drsmatrix_dvec(vec[8], a, vec[6]);
		mkl_cspblas_dcoogemv("N", &dim, mat_a_val, mat_a_row_index, mat_a_col_index, mat_a_num_nonzeros, vec[6], vec[8]);

		/* omega = (t, s) / (t, t) */
		omega_den = cblas_ddot(dim, vec[8], 1, vec[8], 1);
		if(omega_den == 0)
		{
			fprintf(stderr, "Denominator of Omega is zero!(bicgstab, %ld)\n", times);
			return_val = -3; // Fix!
			break;
		}
		omega = cblas_ddot(dim, vec[8], 1, vec[6], 1);
		if(omega == 0)
		{
			fprintf(stderr, "Numerator of Omega is zero!(bicgstab, %ld)\n", times);
			return_val = -4; // Fix!
			break; // Fix!
		}
		omega = omega / omega_den;

		/* x = x + alpha pt + omega st */
		//add_cmul_dvector(vec[4], vec[0], alpha, vec[3]);
		//add_cmul_dvector(vec[0], vec[4], omega, vec[6]);
		cblas_daxpy(dim, alpha, vec[3], 1, vec[0], 1);
		cblas_daxpy(dim, omega, vec[6], 1, vec[0], 1);

		/* residual */
		//add_cmul_dvector(vec[1], vec[6], -omega, vec[8]);
		cblas_dcopy(dim, vec[6], 1, vec[1], 1);
		cblas_daxpy(dim, -omega, vec[8], 1, vec[1], 1);

		beta_num = cblas_ddot(dim, vec[1], 1, vec[1], 1);

		/* Stopping Criteria */
		dtmp = sqrt(beta_num);

		if(history_norm2 != NULL)
			history_norm2[times] = dtmp / init_resnorm;

		if(dtmp <= aeps + reps * init_resnorm)
		{
			cblas_dcopy(dim, vec[0], 1, answer, 1);
			return_val = times; // Fix!
			break; // Fix!
		}

		old_rho = rho;
	}

	/* Not converge */
	cblas_dcopy(dim, vec[0], 1, answer, 1);

	/* free vec[0]..[8]; */
	for(i = 0; i < 9; i++)
		free(vec[i]);

	// Fix!
	if(times >= maxtimes)
	{
		fprintf(stderr, "Not converge!(bicgstab, %ld)\n", times);
		return_val = -5;
	}

	return return_val; // Fix!

}

int main()
{
	int i, j, itimes;
	int row_dim, col_dim, dim, maxtimes;
	double *md, *vy, *vx, *vx_true, *vx_new, *vb, *his_norm2_res;
	double reps, aeps;

	// A: sparse matrix
	unsigned char *mtx_fname;
	// 疎行列
	int ma_num_nonzero;
	double *ma_val;
	int *ma_row_index, *ma_col_index;
	MM_typecode matcode;

	// input mtx file
	//mtx_fname = "mm/b1_ss/b1_ss.mtx"; // (1,1) == (0, 0) element lack!
	//mtx_fname = "mm/sptest.mtx"; // all diagonal elements are found!
	//mtx_fname = "mm/diagtest.mtx"; // Diagonal matrix
	//mtx_fname = "mm/cage4/cage4.mtx"; // all diagonal elements are found!
	//mtx_fname = "mm/cavity10/cavity10.mtx"; // all diagonal elements are found!
	//mtx_fname = "mm/rdb3200l/rdb3200l.mtx";
	//mtx_fname = "mm/circuit_2/circuit_2.mtx";
	mtx_fname = "mm/t2d_q4/t2d_q4.mtx";
	//mtx_fname = "mm/circuit5M/circuit5M.mtx"; // exceed 2GB!

	// ヘッダ情報を表示(ファイルが読めるかどうか一度チェックする)
	if(mm_print_header_mtx_crd(mtx_fname, 100) != MM_SUCCESS)
		return EXIT_FAILURE;

	// read mtx file as coordinate format
	mm_read_mtx_crd(mtx_fname, &row_dim, &col_dim, &ma_num_nonzero, &ma_row_index, &ma_col_index, &ma_val, &matcode);

	// square matrix?
	if(row_dim != col_dim)
	{
		fprintf(stderr, "ERROR: ma is not square matrix(row_dim, col_dim = %d, %d)\n", row_dim, col_dim);
		return EXIT_FAILURE;
	}

	dim = row_dim;

	// Initialize
	vy = (double *)calloc(dim, sizeof(double));
	vx = (double *)calloc(dim, sizeof(double));
	vx_new = (double *)calloc(dim, sizeof(double));
	vx_true = (double *)calloc(dim, sizeof(double));
	vb = (double *)calloc(dim, sizeof(double));

	// input ma and vx_true
	for(i = 0; i < dim; i++)
		vx_true[i] = (double)(i + 1);

	// 1-based index -> 0-based index
	for(i = 0; i < ma_num_nonzero; i++)
	{
		--ma_row_index[i];
		--ma_col_index[i];
	}

	// 行列・ベクトル積(SpMV)
	// vb := 1.0 * ma * vx_true
	mkl_cspblas_dcoogemv("N", &dim, ma_val, ma_row_index, ma_col_index, &ma_num_nonzero, vx_true, vb);

	// print vb
/*	printf("b := A * x\n");
	for(i = 0; i < dim; i++)
		printf("%d %25.17e\n", i, vb[i]);
*/

	reps = 1.0e-10;
	aeps = 0.0;

	maxtimes = dim * 10;
	his_norm2_res = (double *)calloc(maxtimes, sizeof(double));

	// BiCGSTAB
	itimes = bicgstab_coo(vx, ma_val, ma_row_index, ma_col_index, &ma_num_nonzero, vb, reps, aeps, his_norm2_res, dim, maxtimes);

	if(itimes < 0)
		itimes = maxtimes;

	for(i = 0; i < itimes; i++)
		printf("%3d %10.3e\n", i, his_norm2_res[i]);

	free(his_norm2_res);

	// print
	printf("Iterative Times = %d\n", itimes);
	printf("Rel.Diff = %10.3e\n", reldiff_dvector(vx, vx_true, dim));

/*	for(i = 0; i < dim; i++)
	{
		printf("%3d %25.17e %25.17e\n", i, vx[i], vx_true[i]);
	}
*/
	// free
	free(ma_val);
	free(ma_row_index);
	free(ma_col_index);

	free(vy);
	free(vx);
	free(vx_true);
	free(vx_new);
	free(vb);

	return EXIT_SUCCESS;
}
