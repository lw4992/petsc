
static char help[] = "Solves 1D heat equation with FEM formulation.\n\
Input arguments are\n\
  -useAlhs: solve Alhs*U' =  (Arhs*U + g) \n\
            otherwise, solve U' = inv(Alhs)*(Arhs*U + g) \n\n";

/*--------------------------------------------------------------------------
  Solves 1D heat equation U_t = U_xx with FEM formulation:
                          Alhs*U' = rhs (= Arhs*U + g)
  We thank Chris Cox <clcox@clemson.edu> for contributing the original code
----------------------------------------------------------------------------*/

#include "petscksp.h"
#include "petscts.h"

/* special variable - max size of all arrays  */
#define num_z 60

/* 
   User-defined application context - contains data needed by the 
   application-provided call-back routines.
*/
typedef struct{
  Mat 	   	 Amat;		  /* left hand side matrix */
  Vec		 ksp_rhs,ksp_sol; /* working vectors for formulating inv(Alhs)*(Arhs*U+g) */
  int		 max_probsz;      /* max size of the problem */
  PetscTruth     useAlhs;         /* flag (1 indicates solving Alhs*U' = Arhs*U+g */
  int            nz;              /* total number of grid points */
  PetscInt       m;               /* total number of interio grid points */
  Vec            solution;        /* global exact ts solution vector */
  PetscScalar    *z;              /* array of grid points */
  PetscTruth     debug;           /* flag (1 indicates activation of debugging printouts) */
  PetscReal      norm_2,norm_max; /* error norms */
} AppCtx;

extern double exact(double,double);
extern PetscErrorCode Monitor(TS,PetscInt,PetscReal,Vec,void*);
extern void  Petsc_KSPSolve(AppCtx*);
extern double bspl(double[],double,int,int,int[][2],int);
extern void femBg(double[][3],double[], int,double[],double);
extern void femA(AppCtx*,int,double[]);
extern void rhs(AppCtx *,double[], int, double[],double);
extern PetscErrorCode RHSfunction(TS,PetscReal,Vec,Vec,void*);

