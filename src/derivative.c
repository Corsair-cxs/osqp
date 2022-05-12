#include "derivative.h"
#include "lin_alg.h"
#include "util.h"
#include "auxil.h"
#include "lin_sys.h"
#include "proj.h"
#include "error.h"
#include "csc_utils.h"


//increment the D colptr by the number of nonzeros
//in a square diagonal matrix.
static void _colcount_diag(csc* D, c_int initcol, c_int blockcols){

    c_int j;
    for(j = initcol; j < (initcol + blockcols); j++){
        D->p[j]++;
    }
    return;
}

//increment D colptr by the number of nonzeros in M
static void _colcount_block(csc* D, csc* M, c_int initcol, c_int istranspose) {

    c_int nnzM, j;

    if(istranspose){
        nnzM = M->p[M->n];
        for (j = 0; j < nnzM; j++){
            D->p[M->i[j] + initcol]++;
        }
    }
    else {
        //just add the column count
        for (j = 0; j < M->n; j++){
            D->p[j + initcol] += M->p[j+1] - M->p[j];
        }
    }
    return;
}

static void _colcount_to_colptr(csc* D){

    c_int j, count;
    c_int currentptr = 0;

    for(j = 0; j <= D->n; j++){
        count        = D->p[j];
        D->p[j]      = currentptr;
        currentptr  += count;
    }
    return;
}

static void _adj_assemble_csc(csc *D, OSQPMatrix *P_full, OSQPMatrix *G, OSQPMatrix *A_eq, OSQPVectorf *GDiagLambda, OSQPVectorf *slacks) {

    c_int n = OSQPMatrix_get_m(P_full);
    c_int x = OSQPMatrix_get_m(G);        // No. of inequality constraints
    c_int y = OSQPMatrix_get_m(A_eq);     // No. of equality constraints

    c_int j;
    //use D.p to hold nnz entries in each column of the D matrix
    for (j=0; j <= 2*(n+x+y); j++){D->p[j] = 0;}

    _colcount_diag(D, 0, n+x+y);
    _colcount_block(D, P_full, n+x+y, 0);
    _colcount_block(D, G, n+x+y, 0);
    _colcount_block(D, A_eq, n+x+y, 0);
    _colcount_block(D, GDiagLambda, n+x+y+n, 1);
    _colcount_diag(D, n+x+y+n, x);
    _colcount_block(D, A_eq, n+x+y+n+x, 1);

    //cumsum total entries to convert to D.p
    _colcount_to_colptr(D);

    return;
}


