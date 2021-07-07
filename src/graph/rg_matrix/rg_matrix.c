/*
* Copyright 2018-2021 Redis Labs Ltd. and Contributors
*
* This file is available under the Redis Labs Source Available License Agreement
*/

#include "RG.h"
#include "rg_matrix.h"
#include "../../util/rmalloc.h"

void RG_Matrix_setDirty
(
	RG_Matrix C
) {
	ASSERT(C);
	C->dirty = true;
}

RG_Matrix RG_Matrix_getTranspose
(
	const RG_Matrix C
) {
	ASSERT(C != NULL);
	return C->transposed;
}

// returns underlying GraphBLAS matrix
GrB_Matrix RG_Matrix_getGrB_Matrix
(
	const RG_Matrix C
) {
	ASSERT(C != NULL);
	return RG_MATRIX_MATRIX(C);
}

// returns underlying delta plus GraphBLAS matrix
GrB_Matrix RG_Matrix_getDeltaPlus
(
	RG_Matrix C
) {
	ASSERT(C != NULL);
	return RG_MATRIX_DELTA_PLUS(C);
}

bool RG_Matrix_isDirty
(
	const RG_Matrix C
) {
	ASSERT(C);
	return C->dirty;
}

// locks the matrix
void RG_Matrix_Lock
(
	RG_Matrix C
) {
	ASSERT(C);
	pthread_mutex_lock(&C->mutex);
}

// unlocks the matrix
void RG_Matrix_Unlock
(
	RG_Matrix C
) {
	ASSERT(C);
	pthread_mutex_unlock(&C->mutex);
}

void RG_Matrix_setMultiEdge
(
	RG_Matrix C,
	bool multi_edge
) {
	ASSERT(C);
	if(C->maintain_transpose) RG_Matrix_setMultiEdge(C->transposed, multi_edge);
	C->multi_edge = multi_edge;
}

bool RG_Matrix_getMultiEdge
(
	const RG_Matrix C
) {
	ASSERT(C);
	return C->multi_edge;
}

GrB_Info RG_Matrix_nvals    // get the number of entries in a matrix
(
    GrB_Index *nvals,       // matrix has nvals entries
    const RG_Matrix A       // matrix to query
) {
	ASSERT(A      !=  NULL);
	ASSERT(nvals  !=  NULL);

	GrB_Matrix  m;
	GrB_Matrix  dp;
	GrB_Matrix  dm;
	GrB_Info    info;

	GrB_Index  m_nvals   =  0;
	GrB_Index  dp_nvals  =  0;
	GrB_Index  dm_nvals  =  0;

	// nvals = nvals(M) + nvals(DP) - nvals(DM)

	m   =  RG_MATRIX_MATRIX(A);
	dp  =  RG_MATRIX_DELTA_PLUS(A);
	dm  =  RG_MATRIX_DELTA_MINUS(A);

	info = GrB_Matrix_nvals(&m_nvals, m);
	ASSERT(info == GrB_SUCCESS);
	info = GrB_Matrix_nvals(&dp_nvals, dp);
	ASSERT(info == GrB_SUCCESS);
	info = GrB_Matrix_nvals(&dm_nvals, dm);
	ASSERT(info == GrB_SUCCESS);

	*nvals = m_nvals + dp_nvals - dm_nvals;
	return info;
}

GrB_Info RG_Matrix_resize       // change the size of a matrix
(
    RG_Matrix C,                // matrix to modify
    GrB_Index nrows_new,        // new number of rows in matrix
    GrB_Index ncols_new         // new number of columns in matrix
) {
	ASSERT(C != NULL);
	GrB_Info info;

	if(C->maintain_transpose) {
		info = RG_Matrix_resize(C->transposed, nrows_new, ncols_new);
		ASSERT(info == GrB_SUCCESS);
	}

	GrB_Matrix  m            =  RG_MATRIX_MATRIX(C);
	GrB_Matrix  delta_plus   =  RG_MATRIX_DELTA_PLUS(C);
	GrB_Matrix  delta_minus  =  RG_MATRIX_DELTA_MINUS(C);

	info = GrB_Matrix_resize(m, nrows_new, ncols_new);
	ASSERT(info == GrB_SUCCESS);

	info = GrB_Matrix_resize(delta_plus, nrows_new, ncols_new);
	ASSERT(info == GrB_SUCCESS);
	
	info = GrB_Matrix_resize(delta_minus, nrows_new, ncols_new);
	ASSERT(info == GrB_SUCCESS);
	
	return info;
}