#undef __FUNCT__
#define __FUNCT__ "main"
int main(int argc,char **argv)
{
  PetscInt       nt,i,j,k,m,nz,steps, max_steps;
  PetscScalar    zInitial,zFinal,val,*soln,*z;
  PetscReal      tminit,maxtm,stepsz,T,ftime;
  PetscErrorCode ierr;
  TS             ts;
  Mat            Jmat;
  AppCtx         appctx; /* user-defined application context */
  Vec     	 ts_sol; /* ts solution vector */
  PetscMPIInt    size;
  TSType         type;
  PetscTruth     sundialstype=PETSC_FALSE;

  ierr = PetscInitialize(&argc,&argv,(char*)0,help);CHKERRQ(ierr); 
  ierr = MPI_Comm_size(PETSC_COMM_WORLD,&size);CHKERRQ(ierr);
  if (size != 1) SETERRQ(PETSC_ERR_SUP,"This is a uniprocessor example only");

  /* initializations */
  zInitial       = 0.0;
  zFinal         = 1.0;
  T              = 0.014;
  nz             = num_z; 
  m              = nz-2;
  appctx.nz      = nz;
  max_steps      = (PetscInt)10000;
  
  appctx.m          = m;   
  appctx.max_probsz = nz;
  appctx.debug      = PETSC_FALSE;
  appctx.useAlhs    = PETSC_FALSE;
  
  ierr = PetscOptionsHasName(PETSC_NULL,"-debug",&appctx.debug);CHKERRQ(ierr);
  ierr = PetscOptionsHasName(PETSC_NULL,"-useAlhs",&appctx.useAlhs);CHKERRQ(ierr); 

  /* create vector to hold ts solution */
  /*-----------------------------------*/
  ierr = VecCreate(PETSC_COMM_WORLD, &ts_sol);CHKERRQ(ierr);
  ierr = VecSetSizes(ts_sol, PETSC_DECIDE, m);CHKERRQ(ierr);
  ierr = VecSetFromOptions(ts_sol);CHKERRQ(ierr);

  /* create vector to hold true ts soln for comparison */
  ierr = VecDuplicate(ts_sol, &appctx.solution);CHKERRQ(ierr);

  /* create LHS matrix Amat */
  /*------------------------*/
  ierr = MatCreateSeqAIJ(PETSC_COMM_WORLD, m, m, 3, PETSC_NULL, &appctx.Amat);
  ierr = MatSetFromOptions(appctx.Amat);
  /* set space grid points - interio points only! */   
  ierr = PetscMalloc((nz+1)*sizeof(double),&z);CHKERRQ(ierr);
  for (i=0; i<nz; i++) z[i]=(i)*((zFinal-zInitial)/(nz-1)); 
  appctx.z = z;
  femA(&appctx,nz,z);

  /* create the jacobian matrix */
  /*----------------------------*/
  ierr = MatCreate(PETSC_COMM_WORLD, &Jmat);CHKERRQ(ierr);
  ierr = MatSetSizes(Jmat,PETSC_DECIDE,PETSC_DECIDE,m,m);CHKERRQ(ierr);
  ierr = MatSetFromOptions(Jmat);CHKERRQ(ierr);
  
  /* create working vectors for formulating rhs=inv(Alhs)*(Arhs*U + g) */
  ierr = VecDuplicate(ts_sol,&appctx.ksp_rhs);CHKERRQ(ierr); 
  ierr = VecDuplicate(ts_sol,&appctx.ksp_sol);CHKERRQ(ierr);

 

  /* set intial guess */
  /*------------------*/
  for(i=0; i<nz-2; i++){
    val = exact(z[i+1], 0.0); 
    ierr = VecSetValue(ts_sol,i,(PetscScalar)val,INSERT_VALUES);CHKERRQ(ierr);
  }
  ierr = VecAssemblyBegin(ts_sol);
  ierr = VecAssemblyEnd(ts_sol);

  /*create a time-stepping context and set the problem type */
  /*--------------------------------------------------------*/
  ierr = TSCreate(PETSC_COMM_WORLD, &ts);CHKERRQ(ierr);
  ierr = TSSetProblemType(ts,TS_NONLINEAR);CHKERRQ(ierr); 

  /* set time-step method */
  ierr = TSSetType(ts,TS_CRANK_NICHOLSON);CHKERRQ(ierr);
  
  /* Set optional user-defined monitoring routine */
  ierr = TSMonitorSet(ts,Monitor,&appctx,PETSC_NULL);CHKERRQ(ierr);
  
  /* set the right hand side of U_t = RHSfunction(U,t) */
  ierr = TSSetRHSFunction(ts,(PetscErrorCode (*)(TS,double,Vec,Vec,void*))RHSfunction,&appctx);CHKERRQ(ierr);


  if (appctx.useAlhs){
    /* set the left hand side matrix of Amat*U_t = rhs(U,t) */
    ierr = TSSetMatrices(ts,PETSC_NULL,PETSC_NULL,appctx.Amat,PETSC_NULL,DIFFERENT_NONZERO_PATTERN,&appctx);CHKERRQ(ierr); 
  }

  /* use petsc to compute the jacobian by finite differences */
  ierr = TSSetRHSJacobian(ts,Jmat,Jmat,TSDefaultComputeJacobian,&appctx);CHKERRQ(ierr);

  /* get the command line options if there are any and set them */
  ierr = TSSetFromOptions(ts);CHKERRQ(ierr);

#ifdef PETSC_HAVE_SUNDIALS
  ierr = TSGetType(ts,&type);CHKERRQ(ierr);
  ierr = PetscTypeCompare((PetscObject)ts,TS_SUNDIALS,&sundialstype);CHKERRQ(ierr);
  if (sundialstype && appctx.useAlhs){
    SETERRQ(PETSC_ERR_SUP,"Cannot use Alhs formulation for TS_SUNDIALS type");
  }
#endif

  ierr = TSSetSolution(ts,ts_sol);CHKERRQ(ierr);
  stepsz = 1.0/(2.0*(nz-1)*(nz-1)); /* (mesh_size)^2/2.0 */
  ierr = TSSetInitialTimeStep(ts,0.0,stepsz);CHKERRQ(ierr);
  ierr = TSSetDuration(ts,max_steps,T);CHKERRQ(ierr);
  
  /* loop over time steps */
  /*----------------------*/
  ierr = TSStep(ts,&steps,&ftime);CHKERRQ(ierr);
 
  /* free space */ 
  ierr = TSDestroy(ts);CHKERRQ(ierr)
  ierr = MatDestroy(appctx.Amat);CHKERRQ(ierr);
  ierr = MatDestroy(Jmat);CHKERRQ(ierr);
  ierr = VecDestroy(appctx.ksp_rhs);CHKERRQ(ierr);
  ierr = VecDestroy(appctx.ksp_sol);CHKERRQ(ierr);
  ierr = VecDestroy(ts_sol);CHKERRQ(ierr);
  ierr = VecDestroy(appctx.solution);CHKERRQ(ierr);
  ierr = PetscFree(z);CHKERRQ(ierr);

  PetscFinalize();CHKERRQ(ierr); 
  return 0;
}

