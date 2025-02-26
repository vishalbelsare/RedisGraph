//------------------------------------------------------------------------------
// GxB_Vector_select: select entries from a vector
//------------------------------------------------------------------------------

// SuiteSparse:GraphBLAS, Timothy A. Davis, (c) 2017-2021, All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0

//------------------------------------------------------------------------------

#include "GB_select.h"
#include "GB_get_mask.h"

GrB_Info GxB_Vector_select          // w<M> = accum (w, select(u,k))
(
    GrB_Vector w,                   // input/output vector for results
    const GrB_Vector M_in,          // optional mask for w, unused if NULL
    const GrB_BinaryOp accum,       // optional accum for z=accum(w,t)
    const GxB_SelectOp op,          // operator to apply to the entries
    const GrB_Vector u,             // first input:  vector u
    const GrB_Scalar Thunk,         // optional input for select operator
    const GrB_Descriptor desc       // descriptor for w and mask
)
{ 

    //--------------------------------------------------------------------------
    // check inputs
    //--------------------------------------------------------------------------

    GB_WHERE (w, "GxB_Vector_select (w, M, accum, op, u, Thunk, desc)") ;
    GB_BURBLE_START ("GxB_select") ;
    GB_RETURN_IF_NULL_OR_FAULTY (w) ;
    GB_RETURN_IF_FAULTY (M_in) ;
    GB_RETURN_IF_NULL_OR_FAULTY (u) ;

    // get the descriptor
    GB_GET_DESCRIPTOR (info, desc, C_replace, Mask_comp, Mask_struct,
        xx1, xx2, xx3, xx7) ;

    // get the mask
    GrB_Matrix M = GB_get_mask ((GrB_Matrix) M_in, &Mask_comp, &Mask_struct) ;

    //--------------------------------------------------------------------------
    // select the entries; do not transpose; assemble pending entries
    //--------------------------------------------------------------------------

    info = GB_select (
        (GrB_Matrix) w, C_replace,          // w and its descriptor
        M, Mask_comp, Mask_struct,          // mask and its descriptor
        accum,                              // optional accum for Z=accum(C,T)
        (GB_Operator) op,                   // operator to select the entries
        (GrB_Matrix) u,                     // first input: u
        Thunk,                              // optional input for select op
        false,                              // u, not transposed
        Context) ;

    GB_BURBLE_END ;
    return (info) ;
}