c_int adjoint_derivative(OSQPSolver *solver, const csc* check) {

//    c_int m = solver->work->data->m;
//    c_int n = solver->work->data->n;
//
//    OSQPInfo*      info      = solver->info;
//    OSQPSettings*  settings  = solver->settings;
//    OSQPWorkspace* work      = solver->work;
//    OSQPSolution*  solution  = solver->solution;
//
//    OSQPMatrix *P = solver->work->data->P;
//    OSQPVectorf *q = solver->work->data->q;
//    OSQPMatrix *A = solver->work->data->A;
//    OSQPVectorf *l = solver->work->data->l;
//    OSQPVectorf *u = solver->work->data->u;
//    OSQPVectorf *x = solver->work->x;
//    OSQPVectorf *y = solver->work->y;
//
//    c_float *l_data = OSQPVectorf_data(l);
//    c_float *u_data = OSQPVectorf_data(u);
//
//    c_int *A_ineq_l_vec = (c_int *) c_malloc(m * sizeof(c_int));
//    c_int *A_ineq_u_vec = (c_int *) c_malloc(m * sizeof(c_int));
//    c_int *A_eq_vec = (c_int *) c_malloc(m * sizeof(c_int));
//
//    // TODO: We could use constr_type in OSQPWorkspace but it only tells us whether a constraint is 'loose'
//    // not 'upper loose' or 'lower loose', which we seem to need here.
//    c_float infval = OSQP_INFTY;  // TODO: Should we be multiplying this by OSQP_MIN_SCALING ?
//
//    c_int n_ineq = 0;
//    c_int n_eq = 0;
//    c_int j;
//    for (j = 0; j < m; j++) {
//        c_float _l = l_data[j];
//        c_float _u = u_data[j];
//        if (_l < _u) {
//            A_eq_vec[j] = 0;
//            if (_l > -infval)
//                A_ineq_l_vec[j] = 1;
//            else
//                A_ineq_l_vec[j] = 0;
//            if (_u < infval)
//                A_ineq_u_vec[j] = 1;
//            else
//                A_ineq_u_vec[j] = 0;
//            n_ineq = n_ineq + 2;  // we add two constraints, (one lower, one upper) for every inequality constraint
//        } else {
//            A_eq_vec[j] = 1;
//            A_ineq_l_vec[j] = 0;
//            A_ineq_u_vec[j] = 0;
//            n_eq++;
//        }
//    }
//
//    OSQPVectori *A_ineq_l_i = OSQPVectori_malloc(m);
//    OSQPVectori_from_raw(A_ineq_l_i, A_ineq_l_vec);
//    c_free(A_ineq_l_vec);
//    OSQPMatrix *A_ineq_l = OSQPMatrix_submatrix_byrows(A, A_ineq_l_i);
//    OSQPMatrix_mult_scalar(A_ineq_l, -1);
//
//    OSQPVectori *A_ineq_u_i = OSQPVectori_malloc(m);
//    OSQPVectori_from_raw(A_ineq_u_i, A_ineq_u_vec);
//    c_free(A_ineq_u_vec);
//    OSQPMatrix *A_ineq_u = OSQPMatrix_submatrix_byrows(A, A_ineq_u_i);
//
//    c_int A_ineq_l_nnz = OSQPMatrix_get_nz(A_ineq_l);
//    c_int A_ineq_u_nnz = OSQPMatrix_get_nz(A_ineq_u);
//    OSQPMatrix *G = OSQPMatrix_vstack(A_ineq_l, A_ineq_u);
//
//    OSQPMatrix_free(A_ineq_l);
//    OSQPMatrix_free(A_ineq_u);
//
//    OSQPVectori *A_eq_i = OSQPVectori_malloc(m);
//    OSQPVectori_from_raw(A_eq_i, A_eq_vec);
//    c_free(A_eq_vec);
//    OSQPMatrix *A_eq = OSQPMatrix_submatrix_byrows(A, A_eq_i);
//
//    OSQPVectorf *zero = OSQPVectorf_malloc(m);
//    OSQPVectorf_set_scalar(zero, 0);
//
//    // --------- lambda
//    OSQPVectorf *_y_l_ineq = OSQPVectorf_subvector_byrows(y, A_ineq_l_i);
//    OSQPVectorf *y_l_ineq = OSQPVectorf_malloc(OSQPVectorf_length(_y_l_ineq));
//    OSQPVectorf_ew_min_vec(y_l_ineq, _y_l_ineq, zero);
//    OSQPVectorf_free(_y_l_ineq);
//    OSQPVectorf_mult_scalar(y_l_ineq, -1);
//
//    OSQPVectorf *_y_u_ineq = OSQPVectorf_subvector_byrows(y, A_ineq_u_i);
//    OSQPVectorf *y_u_ineq = OSQPVectorf_malloc(OSQPVectorf_length(_y_u_ineq));
//    OSQPVectorf_ew_max_vec(y_u_ineq, _y_u_ineq, zero);
//    OSQPVectorf_free(_y_u_ineq);
//
//    OSQPVectorf *lambda = OSQPVectorf_concat(y_l_ineq, y_u_ineq);
//
//    OSQPVectorf_free(y_l_ineq);
//    OSQPVectorf_free(y_u_ineq);
//    // ---------- lambda
//
//    // --------- h
//    OSQPVectorf *_l_ineq = OSQPVectorf_subvector_byrows(l, A_ineq_l_i);
//    OSQPVectorf *l_ineq = OSQPVectorf_malloc(OSQPVectorf_length(_l_ineq));
//    OSQPVectorf_ew_min_vec(l_ineq, _l_ineq, zero);
//    OSQPVectorf_free(_l_ineq);
//    OSQPVectorf_mult_scalar(l_ineq, -1);
//
//    OSQPVectorf *_u_ineq = OSQPVectorf_subvector_byrows(u, A_ineq_u_i);
//    OSQPVectorf *u_ineq = OSQPVectorf_malloc(OSQPVectorf_length(_u_ineq));
//    OSQPVectorf_ew_max_vec(u_ineq, _u_ineq, zero);
//    OSQPVectorf_free(_u_ineq);
//
//    OSQPVectorf *h = OSQPVectorf_concat(l_ineq, u_ineq);
//
//    OSQPVectorf_free(l_ineq);
//    OSQPVectorf_free(u_ineq);
//    // ---------- h
//
//    OSQPVectorf_free(zero);
//
//    // ---------- GDiagLambda
//    OSQPMatrix *GDiagLambda = OSQPMatrix_copy_new(G);
//    OSQPMatrix_rmult_diag(GDiagLambda, lambda);
//
//    // ---------- Slacks
//    OSQPVectorf* slacks = OSQPVectorf_copy_new(h);
//    OSQPMatrix_Axpy(G, x, slacks, 1, -1);
//
//    OSQPMatrix *P_full = OSQPMatrix_triu_to_symm(P);
//
//    c_int status = adjoint_derivative_linsys_solver(solver, settings, P_full);
//
//    OSQPMatrix *checkmat = OSQPMatrix_new_from_csc(check, 1);
//    c_int iseq = OSQPMatrix_is_eq(P_full, checkmat, 0.0001);
//
//    // TODO: Make sure we're freeing everything we should!
//    OSQPMatrix_free(G);
//    OSQPMatrix_free(A_eq);
//    OSQPMatrix_free(P_full);
//    OSQPMatrix_free(GDiagLambda);
//
//    OSQPVectorf_free(lambda);
//    OSQPVectorf_free(slacks);

    return 0;
}