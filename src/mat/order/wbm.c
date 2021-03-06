#include <petscmat.h>
#include <petsc-private/matorderimpl.h>

#if defined(PETSC_HAVE_SUPERLU_DIST)

#  if defined(PETSC_HAVE_FORTRAN_CAPS)
#    define mc64id_    MC64ID
#    define mc64ad_    MC64AD
#  elif !defined(PETSC_HAVE_FORTRAN_UNDERSCORE)
#    define mc64id_    mc64id
#    define mc64ad_    mc64ad
#  endif

/* SuperLU_DIST bundles f2ced mc64ad_() from HSL */
PETSC_EXTERN PetscInt mc64id_(PetscInt *icntl);
PETSC_EXTERN PetscInt mc64ad_(const PetscInt *job, PetscInt *n, PetscInt *ne, const PetscInt *ip, const PetscInt *irn, PetscScalar *a, PetscInt *num,
                              PetscInt *perm, PetscInt *liw, PetscInt *iw, PetscInt *ldw, PetscScalar *dw, PetscInt *icntl, PetscInt *info);
#endif

/*
  MatGetOrdering_WBM - Find the nonsymmetric reordering of the graph which maximizes the product of diagonal entries,
    using weighted bipartite graph matching. This is MC64 in the Harwell-Boeing library.
*/
#undef __FUNCT__
#define __FUNCT__ "MatGetOrdering_WBM"
PETSC_EXTERN PetscErrorCode MatGetOrdering_WBM(Mat mat, MatOrderingType type, IS *row, IS *col)
{
  PetscScalar    *a, *dw;
  const PetscInt *ia, *ja;
  const PetscInt  job = 5;
  PetscInt       *perm, nrow, ncol, nnz, liw, *iw, ldw, i;
  PetscBool       done;
  PetscErrorCode  ierr;

  PetscFunctionBegin;
  ierr = MatGetRowIJ(mat,1,PETSC_TRUE,PETSC_TRUE,&nrow,&ia,&ja,&done);CHKERRQ(ierr);
  ncol = nrow;
  nnz  = ia[nrow];
  if (!done) SETERRQ(PetscObjectComm((PetscObject)mat),PETSC_ERR_SUP,"Cannot get rows for matrix");
  ierr = MatSeqAIJGetArray(mat, &a);CHKERRQ(ierr);
  switch (job) {
  case 1: liw = 4*nrow +   ncol; ldw = 0;break;
  case 2: liw = 2*nrow + 2*ncol; ldw = ncol;break;
  case 3: liw = 8*nrow + 2*ncol + nnz; ldw = nnz;break;
  case 4: liw = 3*nrow + 2*ncol; ldw = 2*ncol + nnz;break;
  case 5: liw = 3*nrow + 2*ncol; ldw = nrow + 2*ncol + nnz;break;
  }

  ierr = PetscMalloc3(liw,&iw,ldw,&dw,nrow,&perm);CHKERRQ(ierr);
#if defined(PETSC_HAVE_SUPERLU_DIST)
  {
    PetscInt        num, info[10], icntl[10];

    ierr = mc64id_(icntl);
    if (ierr) SETERRQ1(PETSC_COMM_SELF,PETSC_ERR_LIB,"HSL mc64id_ returned %d\n",ierr);
    icntl[0] = 0;              /* allow printing error messages (f2c'd code uses if non-negative, ignores value otherwise) */
    icntl[1] = -1;             /* suppress warnings */
    icntl[2] = -1;             /* ignore diagnostic output [default] */
    icntl[3] = 0;              /* perform consistency checks [default] */
    ierr = mc64ad_(&job, &nrow, &nnz, ia, ja, a, &num, perm, &liw, iw, &ldw, dw, icntl, info);
    if (ierr) SETERRQ1(PETSC_COMM_SELF,PETSC_ERR_LIB,"HSL mc64ad_ returned %d\n",ierr);
  }
#else
  SETERRQ(PetscObjectComm((PetscObject) mat), PETSC_ERR_SUP, "WBM using MC64 does not support complex numbers");
#endif
  ierr = MatRestoreRowIJ(mat, 1, PETSC_TRUE, PETSC_TRUE, NULL, &ia, &ja, &done);CHKERRQ(ierr);
  for (i = 0; i < nrow; ++i) perm[i]--;
  /* If job == 5, dw[0..ncols] contains the column scaling and dw[ncols..ncols+nrows] contains the row scaling */
  ierr = ISCreateStride(PETSC_COMM_SELF, nrow, 0, 1, row);CHKERRQ(ierr);
  ierr = ISCreateGeneral(PETSC_COMM_SELF,nrow,perm,PETSC_COPY_VALUES,col);CHKERRQ(ierr);
  ierr = PetscFree3(iw,dw,perm);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}
