#include <petsc-private/petscimpl.h>
#include <petsctao.h>      /*I "petsctao.h" I*/


PETSC_STATIC_INLINE PetscReal Fischer(PetscReal a, PetscReal b)
{
  /* Method suggested by Bob Vanderbei */
   if (a + b <= 0) {
     return PetscSqrtScalar(a*a + b*b) - (a + b);
   }
   return -2.0*a*b / (PetscSqrtScalar(a*a + b*b) + (a + b));
}

#undef __FUNCT__
#define __FUNCT__ "VecFischer"
/*@
   VecFischer - Evaluates the Fischer-Burmeister function for complementarity
   problems.

   Logically Collective on vectors

   Input Parameters:
+  X - current point
.  F - function evaluated at x
.  L - lower bounds
-  U - upper bounds

   Output Parameters:
.  FB - The Fischer-Burmeister function vector

   Notes:
   The Fischer-Burmeister function is defined as
$        phi(a,b) := sqrt(a*a + b*b) - a - b
   and is used reformulate a complementarity problem as a semismooth
   system of equations.

   The result of this function is done by cases:
+  l[i] == -infinity, u[i] == infinity  -- fb[i] = -f[i]
.  l[i] == -infinity, u[i] finite       -- fb[i] = phi(u[i]-x[i], -f[i])
.  l[i] finite,       u[i] == infinity  -- fb[i] = phi(x[i]-l[i],  f[i])
.  l[i] finite < u[i] finite -- fb[i] = phi(x[i]-l[i], phi(u[i]-x[i], -f[u]))
-  otherwise l[i] == u[i] -- fb[i] = l[i] - x[i]

   Level: developer

@*/
PetscErrorCode VecFischer(Vec X, Vec F, Vec L, Vec U, Vec FB)
{
  const PetscReal *x, *f, *l, *u;
  PetscReal       *fb;
  PetscReal       xval, fval, lval, uval;
  PetscErrorCode  ierr;
  PetscInt        low[5], high[5], n, i;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(X, VEC_CLASSID,1);
  PetscValidHeaderSpecific(F, VEC_CLASSID,2);
  PetscValidHeaderSpecific(L, VEC_CLASSID,3);
  PetscValidHeaderSpecific(U, VEC_CLASSID,4);
  PetscValidHeaderSpecific(FB, VEC_CLASSID,4);

  ierr = VecGetOwnershipRange(X, low, high);CHKERRQ(ierr);
  ierr = VecGetOwnershipRange(F, low + 1, high + 1);CHKERRQ(ierr);
  ierr = VecGetOwnershipRange(L, low + 2, high + 2);CHKERRQ(ierr);
  ierr = VecGetOwnershipRange(U, low + 3, high + 3);CHKERRQ(ierr);
  ierr = VecGetOwnershipRange(FB, low + 4, high + 4);CHKERRQ(ierr);

  for (i = 1; i < 4; ++i) {
    if (low[0] != low[i] || high[0] != high[i]) SETERRQ(PETSC_COMM_SELF,1,"Vectors must be identically loaded over processors");
  }

  ierr = VecGetArrayRead(X, &x);CHKERRQ(ierr);
  ierr = VecGetArrayRead(F, &f);CHKERRQ(ierr);
  ierr = VecGetArrayRead(L, &l);CHKERRQ(ierr);
  ierr = VecGetArrayRead(U, &u);CHKERRQ(ierr);
  ierr = VecGetArray(FB, &fb);CHKERRQ(ierr);

  ierr = VecGetLocalSize(X, &n);CHKERRQ(ierr);

  for (i = 0; i < n; ++i) {
    xval = x[i]; fval = f[i];
    lval = l[i]; uval = u[i];

    if ((lval <= -PETSC_INFINITY) && (uval >= PETSC_INFINITY)) {
      fb[i] = -fval;
    } else if (lval <= -PETSC_INFINITY) {
      fb[i] = -Fischer(uval - xval, -fval);
    } else if (uval >=  PETSC_INFINITY) {
      fb[i] =  Fischer(xval - lval,  fval);
    } else if (lval == uval) {
      fb[i] = lval - xval;
    } else {
      fval  =  Fischer(uval - xval, -fval);
      fb[i] =  Fischer(xval - lval,  fval);
    }
  }

  ierr = VecRestoreArrayRead(X, &x);CHKERRQ(ierr);
  ierr = VecRestoreArrayRead(F, &f);CHKERRQ(ierr);
  ierr = VecRestoreArrayRead(L, &l);CHKERRQ(ierr);
  ierr = VecRestoreArrayRead(U, &u);CHKERRQ(ierr);
  ierr = VecRestoreArray(FB, &fb);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

PETSC_STATIC_INLINE PetscReal SFischer(PetscReal a, PetscReal b, PetscReal c)
{
  /* Method suggested by Bob Vanderbei */
   if (a + b <= 0) {
     return PetscSqrtScalar(a*a + b*b + 2.0*c*c) - (a + b);
   }
   return 2.0*(c*c - a*b) / (PetscSqrtScalar(a*a + b*b + 2.0*c*c) + (a + b));
}

#undef __FUNCT__
#define __FUNCT__ "VecSFischer"
/*@
   VecSFischer - Evaluates the Smoothed Fischer-Burmeister function for
   complementarity problems.

   Logically Collective on vectors

   Input Parameters:
+  X - current point
.  F - function evaluated at x
.  L - lower bounds
.  U - upper bounds
-  mu - smoothing parameter

   Output Parameters:
.  FB - The Smoothed Fischer-Burmeister function vector

   Notes:
   The Smoothed Fischer-Burmeister function is defined as
$        phi(a,b) := sqrt(a*a + b*b + 2*mu*mu) - a - b
   and is used reformulate a complementarity problem as a semismooth
   system of equations.

   The result of this function is done by cases:
+  l[i] == -infinity, u[i] == infinity  -- fb[i] = -f[i] - 2*mu*x[i]
.  l[i] == -infinity, u[i] finite       -- fb[i] = phi(u[i]-x[i], -f[i], mu)
.  l[i] finite,       u[i] == infinity  -- fb[i] = phi(x[i]-l[i],  f[i], mu)
.  l[i] finite < u[i] finite -- fb[i] = phi(x[i]-l[i], phi(u[i]-x[i], -f[u], mu), mu)
-  otherwise l[i] == u[i] -- fb[i] = l[i] - x[i]

   Level: developer

.seealso  VecFischer()
@*/
PetscErrorCode VecSFischer(Vec X, Vec F, Vec L, Vec U, PetscReal mu, Vec FB)
{
  const PetscReal *x, *f, *l, *u;
  PetscReal       *fb;
  PetscReal       xval, fval, lval, uval;
  PetscErrorCode  ierr;
  PetscInt        low[5], high[5], n, i;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(X, VEC_CLASSID,1);
  PetscValidHeaderSpecific(F, VEC_CLASSID,2);
  PetscValidHeaderSpecific(L, VEC_CLASSID,3);
  PetscValidHeaderSpecific(U, VEC_CLASSID,4);
  PetscValidHeaderSpecific(FB, VEC_CLASSID,6);

  ierr = VecGetOwnershipRange(X, low, high);CHKERRQ(ierr);
  ierr = VecGetOwnershipRange(F, low + 1, high + 1);CHKERRQ(ierr);
  ierr = VecGetOwnershipRange(L, low + 2, high + 2);CHKERRQ(ierr);
  ierr = VecGetOwnershipRange(U, low + 3, high + 3);CHKERRQ(ierr);
  ierr = VecGetOwnershipRange(FB, low + 4, high + 4);CHKERRQ(ierr);

  for (i = 1; i < 4; ++i) {
    if (low[0] != low[i] || high[0] != high[i]) SETERRQ(PETSC_COMM_SELF,1,"Vectors must be identically loaded over processors");
  }

  ierr = VecGetArrayRead(X, &x);CHKERRQ(ierr);
  ierr = VecGetArrayRead(F, &f);CHKERRQ(ierr);
  ierr = VecGetArrayRead(L, &l);CHKERRQ(ierr);
  ierr = VecGetArrayRead(U, &u);CHKERRQ(ierr);
  ierr = VecGetArray(FB, &fb);CHKERRQ(ierr);

  ierr = VecGetLocalSize(X, &n);CHKERRQ(ierr);

  for (i = 0; i < n; ++i) {
    xval = (*x++); fval = (*f++);
    lval = (*l++); uval = (*u++);

    if ((lval <= -PETSC_INFINITY) && (uval >= PETSC_INFINITY)) {
      (*fb++) = -fval - mu*xval;
    } else if (lval <= -PETSC_INFINITY) {
      (*fb++) = -SFischer(uval - xval, -fval, mu);
    } else if (uval >=  PETSC_INFINITY) {
      (*fb++) =  SFischer(xval - lval,  fval, mu);
    } else if (lval == uval) {
      (*fb++) = lval - xval;
    } else {
      fval    =  SFischer(uval - xval, -fval, mu);
      (*fb++) =  SFischer(xval - lval,  fval, mu);
    }
  }
  x -= n; f -= n; l -=n; u -= n; fb -= n;

  ierr = VecRestoreArrayRead(X, &x);CHKERRQ(ierr);
  ierr = VecRestoreArrayRead(F, &f);CHKERRQ(ierr);
  ierr = VecRestoreArrayRead(L, &l);CHKERRQ(ierr);
  ierr = VecRestoreArrayRead(U, &u);CHKERRQ(ierr);
  ierr = VecRestoreArray(FB, &fb);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

PETSC_STATIC_INLINE PetscReal fischnorm(PetscReal a, PetscReal b)
{
  return PetscSqrtScalar(a*a + b*b);
}

PETSC_STATIC_INLINE PetscReal fischsnorm(PetscReal a, PetscReal b, PetscReal c)
{
  return PetscSqrtScalar(a*a + b*b + 2.0*c*c);
}

#undef __FUNCT__
#define __FUNCT__ "MatDFischer"
/*@
   MatDFischer - Calculates an element of the B-subdifferential of the
   Fischer-Burmeister function for complementarity problems.

   Collective on jac

   Input Parameters:
+  jac - the jacobian of f at X
.  X - current point
.  Con - constraints function evaluated at X
.  XL - lower bounds
.  XU - upper bounds
.  t1 - work vector
-  t2 - work vector

   Output Parameters:
+  Da - diagonal perturbation component of the result
-  Db - row scaling component of the result

   Level: developer

.seealso: VecFischer()
@*/
PetscErrorCode MatDFischer(Mat jac, Vec X, Vec Con, Vec XL, Vec XU, Vec T1, Vec T2, Vec Da, Vec Db)
{
  PetscErrorCode  ierr;
  PetscInt        i,nn;
  const PetscReal *x,*f,*l,*u,*t2;
  PetscReal       *da,*db,*t1;
  PetscReal        ai,bi,ci,di,ei;

  PetscFunctionBegin;
  ierr = VecGetLocalSize(X,&nn);CHKERRQ(ierr);
  ierr = VecGetArrayRead(X,&x);CHKERRQ(ierr);
  ierr = VecGetArrayRead(Con,&f);CHKERRQ(ierr);
  ierr = VecGetArrayRead(XL,&l);CHKERRQ(ierr);
  ierr = VecGetArrayRead(XU,&u);CHKERRQ(ierr);
  ierr = VecGetArray(Da,&da);CHKERRQ(ierr);
  ierr = VecGetArray(Db,&db);CHKERRQ(ierr);
  ierr = VecGetArray(T1,&t1);CHKERRQ(ierr);
  ierr = VecGetArrayRead(T2,&t2);CHKERRQ(ierr);

  for (i = 0; i < nn; i++) {
    da[i] = 0;
    db[i] = 0;
    t1[i] = 0;

    if (PetscAbsReal(f[i]) <= PETSC_MACHINE_EPSILON) {
      if (l[i] > PETSC_NINFINITY && PetscAbsReal(x[i] - l[i]) <= PETSC_MACHINE_EPSILON) {
        t1[i] = 1;
        da[i] = 1;
      }

      if (u[i] <  PETSC_INFINITY && PetscAbsReal(u[i] - x[i]) <= PETSC_MACHINE_EPSILON) {
        t1[i] = 1;
        db[i] = 1;
      }
    }
  }

  ierr = VecRestoreArray(T1,&t1);CHKERRQ(ierr);
  ierr = VecRestoreArrayRead(T2,&t2);CHKERRQ(ierr);
  ierr = MatMult(jac,T1,T2);CHKERRQ(ierr);
  ierr = VecGetArrayRead(T2,&t2);CHKERRQ(ierr);

  for (i = 0; i < nn; i++) {
    if ((l[i] <= PETSC_NINFINITY) && (u[i] >= PETSC_INFINITY)) {
      da[i] = 0;
      db[i] = -1;
    } else if (l[i] <= PETSC_NINFINITY) {
      if (db[i] >= 1) {
        ai = fischnorm(1, t2[i]);

        da[i] = -1/ai - 1;
        db[i] = -t2[i]/ai - 1;
      } else {
        bi = u[i] - x[i];
        ai = fischnorm(bi, f[i]);
        ai = PetscMax(PETSC_MACHINE_EPSILON, ai);

        da[i] = bi / ai - 1;
        db[i] = -f[i] / ai - 1;
      }
    } else if (u[i] >=  PETSC_INFINITY) {
      if (da[i] >= 1) {
        ai = fischnorm(1, t2[i]);

        da[i] = 1 / ai - 1;
        db[i] = t2[i] / ai - 1;
      } else {
        bi = x[i] - l[i];
        ai = fischnorm(bi, f[i]);
        ai = PetscMax(PETSC_MACHINE_EPSILON, ai);

        da[i] = bi / ai - 1;
        db[i] = f[i] / ai - 1;
      }
    } else if (l[i] == u[i]) {
      da[i] = -1;
      db[i] = 0;
    } else {
      if (db[i] >= 1) {
        ai = fischnorm(1, t2[i]);

        ci = 1 / ai + 1;
        di = t2[i] / ai + 1;
      } else {
        bi = x[i] - u[i];
        ai = fischnorm(bi, f[i]);
        ai = PetscMax(PETSC_MACHINE_EPSILON, ai);

        ci = bi / ai + 1;
        di = f[i] / ai + 1;
      }

      if (da[i] >= 1) {
        bi = ci + di*t2[i];
        ai = fischnorm(1, bi);

        bi = bi / ai - 1;
        ai = 1 / ai - 1;
      } else {
        ei = Fischer(u[i] - x[i], -f[i]);
        ai = fischnorm(x[i] - l[i], ei);
        ai = PetscMax(PETSC_MACHINE_EPSILON, ai);

        bi = ei / ai - 1;
        ai = (x[i] - l[i]) / ai - 1;
      }

      da[i] = ai + bi*ci;
      db[i] = bi*di;
    }
  }

  ierr = VecRestoreArray(Da,&da);CHKERRQ(ierr);
  ierr = VecRestoreArray(Db,&db);CHKERRQ(ierr);
  ierr = VecRestoreArrayRead(X,&x);CHKERRQ(ierr);
  ierr = VecRestoreArrayRead(Con,&f);CHKERRQ(ierr);
  ierr = VecRestoreArrayRead(XL,&l);CHKERRQ(ierr);
  ierr = VecRestoreArrayRead(XU,&u);CHKERRQ(ierr);
  ierr = VecRestoreArrayRead(T2,&t2);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "MatDSFischer"
/*@
   MatDSFischer - Calculates an element of the B-subdifferential of the
   smoothed Fischer-Burmeister function for complementarity problems.

   Collective on jac

   Input Parameters:
+  jac - the jacobian of f at X
.  X - current point
.  F - constraint function evaluated at X
.  XL - lower bounds
.  XU - upper bounds
.  mu - smoothing parameter
.  T1 - work vector
-  T2 - work vector

   Output Parameter:
+  Da - diagonal perturbation component of the result
.  Db - row scaling component of the result
-  Dm - derivative with respect to scaling parameter

   Level: developer

.seealso MatDFischer()
@*/
PetscErrorCode MatDSFischer(Mat jac, Vec X, Vec Con,Vec XL, Vec XU, PetscReal mu,Vec T1, Vec T2,Vec Da, Vec Db, Vec Dm)
{
  PetscErrorCode  ierr;
  PetscInt        i,nn;
  const PetscReal *x, *f, *l, *u;
  PetscReal       *da, *db, *dm;
  PetscReal       ai, bi, ci, di, ei, fi;

  PetscFunctionBegin;
  if (PetscAbsReal(mu) <= PETSC_MACHINE_EPSILON) {
    ierr = VecZeroEntries(Dm);CHKERRQ(ierr);
    ierr = MatDFischer(jac, X, Con, XL, XU, T1, T2, Da, Db);CHKERRQ(ierr);
  } else {
    ierr = VecGetLocalSize(X,&nn);CHKERRQ(ierr);
    ierr = VecGetArrayRead(X,&x);CHKERRQ(ierr);
    ierr = VecGetArrayRead(Con,&f);CHKERRQ(ierr);
    ierr = VecGetArrayRead(XL,&l);CHKERRQ(ierr);
    ierr = VecGetArrayRead(XU,&u);CHKERRQ(ierr);
    ierr = VecGetArray(Da,&da);CHKERRQ(ierr);
    ierr = VecGetArray(Db,&db);CHKERRQ(ierr);
    ierr = VecGetArray(Dm,&dm);CHKERRQ(ierr);

    for (i = 0; i < nn; ++i) {
      if ((l[i] <= PETSC_NINFINITY) && (u[i] >= PETSC_INFINITY)) {
        da[i] = -mu;
        db[i] = -1;
        dm[i] = -x[i];
      } else if (l[i] <= PETSC_NINFINITY) {
        bi = u[i] - x[i];
        ai = fischsnorm(bi, f[i], mu);
        ai = PetscMax(PETSC_MACHINE_EPSILON, ai);

        da[i] = bi / ai - 1;
        db[i] = -f[i] / ai - 1;
        dm[i] = 2.0 * mu / ai;
      } else if (u[i] >=  PETSC_INFINITY) {
        bi = x[i] - l[i];
        ai = fischsnorm(bi, f[i], mu);
        ai = PetscMax(PETSC_MACHINE_EPSILON, ai);

        da[i] = bi / ai - 1;
        db[i] = f[i] / ai - 1;
        dm[i] = 2.0 * mu / ai;
      } else if (l[i] == u[i]) {
        da[i] = -1;
        db[i] = 0;
        dm[i] = 0;
      } else {
        bi = x[i] - u[i];
        ai = fischsnorm(bi, f[i], mu);
        ai = PetscMax(PETSC_MACHINE_EPSILON, ai);

        ci = bi / ai + 1;
        di = f[i] / ai + 1;
        fi = 2.0 * mu / ai;

        ei = SFischer(u[i] - x[i], -f[i], mu);
        ai = fischsnorm(x[i] - l[i], ei, mu);
        ai = PetscMax(PETSC_MACHINE_EPSILON, ai);

        bi = ei / ai - 1;
        ei = 2.0 * mu / ei;
        ai = (x[i] - l[i]) / ai - 1;

        da[i] = ai + bi*ci;
        db[i] = bi*di;
        dm[i] = ei + bi*fi;
      }
    }

    ierr = VecRestoreArrayRead(X,&x);CHKERRQ(ierr);
    ierr = VecRestoreArrayRead(Con,&f);CHKERRQ(ierr);
    ierr = VecRestoreArrayRead(XL,&l);CHKERRQ(ierr);
    ierr = VecRestoreArrayRead(XU,&u);CHKERRQ(ierr);
    ierr = VecRestoreArray(Da,&da);CHKERRQ(ierr);
    ierr = VecRestoreArray(Db,&db);CHKERRQ(ierr);
    ierr = VecRestoreArray(Dm,&dm);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