/*-----------------------------------------------------------------------*/
/*------------------------------------------------------------------------
  Set exact solution 
  u(z,t) = sin(6*PI*z)*exp(-36.*PI*PI*t) + 3.*sin(2*PI*z)*exp(-4.*PI*PI*t)
--------------------------------------------------------------------------*/
double exact(double z,double t)
{
  double val, ex1, ex2;

  ex1 = exp(-36.*PETSC_PI*PETSC_PI*t);
  ex2 = exp(-4.*PETSC_PI*PETSC_PI*t);
  val = sin(6*PETSC_PI*z)*ex1 + 3.*sin(2*PETSC_PI*z)*ex2;
  return val;
}

#undef __FUNCT__
#define __FUNCT__ "Monitor"
/*
   Monitor - User-provided routine to monitor the solution computed at 
   each timestep.  This example plots the solution and computes the
   error in two different norms.

   Input Parameters:
   ts     - the timestep context
   step   - the count of the current step (with 0 meaning the
             initial condition)
   time   - the current time
   u      - the solution at this timestep
   ctx    - the user-provided context for this monitoring routine.
            In this case we use the application context which contains 
            information about the problem size, workspace and the exact 
            solution.
*/
PetscErrorCode Monitor(TS ts,PetscInt step,PetscReal time,Vec u,void *ctx)
{
  AppCtx         *appctx = (AppCtx*)ctx;   
  PetscErrorCode ierr;
  PetscInt       i,m=appctx->m;
  PetscReal      norm_2,norm_max,h=1.0/(m+1);
  PetscScalar    *u_exact;

  /* Compute the exact solution */
  ierr = VecGetArray(appctx->solution,&u_exact);CHKERRQ(ierr);
  for (i=0; i<m; i++){
    u_exact[i] = exact(appctx->z[i+1],time);
  }
  ierr = VecRestoreArray(appctx->solution,&u_exact);CHKERRQ(ierr);

  /* Print debugging information if desired */
  if (appctx->debug) {
     ierr = PetscPrintf(PETSC_COMM_SELF,"Computed solution vector\n");CHKERRQ(ierr);
     ierr = VecView(u,PETSC_VIEWER_STDOUT_SELF);CHKERRQ(ierr);
     ierr = PetscPrintf(PETSC_COMM_SELF,"Exact solution vector\n");CHKERRQ(ierr);
     ierr = VecView(appctx->solution,PETSC_VIEWER_STDOUT_SELF);CHKERRQ(ierr);
  }

  /* Compute the 2-norm and max-norm of the error */
  ierr = VecAXPY(appctx->solution,-1.0,u);CHKERRQ(ierr);
  ierr = VecNorm(appctx->solution,NORM_2,&norm_2);CHKERRQ(ierr);

  norm_2 = sqrt(h)*norm_2;
  ierr = VecNorm(appctx->solution,NORM_MAX,&norm_max);CHKERRQ(ierr);

  ierr = PetscPrintf(PETSC_COMM_SELF,"Timestep %D: time = %G, 2-norm error = %6.4f, max norm error = %6.4f\n",
              step,time,norm_2,norm_max);CHKERRQ(ierr);
  appctx->norm_2   += norm_2;
  appctx->norm_max += norm_max;

  /*
     Print debugging information if desired
  */
  if (appctx->debug) {
     ierr = PetscPrintf(PETSC_COMM_SELF,"Error vector\n");CHKERRQ(ierr);
     ierr = VecView(appctx->solution,PETSC_VIEWER_STDOUT_SELF);CHKERRQ(ierr);
  }
  return 0;
}

