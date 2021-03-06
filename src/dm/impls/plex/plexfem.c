#include <petsc-private/dmpleximpl.h>   /*I      "petscdmplex.h"   I*/

#include <petscfe.h>

#undef __FUNCT__
#define __FUNCT__ "DMPlexGetScale"
PetscErrorCode DMPlexGetScale(DM dm, PetscUnit unit, PetscReal *scale)
{
  DM_Plex *mesh = (DM_Plex*) dm->data;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(dm, DM_CLASSID, 1);
  PetscValidPointer(scale, 3);
  *scale = mesh->scale[unit];
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "DMPlexSetScale"
PetscErrorCode DMPlexSetScale(DM dm, PetscUnit unit, PetscReal scale)
{
  DM_Plex *mesh = (DM_Plex*) dm->data;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(dm, DM_CLASSID, 1);
  mesh->scale[unit] = scale;
  PetscFunctionReturn(0);
}

PETSC_STATIC_INLINE PetscInt epsilon(PetscInt i, PetscInt j, PetscInt k)
{
  switch (i) {
  case 0:
    switch (j) {
    case 0: return 0;
    case 1:
      switch (k) {
      case 0: return 0;
      case 1: return 0;
      case 2: return 1;
      }
    case 2:
      switch (k) {
      case 0: return 0;
      case 1: return -1;
      case 2: return 0;
      }
    }
  case 1:
    switch (j) {
    case 0:
      switch (k) {
      case 0: return 0;
      case 1: return 0;
      case 2: return -1;
      }
    case 1: return 0;
    case 2:
      switch (k) {
      case 0: return 1;
      case 1: return 0;
      case 2: return 0;
      }
    }
  case 2:
    switch (j) {
    case 0:
      switch (k) {
      case 0: return 0;
      case 1: return 1;
      case 2: return 0;
      }
    case 1:
      switch (k) {
      case 0: return -1;
      case 1: return 0;
      case 2: return 0;
      }
    case 2: return 0;
    }
  }
  return 0;
}

#undef __FUNCT__
#define __FUNCT__ "DMPlexCreateRigidBody"
/*@C
  DMPlexCreateRigidBody - create rigid body modes from coordinates

  Collective on DM

  Input Arguments:
+ dm - the DM
. section - the local section associated with the rigid field, or NULL for the default section
- globalSection - the global section associated with the rigid field, or NULL for the default section

  Output Argument:
. sp - the null space

  Note: This is necessary to take account of Dirichlet conditions on the displacements

  Level: advanced

.seealso: MatNullSpaceCreate()
@*/
PetscErrorCode DMPlexCreateRigidBody(DM dm, PetscSection section, PetscSection globalSection, MatNullSpace *sp)
{
  MPI_Comm       comm;
  Vec            coordinates, localMode, mode[6];
  PetscSection   coordSection;
  PetscScalar   *coords;
  PetscInt       dim, vStart, vEnd, v, n, m, d, i, j;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = PetscObjectGetComm((PetscObject)dm,&comm);CHKERRQ(ierr);
  ierr = DMPlexGetDimension(dm, &dim);CHKERRQ(ierr);
  if (dim == 1) {
    ierr = MatNullSpaceCreate(comm, PETSC_TRUE, 0, NULL, sp);CHKERRQ(ierr);
    PetscFunctionReturn(0);
  }
  if (!section)       {ierr = DMGetDefaultSection(dm, &section);CHKERRQ(ierr);}
  if (!globalSection) {ierr = DMGetDefaultGlobalSection(dm, &globalSection);CHKERRQ(ierr);}
  ierr = PetscSectionGetConstrainedStorageSize(globalSection, &n);CHKERRQ(ierr);
  ierr = DMPlexGetDepthStratum(dm, 0, &vStart, &vEnd);CHKERRQ(ierr);
  ierr = DMGetCoordinateSection(dm, &coordSection);CHKERRQ(ierr);
  ierr = DMGetCoordinatesLocal(dm, &coordinates);CHKERRQ(ierr);
  m    = (dim*(dim+1))/2;
  ierr = VecCreate(comm, &mode[0]);CHKERRQ(ierr);
  ierr = VecSetSizes(mode[0], n, PETSC_DETERMINE);CHKERRQ(ierr);
  ierr = VecSetUp(mode[0]);CHKERRQ(ierr);
  for (i = 1; i < m; ++i) {ierr = VecDuplicate(mode[0], &mode[i]);CHKERRQ(ierr);}
  /* Assume P1 */
  ierr = DMGetLocalVector(dm, &localMode);CHKERRQ(ierr);
  for (d = 0; d < dim; ++d) {
    PetscScalar values[3] = {0.0, 0.0, 0.0};

    values[d] = 1.0;
    ierr      = VecSet(localMode, 0.0);CHKERRQ(ierr);
    for (v = vStart; v < vEnd; ++v) {
      ierr = DMPlexVecSetClosure(dm, section, localMode, v, values, INSERT_VALUES);CHKERRQ(ierr);
    }
    ierr = DMLocalToGlobalBegin(dm, localMode, INSERT_VALUES, mode[d]);CHKERRQ(ierr);
    ierr = DMLocalToGlobalEnd(dm, localMode, INSERT_VALUES, mode[d]);CHKERRQ(ierr);
  }
  ierr = VecGetArray(coordinates, &coords);CHKERRQ(ierr);
  for (d = dim; d < dim*(dim+1)/2; ++d) {
    PetscInt i, j, k = dim > 2 ? d - dim : d;

    ierr = VecSet(localMode, 0.0);CHKERRQ(ierr);
    for (v = vStart; v < vEnd; ++v) {
      PetscScalar values[3] = {0.0, 0.0, 0.0};
      PetscInt    off;

      ierr = PetscSectionGetOffset(coordSection, v, &off);CHKERRQ(ierr);
      for (i = 0; i < dim; ++i) {
        for (j = 0; j < dim; ++j) {
          values[j] += epsilon(i, j, k)*PetscRealPart(coords[off+i]);
        }
      }
      ierr = DMPlexVecSetClosure(dm, section, localMode, v, values, INSERT_VALUES);CHKERRQ(ierr);
    }
    ierr = DMLocalToGlobalBegin(dm, localMode, INSERT_VALUES, mode[d]);CHKERRQ(ierr);
    ierr = DMLocalToGlobalEnd(dm, localMode, INSERT_VALUES, mode[d]);CHKERRQ(ierr);
  }
  ierr = VecRestoreArray(coordinates, &coords);CHKERRQ(ierr);
  ierr = DMRestoreLocalVector(dm, &localMode);CHKERRQ(ierr);
  for (i = 0; i < dim; ++i) {ierr = VecNormalize(mode[i], NULL);CHKERRQ(ierr);}
  /* Orthonormalize system */
  for (i = dim; i < m; ++i) {
    PetscScalar dots[6];

    ierr = VecMDot(mode[i], i, mode, dots);CHKERRQ(ierr);
    for (j = 0; j < i; ++j) dots[j] *= -1.0;
    ierr = VecMAXPY(mode[i], i, dots, mode);CHKERRQ(ierr);
    ierr = VecNormalize(mode[i], NULL);CHKERRQ(ierr);
  }
  ierr = MatNullSpaceCreate(comm, PETSC_FALSE, m, mode, sp);CHKERRQ(ierr);
  for (i = 0; i< m; ++i) {ierr = VecDestroy(&mode[i]);CHKERRQ(ierr);}
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "DMPlexProjectFunctionLabelLocal"
PetscErrorCode DMPlexProjectFunctionLabelLocal(DM dm, DMLabel label, PetscInt numIds, const PetscInt ids[], PetscFE fe[], void (**funcs)(const PetscReal [], PetscScalar *, void *), void **ctxs, InsertMode mode, Vec localX)
{
  PetscDualSpace *sp;
  PetscSection    section;
  PetscScalar    *values;
  PetscReal      *v0, *J, detJ;
  PetscInt        numFields, numComp, dim, spDim, totDim = 0, numValues, cStart, cEnd, f, d, v, i, comp;
  PetscErrorCode  ierr;

  PetscFunctionBegin;
  ierr = DMPlexGetDimension(dm, &dim);CHKERRQ(ierr);
  ierr = DMGetDefaultSection(dm, &section);CHKERRQ(ierr);
  ierr = PetscSectionGetNumFields(section, &numFields);CHKERRQ(ierr);
  ierr = PetscMalloc3(numFields,&sp,dim,&v0,dim*dim,&J);CHKERRQ(ierr);
  for (f = 0; f < numFields; ++f) {
    ierr = PetscFEGetDualSpace(fe[f], &sp[f]);CHKERRQ(ierr);
    ierr = PetscFEGetNumComponents(fe[f], &numComp);CHKERRQ(ierr);
    ierr = PetscDualSpaceGetDimension(sp[f], &spDim);CHKERRQ(ierr);
    totDim += spDim*numComp;
  }
  ierr = DMPlexGetHeightStratum(dm, 0, &cStart, &cEnd);CHKERRQ(ierr);
  ierr = DMPlexVecGetClosure(dm, section, localX, cStart, &numValues, NULL);CHKERRQ(ierr);
  if (numValues != totDim) SETERRQ2(PETSC_COMM_SELF, PETSC_ERR_ARG_WRONG, "The section cell closure size %d != dual space dimension %d", numValues, totDim);
  ierr = DMGetWorkArray(dm, numValues, PETSC_SCALAR, &values);CHKERRQ(ierr);
  for (i = 0; i < numIds; ++i) {
    IS              pointIS;
    const PetscInt *points;
    PetscInt        n, p;

    ierr = DMLabelGetStratumIS(label, ids[i], &pointIS);CHKERRQ(ierr);
    ierr = ISGetLocalSize(pointIS, &n);CHKERRQ(ierr);
    ierr = ISGetIndices(pointIS, &points);CHKERRQ(ierr);
    for (p = 0; p < n; ++p) {
      const PetscInt    point = points[p];
      PetscCellGeometry geom;

      if ((point < cStart) || (point >= cEnd)) continue;
      ierr = DMPlexComputeCellGeometry(dm, point, v0, J, NULL, &detJ);CHKERRQ(ierr);
      geom.v0   = v0;
      geom.J    = J;
      geom.detJ = &detJ;
      for (f = 0, v = 0; f < numFields; ++f) {
        void * const ctx = ctxs ? ctxs[f] : NULL;
        ierr = PetscFEGetNumComponents(fe[f], &numComp);CHKERRQ(ierr);
        ierr = PetscDualSpaceGetDimension(sp[f], &spDim);CHKERRQ(ierr);
        for (d = 0; d < spDim; ++d) {
          if (funcs[f]) {
            ierr = PetscDualSpaceApply(sp[f], d, geom, numComp, funcs[f], ctx, &values[v]);CHKERRQ(ierr);
          } else {
            for (comp = 0; comp < numComp; ++comp) values[v+comp] = 0.0;
          }
          v += numComp;
        }
      }
      ierr = DMPlexVecSetClosure(dm, section, localX, point, values, mode);CHKERRQ(ierr);
    }
    ierr = ISRestoreIndices(pointIS, &points);CHKERRQ(ierr);
    ierr = ISDestroy(&pointIS);CHKERRQ(ierr);
  }
  ierr = DMRestoreWorkArray(dm, numValues, PETSC_SCALAR, &values);CHKERRQ(ierr);
  ierr = PetscFree3(sp,v0,J);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "DMPlexProjectFunctionLocal"
PetscErrorCode DMPlexProjectFunctionLocal(DM dm, PetscFE fe[], void (**funcs)(const PetscReal [], PetscScalar *, void *), void **ctxs, InsertMode mode, Vec localX)
{
  PetscDualSpace *sp;
  PetscSection    section;
  PetscScalar    *values;
  PetscReal      *v0, *J, detJ;
  PetscInt        numFields, numComp, dim, spDim, totDim = 0, numValues, cStart, cEnd, c, f, d, v, comp;
  PetscErrorCode  ierr;

  PetscFunctionBegin;
  ierr = DMGetDefaultSection(dm, &section);CHKERRQ(ierr);
  ierr = PetscSectionGetNumFields(section, &numFields);CHKERRQ(ierr);
  ierr = PetscMalloc1(numFields, &sp);CHKERRQ(ierr);
  for (f = 0; f < numFields; ++f) {
    ierr = PetscFEGetDualSpace(fe[f], &sp[f]);CHKERRQ(ierr);
    ierr = PetscFEGetNumComponents(fe[f], &numComp);CHKERRQ(ierr);
    ierr = PetscDualSpaceGetDimension(sp[f], &spDim);CHKERRQ(ierr);
    totDim += spDim*numComp;
  }
  ierr = DMPlexGetDimension(dm, &dim);CHKERRQ(ierr);
  ierr = DMPlexGetHeightStratum(dm, 0, &cStart, &cEnd);CHKERRQ(ierr);
  ierr = DMPlexVecGetClosure(dm, section, localX, cStart, &numValues, NULL);CHKERRQ(ierr);
  if (numValues != totDim) SETERRQ2(PETSC_COMM_SELF, PETSC_ERR_ARG_WRONG, "The section cell closure size %d != dual space dimension %d", numValues, totDim);
  ierr = DMGetWorkArray(dm, numValues, PETSC_SCALAR, &values);CHKERRQ(ierr);
  ierr = PetscMalloc2(dim,&v0,dim*dim,&J);CHKERRQ(ierr);
  for (c = cStart; c < cEnd; ++c) {
    PetscCellGeometry geom;

    ierr = DMPlexComputeCellGeometry(dm, c, v0, J, NULL, &detJ);CHKERRQ(ierr);
    geom.v0   = v0;
    geom.J    = J;
    geom.detJ = &detJ;
    for (f = 0, v = 0; f < numFields; ++f) {
      void * const ctx = ctxs ? ctxs[f] : NULL;
      ierr = PetscFEGetNumComponents(fe[f], &numComp);CHKERRQ(ierr);
      ierr = PetscDualSpaceGetDimension(sp[f], &spDim);CHKERRQ(ierr);
      for (d = 0; d < spDim; ++d) {
        if (funcs[f]) {
          ierr = PetscDualSpaceApply(sp[f], d, geom, numComp, funcs[f], ctx, &values[v]);CHKERRQ(ierr);
        } else {
          for (comp = 0; comp < numComp; ++comp) values[v+comp] = 0.0;
        }
        v += numComp;
      }
    }
    ierr = DMPlexVecSetClosure(dm, section, localX, c, values, mode);CHKERRQ(ierr);
  }
  ierr = DMRestoreWorkArray(dm, numValues, PETSC_SCALAR, &values);CHKERRQ(ierr);
  ierr = PetscFree2(v0,J);CHKERRQ(ierr);
  ierr = PetscFree(sp);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "DMPlexProjectFunction"
/*@C
  DMPlexProjectFunction - This projects the given function into the function space provided.

  Input Parameters:
+ dm      - The DM
. fe      - The PetscFE associated with the field
. funcs   - The coordinate functions to evaluate, one per field
. ctxs    - Optional array of contexts to pass to each coordinate function.  ctxs itself may be null.
- mode    - The insertion mode for values

  Output Parameter:
. X - vector

  Level: developer

.seealso: DMPlexComputeL2Diff()
@*/
PetscErrorCode DMPlexProjectFunction(DM dm, PetscFE fe[], void (**funcs)(const PetscReal [], PetscScalar *, void *), void **ctxs, InsertMode mode, Vec X)
{
  Vec            localX;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(dm, DM_CLASSID, 1);
  ierr = DMGetLocalVector(dm, &localX);CHKERRQ(ierr);
  ierr = DMPlexProjectFunctionLocal(dm, fe, funcs, ctxs, mode, localX);CHKERRQ(ierr);
  ierr = DMLocalToGlobalBegin(dm, localX, mode, X);CHKERRQ(ierr);
  ierr = DMLocalToGlobalEnd(dm, localX, mode, X);CHKERRQ(ierr);
  ierr = DMRestoreLocalVector(dm, &localX);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "DMPlexInsertBoundaryValuesFEM"
PetscErrorCode DMPlexInsertBoundaryValuesFEM(DM dm, Vec localX)
{
  void        (**funcs)(const PetscReal x[], PetscScalar *u, void *ctx);
  void         **ctxs;
  PetscFE       *fe;
  PetscInt       numFields, f, numBd, b;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(dm, DM_CLASSID, 1);
  PetscValidHeaderSpecific(localX, VEC_CLASSID, 2);
  ierr = DMGetNumFields(dm, &numFields);CHKERRQ(ierr);
  ierr = PetscMalloc3(numFields,&fe,numFields,&funcs,numFields,&ctxs);CHKERRQ(ierr);
  for (f = 0; f < numFields; ++f) {ierr = DMGetField(dm, f, (PetscObject *) &fe[f]);CHKERRQ(ierr);}
  /* OPT: Could attempt to do multiple BCs at once */
  ierr = DMPlexGetNumBoundary(dm, &numBd);CHKERRQ(ierr);
  for (b = 0; b < numBd; ++b) {
    DMLabel         label;
    const PetscInt *ids;
    const char     *name;
    PetscInt        numids, field;
    PetscBool       isEssential;
    void          (*func)();
    void           *ctx;

    /* TODO: We need to set only the part indicated by the ids */
    ierr = DMPlexGetBoundary(dm, b, &isEssential, &name, &field, &func, &numids, &ids, &ctx);CHKERRQ(ierr);
    ierr = DMPlexGetLabel(dm, name, &label);CHKERRQ(ierr);
    for (f = 0; f < numFields; ++f) {
      funcs[f] = field == f ? (void (*)(const PetscReal[], PetscScalar *, void *)) func : NULL;
      ctxs[f]  = field == f ? ctx : NULL;
    }
    ierr = DMPlexProjectFunctionLabelLocal(dm, label, numids, ids, fe, funcs, ctxs, INSERT_BC_VALUES, localX);CHKERRQ(ierr);
  }
  ierr = PetscFree3(fe,funcs,ctxs);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

/* Assuming dim == 3 */
typedef struct {
  PetscScalar normal[3];   /* Area-scaled normals */
  PetscScalar centroid[3]; /* Location of centroid (quadrature point) */
  PetscScalar grad[2][3];  /* Face contribution to gradient in left and right cell */
} FaceGeom;

#undef __FUNCT__
#define __FUNCT__ "DMPlexInsertBoundaryValuesFVM"
PetscErrorCode DMPlexInsertBoundaryValuesFVM(DM dm, PetscReal time, Vec locX)
{
  DM                 dmFace;
  Vec                faceGeometry;
  DMLabel            label;
  const PetscScalar *facegeom;
  PetscScalar       *x;
  PetscInt           numBd, b;
  PetscErrorCode     ierr;

  PetscFunctionBegin;
  /* TODO Pull this geometry calculation into the library */
  ierr = PetscObjectQuery((PetscObject) dm, "FaceGeometry", (PetscObject *) &faceGeometry);CHKERRQ(ierr);
  ierr = DMPlexGetLabel(dm, "Face Sets", &label);CHKERRQ(ierr);
  ierr = DMPlexGetNumBoundary(dm, &numBd);CHKERRQ(ierr);
  ierr = VecGetDM(faceGeometry, &dmFace);CHKERRQ(ierr);
  ierr = VecGetArrayRead(faceGeometry, &facegeom);CHKERRQ(ierr);
  ierr = VecGetArray(locX, &x);CHKERRQ(ierr);
  for (b = 0; b < numBd; ++b) {
    PetscErrorCode (*func)(PetscReal,const PetscScalar*,const PetscScalar*,const PetscScalar*,PetscScalar*,void*);
    const PetscInt  *ids;
    PetscInt         numids, i;
    void            *ctx;

    ierr = DMPlexGetBoundary(dm, b, NULL, NULL, NULL, (void (**)()) &func, &numids, &ids, &ctx);CHKERRQ(ierr);
    for (i = 0; i < numids; ++i) {
      IS              faceIS;
      const PetscInt *faces;
      PetscInt        numFaces, f;

      ierr = DMLabelGetStratumIS(label, ids[i], &faceIS);CHKERRQ(ierr);
      if (!faceIS) continue; /* No points with that id on this process */
      ierr = ISGetLocalSize(faceIS, &numFaces);CHKERRQ(ierr);
      ierr = ISGetIndices(faceIS, &faces);CHKERRQ(ierr);
      for (f = 0; f < numFaces; ++f) {
        const PetscInt     face = faces[f], *cells;
        const PetscScalar *xI;
        PetscScalar       *xG;
        const FaceGeom    *fg;

        ierr = DMPlexPointLocalRead(dmFace, face, facegeom, &fg);CHKERRQ(ierr);
        ierr = DMPlexGetSupport(dm, face, &cells);CHKERRQ(ierr);
        ierr = DMPlexPointLocalRead(dm, cells[0], x, &xI);CHKERRQ(ierr);
        ierr = DMPlexPointLocalRef(dm, cells[1], x, &xG);CHKERRQ(ierr);
        ierr = (*func)(time, fg->centroid, fg->normal, xI, xG, ctx);CHKERRQ(ierr);
      }
      ierr = ISRestoreIndices(faceIS, &faces);CHKERRQ(ierr);
      ierr = ISDestroy(&faceIS);CHKERRQ(ierr);
    }
  }
  ierr = VecRestoreArrayRead(faceGeometry, &facegeom);CHKERRQ(ierr);
  ierr = VecRestoreArray(locX, &x);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "DMPlexComputeL2Diff"
/*@C
  DMPlexComputeL2Diff - This function computes the L_2 difference between a function u and an FEM interpolant solution u_h.

  Input Parameters:
+ dm    - The DM
. fe    - The PetscFE object for each field
. funcs - The functions to evaluate for each field component
. ctxs  - Optional array of contexts to pass to each function, or NULL.
- X     - The coefficient vector u_h

  Output Parameter:
. diff - The diff ||u - u_h||_2

  Level: developer

.seealso: DMPlexProjectFunction(), DMPlexComputeL2GradientDiff()
@*/
PetscErrorCode DMPlexComputeL2Diff(DM dm, PetscFE fe[], void (**funcs)(const PetscReal [], PetscScalar *, void *), void **ctxs, Vec X, PetscReal *diff)
{
  const PetscInt  debug = 0;
  PetscSection    section;
  PetscQuadrature quad;
  Vec             localX;
  PetscScalar    *funcVal;
  PetscReal      *coords, *v0, *J, *invJ, detJ;
  PetscReal       localDiff = 0.0;
  PetscInt        dim, numFields, numComponents = 0, cStart, cEnd, c, field, fieldOffset, comp;
  PetscErrorCode  ierr;

  PetscFunctionBegin;
  ierr = DMPlexGetDimension(dm, &dim);CHKERRQ(ierr);
  ierr = DMGetDefaultSection(dm, &section);CHKERRQ(ierr);
  ierr = PetscSectionGetNumFields(section, &numFields);CHKERRQ(ierr);
  ierr = DMGetLocalVector(dm, &localX);CHKERRQ(ierr);
  ierr = DMGlobalToLocalBegin(dm, X, INSERT_VALUES, localX);CHKERRQ(ierr);
  ierr = DMGlobalToLocalEnd(dm, X, INSERT_VALUES, localX);CHKERRQ(ierr);
  for (field = 0; field < numFields; ++field) {
    PetscInt Nc;

    ierr = PetscFEGetNumComponents(fe[field], &Nc);CHKERRQ(ierr);
    numComponents += Nc;
  }
  ierr = DMPlexProjectFunctionLocal(dm, fe, funcs, ctxs, INSERT_BC_VALUES, localX);CHKERRQ(ierr);
  ierr = PetscMalloc5(numComponents,&funcVal,dim,&coords,dim,&v0,dim*dim,&J,dim*dim,&invJ);CHKERRQ(ierr);
  ierr = DMPlexGetHeightStratum(dm, 0, &cStart, &cEnd);CHKERRQ(ierr);
  ierr = PetscFEGetQuadrature(fe[0], &quad);CHKERRQ(ierr);
  for (c = cStart; c < cEnd; ++c) {
    PetscScalar *x = NULL;
    PetscReal    elemDiff = 0.0;

    ierr = DMPlexComputeCellGeometry(dm, c, v0, J, invJ, &detJ);CHKERRQ(ierr);
    if (detJ <= 0.0) SETERRQ2(PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Invalid determinant %g for element %d", detJ, c);
    ierr = DMPlexVecGetClosure(dm, NULL, localX, c, NULL, &x);CHKERRQ(ierr);

    for (field = 0, comp = 0, fieldOffset = 0; field < numFields; ++field) {
      void * const     ctx = ctxs ? ctxs[field] : NULL;
      const PetscReal *quadPoints, *quadWeights;
      PetscReal       *basis;
      PetscInt         numQuadPoints, numBasisFuncs, numBasisComps, q, d, e, fc, f;

      ierr = PetscQuadratureGetData(quad, NULL, &numQuadPoints, &quadPoints, &quadWeights);CHKERRQ(ierr);
      ierr = PetscFEGetDimension(fe[field], &numBasisFuncs);CHKERRQ(ierr);
      ierr = PetscFEGetNumComponents(fe[field], &numBasisComps);CHKERRQ(ierr);
      ierr = PetscFEGetDefaultTabulation(fe[field], &basis, NULL, NULL);CHKERRQ(ierr);
      if (debug) {
        char title[1024];
        ierr = PetscSNPrintf(title, 1023, "Solution for Field %d", field);CHKERRQ(ierr);
        ierr = DMPrintCellVector(c, title, numBasisFuncs*numBasisComps, &x[fieldOffset]);CHKERRQ(ierr);
      }
      for (q = 0; q < numQuadPoints; ++q) {
        for (d = 0; d < dim; d++) {
          coords[d] = v0[d];
          for (e = 0; e < dim; e++) {
            coords[d] += J[d*dim+e]*(quadPoints[q*dim+e] + 1.0);
          }
        }
        (*funcs[field])(coords, funcVal, ctx);
        for (fc = 0; fc < numBasisComps; ++fc) {
          PetscScalar interpolant = 0.0;

          for (f = 0; f < numBasisFuncs; ++f) {
            const PetscInt fidx = f*numBasisComps+fc;
            interpolant += x[fieldOffset+fidx]*basis[q*numBasisFuncs*numBasisComps+fidx];
          }
          if (debug) {ierr = PetscPrintf(PETSC_COMM_SELF, "    elem %d field %d diff %g\n", c, field, PetscSqr(PetscRealPart(interpolant - funcVal[fc]))*quadWeights[q]*detJ);CHKERRQ(ierr);}
          elemDiff += PetscSqr(PetscRealPart(interpolant - funcVal[fc]))*quadWeights[q]*detJ;
        }
      }
      comp        += numBasisComps;
      fieldOffset += numBasisFuncs*numBasisComps;
    }
    ierr = DMPlexVecRestoreClosure(dm, NULL, localX, c, NULL, &x);CHKERRQ(ierr);
    if (debug) {ierr = PetscPrintf(PETSC_COMM_SELF, "  elem %d diff %g\n", c, elemDiff);CHKERRQ(ierr);}
    localDiff += elemDiff;
  }
  ierr  = PetscFree5(funcVal,coords,v0,J,invJ);CHKERRQ(ierr);
  ierr  = DMRestoreLocalVector(dm, &localX);CHKERRQ(ierr);
  ierr  = MPI_Allreduce(&localDiff, diff, 1, MPIU_REAL, MPI_SUM, PetscObjectComm((PetscObject)dm));CHKERRQ(ierr);
  *diff = PetscSqrtReal(*diff);
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "DMPlexComputeL2GradientDiff"
/*@C
  DMPlexComputeL2GradientDiff - This function computes the L_2 difference between the gradient of a function u and an FEM interpolant solution grad u_h.

  Input Parameters:
+ dm    - The DM
. fe    - The PetscFE object for each field
. funcs - The gradient functions to evaluate for each field component
. ctxs  - Optional array of contexts to pass to each function, or NULL.
. X     - The coefficient vector u_h
- n     - The vector to project along

  Output Parameter:
. diff - The diff ||(grad u - grad u_h) . n||_2

  Level: developer

.seealso: DMPlexProjectFunction(), DMPlexComputeL2Diff()
@*/
PetscErrorCode DMPlexComputeL2GradientDiff(DM dm, PetscFE fe[], void (**funcs)(const PetscReal [], const PetscReal [], PetscScalar *, void *), void **ctxs, Vec X, const PetscReal n[], PetscReal *diff)
{
  const PetscInt  debug = 0;
  PetscSection    section;
  PetscQuadrature quad;
  Vec             localX;
  PetscScalar    *funcVal, *interpolantVec;
  PetscReal      *coords, *realSpaceDer, *v0, *J, *invJ, detJ;
  PetscReal       localDiff = 0.0;
  PetscInt        dim, numFields, numComponents = 0, cStart, cEnd, c, field, fieldOffset, comp;
  PetscErrorCode  ierr;

  PetscFunctionBegin;
  ierr = DMPlexGetDimension(dm, &dim);CHKERRQ(ierr);
  ierr = DMGetDefaultSection(dm, &section);CHKERRQ(ierr);
  ierr = PetscSectionGetNumFields(section, &numFields);CHKERRQ(ierr);
  ierr = DMGetLocalVector(dm, &localX);CHKERRQ(ierr);
  ierr = DMGlobalToLocalBegin(dm, X, INSERT_VALUES, localX);CHKERRQ(ierr);
  ierr = DMGlobalToLocalEnd(dm, X, INSERT_VALUES, localX);CHKERRQ(ierr);
  for (field = 0; field < numFields; ++field) {
    PetscInt Nc;

    ierr = PetscFEGetNumComponents(fe[field], &Nc);CHKERRQ(ierr);
    numComponents += Nc;
  }
  /* ierr = DMPlexProjectFunctionLocal(dm, fe, funcs, INSERT_BC_VALUES, localX);CHKERRQ(ierr); */
  ierr = PetscMalloc7(numComponents,&funcVal,dim,&coords,dim,&realSpaceDer,dim,&v0,dim*dim,&J,dim*dim,&invJ,dim,&interpolantVec);CHKERRQ(ierr);
  ierr = DMPlexGetHeightStratum(dm, 0, &cStart, &cEnd);CHKERRQ(ierr);
  ierr = PetscFEGetQuadrature(fe[0], &quad);CHKERRQ(ierr);
  for (c = cStart; c < cEnd; ++c) {
    PetscScalar *x = NULL;
    PetscReal    elemDiff = 0.0;

    ierr = DMPlexComputeCellGeometry(dm, c, v0, J, invJ, &detJ);CHKERRQ(ierr);
    if (detJ <= 0.0) SETERRQ2(PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Invalid determinant %g for element %d", detJ, c);
    ierr = DMPlexVecGetClosure(dm, NULL, localX, c, NULL, &x);CHKERRQ(ierr);

    for (field = 0, comp = 0, fieldOffset = 0; field < numFields; ++field) {
      void * const     ctx = ctxs ? ctxs[field] : NULL;
      const PetscReal *quadPoints, *quadWeights;
      PetscReal       *basisDer;
      PetscInt         numQuadPoints, Nb, Ncomp, q, d, e, fc, f, g;

      ierr = PetscQuadratureGetData(quad, NULL, &numQuadPoints, &quadPoints, &quadWeights);CHKERRQ(ierr);
      ierr = PetscFEGetDimension(fe[field], &Nb);CHKERRQ(ierr);
      ierr = PetscFEGetNumComponents(fe[field], &Ncomp);CHKERRQ(ierr);
      ierr = PetscFEGetDefaultTabulation(fe[field], NULL, &basisDer, NULL);CHKERRQ(ierr);
      if (debug) {
        char title[1024];
        ierr = PetscSNPrintf(title, 1023, "Solution for Field %d", field);CHKERRQ(ierr);
        ierr = DMPrintCellVector(c, title, Nb*Ncomp, &x[fieldOffset]);CHKERRQ(ierr);
      }
      for (q = 0; q < numQuadPoints; ++q) {
        for (d = 0; d < dim; d++) {
          coords[d] = v0[d];
          for (e = 0; e < dim; e++) {
            coords[d] += J[d*dim+e]*(quadPoints[q*dim+e] + 1.0);
          }
        }
        (*funcs[field])(coords, n, funcVal, ctx);
        for (fc = 0; fc < Ncomp; ++fc) {
          PetscScalar interpolant = 0.0;

          for (d = 0; d < dim; ++d) interpolantVec[d] = 0.0;
          for (f = 0; f < Nb; ++f) {
            const PetscInt fidx = f*Ncomp+fc;

            for (d = 0; d < dim; ++d) {
              realSpaceDer[d] = 0.0;
              for (g = 0; g < dim; ++g) {
                realSpaceDer[d] += invJ[g*dim+d]*basisDer[(q*Nb*Ncomp+fidx)*dim+g];
              }
              interpolantVec[d] += x[fieldOffset+fidx]*realSpaceDer[d];
            }
          }
          for (d = 0; d < dim; ++d) interpolant += interpolantVec[d]*n[d];
          if (debug) {ierr = PetscPrintf(PETSC_COMM_SELF, "    elem %d fieldDer %d diff %g\n", c, field, PetscSqr(PetscRealPart(interpolant - funcVal[fc]))*quadWeights[q]*detJ);CHKERRQ(ierr);}
          elemDiff += PetscSqr(PetscRealPart(interpolant - funcVal[fc]))*quadWeights[q]*detJ;
        }
      }
      comp        += Ncomp;
      fieldOffset += Nb*Ncomp;
    }
    ierr = DMPlexVecRestoreClosure(dm, NULL, localX, c, NULL, &x);CHKERRQ(ierr);
    if (debug) {ierr = PetscPrintf(PETSC_COMM_SELF, "  elem %d diff %g\n", c, elemDiff);CHKERRQ(ierr);}
    localDiff += elemDiff;
  }
  ierr  = PetscFree7(funcVal,coords,realSpaceDer,v0,J,invJ,interpolantVec);CHKERRQ(ierr);
  ierr  = DMRestoreLocalVector(dm, &localX);CHKERRQ(ierr);
  ierr  = MPI_Allreduce(&localDiff, diff, 1, MPIU_REAL, MPI_SUM, PetscObjectComm((PetscObject)dm));CHKERRQ(ierr);
  *diff = PetscSqrtReal(*diff);
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "DMPlexComputeResidualFEM"
/*@
  DMPlexComputeResidualFEM - Form the local residual F from the local input X using pointwise functions specified by the user

  Input Parameters:
+ dm - The mesh
. X  - Local input vector
- user - The user context

  Output Parameter:
. F  - Local output vector

  Note:
  The first member of the user context must be an FEMContext.

  We form the residual one batch of elements at a time. This allows us to offload work onto an accelerator,
  like a GPU, or vectorize on a multicore machine.

  Level: developer

.seealso: DMPlexComputeJacobianActionFEM()
@*/
PetscErrorCode DMPlexComputeResidualFEM(DM dm, Vec X, Vec F, void *user)
{
  DM_Plex          *mesh  = (DM_Plex *) dm->data;
  PetscFEM         *fem   = (PetscFEM *) user;
  PetscFE          *fe    = fem->fe;
  PetscFE          *feAux = fem->feAux;
  PetscFE          *feBd  = fem->feBd;
  const char       *name  = "Residual";
  DM                dmAux;
  Vec               A;
  PetscQuadrature   q;
  PetscCellGeometry geom;
  PetscSection      section, sectionAux;
  PetscReal        *v0, *J, *invJ, *detJ;
  PetscScalar      *elemVec, *u, *a = NULL;
  PetscInt          dim, Nf, NfAux = 0, f, numCells, cStart, cEnd, c;
  PetscInt          cellDof = 0, numComponents = 0;
  PetscInt          cellDofAux = 0, numComponentsAux = 0;
  PetscErrorCode    ierr;

  PetscFunctionBegin;
  ierr = PetscLogEventBegin(DMPLEX_ResidualFEM,dm,0,0,0);CHKERRQ(ierr);
  ierr = DMPlexGetDimension(dm, &dim);CHKERRQ(ierr);
  ierr = DMGetDefaultSection(dm, &section);CHKERRQ(ierr);
  ierr = PetscSectionGetNumFields(section, &Nf);CHKERRQ(ierr);
  ierr = DMPlexGetHeightStratum(dm, 0, &cStart, &cEnd);CHKERRQ(ierr);
  numCells = cEnd - cStart;
  for (f = 0; f < Nf; ++f) {
    PetscInt Nb, Nc;

    ierr = PetscFEGetDimension(fe[f], &Nb);CHKERRQ(ierr);
    ierr = PetscFEGetNumComponents(fe[f], &Nc);CHKERRQ(ierr);
    cellDof       += Nb*Nc;
    numComponents += Nc;
  }
  ierr = PetscObjectQuery((PetscObject) dm, "dmAux", (PetscObject *) &dmAux);CHKERRQ(ierr);
  ierr = PetscObjectQuery((PetscObject) dm, "A", (PetscObject *) &A);CHKERRQ(ierr);
  if (dmAux) {
    ierr = DMGetDefaultSection(dmAux, &sectionAux);CHKERRQ(ierr);
    ierr = PetscSectionGetNumFields(sectionAux, &NfAux);CHKERRQ(ierr);
  }
  for (f = 0; f < NfAux; ++f) {
    PetscInt Nb, Nc;

    ierr = PetscFEGetDimension(feAux[f], &Nb);CHKERRQ(ierr);
    ierr = PetscFEGetNumComponents(feAux[f], &Nc);CHKERRQ(ierr);
    cellDofAux       += Nb*Nc;
    numComponentsAux += Nc;
  }
  ierr = DMPlexInsertBoundaryValuesFEM(dm, X);CHKERRQ(ierr);
  ierr = VecSet(F, 0.0);CHKERRQ(ierr);
  ierr = PetscMalloc6(numCells*cellDof,&u,numCells*dim,&v0,numCells*dim*dim,&J,numCells*dim*dim,&invJ,numCells,&detJ,numCells*cellDof,&elemVec);CHKERRQ(ierr);
  if (dmAux) {ierr = PetscMalloc1(numCells*cellDofAux, &a);CHKERRQ(ierr);}
  for (c = cStart; c < cEnd; ++c) {
    PetscScalar *x = NULL;
    PetscInt     i;

    ierr = DMPlexComputeCellGeometry(dm, c, &v0[c*dim], &J[c*dim*dim], &invJ[c*dim*dim], &detJ[c]);CHKERRQ(ierr);
    if (detJ[c] <= 0.0) SETERRQ2(PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Invalid determinant %g for element %d", detJ[c], c);
    ierr = DMPlexVecGetClosure(dm, section, X, c, NULL, &x);CHKERRQ(ierr);
    for (i = 0; i < cellDof; ++i) u[c*cellDof+i] = x[i];
    ierr = DMPlexVecRestoreClosure(dm, section, X, c, NULL, &x);CHKERRQ(ierr);
    if (dmAux) {
      ierr = DMPlexVecGetClosure(dmAux, sectionAux, A, c, NULL, &x);CHKERRQ(ierr);
      for (i = 0; i < cellDofAux; ++i) a[c*cellDofAux+i] = x[i];
      ierr = DMPlexVecRestoreClosure(dmAux, sectionAux, A, c, NULL, &x);CHKERRQ(ierr);
    }
  }
  for (f = 0; f < Nf; ++f) {
    void   (*f0)(const PetscScalar[], const PetscScalar[], const PetscScalar[], const PetscScalar[], const PetscReal[], PetscScalar[]) = fem->f0Funcs[f];
    void   (*f1)(const PetscScalar[], const PetscScalar[], const PetscScalar[], const PetscScalar[], const PetscReal[], PetscScalar[]) = fem->f1Funcs[f];
    PetscInt numQuadPoints, Nb;
    /* Conforming batches */
    PetscInt numChunks, numBatches, numBlocks, Ne, blockSize, batchSize;
    /* Remainder */
    PetscInt Nr, offset;

    ierr = PetscFEGetQuadrature(fe[f], &q);CHKERRQ(ierr);
    ierr = PetscFEGetDimension(fe[f], &Nb);CHKERRQ(ierr);
    ierr = PetscFEGetTileSizes(fe[f], NULL, &numBlocks, NULL, &numBatches);CHKERRQ(ierr);
    ierr = PetscQuadratureGetData(q, NULL, &numQuadPoints, NULL, NULL);CHKERRQ(ierr);
    blockSize = Nb*numQuadPoints;
    batchSize = numBlocks * blockSize;
    ierr =  PetscFESetTileSizes(fe[f], blockSize, numBlocks, batchSize, numBatches);CHKERRQ(ierr);
    numChunks = numCells / (numBatches*batchSize);
    Ne        = numChunks*numBatches*batchSize;
    Nr        = numCells % (numBatches*batchSize);
    offset    = numCells - Nr;
    geom.v0   = v0;
    geom.J    = J;
    geom.invJ = invJ;
    geom.detJ = detJ;
    ierr = PetscFEIntegrateResidual(fe[f], Ne, Nf, fe, f, geom, u, NfAux, feAux, a, f0, f1, elemVec);CHKERRQ(ierr);
    geom.v0   = &v0[offset*dim];
    geom.J    = &J[offset*dim*dim];
    geom.invJ = &invJ[offset*dim*dim];
    geom.detJ = &detJ[offset];
    ierr = PetscFEIntegrateResidual(fe[f], Nr, Nf, fe, f, geom, &u[offset*cellDof], NfAux, feAux, &a[offset*cellDofAux], f0, f1, &elemVec[offset*cellDof]);CHKERRQ(ierr);
  }
  for (c = cStart; c < cEnd; ++c) {
    if (mesh->printFEM > 1) {ierr = DMPrintCellVector(c, name, cellDof, &elemVec[c*cellDof]);CHKERRQ(ierr);}
    ierr = DMPlexVecSetClosure(dm, section, F, c, &elemVec[c*cellDof], ADD_VALUES);CHKERRQ(ierr);
  }
  ierr = PetscFree6(u,v0,J,invJ,detJ,elemVec);CHKERRQ(ierr);
  if (dmAux) {ierr = PetscFree(a);CHKERRQ(ierr);}
  if (feBd) {
    DMLabel         label, depth;
    IS              pointIS;
    const PetscInt *points;
    PetscInt        dep, numPoints, p, numFaces;
    PetscReal      *n;

    ierr = DMPlexGetLabel(dm, "boundary", &label);CHKERRQ(ierr);
    ierr = DMPlexGetDepthLabel(dm, &depth);CHKERRQ(ierr);
    ierr = DMLabelGetStratumSize(label, 1, &numPoints);CHKERRQ(ierr);
    ierr = DMLabelGetStratumIS(label, 1, &pointIS);CHKERRQ(ierr);
    ierr = ISGetIndices(pointIS, &points);CHKERRQ(ierr);
    for (f = 0, cellDof = 0, numComponents = 0; f < Nf; ++f) {
      PetscInt Nb, Nc;

      ierr = PetscFEGetDimension(feBd[f], &Nb);CHKERRQ(ierr);
      ierr = PetscFEGetNumComponents(feBd[f], &Nc);CHKERRQ(ierr);
      cellDof       += Nb*Nc;
      numComponents += Nc;
    }
    for (p = 0, numFaces = 0; p < numPoints; ++p) {
      ierr = DMLabelGetValue(depth, points[p], &dep);CHKERRQ(ierr);
      if (dep == dim-1) ++numFaces;
    }
    ierr = PetscMalloc7(numFaces*cellDof,&u,numFaces*dim,&v0,numFaces*dim,&n,numFaces*dim*dim,&J,numFaces*dim*dim,&invJ,numFaces,&detJ,numFaces*cellDof,&elemVec);CHKERRQ(ierr);
    for (p = 0, f = 0; p < numPoints; ++p) {
      const PetscInt point = points[p];
      PetscScalar   *x     = NULL;
      PetscInt       i;

      ierr = DMLabelGetValue(depth, points[p], &dep);CHKERRQ(ierr);
      if (dep != dim-1) continue;
      ierr = DMPlexComputeCellGeometry(dm, point, &v0[f*dim], &J[f*dim*dim], &invJ[f*dim*dim], &detJ[f]);CHKERRQ(ierr);
      ierr = DMPlexComputeCellGeometryFVM(dm, point, NULL, NULL, &n[f*dim]);
      if (detJ[f] <= 0.0) SETERRQ2(PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Invalid determinant %g for face %d", detJ[f], point);
      ierr = DMPlexVecGetClosure(dm, section, X, point, NULL, &x);CHKERRQ(ierr);
      for (i = 0; i < cellDof; ++i) u[f*cellDof+i] = x[i];
      ierr = DMPlexVecRestoreClosure(dm, section, X, point, NULL, &x);CHKERRQ(ierr);
      ++f;
    }
    for (f = 0; f < Nf; ++f) {
      void   (*f0)(const PetscScalar[], const PetscScalar[], const PetscScalar[], const PetscScalar[], const PetscReal[], const PetscReal[], PetscScalar[]) = fem->f0BdFuncs[f];
      void   (*f1)(const PetscScalar[], const PetscScalar[], const PetscScalar[], const PetscScalar[], const PetscReal[], const PetscReal[], PetscScalar[]) = fem->f1BdFuncs[f];
      PetscInt numQuadPoints, Nb;
      /* Conforming batches */
      PetscInt numChunks, numBatches, numBlocks, Ne, blockSize, batchSize;
      /* Remainder */
      PetscInt Nr, offset;

      ierr = PetscFEGetQuadrature(feBd[f], &q);CHKERRQ(ierr);
      ierr = PetscFEGetDimension(feBd[f], &Nb);CHKERRQ(ierr);
      ierr = PetscFEGetTileSizes(feBd[f], NULL, &numBlocks, NULL, &numBatches);CHKERRQ(ierr);
      ierr = PetscQuadratureGetData(q, NULL, &numQuadPoints, NULL, NULL);CHKERRQ(ierr);
      blockSize = Nb*numQuadPoints;
      batchSize = numBlocks * blockSize;
      ierr =  PetscFESetTileSizes(feBd[f], blockSize, numBlocks, batchSize, numBatches);CHKERRQ(ierr);
      numChunks = numFaces / (numBatches*batchSize);
      Ne        = numChunks*numBatches*batchSize;
      Nr        = numFaces % (numBatches*batchSize);
      offset    = numFaces - Nr;
      geom.v0   = v0;
      geom.n    = n;
      geom.J    = J;
      geom.invJ = invJ;
      geom.detJ = detJ;
      ierr = PetscFEIntegrateBdResidual(feBd[f], Ne, Nf, feBd, f, geom, u, 0, NULL, NULL, f0, f1, elemVec);CHKERRQ(ierr);
      geom.v0   = &v0[offset*dim];
      geom.n    = &n[offset*dim];
      geom.J    = &J[offset*dim*dim];
      geom.invJ = &invJ[offset*dim*dim];
      geom.detJ = &detJ[offset];
      ierr = PetscFEIntegrateBdResidual(feBd[f], Nr, Nf, feBd, f, geom, &u[offset*cellDof], 0, NULL, NULL, f0, f1, &elemVec[offset*cellDof]);CHKERRQ(ierr);
    }
    for (p = 0, f = 0; p < numPoints; ++p) {
      const PetscInt point = points[p];

      ierr = DMLabelGetValue(depth, point, &dep);CHKERRQ(ierr);
      if (dep != dim-1) continue;
      if (mesh->printFEM > 1) {ierr = DMPrintCellVector(point, "BdResidual", cellDof, &elemVec[f*cellDof]);CHKERRQ(ierr);}
      ierr = DMPlexVecSetClosure(dm, NULL, F, point, &elemVec[f*cellDof], ADD_VALUES);CHKERRQ(ierr);
      ++f;
    }
    ierr = ISRestoreIndices(pointIS, &points);CHKERRQ(ierr);
    ierr = ISDestroy(&pointIS);CHKERRQ(ierr);
    ierr = PetscFree7(u,v0,n,J,invJ,detJ,elemVec);CHKERRQ(ierr);
  }
  if (mesh->printFEM) {ierr = DMPrintLocalVec(dm, name, mesh->printTol, F);CHKERRQ(ierr);}
  ierr = PetscLogEventEnd(DMPLEX_ResidualFEM,dm,0,0,0);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "DMPlexComputeIFunctionFEM"
/*@
  DMPlexComputeIFunctionFEM - Form the local implicit function F from the local input X, X_t using pointwise functions specified by the user

  Input Parameters:
+ dm - The mesh
. time - The current time
. X  - Local input vector
. X_t  - Time derivative of the local input vector
- user - The user context

  Output Parameter:
. F  - Local output vector

  Note:
  The first member of the user context must be an FEMContext.

  We form the residual one batch of elements at a time. This allows us to offload work onto an accelerator,
  like a GPU, or vectorize on a multicore machine.

  Level: developer

.seealso: DMPlexComputeResidualFEM()
@*/
PetscErrorCode DMPlexComputeIFunctionFEM(DM dm, PetscReal time, Vec X, Vec X_t, Vec F, void *user)
{
  DM_Plex          *mesh  = (DM_Plex *) dm->data;
  PetscFEM         *fem   = (PetscFEM *) user;
  PetscFE          *fe    = fem->fe;
  PetscFE          *feAux = fem->feAux;
  PetscFE          *feBd  = fem->feBd;
  const char       *name  = "Residual";
  DM                dmAux;
  Vec               A;
  PetscQuadrature   q;
  PetscCellGeometry geom;
  PetscSection      section, sectionAux;
  PetscReal        *v0, *J, *invJ, *detJ;
  PetscScalar      *elemVec, *u, *u_t, *a = NULL;
  PetscInt          dim, Nf, NfAux = 0, f, numCells, cStart, cEnd, c;
  PetscInt          cellDof = 0, numComponents = 0;
  PetscInt          cellDofAux = 0, numComponentsAux = 0;
  PetscErrorCode    ierr;

  PetscFunctionBegin;
  ierr = PetscLogEventBegin(DMPLEX_ResidualFEM,dm,0,0,0);CHKERRQ(ierr);
  ierr = DMPlexGetDimension(dm, &dim);CHKERRQ(ierr);
  ierr = DMGetDefaultSection(dm, &section);CHKERRQ(ierr);
  ierr = PetscSectionGetNumFields(section, &Nf);CHKERRQ(ierr);
  ierr = DMPlexGetHeightStratum(dm, 0, &cStart, &cEnd);CHKERRQ(ierr);
  numCells = cEnd - cStart;
  for (f = 0; f < Nf; ++f) {
    PetscInt Nb, Nc;

    ierr = PetscFEGetDimension(fe[f], &Nb);CHKERRQ(ierr);
    ierr = PetscFEGetNumComponents(fe[f], &Nc);CHKERRQ(ierr);
    cellDof       += Nb*Nc;
    numComponents += Nc;
  }
  ierr = PetscObjectQuery((PetscObject) dm, "dmAux", (PetscObject *) &dmAux);CHKERRQ(ierr);
  ierr = PetscObjectQuery((PetscObject) dm, "A", (PetscObject *) &A);CHKERRQ(ierr);
  if (dmAux) {
    ierr = DMGetDefaultSection(dmAux, &sectionAux);CHKERRQ(ierr);
    ierr = PetscSectionGetNumFields(sectionAux, &NfAux);CHKERRQ(ierr);
  }
  for (f = 0; f < NfAux; ++f) {
    PetscInt Nb, Nc;

    ierr = PetscFEGetDimension(feAux[f], &Nb);CHKERRQ(ierr);
    ierr = PetscFEGetNumComponents(feAux[f], &Nc);CHKERRQ(ierr);
    cellDofAux       += Nb*Nc;
    numComponentsAux += Nc;
  }
  ierr = DMPlexInsertBoundaryValuesFEM(dm, X);CHKERRQ(ierr);
  ierr = VecSet(F, 0.0);CHKERRQ(ierr);
  ierr = PetscMalloc7(numCells*cellDof,&u,numCells*cellDof,&u_t,numCells*dim,&v0,numCells*dim*dim,&J,numCells*dim*dim,&invJ,numCells,&detJ,numCells*cellDof,&elemVec);CHKERRQ(ierr);
  if (dmAux) {ierr = PetscMalloc1(numCells*cellDofAux, &a);CHKERRQ(ierr);}
  for (c = cStart; c < cEnd; ++c) {
    PetscScalar *x = NULL, *x_t = NULL;
    PetscInt     i;

    ierr = DMPlexComputeCellGeometry(dm, c, &v0[c*dim], &J[c*dim*dim], &invJ[c*dim*dim], &detJ[c]);CHKERRQ(ierr);
    if (detJ[c] <= 0.0) SETERRQ2(PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Invalid determinant %g for element %d", detJ[c], c);
    ierr = DMPlexVecGetClosure(dm, section, X, c, NULL, &x);CHKERRQ(ierr);
    for (i = 0; i < cellDof; ++i) u[c*cellDof+i] = x[i];
    ierr = DMPlexVecRestoreClosure(dm, section, X, c, NULL, &x);CHKERRQ(ierr);
    ierr = DMPlexVecGetClosure(dm, section, X_t, c, NULL, &x_t);CHKERRQ(ierr);
    for (i = 0; i < cellDof; ++i) u_t[c*cellDof+i] = x_t[i];
    ierr = DMPlexVecRestoreClosure(dm, section, X_t, c, NULL, &x_t);CHKERRQ(ierr);
    if (dmAux) {
      PetscScalar *x_a = NULL;
      ierr = DMPlexVecGetClosure(dmAux, sectionAux, A, c, NULL, &x_a);CHKERRQ(ierr);
      for (i = 0; i < cellDofAux; ++i) a[c*cellDofAux+i] = x_a[i];
      ierr = DMPlexVecRestoreClosure(dmAux, sectionAux, A, c, NULL, &x_a);CHKERRQ(ierr);
    }
  }
  for (f = 0; f < Nf; ++f) {
    void   (*f0)(const PetscScalar[], const PetscScalar[], const PetscScalar[], const PetscScalar[], const PetscScalar[], const PetscReal[], PetscScalar[]) = fem->f0IFuncs[f];
    void   (*f1)(const PetscScalar[], const PetscScalar[], const PetscScalar[], const PetscScalar[], const PetscScalar[], const PetscReal[], PetscScalar[]) = fem->f1IFuncs[f];
    PetscInt numQuadPoints, Nb;
    /* Conforming batches */
    PetscInt numChunks, numBatches, numBlocks, Ne, blockSize, batchSize;
    /* Remainder */
    PetscInt Nr, offset;

    ierr = PetscFEGetQuadrature(fe[f], &q);CHKERRQ(ierr);
    ierr = PetscFEGetDimension(fe[f], &Nb);CHKERRQ(ierr);
    ierr = PetscFEGetTileSizes(fe[f], NULL, &numBlocks, NULL, &numBatches);CHKERRQ(ierr);
    ierr = PetscQuadratureGetData(q, NULL, &numQuadPoints, NULL, NULL);CHKERRQ(ierr);
    blockSize = Nb*numQuadPoints;
    batchSize = numBlocks * blockSize;
    ierr =  PetscFESetTileSizes(fe[f], blockSize, numBlocks, batchSize, numBatches);CHKERRQ(ierr);
    numChunks = numCells / (numBatches*batchSize);
    Ne        = numChunks*numBatches*batchSize;
    Nr        = numCells % (numBatches*batchSize);
    offset    = numCells - Nr;
    geom.v0   = v0;
    geom.J    = J;
    geom.invJ = invJ;
    geom.detJ = detJ;
    ierr = PetscFEIntegrateIFunction(fe[f], Ne, Nf, fe, f, geom, u, u_t, NfAux, feAux, a, f0, f1, elemVec);CHKERRQ(ierr);
    geom.v0   = &v0[offset*dim];
    geom.J    = &J[offset*dim*dim];
    geom.invJ = &invJ[offset*dim*dim];
    geom.detJ = &detJ[offset];
    ierr = PetscFEIntegrateIFunction(fe[f], Nr, Nf, fe, f, geom, &u[offset*cellDof], &u_t[offset*cellDof], NfAux, feAux, &a[offset*cellDofAux], f0, f1, &elemVec[offset*cellDof]);CHKERRQ(ierr);
  }
  for (c = cStart; c < cEnd; ++c) {
    if (mesh->printFEM > 1) {ierr = DMPrintCellVector(c, name, cellDof, &elemVec[c*cellDof]);CHKERRQ(ierr);}
    ierr = DMPlexVecSetClosure(dm, section, F, c, &elemVec[c*cellDof], ADD_VALUES);CHKERRQ(ierr);
  }
  ierr = PetscFree7(u,u_t,v0,J,invJ,detJ,elemVec);CHKERRQ(ierr);
  if (dmAux) {ierr = PetscFree(a);CHKERRQ(ierr);}
  if (feBd) {
    DMLabel         label, depth;
    IS              pointIS;
    const PetscInt *points;
    PetscInt        dep, numPoints, p, numFaces;
    PetscReal      *n;

    ierr = DMPlexGetLabel(dm, "boundary", &label);CHKERRQ(ierr);
    ierr = DMPlexGetDepthLabel(dm, &depth);CHKERRQ(ierr);
    ierr = DMLabelGetStratumSize(label, 1, &numPoints);CHKERRQ(ierr);
    ierr = DMLabelGetStratumIS(label, 1, &pointIS);CHKERRQ(ierr);
    ierr = ISGetIndices(pointIS, &points);CHKERRQ(ierr);
    for (f = 0, cellDof = 0, numComponents = 0; f < Nf; ++f) {
      PetscInt Nb, Nc;

      ierr = PetscFEGetDimension(feBd[f], &Nb);CHKERRQ(ierr);
      ierr = PetscFEGetNumComponents(feBd[f], &Nc);CHKERRQ(ierr);
      cellDof       += Nb*Nc;
      numComponents += Nc;
    }
    for (p = 0, numFaces = 0; p < numPoints; ++p) {
      ierr = DMLabelGetValue(depth, points[p], &dep);CHKERRQ(ierr);
      if (dep == dim-1) ++numFaces;
    }
    ierr = PetscMalloc7(numFaces*cellDof,&u,numFaces*dim,&v0,numFaces*dim,&n,numFaces*dim*dim,&J,numFaces*dim*dim,&invJ,numFaces,&detJ,numFaces*cellDof,&elemVec);CHKERRQ(ierr);
    ierr = PetscMalloc1(numFaces*cellDof,&u_t);CHKERRQ(ierr);
    for (p = 0, f = 0; p < numPoints; ++p) {
      const PetscInt point = points[p];
      PetscScalar   *x     = NULL;
      PetscInt       i;

      ierr = DMLabelGetValue(depth, points[p], &dep);CHKERRQ(ierr);
      if (dep != dim-1) continue;
      ierr = DMPlexComputeCellGeometry(dm, point, &v0[f*dim], &J[f*dim*dim], &invJ[f*dim*dim], &detJ[f]);CHKERRQ(ierr);
      ierr = DMPlexComputeCellGeometryFVM(dm, point, NULL, NULL, &n[f*dim]);
      if (detJ[f] <= 0.0) SETERRQ2(PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Invalid determinant %g for face %d", detJ[f], point);
      ierr = DMPlexVecGetClosure(dm, section, X, point, NULL, &x);CHKERRQ(ierr);
      for (i = 0; i < cellDof; ++i) u[f*cellDof+i] = x[i];
      ierr = DMPlexVecRestoreClosure(dm, section, X, point, NULL, &x);CHKERRQ(ierr);
      ierr = DMPlexVecGetClosure(dm, section, X_t, point, NULL, &x);CHKERRQ(ierr);
      for (i = 0; i < cellDof; ++i) u_t[f*cellDof+i] = x[i];
      ierr = DMPlexVecRestoreClosure(dm, section, X_t, point, NULL, &x);CHKERRQ(ierr);
      ++f;
    }
    for (f = 0; f < Nf; ++f) {
      void   (*f0)(const PetscScalar[], const PetscScalar[], const PetscScalar[], const PetscScalar[], const PetscScalar[], const PetscReal[], const PetscReal[], PetscScalar[]) = fem->f0BdIFuncs[f];
      void   (*f1)(const PetscScalar[], const PetscScalar[], const PetscScalar[], const PetscScalar[], const PetscScalar[], const PetscReal[], const PetscReal[], PetscScalar[]) = fem->f1BdIFuncs[f];
      PetscInt numQuadPoints, Nb;
      /* Conforming batches */
      PetscInt numChunks, numBatches, numBlocks, Ne, blockSize, batchSize;
      /* Remainder */
      PetscInt Nr, offset;

      ierr = PetscFEGetQuadrature(feBd[f], &q);CHKERRQ(ierr);
      ierr = PetscFEGetDimension(feBd[f], &Nb);CHKERRQ(ierr);
      ierr = PetscFEGetTileSizes(feBd[f], NULL, &numBlocks, NULL, &numBatches);CHKERRQ(ierr);
      ierr = PetscQuadratureGetData(q, NULL, &numQuadPoints, NULL, NULL);CHKERRQ(ierr);
      blockSize = Nb*numQuadPoints;
      batchSize = numBlocks * blockSize;
      ierr =  PetscFESetTileSizes(feBd[f], blockSize, numBlocks, batchSize, numBatches);CHKERRQ(ierr);
      numChunks = numFaces / (numBatches*batchSize);
      Ne        = numChunks*numBatches*batchSize;
      Nr        = numFaces % (numBatches*batchSize);
      offset    = numFaces - Nr;
      geom.v0   = v0;
      geom.n    = n;
      geom.J    = J;
      geom.invJ = invJ;
      geom.detJ = detJ;
      ierr = PetscFEIntegrateBdIFunction(feBd[f], Ne, Nf, feBd, f, geom, u, u_t, 0, NULL, NULL, f0, f1, elemVec);CHKERRQ(ierr);
      geom.v0   = &v0[offset*dim];
      geom.n    = &n[offset*dim];
      geom.J    = &J[offset*dim*dim];
      geom.invJ = &invJ[offset*dim*dim];
      geom.detJ = &detJ[offset];
      ierr = PetscFEIntegrateBdIFunction(feBd[f], Nr, Nf, feBd, f, geom, &u[offset*cellDof], &u_t[offset*cellDof], 0, NULL, NULL, f0, f1, &elemVec[offset*cellDof]);CHKERRQ(ierr);
    }
    for (p = 0, f = 0; p < numPoints; ++p) {
      const PetscInt point = points[p];

      ierr = DMLabelGetValue(depth, point, &dep);CHKERRQ(ierr);
      if (dep != dim-1) continue;
      if (mesh->printFEM > 1) {ierr = DMPrintCellVector(point, "BdResidual", cellDof, &elemVec[f*cellDof]);CHKERRQ(ierr);}
      ierr = DMPlexVecSetClosure(dm, NULL, F, point, &elemVec[f*cellDof], ADD_VALUES);CHKERRQ(ierr);
      ++f;
    }
    ierr = ISRestoreIndices(pointIS, &points);CHKERRQ(ierr);
    ierr = ISDestroy(&pointIS);CHKERRQ(ierr);
    ierr = PetscFree7(u,v0,n,J,invJ,detJ,elemVec);CHKERRQ(ierr);
    ierr = PetscFree(u_t);CHKERRQ(ierr);
  }
  if (mesh->printFEM) {ierr = DMPrintLocalVec(dm, name, mesh->printTol, F);CHKERRQ(ierr);}
  ierr = PetscLogEventEnd(DMPLEX_ResidualFEM,dm,0,0,0);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "DMPlexComputeJacobianActionFEM"
/*@C
  DMPlexComputeJacobianActionFEM - Form the local action of Jacobian J(u) on the local input X using pointwise functions specified by the user

  Input Parameters:
+ dm - The mesh
. J  - The Jacobian shell matrix
. X  - Local input vector
- user - The user context

  Output Parameter:
. F  - Local output vector

  Note:
  The first member of the user context must be an FEMContext.

  We form the residual one batch of elements at a time. This allows us to offload work onto an accelerator,
  like a GPU, or vectorize on a multicore machine.

  Level: developer

.seealso: DMPlexComputeResidualFEM()
@*/
PetscErrorCode DMPlexComputeJacobianActionFEM(DM dm, Mat Jac, Vec X, Vec F, void *user)
{
  DM_Plex          *mesh = (DM_Plex *) dm->data;
  PetscFEM         *fem  = (PetscFEM *) user;
  PetscFE          *fe   = fem->fe;
  PetscQuadrature   quad;
  PetscCellGeometry geom;
  PetscSection      section;
  JacActionCtx     *jctx;
  PetscReal        *v0, *J, *invJ, *detJ;
  PetscScalar      *elemVec, *u, *a;
  PetscInt          dim, numFields, field, numCells, cStart, cEnd, c;
  PetscInt          cellDof = 0;
  PetscErrorCode    ierr;

  PetscFunctionBegin;
  /* ierr = PetscLogEventBegin(DMPLEX_JacobianActionFEM,dm,0,0,0);CHKERRQ(ierr); */
  ierr = MatShellGetContext(Jac, &jctx);CHKERRQ(ierr);
  ierr = DMPlexGetDimension(dm, &dim);CHKERRQ(ierr);
  ierr = DMGetDefaultSection(dm, &section);CHKERRQ(ierr);
  ierr = PetscSectionGetNumFields(section, &numFields);CHKERRQ(ierr);
  ierr = DMPlexGetHeightStratum(dm, 0, &cStart, &cEnd);CHKERRQ(ierr);
  numCells = cEnd - cStart;
  for (field = 0; field < numFields; ++field) {
    PetscInt Nb, Nc;

    ierr = PetscFEGetDimension(fe[field], &Nb);CHKERRQ(ierr);
    ierr = PetscFEGetNumComponents(fe[field], &Nc);CHKERRQ(ierr);
    cellDof += Nb*Nc;
  }
  ierr = VecSet(F, 0.0);CHKERRQ(ierr);
  ierr = PetscMalloc7(numCells*cellDof,&u,numCells*cellDof,&a,numCells*dim,&v0,numCells*dim*dim,&J,numCells*dim*dim,&invJ,numCells,&detJ,numCells*cellDof,&elemVec);CHKERRQ(ierr);
  for (c = cStart; c < cEnd; ++c) {
    PetscScalar *x = NULL;
    PetscInt     i;

    ierr = DMPlexComputeCellGeometry(dm, c, &v0[c*dim], &J[c*dim*dim], &invJ[c*dim*dim], &detJ[c]);CHKERRQ(ierr);
    if (detJ[c] <= 0.0) SETERRQ2(PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Invalid determinant %g for element %d", detJ[c], c);
    ierr = DMPlexVecGetClosure(dm, NULL, jctx->u, c, NULL, &x);CHKERRQ(ierr);
    for (i = 0; i < cellDof; ++i) u[c*cellDof+i] = x[i];
    ierr = DMPlexVecRestoreClosure(dm, NULL, jctx->u, c, NULL, &x);CHKERRQ(ierr);
    ierr = DMPlexVecGetClosure(dm, NULL, X, c, NULL, &x);CHKERRQ(ierr);
    for (i = 0; i < cellDof; ++i) a[c*cellDof+i] = x[i];
    ierr = DMPlexVecRestoreClosure(dm, NULL, X, c, NULL, &x);CHKERRQ(ierr);
  }
  for (field = 0; field < numFields; ++field) {
    PetscInt numQuadPoints, Nb;
    /* Conforming batches */
    PetscInt numBlocks  = 1;
    PetscInt numBatches = 1;
    PetscInt numChunks, Ne, blockSize, batchSize;
    /* Remainder */
    PetscInt Nr, offset;

    ierr = PetscFEGetQuadrature(fe[field], &quad);CHKERRQ(ierr);
    ierr = PetscFEGetDimension(fe[field], &Nb);CHKERRQ(ierr);
    ierr = PetscQuadratureGetData(quad, NULL, &numQuadPoints, NULL, NULL);CHKERRQ(ierr);
    blockSize = Nb*numQuadPoints;
    batchSize = numBlocks * blockSize;
    numChunks = numCells / (numBatches*batchSize);
    Ne        = numChunks*numBatches*batchSize;
    Nr        = numCells % (numBatches*batchSize);
    offset    = numCells - Nr;
    geom.v0   = v0;
    geom.J    = J;
    geom.invJ = invJ;
    geom.detJ = detJ;
    ierr = PetscFEIntegrateJacobianAction(fe[field], Ne, numFields, fe, field, geom, u, a, fem->g0Funcs, fem->g1Funcs, fem->g2Funcs, fem->g3Funcs, elemVec);CHKERRQ(ierr);
    geom.v0   = &v0[offset*dim];
    geom.J    = &J[offset*dim*dim];
    geom.invJ = &invJ[offset*dim*dim];
    geom.detJ = &detJ[offset];
    ierr = PetscFEIntegrateJacobianAction(fe[field], Nr, numFields, fe, field, geom, &u[offset*cellDof], &a[offset*cellDof],
                                          fem->g0Funcs, fem->g1Funcs, fem->g2Funcs, fem->g3Funcs, &elemVec[offset*cellDof]);CHKERRQ(ierr);
  }
  for (c = cStart; c < cEnd; ++c) {
    if (mesh->printFEM > 1) {ierr = DMPrintCellVector(c, "Jacobian Action", cellDof, &elemVec[c*cellDof]);CHKERRQ(ierr);}
    ierr = DMPlexVecSetClosure(dm, NULL, F, c, &elemVec[c*cellDof], ADD_VALUES);CHKERRQ(ierr);
  }
  ierr = PetscFree7(u,a,v0,J,invJ,detJ,elemVec);CHKERRQ(ierr);
  if (mesh->printFEM) {
    PetscMPIInt rank, numProcs;
    PetscInt    p;

    ierr = MPI_Comm_rank(PetscObjectComm((PetscObject)dm), &rank);CHKERRQ(ierr);
    ierr = MPI_Comm_size(PetscObjectComm((PetscObject)dm), &numProcs);CHKERRQ(ierr);
    ierr = PetscPrintf(PetscObjectComm((PetscObject)dm), "Jacobian Action:\n");CHKERRQ(ierr);
    for (p = 0; p < numProcs; ++p) {
      if (p == rank) {ierr = VecView(F, PETSC_VIEWER_STDOUT_SELF);CHKERRQ(ierr);}
      ierr = PetscBarrier((PetscObject) dm);CHKERRQ(ierr);
    }
  }
  /* ierr = PetscLogEventEnd(DMPLEX_JacobianActionFEM,dm,0,0,0);CHKERRQ(ierr); */
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "DMPlexComputeJacobianFEM"
/*@
  DMPlexComputeJacobianFEM - Form the local portion of the Jacobian matrix J at the local solution X using pointwise functions specified by the user.

  Input Parameters:
+ dm - The mesh
. X  - Local input vector
- user - The user context

  Output Parameter:
. Jac  - Jacobian matrix

  Note:
  The first member of the user context must be an FEMContext.

  We form the residual one batch of elements at a time. This allows us to offload work onto an accelerator,
  like a GPU, or vectorize on a multicore machine.

  Level: developer

.seealso: FormFunctionLocal()
@*/
PetscErrorCode DMPlexComputeJacobianFEM(DM dm, Vec X, Mat Jac, Mat JacP,void *user)
{
  DM_Plex          *mesh  = (DM_Plex *) dm->data;
  PetscFEM         *fem   = (PetscFEM *) user;
  PetscFE          *fe    = fem->fe;
  PetscFE          *feAux = fem->feAux;
  const char       *name  = "Jacobian";
  DM                dmAux;
  Vec               A;
  PetscQuadrature   quad;
  PetscCellGeometry geom;
  PetscSection      section, globalSection, sectionAux;
  PetscReal        *v0, *J, *invJ, *detJ;
  PetscScalar      *elemMat, *u, *a;
  PetscInt          dim, Nf, NfAux = 0, f, fieldI, fieldJ, numCells, cStart, cEnd, c;
  PetscInt          cellDof = 0, numComponents = 0;
  PetscInt          cellDofAux = 0, numComponentsAux = 0;
  PetscBool         isShell;
  PetscErrorCode    ierr;

  PetscFunctionBegin;
  ierr = PetscLogEventBegin(DMPLEX_JacobianFEM,dm,0,0,0);CHKERRQ(ierr);
  ierr = DMPlexGetDimension(dm, &dim);CHKERRQ(ierr);
  ierr = DMGetDefaultSection(dm, &section);CHKERRQ(ierr);
  ierr = DMGetDefaultGlobalSection(dm, &globalSection);CHKERRQ(ierr);
  ierr = PetscSectionGetNumFields(section, &Nf);CHKERRQ(ierr);
  ierr = DMPlexGetHeightStratum(dm, 0, &cStart, &cEnd);CHKERRQ(ierr);
  numCells = cEnd - cStart;
  for (f = 0; f < Nf; ++f) {
    PetscInt Nb, Nc;

    ierr = PetscFEGetDimension(fe[f], &Nb);CHKERRQ(ierr);
    ierr = PetscFEGetNumComponents(fe[f], &Nc);CHKERRQ(ierr);
    cellDof       += Nb*Nc;
    numComponents += Nc;
  }
  ierr = PetscObjectQuery((PetscObject) dm, "dmAux", (PetscObject *) &dmAux);CHKERRQ(ierr);
  ierr = PetscObjectQuery((PetscObject) dm, "A", (PetscObject *) &A);CHKERRQ(ierr);
  if (dmAux) {
    ierr = DMGetDefaultSection(dmAux, &sectionAux);CHKERRQ(ierr);
    ierr = PetscSectionGetNumFields(sectionAux, &NfAux);CHKERRQ(ierr);
  }
  for (f = 0; f < NfAux; ++f) {
    PetscInt Nb, Nc;

    ierr = PetscFEGetDimension(feAux[f], &Nb);CHKERRQ(ierr);
    ierr = PetscFEGetNumComponents(feAux[f], &Nc);CHKERRQ(ierr);
    cellDofAux       += Nb*Nc;
    numComponentsAux += Nc;
  }
  ierr = DMPlexInsertBoundaryValuesFEM(dm, X);CHKERRQ(ierr);
  ierr = MatZeroEntries(JacP);CHKERRQ(ierr);
  ierr = PetscMalloc6(numCells*cellDof,&u,numCells*dim,&v0,numCells*dim*dim,&J,numCells*dim*dim,&invJ,numCells,&detJ,numCells*cellDof*cellDof,&elemMat);CHKERRQ(ierr);
  if (dmAux) {ierr = PetscMalloc1(numCells*cellDofAux, &a);CHKERRQ(ierr);}
  for (c = cStart; c < cEnd; ++c) {
    PetscScalar *x = NULL;
    PetscInt     i;

    ierr = DMPlexComputeCellGeometry(dm, c, &v0[c*dim], &J[c*dim*dim], &invJ[c*dim*dim], &detJ[c]);CHKERRQ(ierr);
    if (detJ[c] <= 0.0) SETERRQ2(PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Invalid determinant %g for element %d", detJ[c], c);
    ierr = DMPlexVecGetClosure(dm, section, X, c, NULL, &x);CHKERRQ(ierr);
    for (i = 0; i < cellDof; ++i) u[c*cellDof+i] = x[i];
    ierr = DMPlexVecRestoreClosure(dm, section, X, c, NULL, &x);CHKERRQ(ierr);
    if (dmAux) {
      ierr = DMPlexVecGetClosure(dmAux, sectionAux, A, c, NULL, &x);CHKERRQ(ierr);
      for (i = 0; i < cellDofAux; ++i) a[c*cellDofAux+i] = x[i];
      ierr = DMPlexVecRestoreClosure(dmAux, sectionAux, A, c, NULL, &x);CHKERRQ(ierr);
    }
  }
  ierr = PetscMemzero(elemMat, numCells*cellDof*cellDof * sizeof(PetscScalar));CHKERRQ(ierr);
  for (fieldI = 0; fieldI < Nf; ++fieldI) {
    PetscInt numQuadPoints, Nb;
    /* Conforming batches */
    PetscInt numChunks, numBatches, numBlocks, Ne, blockSize, batchSize;
    /* Remainder */
    PetscInt Nr, offset;

    ierr = PetscFEGetQuadrature(fe[fieldI], &quad);CHKERRQ(ierr);
    ierr = PetscFEGetDimension(fe[fieldI], &Nb);CHKERRQ(ierr);
    ierr = PetscFEGetTileSizes(fe[fieldI], NULL, &numBlocks, NULL, &numBatches);CHKERRQ(ierr);
    ierr = PetscQuadratureGetData(quad, NULL, &numQuadPoints, NULL, NULL);CHKERRQ(ierr);
    blockSize = Nb*numQuadPoints;
    batchSize = numBlocks * blockSize;
    ierr = PetscFESetTileSizes(fe[fieldI], blockSize, numBlocks, batchSize, numBatches);CHKERRQ(ierr);
    numChunks = numCells / (numBatches*batchSize);
    Ne        = numChunks*numBatches*batchSize;
    Nr        = numCells % (numBatches*batchSize);
    offset    = numCells - Nr;
    for (fieldJ = 0; fieldJ < Nf; ++fieldJ) {
      void   (*g0)(const PetscScalar[], const PetscScalar[], const PetscScalar[], const PetscScalar[], const PetscReal[], PetscScalar[]) = fem->g0Funcs[fieldI*Nf+fieldJ];
      void   (*g1)(const PetscScalar[], const PetscScalar[], const PetscScalar[], const PetscScalar[], const PetscReal[], PetscScalar[]) = fem->g1Funcs[fieldI*Nf+fieldJ];
      void   (*g2)(const PetscScalar[], const PetscScalar[], const PetscScalar[], const PetscScalar[], const PetscReal[], PetscScalar[]) = fem->g2Funcs[fieldI*Nf+fieldJ];
      void   (*g3)(const PetscScalar[], const PetscScalar[], const PetscScalar[], const PetscScalar[], const PetscReal[], PetscScalar[]) = fem->g3Funcs[fieldI*Nf+fieldJ];

      geom.v0   = v0;
      geom.J    = J;
      geom.invJ = invJ;
      geom.detJ = detJ;
      ierr = PetscFEIntegrateJacobian(fe[fieldI], Ne, Nf, fe, fieldI, fieldJ, geom, u, NfAux, feAux, a, g0, g1, g2, g3, elemMat);CHKERRQ(ierr);
      geom.v0   = &v0[offset*dim];
      geom.J    = &J[offset*dim*dim];
      geom.invJ = &invJ[offset*dim*dim];
      geom.detJ = &detJ[offset];
      ierr = PetscFEIntegrateJacobian(fe[fieldI], Nr, Nf, fe, fieldI, fieldJ, geom, &u[offset*cellDof], NfAux, feAux, &a[offset*cellDofAux], g0, g1, g2, g3, &elemMat[offset*cellDof*cellDof]);CHKERRQ(ierr);
    }
  }
  for (c = cStart; c < cEnd; ++c) {
    if (mesh->printFEM > 1) {ierr = DMPrintCellMatrix(c, name, cellDof, cellDof, &elemMat[c*cellDof*cellDof]);CHKERRQ(ierr);}
    ierr = DMPlexMatSetClosure(dm, section, globalSection, JacP, c, &elemMat[c*cellDof*cellDof], ADD_VALUES);CHKERRQ(ierr);
  }
  ierr = PetscFree6(u,v0,J,invJ,detJ,elemMat);CHKERRQ(ierr);
  if (dmAux) {ierr = PetscFree(a);CHKERRQ(ierr);}
  ierr = MatAssemblyBegin(JacP, MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  ierr = MatAssemblyEnd(JacP, MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  if (mesh->printFEM) {
    ierr = PetscPrintf(PETSC_COMM_WORLD, "%s:\n", name);CHKERRQ(ierr);
    ierr = MatChop(JacP, 1.0e-10);CHKERRQ(ierr);
    ierr = MatView(JacP, PETSC_VIEWER_STDOUT_WORLD);CHKERRQ(ierr);
  }
  ierr = PetscLogEventEnd(DMPLEX_JacobianFEM,dm,0,0,0);CHKERRQ(ierr);
  ierr = PetscObjectTypeCompare((PetscObject) Jac, MATSHELL, &isShell);CHKERRQ(ierr);
  if (isShell) {
    JacActionCtx *jctx;

    ierr = MatShellGetContext(Jac, &jctx);CHKERRQ(ierr);
    ierr = VecCopy(X, jctx->u);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

#if 0

static void g0_identity_1d_static(const PetscScalar u[], const PetscScalar gradU[], const PetscScalar a[], const PetscScalar gradA[], const PetscReal x[], PetscScalar g3[])
{
  g3[0] = 1.0;
}

static void g0_identity_2d_static(const PetscScalar u[], const PetscScalar gradU[], const PetscScalar a[], const PetscScalar gradA[], const PetscReal x[], PetscScalar g3[])
{
  g3[0] = g3[3] = 1.0;
}

static void g0_identity_3d_static(const PetscScalar u[], const PetscScalar gradU[], const PetscScalar a[], const PetscScalar gradA[], const PetscReal x[], PetscScalar g3[])
{
  g3[0] = g3[4] = g3[8] = 1.0;
}

#undef __FUNCT__
#define __FUNCT__ "DMPlexComputeInterpolatorFEMBroken"
PetscErrorCode DMPlexComputeInterpolatorFEMBroken(DM dmc, DM dmf, Mat I, void *user)
{
  DM_Plex          *mesh  = (DM_Plex *) dmc->data;
  PetscFEM         *fem   = (PetscFEM *) user;
  PetscFE          *fe    = fem->fe;
  const char       *name  = "Interpolator";
  PetscFE          *feRef;
  PetscQuadrature   quad, quadOld;
  PetscCellGeometry geom;
  PetscSection      fsection, fglobalSection;
  PetscSection      csection, cglobalSection;
  PetscReal        *v0, *J, *invJ, *detJ;
  PetscScalar      *elemMat;
  PetscInt          dim, Nf, f, fieldI, fieldJ, numCells, cStart, cEnd, c;
  PetscInt          rCellDof = 0, cCellDof = 0, numComponents = 0;
  PetscErrorCode    ierr;

  PetscFunctionBegin;
#if 0
  ierr = PetscLogEventBegin(DMPLEX_InterpolatorFEM,dmc,dmf,0,0);CHKERRQ(ierr);
#endif
  ierr = DMPlexGetDimension(dmf, &dim);CHKERRQ(ierr);
  ierr = DMGetDefaultSection(dmf, &fsection);CHKERRQ(ierr);
  ierr = DMGetDefaultGlobalSection(dmf, &fglobalSection);CHKERRQ(ierr);
  ierr = DMGetDefaultSection(dmc, &csection);CHKERRQ(ierr);
  ierr = DMGetDefaultGlobalSection(dmc, &cglobalSection);CHKERRQ(ierr);
  ierr = PetscSectionGetNumFields(fsection, &Nf);CHKERRQ(ierr);
  ierr = DMPlexGetHeightStratum(dmc, 0, &cStart, &cEnd);CHKERRQ(ierr);
  numCells = cEnd - cStart;
  ierr = PetscMalloc1(Nf,&feRef);CHKERRQ(ierr);
  for (fieldI = 0; fieldI < Nf; ++fieldI) {
    ierr = PetscFERefine(fe[fieldI], &feRef[fieldI]);CHKERRQ(ierr);
  }
  for (f = 0; f < Nf; ++f) {
    PetscInt rNb, cNb, Nc;

    ierr = PetscFEGetDimension(feRef[f], &rNb);CHKERRQ(ierr);
    ierr = PetscFEGetDimension(fe[f], &cNb);CHKERRQ(ierr);
    ierr = PetscFEGetNumComponents(fe[f], &Nc);CHKERRQ(ierr);
    numComponents += Nc;
    rCellDof += rNb*Nc;
    cCellDof += cNb*Nc;
  }
  ierr = MatZeroEntries(I);CHKERRQ(ierr);
  ierr = PetscMalloc5(numCells*dim,&v0,numCells*dim*dim,&J,numCells*dim*dim,&invJ,numCells,&detJ,numCells*rCellDof*cCellDof,&elemMat);CHKERRQ(ierr);
  for (c = cStart; c < cEnd; ++c) {
    ierr = DMPlexComputeCellGeometry(dmc, c, &v0[c*dim], &J[c*dim*dim], &invJ[c*dim*dim], &detJ[c]);CHKERRQ(ierr);
    if (detJ[c] <= 0.0) SETERRQ2(PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Invalid determinant %g for element %d", detJ[c], c);
  }
  ierr = PetscMemzero(elemMat, numCells*rCellDof*cCellDof * sizeof(PetscScalar));CHKERRQ(ierr);
  for (fieldI = 0; fieldI < Nf; ++fieldI) {
    PetscInt numQuadPoints, Nb;
    /* Conforming batches */
    PetscInt numChunks, numBatches, numBlocks, Ne, blockSize, batchSize;
    /* Remainder */
    PetscInt Nr, offset;

    /* Make new fine FE which refines the ref cell and the quadrature rule */
    ierr = PetscFEGetQuadrature(feRef[fieldI], &quad);CHKERRQ(ierr);
    ierr = PetscFEGetDimension(feRef[fieldI], &Nb);CHKERRQ(ierr);
    ierr = PetscFEGetTileSizes(feRef[fieldI], NULL, &numBlocks, NULL, &numBatches);CHKERRQ(ierr);
    ierr = PetscQuadratureGetData(quad, NULL, &numQuadPoints, NULL, NULL);CHKERRQ(ierr);
    blockSize = Nb*numQuadPoints;
    batchSize = numBlocks * blockSize;
    ierr = PetscFESetTileSizes(feRef[fieldI], blockSize, numBlocks, batchSize, numBatches);CHKERRQ(ierr);
    numChunks = numCells / (numBatches*batchSize);
    Ne        = numChunks*numBatches*batchSize;
    Nr        = numCells % (numBatches*batchSize);
    offset    = numCells - Nr;

    for (fieldJ = 0; fieldJ < Nf; ++fieldJ) {
      /* void   (*g0)(const PetscScalar[], const PetscScalar[], const PetscScalar[], const PetscScalar[], const PetscReal[], PetscScalar[]) = fem->g0Funcs[fieldI*Nf+fieldJ]; */
      void   (*g0)(const PetscScalar[], const PetscScalar[], const PetscScalar[], const PetscScalar[], const PetscReal[], PetscScalar[]) = g0_identity_2d_static;

      /* Replace quadrature in coarse FE with refined quadrature */
      ierr = PetscFEGetQuadrature(fe[fieldJ], &quadOld);CHKERRQ(ierr);
      ierr = PetscObjectReference((PetscObject) quadOld);CHKERRQ(ierr);
      ierr = PetscFESetQuadrature(fe[fieldJ], quad);CHKERRQ(ierr);
      geom.v0   = v0;
      geom.J    = J;
      geom.invJ = invJ;
      geom.detJ = detJ;
      ierr = PetscFEIntegrateInterpolator_Basic(feRef[fieldI], Ne, Nf, feRef, fieldI, fe, fieldJ, geom, g0, elemMat);CHKERRQ(ierr);
      geom.v0   = &v0[offset*dim];
      geom.J    = &J[offset*dim*dim];
      geom.invJ = &invJ[offset*dim*dim];
      geom.detJ = &detJ[offset];
      ierr = PetscFEIntegrateInterpolator_Basic(feRef[fieldI], Nr, Nf, feRef, fieldI, fe, fieldJ, geom, g0, &elemMat[offset*rCellDof*cCellDof]);CHKERRQ(ierr);
      ierr = PetscFESetQuadrature(fe[fieldJ], quadOld);CHKERRQ(ierr);
    }
  }
  for (c = cStart; c < cEnd; ++c) {
    if (mesh->printFEM > 1) {ierr = DMPrintCellMatrix(c, name, rCellDof, cCellDof, &elemMat[c*rCellDof*cCellDof]);CHKERRQ(ierr);}
    ierr = DMPlexMatSetClosureRefined(dmf, fsection, fglobalSection, dmc, csection, cglobalSection, I, c, &elemMat[c*rCellDof*cCellDof], ADD_VALUES);CHKERRQ(ierr);
  }
  ierr = PetscFree5(v0,J,invJ,detJ,elemMat);CHKERRQ(ierr);
  ierr = PetscFree(feRef);CHKERRQ(ierr);
  ierr = MatAssemblyBegin(I, MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  ierr = MatAssemblyEnd(I, MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  if (mesh->printFEM) {
    ierr = PetscPrintf(PETSC_COMM_WORLD, "%s:\n", name);CHKERRQ(ierr);
    ierr = MatChop(I, 1.0e-10);CHKERRQ(ierr);
    ierr = MatView(I, PETSC_VIEWER_STDOUT_WORLD);CHKERRQ(ierr);
  }
#if 0
  ierr = PetscLogEventEnd(DMPLEX_InterpolatorFEM,dmc,dmf,0,0);CHKERRQ(ierr);
#endif
  PetscFunctionReturn(0);
}
#endif

#undef __FUNCT__
#define __FUNCT__ "DMPlexComputeInterpolatorFEM"
/*@
  DMPlexComputeInterpolatorFEM - Form the local portion of the interpolation matrix I from the coarse DM to the uniformly refined DM.

  Input Parameters:
+ dmf  - The fine mesh
. dmc  - The coarse mesh
- user - The user context

  Output Parameter:
. In  - The interpolation matrix

  Note:
  The first member of the user context must be an FEMContext.

  We form the residual one batch of elements at a time. This allows us to offload work onto an accelerator,
  like a GPU, or vectorize on a multicore machine.

  Level: developer

.seealso: DMPlexComputeJacobianFEM()
@*/
PetscErrorCode DMPlexComputeInterpolatorFEM(DM dmc, DM dmf, Mat In, void *user)
{
  DM_Plex          *mesh  = (DM_Plex *) dmc->data;
  PetscFEM         *fem   = (PetscFEM *) user;
  PetscFE          *fe    = fem->fe;
  const char       *name  = "Interpolator";
  PetscFE          *feRef;
  PetscSection      fsection, fglobalSection;
  PetscSection      csection, cglobalSection;
  PetscScalar      *elemMat;
  PetscInt          dim, Nf, f, fieldI, fieldJ, offsetI, offsetJ, cStart, cEnd, c;
  PetscInt          rCellDof = 0, cCellDof = 0;
  PetscErrorCode    ierr;

  PetscFunctionBegin;
#if 0
  ierr = PetscLogEventBegin(DMPLEX_InterpolatorFEM,dmc,dmf,0,0);CHKERRQ(ierr);
#endif
  ierr = DMPlexGetDimension(dmf, &dim);CHKERRQ(ierr);
  ierr = DMGetDefaultSection(dmf, &fsection);CHKERRQ(ierr);
  ierr = DMGetDefaultGlobalSection(dmf, &fglobalSection);CHKERRQ(ierr);
  ierr = DMGetDefaultSection(dmc, &csection);CHKERRQ(ierr);
  ierr = DMGetDefaultGlobalSection(dmc, &cglobalSection);CHKERRQ(ierr);
  ierr = PetscSectionGetNumFields(fsection, &Nf);CHKERRQ(ierr);
  ierr = DMPlexGetHeightStratum(dmc, 0, &cStart, &cEnd);CHKERRQ(ierr);
  ierr = PetscMalloc1(Nf,&feRef);CHKERRQ(ierr);
  for (f = 0; f < Nf; ++f) {
    PetscInt rNb, cNb, Nc;

    ierr = PetscFERefine(fe[f], &feRef[f]);CHKERRQ(ierr);
    ierr = PetscFEGetDimension(feRef[f], &rNb);CHKERRQ(ierr);
    ierr = PetscFEGetDimension(fe[f], &cNb);CHKERRQ(ierr);
    ierr = PetscFEGetNumComponents(fe[f], &Nc);CHKERRQ(ierr);
    rCellDof += rNb*Nc;
    cCellDof += cNb*Nc;
  }
  ierr = MatZeroEntries(In);CHKERRQ(ierr);
  ierr = PetscMalloc1(rCellDof*cCellDof,&elemMat);CHKERRQ(ierr);
  ierr = PetscMemzero(elemMat, rCellDof*cCellDof * sizeof(PetscScalar));CHKERRQ(ierr);
  for (fieldI = 0, offsetI = 0; fieldI < Nf; ++fieldI) {
    PetscDualSpace   Qref;
    PetscQuadrature  f;
    const PetscReal *qpoints, *qweights;
    PetscReal       *points;
    PetscInt         npoints = 0, Nc, Np, fpdim, i, k, p, d;

    /* Compose points from all dual basis functionals */
    ierr = PetscFEGetNumComponents(fe[fieldI], &Nc);CHKERRQ(ierr);
    ierr = PetscFEGetDualSpace(feRef[fieldI], &Qref);CHKERRQ(ierr);
    ierr = PetscDualSpaceGetDimension(Qref, &fpdim);CHKERRQ(ierr);
    for (i = 0; i < fpdim; ++i) {
      ierr = PetscDualSpaceGetFunctional(Qref, i, &f);CHKERRQ(ierr);
      ierr = PetscQuadratureGetData(f, NULL, &Np, NULL, NULL);CHKERRQ(ierr);
      npoints += Np;
    }
    ierr = PetscMalloc1(npoints*dim,&points);CHKERRQ(ierr);
    for (i = 0, k = 0; i < fpdim; ++i) {
      ierr = PetscDualSpaceGetFunctional(Qref, i, &f);CHKERRQ(ierr);
      ierr = PetscQuadratureGetData(f, NULL, &Np, &qpoints, NULL);CHKERRQ(ierr);
      for (p = 0; p < Np; ++p, ++k) for (d = 0; d < dim; ++d) points[k*dim+d] = qpoints[p*dim+d];
    }

    for (fieldJ = 0, offsetJ = 0; fieldJ < Nf; ++fieldJ) {
      PetscReal *B;
      PetscInt   NcJ, cpdim, j;

      /* Evaluate basis at points */
      ierr = PetscFEGetNumComponents(fe[fieldJ], &NcJ);CHKERRQ(ierr);
      if (Nc != NcJ) SETERRQ2(PETSC_COMM_SELF, PETSC_ERR_ARG_WRONG, "Number of components in fine space field %d does not match coarse field %d", Nc, NcJ);
      ierr = PetscFEGetDimension(fe[fieldJ], &cpdim);CHKERRQ(ierr);
      /* For now, fields only interpolate themselves */
      if (fieldI == fieldJ) {
        ierr = PetscFEGetTabulation(fe[fieldJ], npoints, points, &B, NULL, NULL);CHKERRQ(ierr);
        for (i = 0, k = 0; i < fpdim; ++i) {
          ierr = PetscDualSpaceGetFunctional(Qref, i, &f);CHKERRQ(ierr);
          ierr = PetscQuadratureGetData(f, NULL, &Np, NULL, &qweights);CHKERRQ(ierr);
          for (p = 0; p < Np; ++p, ++k) {
            for (j = 0; j < cpdim; ++j) {
              for (c = 0; c < Nc; ++c) elemMat[(offsetI + i*Nc + c)*cCellDof + offsetJ + j*NcJ + c] += B[k*cpdim*NcJ+j*Nc+c]*qweights[p];
            }
          }
        }
        ierr = PetscFERestoreTabulation(fe[fieldJ], npoints, points, &B, NULL, NULL);CHKERRQ(ierr);CHKERRQ(ierr);
      }
      offsetJ += cpdim*NcJ;
    }
    offsetI += fpdim*Nc;
    ierr = PetscFree(points);CHKERRQ(ierr);
  }
  if (mesh->printFEM > 1) {ierr = DMPrintCellMatrix(0, name, rCellDof, cCellDof, elemMat);CHKERRQ(ierr);}
  for (c = cStart; c < cEnd; ++c) {
    ierr = DMPlexMatSetClosureRefined(dmf, fsection, fglobalSection, dmc, csection, cglobalSection, In, c, elemMat, INSERT_VALUES);CHKERRQ(ierr);
  }
  for (f = 0; f < Nf; ++f) {ierr = PetscFEDestroy(&feRef[f]);CHKERRQ(ierr);}
  ierr = PetscFree(feRef);CHKERRQ(ierr);
  ierr = PetscFree(elemMat);CHKERRQ(ierr);
  ierr = MatAssemblyBegin(In, MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  ierr = MatAssemblyEnd(In, MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  if (mesh->printFEM) {
    ierr = PetscPrintf(PETSC_COMM_WORLD, "%s:\n", name);CHKERRQ(ierr);
    ierr = MatChop(In, 1.0e-10);CHKERRQ(ierr);
    ierr = MatView(In, PETSC_VIEWER_STDOUT_WORLD);CHKERRQ(ierr);
  }
#if 0
  ierr = PetscLogEventEnd(DMPLEX_InterpolatorFEM,dmc,dmf,0,0);CHKERRQ(ierr);
#endif
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "DMPlexAddBoundary"
/* The ids can be overridden by the command line option -bc_<boundary name> */
PetscErrorCode DMPlexAddBoundary(DM dm, PetscBool isEssential, const char name[], PetscInt field, void (*bcFunc)(), PetscInt numids, const PetscInt *ids, void *ctx)
{
  DM_Plex       *mesh = (DM_Plex *) dm->data;
  DMBoundary     b;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = PetscNew(&b);CHKERRQ(ierr);
  ierr = PetscStrallocpy(name, (char **) &b->name);CHKERRQ(ierr);
  ierr = PetscMalloc1(numids, &b->ids);CHKERRQ(ierr);
  ierr = PetscMemcpy(b->ids, ids, numids*sizeof(PetscInt));CHKERRQ(ierr);
  b->essential   = isEssential;
  b->field       = field;
  b->func        = bcFunc;
  b->numids      = numids;
  b->ctx         = ctx;
  b->next        = mesh->boundary;
  mesh->boundary = b;
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "DMPlexGetNumBoundary"
PetscErrorCode DMPlexGetNumBoundary(DM dm, PetscInt *numBd)
{
  DM_Plex   *mesh = (DM_Plex *) dm->data;
  DMBoundary b    = mesh->boundary;

  PetscFunctionBegin;
  *numBd = 0;
  while (b) {++(*numBd); b = b->next;}
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "DMPlexGetBoundary"
PetscErrorCode DMPlexGetBoundary(DM dm, PetscInt bd, PetscBool *isEssential, const char **name, PetscInt *field, void (**func)(), PetscInt *numids, const PetscInt **ids, void **ctx)
{
  DM_Plex   *mesh = (DM_Plex *) dm->data;
  DMBoundary b    = mesh->boundary;
  PetscInt   n    = 0;

  PetscFunctionBegin;
  while (b) {
    if (n == bd) break;
    b = b->next;
    ++n;
  }
  if (n != bd) SETERRQ2(PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Boundary %d is not in [0, %d)", bd, n);
  if (isEssential) {
    PetscValidPointer(isEssential, 3);
    *isEssential = b->essential;
  }
  if (name) {
    PetscValidPointer(name, 4);
    *name = b->name;
  }
  if (field) {
    PetscValidPointer(field, 5);
    *field = b->field;
  }
  if (func) {
    PetscValidPointer(func, 6);
    *func = b->func;
  }
  if (numids) {
    PetscValidPointer(numids, 7);
    *numids = b->numids;
  }
  if (ids) {
    PetscValidPointer(ids, 8);
    *ids = b->ids;
  }
  if (ctx) {
    PetscValidPointer(ctx, 9);
    *ctx = b->ctx;
  }
  PetscFunctionReturn(0);
}