GrB_Info RG_Matrix_extractElement_BOOL     // x = A(i,j)
(
    bool *x,                               // extracted scalar
    const RG_Matrix A,                     // matrix to extract a scalar from
    GrB_Index i,                           // row index
    GrB_Index j                            // column index
) {
	ASSERT(x != NULL);
	ASSERT(A != NULL);

	GrB_Info info;
	GrB_Matrix  m      =  RG_MATRIX_MATRIX(A);
	GrB_Matrix  dp     =  RG_MATRIX_DELTA_PLUS(A);
	GrB_Matrix  dm     =  RG_MATRIX_DELTA_MINUS(A);

	// if 'delta-plus' exists return dp[i,j]
	info = GrB_Matrix_extractElement(x, dp, i, j);
	if(info == GrB_SUCCESS) {
		return info;
	}

	// if dm[i,j] doesn't exists, return M[i,j]
	info = GrB_Matrix_extractElement(x, dm, i, j);
	if(info != GrB_SUCCESS) {
		info = GrB_Matrix_extractElement(x, m, i, j);
		return info;
	}

	return info;
}

GrB_Info RG_Matrix_extractElement_UINT64   // x = A(i,j)
(
    uint64_t *x,                           // extracted scalar
    const RG_Matrix A,                     // matrix to extract a scalar from
    GrB_Index i,                           // row index
    GrB_Index j                            // column index
) {
	ASSERT(x != NULL);
	ASSERT(A != NULL);

	GrB_Info info;
	GrB_Matrix  m      =  RG_MATRIX_MATRIX(A);
	GrB_Matrix  dp     =  RG_MATRIX_DELTA_PLUS(A);
	GrB_Matrix  dm     =  RG_MATRIX_DELTA_MINUS(A);

	// if 'delta-plus' exists return dp[i,j]
	info = GrB_Matrix_extractElement(x, dp, i, j);
	if(info == GrB_SUCCESS) {
		return info;
	}

	// if dm[i,j] doesn't exists, return M[i,j]
	info = GrB_Matrix_extractElement(x, dm, i, j);
	if(info != GrB_SUCCESS) {
		info = GrB_Matrix_extractElement(x, m, i, j);
		return info;
	}

	return info;
}

GrB_Info RG_Matrix_subassign_UINT64 // C(I,J)<Mask> = accum (C(I,J),x)
(
    RG_Matrix C,                    // input/output matrix for results
    const GrB_Matrix Mask,          // optional mask for C(I,J), unused if NULL
    const GrB_BinaryOp accum,       // optional accum for Z=accum(C(I,J),x)
    uint64_t x,                     // scalar to assign to C(I,J)
    const GrB_Index *I,             // row indices
    GrB_Index ni,                   // number of row indices
    const GrB_Index *J,             // column indices
    GrB_Index nj,                   // number of column indices
    const GrB_Descriptor desc       // descriptor for C(I,J) and Mask
) {
	// TODO: do we really need this function?
	ASSERT(false);
	ASSERT(C != NULL);
	GrB_Info info;

	if(C->maintain_transpose) {
		info = RG_Matrix_subassign_UINT64(C->transposed, Mask, accum, x, J, nj,
				I, ni, desc); 
		ASSERT(info == GrB_SUCCESS);
	}

	GrB_Matrix delta_plus = RG_MATRIX_DELTA_PLUS(C);

	info = GxB_Matrix_subassign_UINT64   // C(I,J)<Mask> = accum (C(I,J),x)
		(
		 delta_plus,           // input/output matrix for results
		 Mask,                 // optional mask for C(I,J), unused if NULL
		 accum,                // optional accum for Z=accum(C(I,J),x)
		 x,                    // scalar to assign to C(I,J)
		 I,                    // row indices
		 ni,                   // number of row indices
		 J,                    // column indices
		 nj,                   // number of column indices
		 desc                  // descriptor for C(I,J) and Mask
		);

	if(info == GrB_SUCCESS) RG_Matrix_setDirty(C);
	return info;
}