/*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%% 	Function to solve a linear system using KSP					      %%
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%*/

void  Petsc_KSPSolve(AppCtx *obj)
{
   PetscErrorCode  ierr;
   KSP             ksp;
   PC              pc;

   /*create the ksp context and set the operators,that is, associate the system matrix with it*/
   ierr = KSPCreate(PETSC_COMM_WORLD,&ksp);
   ierr = KSPSetOperators(ksp,obj->Amat,obj->Amat,DIFFERENT_NONZERO_PATTERN);

   /*get the preconditioner context, set its type and the tolerances*/
   ierr = KSPGetPC(ksp,&pc);
   ierr = PCSetType(pc,PCLU);
   ierr = KSPSetTolerances(ksp,1.e-7,PETSC_DEFAULT,PETSC_DEFAULT,PETSC_DEFAULT);

   /*get the command line options if there are any and set them*/
   ierr = KSPSetFromOptions(ksp);

   /*get the linear system (ksp) solve*/
   ierr = KSPSolve(ksp,obj->ksp_rhs,obj->ksp_sol);

   KSPDestroy(ksp);
   return;
}

/***********************************************************************
 * Function to return value of basis function or derivative of basis   *
 *              function.                                              *
 ***********************************************************************
 *                                                                     *
 *       Arguments:                                                    *
 *         x       = array of xpoints or nodal values                  *
 *         xx      = point at which the basis function is to be        *
 *                     evaluated.                                      *
 *         il      = interval containing xx.                           *
 *         iq      = indicates which of the two basis functions in     *
 *                     interval intrvl should be used                  *
 *         nll     = array containing the endpoints of each interval.  *
 *         id      = If id ~= 2, the value of the basis function       *
 *                     is calculated; if id = 2, the value of the      *
 *                     derivative of the basis function is returned.   *
 ***********************************************************************/

double bspl(double x[],double xx,int il,int iq,int nll[][2],int id)
{
  double  x1,x2,bfcn;
  int i1,i2,iq1,iq2;

  /*** Determine which basis function in interval intrvl is to be used in ***/                        
  iq1 = iq;
  if(iq1==0) iq2 = 1;
  else iq2 = 0;
        
  /***  Determine endpoint of the interval intrvl ***/
  i1=nll[il][iq1];
  i2=nll[il][iq2];

  /*** Determine nodal values at the endpoints of the interval intrvl ***/ 
  x1=x[i1];
  x2=x[i2];
  //printf("x1=%g\tx2=%g\txx=%g\n",x1,x2,xx);
  /*** Evaluate basis function ***/
  if(id == 2) bfcn=(1.0)/(x1-x2);
  else bfcn=(xx-x2)/(x1-x2);
  //printf("bfcn=%g\n",bfcn);
  return bfcn;
}

/*---------------------------------------------------------
  Function called by rhs function to get B and g 
---------------------------------------------------------*/
void femBg(double btri[][3],double f[],int nz,double z[], double t)
{  
  int i,j,jj,il,ip,ipp,ipq,iq,iquad,iqq;
  int nli[num_z][2],indx[num_z];
  double dd,dl,zip,zipq,zz,bb,b_z,bbb,bb_z,bij;
  double zquad[num_z][3],dlen[num_z],qdwt[3];

  /*  initializing everything - btri and f are initialized in rhs.c  */  
  for(i=0; i < nz; i++){
    nli[i][0] = 0;
    nli[i][1] = 0;
    indx[i] = 0;
    zquad[i][0] = 0.0;
    zquad[i][1] = 0.0;
    zquad[i][2] = 0.0;
    dlen[i] = 0.0;
  }/*end for(i)*/

  /*  quadrature weights  */
  qdwt[0] = 1.0/6.0;
  qdwt[1] = 4.0/6.0;
  qdwt[2] = 1.0/6.0;

  /* 1st and last nodes have Dirichlet boundary condition -
     set indices there to -1 */

  for(i=0; i < nz-1; i++){
    indx[i]=i-1;
  }
  indx[nz-1]=-1;
  
  ipq = 0;
  for (il=0; il < nz-1; il++){
    ip = ipq;
    ipq = ip+1;
    zip = z[ip];
    zipq = z[ipq];
    dl = zipq-zip;
    zquad[il][0] = zip;
    zquad[il][1] = (0.5)*(zip+zipq);
    zquad[il][2] = zipq;
    dlen[il] = fabs(dl);
    nli[il][0] = ip;
    nli[il][1] = ipq;
  }

  for (il=0; il < nz-1; il++){
    for (iquad=0; iquad < 3; iquad++){
      dd = (dlen[il])*(qdwt[iquad]);
      zz = zquad[il][iquad];
        
      for (iq=0; iq < 2; iq++){
        ip = nli[il][iq];
        bb = bspl(z,zz,il,iq,nli,1);
        b_z = bspl(z,zz,il,iq,nli,2);
        i = indx[ip];
           
        if(i > -1){
          for(iqq=0; iqq < 2; iqq++){
            ipp = nli[il][iqq];
            bbb = bspl(z,zz,il,iqq,nli,1);
            bb_z = bspl(z,zz,il,iqq,nli,2);
            j = indx[ipp];
            bij = -b_z*bb_z;
                
            if (j > -1){
              jj = 1+j-i;
              btri[i][jj] += bij*dd;
            } else {  
              f[i] += bij*dd*exact(z[ipp], t);
              // f[i] += 0.0;
              // if(il==0 && j==-1){
              // f[i] += bij*dd*exact(zz,t);
              // }/*end if*/
            } /*end else*/
          }/*end for(iqq)*/
        }/*end if(i>0)*/
      }/*end for(iq)*/
    }/*end for(iquad)*/
  }/*end for(il)*/                    
  return;
}

void femA(AppCtx *obj,int nz,double z[])
{  
  int             i,j,il,ip,ipp,ipq,iq,iquad,iqq;
  int             nli[num_z][2],indx[num_z];
  double          dd,dl,zip,zipq,zz,bb,bbb,aij;
  double          rquad[num_z][3],dlen[num_z],qdwt[3],add_term;

  PetscErrorCode  ierr;

  /*  initializing everything  */
  
  for(i=0; i < nz; i++)
  {
     nli[i][0] = 0;
     nli[i][1] = 0;
     indx[i] = 0;
     rquad[i][0] = 0.0;
     rquad[i][1] = 0.0;
     rquad[i][2] = 0.0;
     dlen[i] = 0.0;
  }/*end for(i)*/

  /*  quadrature weights  */
  qdwt[0] = 1.0/6.0;
  qdwt[1] = 4.0/6.0;
  qdwt[2] = 1.0/6.0;

  /* 1st and last nodes have Dirichlet boundary condition - 
     set indices there to -1 */

  for(i=0; i < nz-1; i++)
  {
     indx[i]=i-1;

  }/*end for(i)*/
  indx[nz-1]=-1;

  ipq = 0;  

  for(il=0; il < nz-1; il++)
  {
     ip = ipq;
     ipq = ip+1;
     zip = z[ip];
     zipq = z[ipq];
     dl = zipq-zip;
     rquad[il][0] = zip;
     rquad[il][1] = (0.5)*(zip+zipq);
     rquad[il][2] = zipq;
     dlen[il] = fabs(dl);
     nli[il][0] = ip;
     nli[il][1] = ipq;

  }/*end for(il)*/

  for(il=0; il < nz-1; il++){
    for(iquad=0; iquad < 3; iquad++){
      dd = (dlen[il])*(qdwt[iquad]);
      zz = rquad[il][iquad];

      for(iq=0; iq < 2; iq++){
        ip = nli[il][iq];
        bb = bspl(z,zz,il,iq,nli,1);
        i = indx[ip];
        if(i > -1){
          for(iqq=0; iqq < 2; iqq++){
            ipp = nli[il][iqq];
            bbb = bspl(z,zz,il,iqq,nli,1);
            j = indx[ipp];
            aij = bb*bbb;
            if(j > -1) {
              add_term = aij*dd;
              ierr = MatSetValue(obj->Amat,(PetscInt)i,(PetscInt)j,(PetscScalar)add_term,ADD_VALUES);
            }/*endif*/ 
          }/*end for(iqq)*/
        }/*end if(i>0)*/
      }/*end for(iq)*/
    }/*end for(iquad)*/
  }/*end for(il)*/
  MatAssemblyBegin(obj->Amat,MAT_FINAL_ASSEMBLY);
  MatAssemblyEnd(obj->Amat,MAT_FINAL_ASSEMBLY);
  return;
}

/*---------------------------------------------------------
	Function to fill the rhs vector with 
	By + g values ****
---------------------------------------------------------*/
void rhs(AppCtx *obj,double y[], int nz, double z[], double t)
{ 
  int             i,j,js,je,jj;
  double          val;
  double          g[num_z];
  double          btri[num_z][3],add_term;
  PetscErrorCode  ierr;

  for(i=0; i < nz-2; i++){
    for(j=0; j <= 2; j++){
      btri[i][j]=0.0;
    }      
    g[i] = 0.0;      
  }

  /*  call femBg to set the tri-diagonal b matrix and vector g  */
  femBg(btri, g,nz,z,t);

  /*  setting the entries of the right hand side vector  */
  for(i=0; i < nz-2; i++){
    val = 0.0;
    js = 0;
    if(i == 0) js = 1;
    je = 2;
    if(i == nz-2) je = 1;

    for(jj=js; jj <= je; jj++){
      j = i+jj-1;
      val += (btri[i][jj])*(y[j]);
    } 
    add_term = val + g[i];
    ierr = VecSetValue(obj->ksp_rhs,(PetscInt)i,(PetscScalar)add_term,INSERT_VALUES);
  }
  VecAssemblyBegin(obj->ksp_rhs);
  VecAssemblyEnd(obj->ksp_rhs);

  /*  return to main driver function  */
  return;
}

/*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%%   Function to form the right hand side of the time-stepping problem.                       %%
%% -------------------------------------------------------------------------------------------%%
  if (useAlhs):
    globalout = By+g
  else if (!useAlhs):
    globalout = f(y,t)=Ainv(By+g), 
      in which the ksp solver to transform the problem A*ydot=By+g 
      to the problem ydot=f(y,t)=inv(A)*(By+g).                                                      
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%*/

PetscErrorCode RHSfunction(TS ts,PetscReal t,Vec globalin,Vec globalout,void *ctx)
{
  PetscErrorCode ierr;
  AppCtx         *obj = (AppCtx*)ctx;
  PetscScalar    *soln_ptr;
  int            i,nz=obj->nz;
  double	 soln[num_z-2], time;

  /* get the previous solution to compute updated system */
  ierr = VecGetArray(globalin,&soln_ptr);
  for(i=0;i < num_z-2;i++){
    soln[i] = soln_ptr[i];
  }
  ierr = VecRestoreArray(globalin,&soln_ptr);
  
  /* clear out the matrix and rhs for ksp to keep things straight */  
  ierr = VecSet(obj->ksp_rhs,(PetscScalar)0.0);

  time = (double)t;
  /* get the updated system */
  rhs(obj,soln,nz,obj->z,time); /* setup of the By+g rhs */  

  /* do a ksp solve to get the rhs for the ts problem */
  if (obj->useAlhs){
    /* ksp_sol = ksp_rhs */
    ierr = VecCopy(obj->ksp_rhs,globalout);
  } else {
    /* ksp_sol = inv(Amat)*ksp_rhs */
    Petsc_KSPSolve(obj);
    ierr = VecCopy(obj->ksp_sol,globalout);
  } 
  return 0;
}
