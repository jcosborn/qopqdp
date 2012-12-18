/* adapted from MILC version 6 */

/* conventions for raw packed clover term

the clover term is a REAL array of length 72*QDP_sites_on_node
on each site are 72 REALs made of 2 packed Hermitian matrices of 36 REALs each
each packed Hermitian matrix has the following structure:
6 REALs for the diagonal
30 REALs = 15 Complex for the lower triangle stored columnwise

the physical spin and color component order for the clover term is
given by the following order:

color  spin
0      0
0      1
1      0
1      1
2      0
2      1
0      2
0      3
1      2
1      3
2      2
2      3

In other words each chiral block (6x6 matrix) is ordered with color slowest
and the 2 components of spin for that block going fastest.

Explicitly, if c(ic,is,jc,js) is the clover term with ic,is the color and spin
of the row and jc,js the color and spin of the column then the order of
elements is:

real(c(0,0, 0,0))
real(c(0,1, 0,1))
real(c(1,0, 1,0))
real(c(1,1, 1,1))
real(c(2,0, 2,0))
real(c(2,1, 2,1))
c(0,1, 0,0)
c(1,0, 0,0)
c(1,1, 0,0)
c(2,0, 0,0)
c(2,1, 0,0)
c(1,0, 0,1)
c(1,1, 0,1)
c(2,0, 0,1)
c(2,1, 0,1)
c(1,1, 1,0)
c(2,0, 1,0)
c(2,1, 1,0)
c(2,0, 1,1)
c(2,1, 1,1)
c(2,1, 2,0)

and likewise for the second block by adding 2 to all is,js values above.
*/

//#define DO_TRACE
#include <string.h>
//#include <math.h>
#include <qop_internal.h>

//#define printf0 QOP_printf0
#define printf0(...)

#define dblstore_style(x) ((x)&1)
#define shiftd_style(x) ((x)&2)

extern int QOP_wilson_inited;
extern int QOP_wilson_style;
extern int QOP_wilson_nsvec;
extern int QOP_wilson_nvec;
extern int QOP_wilson_cgtype;
extern int QOP_wilson_optnum;

static int old_style=-1;
static int old_optnum=-1;

#define NTMPSUB 2
#define NTMP (3*NTMPSUB)
#define NHTMP 16
#define NDTMP 12
static int dslash_setup = 0;
static QDP_HalfFermion *htemp[NTMP][NHTMP];
static QDP_DiracFermion *dtemp[NTMP][NDTMP];
static QDP_DiracFermion *tin[NTMP];
#define tmpnum(eo,n) ((eo)+3*((n)-1))
#define tmpsub(eo,n) tin[tmpnum(eo,n)]

#define check_setup(flw) \
{ \
  if( (!dslash_setup) || (QOP_wilson_optnum != old_optnum) ) { \
    reset_temps(NCARGVOID); \
  } \
  if( flw->dblstored != dblstore_style(QOP_wilson_style) ) { \
    double_store(flw); \
  } \
}

static void
free_temps(void)
{
  if(dslash_setup) {
    for(int i=0; i<NTMP; i++) {
      QDP_destroy_D(tin[i]);
    }
    if(shiftd_style(old_style)) {
      for(int i=0; i<NTMP; i++) {
	for(int j=0; j<NDTMP; j++) {
	  QDP_destroy_D(dtemp[i][j]);
	}
      }
    } else {
      for(int i=0; i<NTMP; i++) {
	for(int j=0; j<NHTMP; j++) {
	  QDP_destroy_H(htemp[i][j]);
	}
      }
    }
  }
  dslash_setup = 0;
}

#define NC nc
static void
reset_temps(NCPROTVOID)
{
  free_temps();
  for(int i=0; i<NTMP; i++) {
    tin[i] = QDP_create_D();
  }
  if(shiftd_style(QOP_wilson_style)) {
    for(int i=0; i<NTMP; i++) {
      for(int j=0; j<NDTMP; j++) {
	dtemp[i][j] = QDP_create_D();
      }
    }
  } else {
    for(int i=0; i<NTMP; i++) {
      for(int j=0; j<NHTMP; j++) {
	htemp[i][j] = QDP_create_H();
      }
    }
  }
  dslash_setup = 1;
  old_style = QOP_wilson_style;
  old_optnum = QOP_wilson_optnum;
}
#undef NC

static void
double_store(QOP_FermionLinksWilson *flw)
{
#define NC QDP_get_nc(flw->links[0])
  if(flw->dblstored) {
    for(int i=0; i<4; i++) {
      QDP_destroy_M(flw->bcklinks[i]);
    }
    flw->dblstored = 0;
  }
  if(dblstore_style(QOP_wilson_style)) {
    for(int i=0; i<4; i++) {
      flw->bcklinks[i] = QDP_create_M();
    }
    for(int i=0; i<4; i++) {
      flw->dbllinks[2*i] = flw->links[i];
      flw->dbllinks[2*i+1] = flw->bcklinks[i];
    }
    QDP_ColorMatrix *m = QDP_create_M();
    for(int i=0; i<4; i++) {
      QDP_M_eq_sM(m, flw->links[i], QDP_neighbor[i], QDP_backward, QDP_all);
      QDP_M_eq_Ma(flw->bcklinks[i], m, QDP_all);
    }
    QDP_destroy_M(m);
    flw->dblstored = dblstore_style(QOP_wilson_style);
  }
#undef NC
}

QDP_DiracFermion *
QOP_wilson_dslash_get_tmp(QOP_FermionLinksWilson *flw,
			  QOP_evenodd_t eo, int n)
{
#define NC QDP_get_nc(flw->links[0])
  check_setup(flw);
  if(n>=1 && n<=NTMPSUB) return tmpsub(eo,n);
  else return NULL;
#undef NC
}

/* ---------------------------------------------------------------- */
/* This part is added by Bugra :::::------------------------------- */
/* F_mu_nu calculates the $F_{\mu\nu}$ Field Strength Tensor  ----- */
/* --------------------------------------------------------------   */
static void
f_mu_nu(QDP_ColorMatrix *fmn, QLA_Real scale, QDP_ColorMatrix *link[], int mu, int nu)
{
#define NC QDP_get_nc(fmn)
  int order_flag;

  QDP_ColorMatrix *temp1,*temp2,*temp3,*temp4,*tmat4;
  QDP_ColorMatrix *pqt0,*pqt1,*pqt2,*pqt3;
  QDP_ColorMatrix *pqt4;

  temp1 = QDP_create_M();
  temp2 = QDP_create_M();
  temp3 = QDP_create_M();
  temp4 = QDP_create_M();
  tmat4 = QDP_create_M();
  pqt0  = QDP_create_M();
  pqt1  = QDP_create_M();
  pqt2  = QDP_create_M();
  pqt3  = QDP_create_M();
  pqt4  = QDP_create_M();

  /* chech if mu <nu */
  if(mu>nu){
    int i = mu;
    mu = nu;
    nu = i;
    order_flag=1;
  }
  else{
    order_flag=0;
  }

  /* Get pqt0 = U_nu(x+mu) : U_nu(x) from mu direction    */
  /* Get pqt1 = U_mu(x+nu) : U_mu(x) from nu direction    */
  QDP_M_eq_sM(pqt0,link[nu],QDP_neighbor[mu],QDP_forward,QDP_all);
  QDP_M_eq_sM(pqt1,link[mu],QDP_neighbor[nu],QDP_forward,QDP_all);

  /* creating a corner : temp1 = U^d_nu(x) U_mu(x) */
  QDP_M_eq_Ma_times_M(temp1,link[nu],link[mu],QDP_all);

  /* ------------------------------------------------- */
  /* creating a corner : fmn = [pqt0]*[pqt1^d]         */
  /*                     fmn = U_nu(x+mu) U^d_mu(x+nu) */
  QDP_M_eq_M_times_Ma(fmn,pqt0,pqt1,QDP_all);

  /* Create the following loops */
  /* U^d_nu(x) U_mu(x) U_nu(x+mu) U^d_mu(x+nu)  */
  QDP_M_eq_M_times_M(temp2,temp1,fmn,QDP_all);

  /* U_nu(x+mu) U^d_mu(x+nu) U^d_nu(x) U_mu(x) */
  QDP_M_eq_M_times_M(temp3,fmn,temp1,QDP_all);

  /* Creating the following +mu -nu plaquette */
  QDP_M_eq_sM(pqt2,temp2,QDP_neighbor[nu],QDP_backward,QDP_all);

  /* Creating the following -mu +nu plaquette */
  QDP_M_eq_sM(pqt3,temp3,QDP_neighbor[mu],QDP_backward,QDP_all);
  //QDP_discard_M(temp3); /* data in temp3 is no longer needed */

  /* creating +mu +nu plaquette and pit it in fmn      */
  /* tmat4 = U_mu(x) U_nu(x+mu) U^d_mu(x+nu)           */
  /* fmn   = U_mu(x) U_nu(x+mu) U^d_mu(x+nu) U^d_nu(x) */ 
  QDP_M_eq_M_times_M(tmat4,link[mu],fmn,QDP_all);
  QDP_M_eq_M_times_Ma(fmn,tmat4,link[nu],QDP_all);

  /* What is left is +mu -nu plaquette and adding them up */
  /* Right hand side of the clover field */
  QDP_M_eq_M_plus_M(fmn,pqt2,fmn,QDP_all);

  /* tmat4 = [(pqt[1]^d)   ] * [(tempmat1 )            ]  */
  /*       = [U^d_mu(x+nu) ] * [U^dagger_nu(x) *U_mu(x)]  */
  /* temp2 = [tmat4]*[pqt0 ]                            */
  /*       = [U^d_mu(x+nu) U^d_nu(x) U_mu(x)*[U_nu(x+mu)] */
  QDP_M_eq_Ma_times_M(tmat4,pqt1,temp1,QDP_all);
  QDP_M_eq_M_times_M(temp2,tmat4,pqt0,QDP_all);
  /* temp1 is a result of a shift and won't be needed   */
  QDP_discard_M(temp1);

  /* temp2 is now a plaquette -mu -nu and must be gathered */
  /* with displacement -mu-nu */
  /* pqt4 = U^d_mu(x-nu)U^d(x-mu-nu)U_mu(x-mu-nu)U_nu(x-nu) */
  QDP_M_eq_sM(temp4,temp2,QDP_neighbor[mu],QDP_backward,QDP_all);
  QDP_M_eq_sM(pqt4,temp4,QDP_neighbor[nu],QDP_backward,QDP_all);
  QDP_discard_M(temp2);

  /* Now gather -mu +nu  plaquette and add to fmn    */
  /* f_mn was the right hand side of the clover term */
  /* I add the third plaquette : f_mn = f_mn+pqt3    */
  /* U_nu(x) U^d(x-mu+nu) U^d_nu(x-mu) U_mu(x-mu)    */
  QDP_M_eq_M_plus_M(fmn,fmn,pqt3,QDP_all);

  /* finally add the last plaquette        */
  /* fmn = fmn+ pqt4                       */
  /* pqt4 is the last plaquette missing    */
  /* This completes the 4-plaquette        */
  QDP_M_eq_M_plus_M(fmn,fmn,pqt4,QDP_all);

  /* F_munu  is now 1/8 of f_mn-f_mn^dagger */
  /* QDP_T_eqop_Ta(Type *r, Type *a, subset) : r=conjugate(a) */
  /* tmat4  =Hermitian(fmn) */
  QDP_M_eq_Ma(tmat4,fmn,QDP_all);

  if(order_flag ==0){
    QDP_M_eq_M_minus_M(tmat4,fmn,tmat4,QDP_all);
  }
  else {
    QDP_M_eq_M_minus_M(tmat4,tmat4,fmn,QDP_all);
  }

  /* F_mn = 1/8 *tmat4 */
  /* QDP_T_eq_r_times_T(Type *r, QLA_real *a, Type *b, subset); */
  scale *= 0.125;
  QDP_M_eq_r_times_M(fmn, &scale, tmat4, QDP_all);

  QDP_destroy_M(temp1);
  QDP_destroy_M(temp2);
  QDP_destroy_M(temp3);
  QDP_destroy_M(temp4);
  QDP_destroy_M(tmat4);
  QDP_destroy_M(pqt0);
  QDP_destroy_M(pqt1);
  QDP_destroy_M(pqt2);
  QDP_destroy_M(pqt3);
  QDP_destroy_M(pqt4);
#undef NC
}

/* -------------------------------------------------------- */
/* -- Calculation of the clover term :: added by Bugra ---- */
/* -------------------------------------------------------- */
static void
get_clov(QLA_Real *clov, QDP_ColorMatrix *link[], QLA_Real cs, QLA_Real ct)
{
#define NC QDP_get_nc(link[0])
  /* Fist I create A[0],A[1],B[0] and B[1] matrices */
  QDP_ColorMatrix *a[2],*b[2];
  QDP_ColorMatrix *f_mn;
  QDP_ColorMatrix *xm,*ym;

  f_mn  = QDP_create_M();
  a[0]  = QDP_create_M();
  a[1]  = QDP_create_M();
  xm = QDP_create_M();
  ym = QDP_create_M();

  f_mu_nu(f_mn, cs, link, 0, 1);         /* f_mn = cs*F_{01}        */
  QDP_M_eq_M(xm, f_mn, QDP_all);         /* xm = cs*F_{01}        */
  QDP_M_eq_M(ym, f_mn, QDP_all);         /* ym = cs*F_{01}        */

  f_mu_nu(f_mn, ct, link, 2, 3);         /* f_mn = ct*F_{23}        */ 
  QDP_M_meq_M(xm, f_mn, QDP_all);        /* xm = cs*F_{01}-ct*F_{23} */
  QDP_M_peq_M(ym, f_mn, QDP_all);        /* ym = cs*F_{01}+ct*F_{23} */

  // PS: One cannot QDP_M_eq_i_M(a,b,subset) with a=b 
  QDP_M_eq_i_M(a[0], xm, QDP_all);       /* a[0] = i(cs*F_{01}-ct*F_{23}) */ 
  QDP_M_eq_i_M(a[1], ym, QDP_all);       /* a[1] = i(cs*F_{01}+ct*F_{23}) */

  /* PART 1 and PART 2 */
  QLA_ColorMatrix(*A[2]);
  A[0] = QDP_expose_M(a[0]);
  A[1] = QDP_expose_M(a[1]);

  for(int i=0; i<QDP_sites_on_node; i++) {
    for(int j=0; j<QLA_Nc; j++) {
      /* diagoal elements numbered from 00 to 05 for the matrix X */
      /* c(0,0, 0,0) = clov[0]  c(1,1, 1,1) = clov[3]             */
      /* c(0,1, 0,1) = clov[1]  c(2,0, 2,0) = clov[4]             */
      /* c(1,0, 1,0) = clov[2]  c(2,1, 2,1) = clov[5]             */
      clov[(72*i)+(2*j  )] =  QLA_real(QLA_elem_M(A[0][i],j,j));
      clov[(72*i)+(2*j+1)] = -QLA_real(QLA_elem_M(A[0][i],j,j));
      /* diagoal elements numbered from 36 to 41 for the matrix Y */
      /* c(0,2, 0,2) = clov[36]  c(1,3, 1,3) = clov[39]           */
      /* c(0,3, 0,3) = clov[37]  c(2,2, 2,2) = clov[40]           */
      /* c(1,2, 1,2) = clov[38]  c(2,3, 2,3) = clov[41]           */
      clov[(72*i)+(2*j+36)] =  QLA_real(QLA_elem_M(A[1][i],j,j));
      clov[(72*i)+(2*j+37)] = -QLA_real(QLA_elem_M(A[1][i],j,j));
      /* -------------------------------------------------------- */
      /* 12 real number are assigned to the array clov            */

      /* Below the triangular which have A[0] and A[1] only       */
      for(int k=0; k<j; k++) { 
	/* c(1,0, 0,0) = clov[ 8]+i*clov[ 9] */
	/* c(2,0, 0,0) = clov[12]+i*clov[13] */
	/* c(2,0, 1,0) = clov[26]+i*clov[27] */
	int jk = 4 + (4*j) + (14*k);
	clov[(72*i)+(jk  ) ] = QLA_real(QLA_elem_M(A[0][i],j,k));
	clov[(72*i)+(jk+1) ] = QLA_imag(QLA_elem_M(A[0][i],j,k));
	clov[(72*i)+(jk+36)] = QLA_real(QLA_elem_M(A[1][i],j,k));
	clov[(72*i)+(jk+37)] = QLA_imag(QLA_elem_M(A[1][i],j,k));

	/* c(1,1, 0,1) = clov[18]+i*clov[19] */
	/* c(2,1, 0,1) = clov[22]+i*clov[23] */
	/* c(2,1, 1,1) = clov[32]+i*clov[33] */
	jk = 14 + (4*j) + (10*k);
	clov[(72*i)+(jk   )] = -QLA_real(QLA_elem_M(A[0][i],j,k));
	clov[(72*i)+(jk+1 )] = -QLA_imag(QLA_elem_M(A[0][i],j,k));
	clov[(72*i)+(jk+36)] = -QLA_real(QLA_elem_M(A[1][i],j,k));
	clov[(72*i)+(jk+37)] = -QLA_imag(QLA_elem_M(A[1][i],j,k));
      }
    }
  }

  QDP_reset_M(a[0]);
  QDP_reset_M(a[1]);

  QDP_destroy_M(a[0]);
  QDP_destroy_M(a[1]);

  /* Creating B[0] and B[1] : 3x3 matrices for X and Y                     */
  /* ------------------ NOTATION ------------------------------------------*/
  /* For the 3x3 part of X   : B[0] above                                  */
  /* c(1,1, 0,0) = clov[10]+i*clov[11]; c(1,1, 1,0) = clov[24]+i*clov[25]; */
  /* c(2,0, 0,0) = clov[12]+i*clov[13]; c(2,0, 1,0) = clov[26]+i*clov[27]; */
  /* c(2,1, 0,0) = clov[14]+i*clov[15]; c(2,1, 1,0) = clov[28]+i*clov[29]; */
  /* c(1,1, 0,1) = clov[18]+i*clov[19]; */
  /* c(2,0, 0,1) = clov[20]+i*clov[21]; */
  /* c(2,1, 0,1) = clov[22]+i*clov[23]; */
  /* For the 3x3 part of Y   : B[1] above                                  */
  /* c(1,3, 0,2) = clov[46]+i*clov[47]; c(1,3, 1,2) = clov[60]+i*clov[61]; */
  /* c(2,2, 0,2) = clov[48]+i*clov[49]; c(2,2, 1,2) = clov[62]+i*clov[63]; */
  /* c(2,3, 0,2) = clov[50]+i*clov[51]; c(2,3, 1,2) = clov[64]+i*clov[65]; */
  /* c(1,3, 0,3) = clov[54]+i*clov[55]; */
  /* c(2,2, 0,3) = clov[56]+i*clov[57]; */
  /* c(2,3, 0,3) = clov[58]+i*clov[59]; */

  b[0] = QDP_create_M();
  b[1] = QDP_create_M();

  f_mu_nu(xm, cs, link, 1, 2);               /* xm   = cs*F_{12}         */  
  f_mu_nu(f_mn, ct, link, 0, 3);             /* f_mn = ct*F_{03}         */

  QDP_M_eq_M_plus_M(ym, xm, f_mn, QDP_all);  /* ym = cs*F_{12}+ct*F_{03}    */
  QDP_M_meq_M(xm, f_mn, QDP_all);            /* xm = cs*F_{12}-ct*F_{03}    */

  QDP_M_eq_i_M(b[0], xm, QDP_all);           /* b[0] = i(cs*F_{12}-ct*F_{03}) */
  QDP_M_eq_i_M(b[1], ym, QDP_all);           /* b[1] = i(cs*F_{12}+ct*F_{03}) */

  f_mu_nu(f_mn, cs, link, 0, 2);             /* f_mn = cs*F_{02}           */
  QDP_M_meq_M(b[0], f_mn, QDP_all);          /* b[0] = b[0]-cs*F_{02}      */
  QDP_M_meq_M(b[1], f_mn, QDP_all);          /* b[1] = b[1]-cs*F_{02}      */

  f_mu_nu(f_mn, ct, link, 1, 3);             /* f_mn = ct*F_{13}           */
  QDP_M_meq_M(b[0], f_mn, QDP_all);          /* b[0] = b[0]-ct*F_{13}      */
  QDP_M_peq_M(b[1], f_mn, QDP_all);          /* b[1] = b[1]+ct*F_{13}      */

  /*  b[0] = i(cs*F_{12}-ct*F_{03})-(cs*F_{02}+ct*F_{13})    */
  /*  b[1] = i(cs*F_{12}+ct*F_{03})-(cs*F_{02}-ct*F_{13})    */

  QLA_ColorMatrix(*B[2]);
  B[0] = QDP_expose_M(b[0]);
  B[1] = QDP_expose_M(b[1]);
  for(int i=0; i<QDP_sites_on_node; i++) {
#if 1
    for(int j=0; j<QLA_Nc; j++) {
      for(int k=0; k<QLA_Nc; k++) {
	if(j<k) {
	  /* c(0,1, 1,0)* = clov[16]+i*clov[17] */
	  /* c(0,1, 2,0)* = clov[20]+i*clov[21] */
	  /* c(1,1, 2,0)* = clov[30]+i*clov[31] */
	  int jk = 12 + 10*j + 4*k;
	  clov[72*i+(jk   )] =  QLA_real(QLA_elem_M(B[0][i],j,k));
	  clov[72*i+(jk+1 )] = -QLA_imag(QLA_elem_M(B[0][i],j,k));
	  clov[72*i+(jk+36)] =  QLA_real(QLA_elem_M(B[1][i],j,k));
	  clov[72*i+(jk+37)] = -QLA_imag(QLA_elem_M(B[1][i],j,k));
	} else {
	  /* c(0,1, 0,0) = clov[ 6]+i*clov[ 7] */
	  /* c(1,1, 0,0) = clov[10]+i*clov[11] */
	  /* c(2,1, 0,0) = clov[14]+i*clov[15] */
	  /* c(1,1, 1,0) = clov[24]+i*clov[25] */
	  /* c(2,1, 1,0) = clov[28]+i*clov[29] */
	  /* c(2,1, 2,0) = clov[34]+i*clov[35] */
	  int jk = 6 + 4*j + k*(18-4*k);
	  clov[72*i+(jk   )] = QLA_real(QLA_elem_M(B[0][i],j,k));
	  clov[72*i+(jk+1 )] = QLA_imag(QLA_elem_M(B[0][i],j,k));
	  clov[72*i+(jk+36)] = QLA_real(QLA_elem_M(B[1][i],j,k));
	  clov[72*i+(jk+37)] = QLA_imag(QLA_elem_M(B[1][i],j,k));
	}
      }
    }
#else
    for(int tr=0; tr<2; tr++) {
      clov[(72*i)+6+(36*tr)]  = +QLA_real(QLA_elem_M(B[tr][i],0,0));
      clov[(72*i)+7+(36*tr)]  = +QLA_imag(QLA_elem_M(B[tr][i],0,0));
      clov[(72*i)+10+(36*tr)] = +QLA_real(QLA_elem_M(B[tr][i],1,0));
      clov[(72*i)+11+(36*tr)] = +QLA_imag(QLA_elem_M(B[tr][i],1,0));
      clov[(72*i)+14+(36*tr)] = +QLA_real(QLA_elem_M(B[tr][i],2,0));
      clov[(72*i)+15+(36*tr)] = +QLA_imag(QLA_elem_M(B[tr][i],2,0));
      clov[(72*i)+16+(36*tr)] = +QLA_real(QLA_elem_M(B[tr][i],0,1));
      clov[(72*i)+17+(36*tr)] = -QLA_imag(QLA_elem_M(B[tr][i],0,1));
      clov[(72*i)+24+(36*tr)] = +QLA_real(QLA_elem_M(B[tr][i],1,1));
      clov[(72*i)+25+(36*tr)] = +QLA_imag(QLA_elem_M(B[tr][i],1,1));
      clov[(72*i)+28+(36*tr)] = +QLA_real(QLA_elem_M(B[tr][i],2,1));
      clov[(72*i)+29+(36*tr)] = +QLA_imag(QLA_elem_M(B[tr][i],2,1));
      clov[(72*i)+20+(36*tr)] = +QLA_real(QLA_elem_M(B[tr][i],0,2));
      clov[(72*i)+21+(36*tr)] = -QLA_imag(QLA_elem_M(B[tr][i],0,2));
      clov[(72*i)+30+(36*tr)] = +QLA_real(QLA_elem_M(B[tr][i],1,2));
      clov[(72*i)+31+(36*tr)] = -QLA_imag(QLA_elem_M(B[tr][i],1,2));
      clov[(72*i)+34+(36*tr)] = +QLA_real(QLA_elem_M(B[tr][i],2,2));
      clov[(72*i)+35+(36*tr)] = +QLA_imag(QLA_elem_M(B[tr][i],2,2));
    }
#endif
  }
  QDP_destroy_M(b[0]);
  QDP_destroy_M(b[1]);

  QDP_destroy_M(f_mn);
  QDP_destroy_M(xm);
  QDP_destroy_M(ym);

  /* 2*(3x3)=2*(18 real) = 36 real numbers are assiged to array clov  */
  /* ---------------------------------------------------------------- */
#undef NC
}

/* ---------------------------------------------------------------------- */
/* -------- End of the calculation of Clover Coefficients  -------------  */
/* ---------------------------------------------------------------------- */



// ************************************************************************ 

static void
clov_unpack(QLA_Complex u[6][6], REAL *p)
{
  int i, j, k;
  for(i=0; i<6; i++) {
    QLA_c_eq_r(u[i][i], p[i]);
  }
  k = 6;
  for(i=0; i<6; i++) {
    for(j=i+1; j<6; j++) {
      QLA_c_eq_ca(u[i][j], *((QLA_Complex *)(p+k)));
      QLA_c_eq_c(u[j][i], *((QLA_Complex *)(p+k)));
      k += 2;
    }
  }
}

static void
clov_pack(REAL *p, QLA_Complex u[6][6])
{
  int i, j, k;
  for(i=0; i<6; i++) {
    p[i] = QLA_real(u[i][i]);
  }
  k = 6;
  for(i=0; i<6; i++) {
    for(j=i+1; j<6; j++) {
      QLA_Complex z;
      QLA_c_eq_ca(z, u[i][j]);
      QLA_c_peq_c(z, u[j][i]);
      QLA_c_eq_r_times_c(*((QLA_Complex *)(p+k)), 0.5, z);
      k += 2;
    }
  }
}

static void
clov_invert(QLA_Complex ci[6][6], QLA_Complex c[6][6])
{
  int i, j, k;
  for(i=0; i<6; i++) {
    for(j=0; j<6; j++) {
      QLA_c_eq_r(ci[i][j], 0);
    }
    QLA_c_eq_r(ci[i][i], 1);
  }
  for(k=0; k<6; k++) {
    QLA_Complex s;
    //QLA_c_eq_r_div_c(s, 1, c[k][k]);
    {
      QLA_Real r;
      r = 1/QLA_norm2_c(c[k][k]);
      QLA_c_eq_r_times_c(s, r, c[k][k]);
      QLA_c_eq_ca(s, s);
    }
    for(j=0; j<6; j++) {
      QLA_Complex t;
      QLA_c_eq_c_times_c(t, s, c[k][j]);
      QLA_c_eq_c(c[k][j], t);
    }
    for(j=0; j<6; j++) {
      QLA_Complex t;
      QLA_c_eq_c_times_c(t, s, ci[k][j]);
      QLA_c_eq_c(ci[k][j], t);
    }
    for(i=0; i<6; i++) {
      if(i==k) continue;
      QLA_c_eq_c(s, c[i][k]);
      for(j=0; j<6; j++) {
	QLA_c_meq_c_times_c(c[i][j], s, c[k][j]);
      }
      for(j=0; j<6; j++) {
	QLA_c_meq_c_times_c(ci[i][j], s, ci[k][j]);
      }
    }
  }
}
/*
static void
printcm(QLA_Complex *m, int nr, int nc)
{
  int i, j;
  for(i=0; i<nr; i++) {
    for(j=0; j<nc; j++) {
      printf0("%f %f ", QLA_real(m[i*nc+j]), QLA_imag(m[i*nc+j]));
    }
    printf0("\n");
  }
}
*/

static void
get_clovinv(QOP_FermionLinksWilson *flw, REAL kappa)
{
  QLA_Real m4 = 0.5/kappa;
  int i, j;
  if(flw->clovinv==NULL)
    QOP_malloc(flw->clovinv, REAL, QDP_sites_on_node*CLOV_REALS);
  for(i=0; i<2*QDP_sites_on_node; i++) {
    QLA_Complex cu[6][6], ciu[6][6];
    clov_unpack(cu, flw->clov+(CLOV_REALS/2)*i);
    for(j=0; j<6; j++) QLA_c_peq_r(cu[j][j], m4);
    //if(i==0) printcm(flw->clov, 3, 6);
    //if(i==0) printcm(cu, 6, 6);
    clov_invert(ciu, cu);
    clov_pack(flw->clovinv+(CLOV_REALS/2)*i, ciu);
    //if(i==0) printcm(cu, 6, 6);
    //if(i==0) printcm(ciu, 6, 6);
    //if(i==0) printcm(flw->clovinv, 3, 6);
  }
  flw->clovinvkappa = kappa;
}

#define NC int nc
QOP_FermionLinksWilson *
QOP_wilson_create_L_from_raw(REAL *links[], REAL *clov, QOP_evenodd_t evenodd)
#undef NC
{
#define NC nc
  QOP_FermionLinksWilson *flw;
  QOP_GaugeField *gf;

  WILSON_INVERT_BEGIN;

  gf = QOP_create_G_from_raw(links, evenodd);
  flw = QOP_wilson_convert_L_from_qdp(gf->links, NULL);

  if(clov!=NULL) {
    QOP_malloc(flw->clov, REAL, QDP_sites_on_node*CLOV_REALS);
    memcpy(flw->clov, clov, QDP_sites_on_node*CLOV_SIZE);
  } else {
    flw->clov = NULL;
  }

  /* Keep pointer to allocated space so it can be freed */
  flw->qopgf = gf;

  WILSON_INVERT_END;
  return flw;
#undef NC
}
/* --------------------------------------------------- */
/* This part is added by Bugra ----------------------- */

QOP_FermionLinksWilson *
QOP_wilson_initialize_gauge_L()
{
  QOP_FermionLinksWilson *flw;

  WILSON_INVERT_BEGIN;

  QOP_malloc(flw          ,QOP_FermionLinksWilson,1);
  QOP_malloc(flw->links   ,QDP_ColorMatrix *     ,4);
  QOP_malloc(flw->bcklinks,QDP_ColorMatrix *     ,4);
  QOP_malloc(flw->dbllinks,QDP_ColorMatrix *,     8);

  flw->dblstored = 0;
  flw->clov      = NULL;
  flw->rawclov   = NULL;
  flw->clovinv   = NULL;
  flw->rawlinks  = NULL;
  flw->qopgf     = NULL;
  flw->gauge     = NULL;
  flw->qdpclov   = NULL;
  flw->eigcg.u   = NULL;

  WILSON_INVERT_END;

  return flw;
}


/* ----------------This part is added by Bugra ------------- */
/* --------------------------------------------------------- */
QOP_FermionLinksWilson *
QOP_wilson_create_L_from_G(QOP_info_t *info, 
			   QOP_wilson_coeffs_t *coeffs,
			   QOP_GaugeField *gauge)
{ 
#define NC QDP_get_nc(gauge->links[0])
  QOP_FermionLinksWilson *flw;
  QDP_ColorMatrix        *newlinks[4];

  WILSON_INVERT_BEGIN;

  /* initialize FermionLinksWilson and allocate memory---- */
  flw = QOP_wilson_initialize_gauge_L();

  /* First create QDP Color Matrices */
  for(int i=0; i<4; i++) {
    newlinks[i] = QDP_create_M();
    QDP_M_eq_M(newlinks[i], gauge->links[i], QDP_all);
  }

  /* get the clover coefficients and put them in flw->clow */
  /* Usage : get_clov(QLA_Real *clov, QDP_ColorMatrix *link[], QLA_Real csw) */
  if(coeffs->clov_s != 0 || coeffs->clov_t != 0) {
    int nreals = QDP_sites_on_node*CLOV_REALS;
    QOP_malloc(flw->clov, REAL, nreals);
    get_clov(flw->clov, newlinks, 0.5*coeffs->clov_s, 0.5*coeffs->clov_t);
  }

  /* Check the anisotropy -------------------------------  */
  if(coeffs->aniso != 0. && coeffs->aniso != 1.) {
    for(int i=0; i<3; i++) {
      QLA_Real f = coeffs->aniso;
      QDP_M_eq_r_times_M(newlinks[i], &f, newlinks[i], QDP_all);
    }
  }

  /* Scale the links ------------------------------------- */
  for(int i=0; i<4; i++) {
    QLA_Real f    = -0.5;
    QDP_M_eq_r_times_M(newlinks[i], &f, newlinks[i], QDP_all);
  }

  /* newlinks go to flw->links --------------------------- */
  for(int i=0; i<4; i++) {
    flw->links[i] = newlinks[i];
  }

  flw->gauge = gauge;

  WILSON_INVERT_END;
  return flw;
#undef NC
}

/* --------------------------------------------------------- */
/* --------------------------------------------------------- */

void
QOP_wilson_extract_L_to_raw(REAL *links[], REAL *clov,
			    QOP_FermionLinksWilson *src, QOP_evenodd_t evenodd)
{
  WILSON_INVERT_BEGIN;
  QOP_error("QOP_wilson_extract_L_to_raw unimplemented.");
  WILSON_INVERT_END;
}

void
QOP_wilson_destroy_L(QOP_FermionLinksWilson *flw)
{
  WILSON_INVERT_BEGIN;

  if(flw->qopgf) {
    QOP_destroy_G(flw->qopgf);
  } else {
    for(int i=0; i<4; i++) QDP_destroy_M(flw->links[i]);
  }
  free(flw->links);
  if(flw->dblstored) {
    for(int i=0; i<4; i++) QDP_destroy_M(flw->bcklinks[i]);
  }
  if(flw->qdpclov) {
    QDP_destroy_P(flw->qdpclov);
  }
  if(flw->clov) free(flw->clov);
  if(flw->clovinv) free(flw->clovinv);
  free(flw->bcklinks);
  free(flw->dbllinks);
  if(flw->eigcg.u) {
    for(int i=0; i<flw->eigcg.numax; i++) {
      QDP_destroy_D(flw->eigcg.u[i]);
    }
    free(flw->eigcg.u);
    free(flw->eigcg.l);
  }
  free(flw);
  WILSON_INVERT_END;
}

#define NC int nc
QOP_FermionLinksWilson *
QOP_wilson_convert_L_from_raw(REAL *links[], REAL *clov, QOP_evenodd_t evenodd)
#undef NC
{
#define NC nc
  WILSON_INVERT_BEGIN;
  QOP_error("QOP_wilson_convert_L_from_raw unimplemented");
  WILSON_INVERT_END;
  return NULL;
#undef NC
}

void
QOP_wilson_convert_L_to_raw(REAL ***links, REAL **clov,
			    QOP_FermionLinksWilson *src, QOP_evenodd_t evenodd)
{
  WILSON_INVERT_BEGIN;
  QOP_error("QOP_wilson_convert_L_to_raw unimplemented");
  WILSON_INVERT_END;
}

QOP_FermionLinksWilson *
QOP_wilson_convert_L_from_G(QOP_info_t *info, QOP_wilson_coeffs_t *coeffs,
			    QOP_GaugeField *gauge)
{
  WILSON_INVERT_BEGIN;
  QOP_error("QOP_wilson_convert_L_from_G unimplemented");
  WILSON_INVERT_END;
  return NULL;
}

QOP_GaugeField *
QOP_wilson_convert_L_to_G(QOP_FermionLinksWilson *links)
{
  WILSON_INVERT_BEGIN;
  QOP_error("QOP_wilson_convert_L_to_G unimplemented");
  WILSON_INVERT_END;
  return NULL;
}

QOP_FermionLinksWilson *
QOP_wilson_create_L_from_qdp(QDP_ColorMatrix *links[],
			     QDP_DiracPropagator *clov)
{
#define NC QDP_get_nc(links[0])
  QOP_FermionLinksWilson *flw;
  QDP_ColorMatrix *newlinks[4];

  WILSON_INVERT_BEGIN;

  for(int i=0; i<4; i++) {
    newlinks[i] = QDP_create_M();
    QDP_M_eq_M(newlinks[i], links[i], QDP_all);
  }

  flw = QOP_wilson_convert_L_from_qdp(newlinks, clov);

  WILSON_INVERT_END;
  return flw;
#undef NC
}

void
QOP_wilson_extract_L_to_qdp(QDP_ColorMatrix *links[],
			    QDP_DiracPropagator *clov,
			    QOP_FermionLinksWilson *src)
{
  WILSON_INVERT_BEGIN;
  QOP_error("QOP_wilson_extract_L_to_qdp unimplemented");
  WILSON_INVERT_END;
}

QOP_FermionLinksWilson *
QOP_wilson_convert_L_from_qdp(QDP_ColorMatrix *links[],
			      QDP_DiracPropagator *clov)
{
  WILSON_INVERT_BEGIN;
  QOP_FermionLinksWilson *flw = QOP_wilson_initialize_gauge_L();

  if(clov!=NULL) {
    int size = QDP_sites_on_node*CLOV_REALS;
    QOP_malloc(flw->clov, REAL, size);
    {
      QLA_DiracPropagator(*dp);
      int x, b, i, ic, j, jc, is, js, k=0;
      dp = QDP_expose_P(clov);
      for(x=0; x<QDP_sites_on_node; x++) {
	for(b=0; b<2; b++) { // two chiral blocks
	  // first the diagonal
	  for(i=0; i<6; i++) {
	    ic = i/2;
	    is = 2*b + i%2;
	    flw->clov[k++] = QLA_real(QLA_elem_P(dp[x], ic, is, ic, is));
	  }
	  // now the offdiagonal
	  for(i=0; i<6; i++) {
	    ic = i/2;
	    is = 2*b + i%2;
	    for(j=i+1; j<6; j++) {
	      QLA_Complex z1, z2;
	      jc = j/2;
	      js = 2*b + j%2;
	      //QLA_c_eq_c_plus_ca(z1, QLA_elem_P(dp[x], ic, is, jc, js),
	      //                   QLA_elem_P(dp[x], jc, js, ic, is));
	      QLA_c_eq_c(z1, QLA_elem_P(dp[x], ic, is, jc, js));
	      QLA_c_peq_ca(z1, QLA_elem_P(dp[x], jc, js, ic, is));
	      QLA_c_eq_r_times_c(z2, 0.5, z1);
	      flw->clov[k++] = QLA_real(z2);
	      flw->clov[k++] = -QLA_imag(z2); // - since we now store lower tri
	    }
	  }
	}
      }
      QDP_reset_P(clov);
    }
  } else {
    flw->clov = NULL;
  }
  flw->clovinv = NULL;

  flw->dblstored = 0;
  for(int i=0; i<4; i++) {
    flw->links[i] = links[i];
  }
  // scale links
  for(int i=0; i<4; i++) {
    QLA_Real f = -0.5;
    QDP_M_eq_r_times_M(flw->links[i], &f, flw->links[i], QDP_all);
  }

  //check_setup(flw);

  WILSON_INVERT_END;
  return flw;
}

void
QOP_wilson_convert_L_to_qdp(QDP_ColorMatrix ***links,
			    QDP_DiracPropagator **clov,
			    QOP_FermionLinksWilson *src)
{
  WILSON_INVERT_BEGIN;
  QOP_error("QOP_wilson_convert_L_to_qdp unimplemented");
  WILSON_INVERT_END;
}


/********************/
/* Dslash functions */
/********************/

static void
clov(QOP_FermionLinksWilson *flw, REAL kappa, QDP_DiracFermion *out,
     QDP_DiracFermion *in, QDP_Subset subset, int peq);

static void
apply_clov(REAL *clov, QLA_Real m4, QDP_DiracFermion *out,
	   QDP_DiracFermion *in, QDP_Subset subset);

static void
wilson_dslash0(QOP_FermionLinksWilson *flw,
	       QDP_DiracFermion *dest, QDP_DiracFermion *src,
	       int sign, QOP_evenodd_t eo, int n);

static void
wilson_dslash1(QOP_FermionLinksWilson *flw,
	       QDP_DiracFermion *dest, QDP_DiracFermion *src,
	       int sign, QOP_evenodd_t eo, int n);

#define wilson_hop(flw, dest, src, sign, eo) \
{ \
  QDP_DiracFermion *tsrc = src; \
  int _n = 1; \
  while(1) { \
    if(src==tmpsub(eo,_n)) break; \
    if(_n==NTMPSUB) { \
      _n = 1; \
      tsrc = tmpsub(eo,_n); \
      QDP_D_eq_D(tsrc, src, qdpsub(oppsub(eo))); \
      break; \
    } \
    _n++; \
  } \
  TRACE; \
  printf0("%i %i\n", eo, _n); \
  if(dblstore_style(QOP_wilson_style)) { \
    wilson_dslash1(flw, dest, tsrc, sign, eo, _n); \
  } else { \
    wilson_dslash0(flw, dest, tsrc, sign, eo, _n); \
  } \
  TRACE; \
}

void
QOP_wilson_dslash(QOP_info_t *info,
		  QOP_FermionLinksWilson *flw,
		  REAL kappa,
		  int sign,
		  QOP_DiracFermion *out,
		  QOP_DiracFermion *in,
		  QOP_evenodd_t eo_out,
		  QOP_evenodd_t eo_in)
{
  QOP_wilson_dslash_qdp(info,flw,kappa,sign,out->df,in->df,eo_out,eo_in);
}

void
QOP_wilson_dslash_qdp(QOP_info_t *info,
		      QOP_FermionLinksWilson *flw,
                      REAL kappa,
		      int sign,
		      QDP_DiracFermion *out,
		      QDP_DiracFermion *in,
		      QOP_evenodd_t eo_out,
		      QOP_evenodd_t eo_in)
{
#define NC QDP_get_nc(flw->links[0])
  check_setup(flw);

  if(eo_in==eo_out) {
    if(eo_out==QOP_EVENODD) {
      wilson_hop(flw, out, in, sign, QOP_EVENODD);
      clov(flw, kappa, out, in, QDP_all, 1);
    } else if(eo_out==QOP_EVEN) {
      clov(flw, kappa, out, in, QDP_even, 0);
    } else {
      clov(flw, kappa, out, in, QDP_odd, 0);
    }
  } else {
    if(eo_out==QOP_EVEN || eo_out==QOP_EVENODD) {
      if(eo_in==QOP_ODD) {
	wilson_hop(flw, out, in, sign, QOP_EVEN);
      } else if(eo_in==QOP_EVEN) {
	clov(flw, kappa, out, in, QDP_even, 0);
      } else {
	wilson_hop(flw, out, in, sign, QOP_EVEN);
	clov(flw, kappa, out, in, QDP_even, 1);
      }
    }
    if(eo_out==QOP_ODD || eo_out==QOP_EVENODD) {
      if(eo_in==QOP_EVEN) {
	wilson_hop(flw, out, in, sign, QOP_ODD);
      } else if(eo_in==QOP_ODD) {
	clov(flw, kappa, out, in, QDP_odd, 0);
      } else {
	wilson_hop(flw, out, in, sign, QOP_ODD);
	clov(flw, kappa, out, in, QDP_odd, 1);
      }
    }
  }
#undef NC
}

void
QOP_wilson_diaginv(QOP_info_t *info,
		   QOP_FermionLinksWilson *flw,
		   REAL kappa,
		   QOP_DiracFermion *out,
		   QOP_DiracFermion *in,
		   QOP_evenodd_t eo)
{
  QOP_wilson_diaginv_qdp(info, flw, kappa, out->df, in->df, eo);
}

void
QOP_wilson_diaginv_qdp(QOP_info_t *info,
		       QOP_FermionLinksWilson *flw,
		       REAL kappa,
		       QDP_DiracFermion *out,
		       QDP_DiracFermion *in,
		       QOP_evenodd_t eo)
{
#define NC QDP_get_nc(flw->links[0])
  if(flw->clov==NULL) {
    QLA_Real f = 2*kappa;
    QDP_D_eq_r_times_D(out, &f, in, qdpsub(eo));
  } else {
    if( flw->clovinv==NULL || flw->clovinvkappa!=kappa ) {
      get_clovinv(flw, kappa);
    }
    QDP_D_eq_zero(out, qdpsub(eo));
    apply_clov(flw->clovinv, 0, out, in, qdpsub(eo));
  }
#undef NC
}

#define cmplx(x) (*((QLA_Complex *)(&(x))))

static void
apply_clov_qla(REAL *clov, QLA_Real m4, QLA_DiracFermion *restrict clov_out,
	       QLA_DiracFermion *restrict clov_in, QDP_Subset subset)
{
  //QLA_DiracFermion *clov_out, *clov_in;
  //clov_out = QDP_expose_D(out);
  //clov_in = QDP_expose_D(in);
  //{
    int x, start, end;
    if(subset==QDP_odd) start = QDP_subset_len(QDP_even);
    else start = 0;
    end = start + QDP_subset_len(subset);
    for(x=start; x<end; x++) {
      int b;
      for(b=0; b<2; b++) {
	int xb;
	xb = 36*(2*x+b);  // chiral block offset (in REALs)

#define clov_diag(i) clov[xb+i]
#define clov_offd(i) cmplx(clov[xb+6+2*i])
#define src(i) QLA_elem_D(clov_in[x],i/2,2*b+(i&1))
#define dest(i) QLA_elem_D(clov_out[x],i/2,2*b+(i&1))
	//flops = 6*44 = 264; bytes = 4*(36+12+12) = 240
#if 0
	{
	  QLA_Complex z0, z1, z2, z3, z4, z5;

	  QLA_c_eq_r_times_c(z0, m4+clov_diag(0), src(0));
	  QLA_c_eq_c_times_c(z1, clov_offd(0), src(0));
	  QLA_c_eq_c_times_c(z2, clov_offd(1), src(0));
	  QLA_c_eq_c_times_c(z3, clov_offd(2), src(0));
	  QLA_c_eq_c_times_c(z4, clov_offd(3), src(0));
	  QLA_c_eq_c_times_c(z5, clov_offd(4), src(0));

	  QLA_c_peq_ca_times_c(z0, clov_offd(0), src(1));
	  QLA_c_peq_r_times_c(z1, m4+clov_diag(1), src(1));
	  QLA_c_peq_c_times_c(z2, clov_offd(5), src(1));
	  QLA_c_peq_c_times_c(z3, clov_offd(6), src(1));
	  QLA_c_peq_c_times_c(z4, clov_offd(7), src(1));
	  QLA_c_peq_c_times_c(z5, clov_offd(8), src(1));

	  QLA_c_peq_ca_times_c(z0, clov_offd(1), src(2));
 	  QLA_c_peq_ca_times_c(z1, clov_offd(5), src(2));
	  QLA_c_peq_r_times_c(z2, m4+clov_diag(2), src(2));
	  QLA_c_peq_c_times_c(z3, clov_offd(9), src(2));
	  QLA_c_peq_c_times_c(z4, clov_offd(10), src(2));
	  QLA_c_peq_c_times_c(z5, clov_offd(11), src(2));

	  QLA_c_peq_ca_times_c(z0, clov_offd(2), src(3));
	  QLA_c_peq_ca_times_c(z1, clov_offd(6), src(3));
	  QLA_c_peq_ca_times_c(z2, clov_offd(9), src(3));
	  QLA_c_peq_r_times_c(z3, m4+clov_diag(3), src(3));
	  QLA_c_peq_c_times_c(z4, clov_offd(12), src(3));
	  QLA_c_peq_c_times_c(z5, clov_offd(13), src(3));

	  QLA_c_peq_ca_times_c(z0, clov_offd(3), src(4));
	  QLA_c_peq_ca_times_c(z1, clov_offd(7), src(4));
	  QLA_c_peq_ca_times_c(z2, clov_offd(10), src(4));
	  QLA_c_peq_ca_times_c(z3, clov_offd(12), src(4));
	  QLA_c_peq_r_times_c(z4, m4+clov_diag(4), src(4));
	  QLA_c_peq_c_times_c(z5, clov_offd(14), src(4));

	  QLA_c_peq_ca_times_c(z0, clov_offd(4), src(5));
	  QLA_c_peq_ca_times_c(z1, clov_offd(8), src(5));
	  QLA_c_peq_ca_times_c(z2, clov_offd(11), src(5));
	  QLA_c_peq_ca_times_c(z3, clov_offd(13), src(5));
	  QLA_c_peq_ca_times_c(z4, clov_offd(14), src(5));
	  QLA_c_peq_r_times_c(z5, m4+clov_diag(5), src(5));

	  QLA_c_peq_c(dest(0), z0);
	  QLA_c_peq_c(dest(1), z1);
	  QLA_c_peq_c(dest(2), z2);
	  QLA_c_peq_c(dest(3), z3);
	  QLA_c_peq_c(dest(4), z4);
	  QLA_c_peq_c(dest(5), z5);
	}
#else
	{
	  QLA_Complex z0, z1, z2, z3, z4, z5;

	  QLA_c_eq_r_times_c(z0, m4+clov_diag(0), src(0));
	  QLA_c_eq_r_times_c(z1, m4+clov_diag(1), src(1));
	  QLA_c_eq_r_times_c(z2, m4+clov_diag(2), src(2));
	  QLA_c_eq_r_times_c(z3, m4+clov_diag(3), src(3));
	  QLA_c_eq_r_times_c(z4, m4+clov_diag(4), src(4));
	  QLA_c_eq_r_times_c(z5, m4+clov_diag(5), src(5));

	  QLA_c_peq_c_times_c(z1, clov_offd(0), src(0));
	  QLA_c_peq_ca_times_c(z0, clov_offd(0), src(1));

	  QLA_c_peq_c_times_c(z3, clov_offd(9), src(2));
	  QLA_c_peq_ca_times_c(z2, clov_offd(9), src(3));

	  QLA_c_peq_c_times_c(z5, clov_offd(14), src(4));
	  QLA_c_peq_ca_times_c(z4, clov_offd(14), src(5));

	  QLA_c_peq_c_times_c(z2, clov_offd(1), src(0));
	  QLA_c_peq_ca_times_c(z0, clov_offd(1), src(2));

	  QLA_c_peq_c_times_c(z4, clov_offd(12), src(3));
	  QLA_c_peq_ca_times_c(z3, clov_offd(12), src(4));

	  QLA_c_peq_c_times_c(z5, clov_offd(8), src(1));
	  QLA_c_peq_ca_times_c(z1, clov_offd(8), src(5));

	  QLA_c_peq_c_times_c(z3, clov_offd(2), src(0));
	  QLA_c_peq_ca_times_c(z0, clov_offd(2), src(3));

	  QLA_c_peq_c_times_c(z4, clov_offd(7), src(1));
	  QLA_c_peq_ca_times_c(z1, clov_offd(7), src(4));

	  QLA_c_peq_c_times_c(z5, clov_offd(11), src(2));
	  QLA_c_peq_ca_times_c(z2, clov_offd(11), src(5));

	  QLA_c_peq_c_times_c(z4, clov_offd(3), src(0));
	  QLA_c_peq_ca_times_c(z0, clov_offd(3), src(4));

	  QLA_c_peq_c_times_c(z5, clov_offd(13), src(3));
	  QLA_c_peq_ca_times_c(z3, clov_offd(13), src(5));

	  QLA_c_peq_c_times_c(z2, clov_offd(5), src(1));
 	  QLA_c_peq_ca_times_c(z1, clov_offd(5), src(2));

	  QLA_c_peq_c_times_c(z5, clov_offd(4), src(0));
	  QLA_c_peq_ca_times_c(z0, clov_offd(4), src(5));

	  QLA_c_peq_c_times_c(z4, clov_offd(10), src(2));
	  QLA_c_peq_ca_times_c(z2, clov_offd(10), src(4));

	  QLA_c_peq_c_times_c(z3, clov_offd(6), src(1));
	  QLA_c_peq_ca_times_c(z1, clov_offd(6), src(3));

	  QLA_c_peq_c(dest(0), z0);
	  QLA_c_peq_c(dest(1), z1);
	  QLA_c_peq_c(dest(2), z2);
	  QLA_c_peq_c(dest(3), z3);
	  QLA_c_peq_c(dest(4), z4);
	  QLA_c_peq_c(dest(5), z5);
	}
#endif
      }
    }
    //}
    //QDP_reset_D(in);
    //QDP_reset_D(out);
}

static void
apply_clov(REAL *clov, QLA_Real m4, QDP_DiracFermion *out,
	   QDP_DiracFermion *in, QDP_Subset subset)
{
  QLA_DiracFermion *clov_out, *clov_in;
  clov_out = QDP_expose_D(out);
  clov_in = QDP_expose_D(in);
  apply_clov_qla(clov, m4, clov_out, clov_in, subset);
  QDP_reset_D(in);
  QDP_reset_D(out);
}

static void
clov(QOP_FermionLinksWilson *flw, REAL kappa, QDP_DiracFermion *out,
     QDP_DiracFermion *in, QDP_Subset subset, int peq)
{
  QLA_Real m4 = 0.5/kappa;
  if(flw->clov==NULL) {
    if(peq) {
      QDP_D_peq_r_times_D(out, &m4, in, subset);
    } else {
      QDP_D_eq_r_times_D(out, &m4, in, subset);
    }
  } else {
    if(!peq) {
      QDP_D_eq_zero(out, subset);
    }
    apply_clov(flw->clov, m4, out, in, subset);
  }
}


/************ dslash *************/

/* Special dslash for use by congrad.  Uses restart_gather() when
   possible. Last argument is an integer, which will tell if
   gathers have been started.  If is_started=0,use
   start_gather, otherwise use restart_gather.
   Argument "tag" is a vector of a msg_tag *'s to use for
   the gathers.
   The calling program must clean up the gathers! */
static void
wilson_dslash0(QOP_FermionLinksWilson *flw,
	       QDP_DiracFermion *dest, QDP_DiracFermion *src,
	       int sign, QOP_evenodd_t eo, int n)
{
  int mu, ntmp;
  QDP_DiracFermion *vsrc[4];
  QDP_DiracFermion *vdest[4];
  QDP_ShiftDir fwd[4], bck[4];
  int dir[4], sgn[4], msgn[4];
  QDP_Subset subset, othersubset;
  subset = qdpsub(eo);
  othersubset = qdpsub(oppsub(eo));
  ntmp = tmpnum(eo,n);

  sign = -sign;

  for(mu=0; mu<4; mu++) {
    vsrc[mu] = src;
    vdest[mu] = dest;
    fwd[mu] = QDP_forward;
    bck[mu] = QDP_backward;
    dir[mu] = mu;
    sgn[mu] = sign;
    msgn[mu] = -sign;
  }

  if(subset==QDP_even) othersubset = QDP_odd;
  else if(subset==QDP_odd) othersubset = QDP_even;
  else othersubset = QDP_all;

  /* Take Wilson projection for src displaced in up direction, gather
     it to "our site" */

  printf0("dslash0\n");
  if(shiftd_style(QOP_wilson_style)) {
    for(mu=0; mu<4; mu+=QOP_wilson_nsvec) {
      printf0("QDP_D_veq_sD\n");
      QDP_D_veq_sD(dtemp[ntmp]+mu, vsrc+mu, QDP_neighbor+mu, fwd+mu, subset,
		   QOP_wilson_nsvec);
      printf0("end QDP_D_veq_sD\n");
    }
  } else {
    for(mu=0; mu<4; mu+=QOP_wilson_nsvec) {
      printf0("QDP_H_veq_spproj_D\n");
      QDP_H_veq_spproj_D(htemp[ntmp]+8+mu, vsrc+mu, dir+mu, sgn+mu,
			 othersubset, QOP_wilson_nsvec);
      printf0("QDP_H_veq_sH\n");
      QDP_H_veq_sH(htemp[ntmp]+mu, htemp[ntmp]+8+mu, QDP_neighbor+mu, fwd+mu,
		   subset, QOP_wilson_nsvec);
      printf0("end QDP_H_veq_sH\n");
    }
  }

  /* Take Wilson projection for src displaced in down direction,
     multiply it by adjoint link matrix, gather it "up" */

  printf0("dslash0 - back\n");
  if(shiftd_style(QOP_wilson_style)) {
    for(mu=0; mu<4; mu+=QOP_wilson_nsvec) {
      QDP_D_veq_spproj_Ma_times_D(dtemp[ntmp]+8+mu, flw->links+mu, vsrc+mu,
				  dir+mu, msgn+mu, othersubset,
				  QOP_wilson_nsvec);
      QDP_D_veq_sD(dtemp[ntmp]+4+mu, dtemp[ntmp]+8+mu, QDP_neighbor+mu,
		   bck+mu, subset, QOP_wilson_nsvec);
    }
  } else {
    for(mu=0; mu<4; mu+=QOP_wilson_nsvec) {
      printf0("QDP_H_veq_spproj_Ma_times_D\n");
      QDP_H_veq_spproj_Ma_times_D(htemp[ntmp]+12+mu, flw->links+mu, vsrc+mu,
				  dir+mu, msgn+mu, othersubset,
				  QOP_wilson_nsvec);
      printf0("QDP_H_veq_sH\n");
      QDP_H_veq_sH(htemp[ntmp]+4+mu, htemp[ntmp]+12+mu, QDP_neighbor+mu,
		   bck+mu, subset, QOP_wilson_nsvec);
      printf0("end QDP_H_veq_sH\n");
    }
  }

  /* Set dest to zero */
  /* Take Wilson projection for src displaced in up direction, gathered,
     multiply it by link matrix, expand it, and add.
     to dest */

  printf0("dslash0 - fwd\n");
  QDP_D_eq_zero(dest, subset);

  if(shiftd_style(QOP_wilson_style)) {
    for(mu=0; mu<4; mu+=QOP_wilson_nvec) {
      QDP_D_vpeq_spproj_M_times_D(vdest+mu, flw->links+mu, dtemp[ntmp]+mu,
				  dir+mu, sgn+mu, subset, QOP_wilson_nvec);
    }
  } else {
    for(mu=0; mu<4; mu+=QOP_wilson_nvec) {
      QDP_D_vpeq_sprecon_M_times_H(vdest+mu, flw->links+mu, htemp[ntmp]+mu,
				   dir+mu, sgn+mu, subset, QOP_wilson_nvec);
    }
  }

  /* Take Wilson projection for src displaced in down direction,
     expand it, and add to dest */

  printf0("dslash0 - back\n");
  if(shiftd_style(QOP_wilson_style)) {
    for(mu=0; mu<4; mu+=QOP_wilson_nvec) {
      QDP_D_vpeq_D(vdest+mu, dtemp[ntmp]+4+mu, subset, QOP_wilson_nvec);
    }
  } else {
    for(mu=0; mu<4; mu+=QOP_wilson_nvec) {
      QDP_D_vpeq_sprecon_H(vdest+mu, htemp[ntmp]+4+mu, dir+mu, msgn+mu, subset,
			   QOP_wilson_nvec);
    }
  }

  if(shiftd_style(QOP_wilson_style)) {
    for(mu=0; mu<8; mu++) {
      QDP_discard_D(dtemp[ntmp][mu]);
    }
  } else {
    for(mu=0; mu<8; mu++) {
      QDP_discard_H(htemp[ntmp][mu]);
    }
  }
} /* end of dslash_special_qdp() */

/* Special dslash for use by congrad.  Uses restart_gather() when
   possible. Last argument is an integer, which will tell if
   gathers have been started.  If is_started=0,use
   start_gather, otherwise use restart_gather.
   Argument "tag" is a vector of a msg_tag *'s to use for
   the gathers.
   The calling program must clean up the gathers! */
static void
wilson_dslash1(QOP_FermionLinksWilson *flw,
	       QDP_DiracFermion *dest, QDP_DiracFermion *src,
	       int sign, QOP_evenodd_t eo, int n)
{
  TRACE;
  int mu, ntmp;
  QDP_DiracFermion *vsrc[8];
  QDP_DiracFermion *vdest[8];
  QDP_Shift sh[8];
  QDP_ShiftDir sd[8];
  int dir[8], sgn[8];
  QDP_Subset subset, othersubset;
  subset = qdpsub(eo);
  othersubset = qdpsub(oppsub(eo));
  ntmp = tmpnum(eo,n);

  sign = -sign;

  for(mu=0; mu<4; mu++) {
    vsrc[mu] = src;
    vsrc[mu+4] = src;
    vdest[mu] = dest;
    vdest[mu+4] = dest;
    dir[2*mu] = mu;
    dir[2*mu+1] = mu;
    sgn[2*mu] = sign;
    sgn[2*mu+1] = -sign;
    sh[2*mu] = QDP_neighbor[mu];
    sh[2*mu+1] = QDP_neighbor[mu];
    sd[2*mu] = QDP_forward;
    sd[2*mu+1] = QDP_backward;
  }

  /* Take Wilson projection for src displaced in up direction, gather
     it to "our site" */

  TRACE;
  if(shiftd_style(QOP_wilson_style)) {
    for(mu=0; mu<8; mu+=QOP_wilson_nsvec) {
      QDP_D_veq_sD(dtemp[ntmp]+mu, vsrc+mu, sh+mu, sd+mu, subset,
		   QOP_wilson_nsvec);
    }
  } else {
    for(mu=0; mu<8; mu+=QOP_wilson_nsvec) {
      QDP_H_veq_spproj_D(htemp[ntmp]+8+mu, vsrc+mu, dir+mu, sgn+mu,
			 othersubset, QOP_wilson_nsvec);
      QDP_H_veq_sH(htemp[ntmp]+mu, htemp[ntmp]+8+mu, sh+mu, sd+mu, subset,
		   QOP_wilson_nsvec);
    }
  }

  /* Set dest to zero */
  /* Take Wilson projection for src displaced in up direction, gathered,
     multiply it by link matrix, expand it, and add to dest */

  TRACE;
  QDP_D_eq_zero(dest, subset);
  TRACE;
  if(shiftd_style(QOP_wilson_style)) {
    //QOP_wilson_nvec = 1;
    //printf0("%p %p %p %p %p %p %i\n", vdest, flw->dbllinks, dtemp[ntmp], dir, sgn, subset, QOP_wilson_nvec);
    for(mu=0; mu<8; mu+=QOP_wilson_nvec) {
      //TRACE;
      //printf0("%i %p %p %p %i %i\n", mu, vdest[mu], flw->dbllinks[mu], dtemp[ntmp][mu], dir[mu], sgn[mu]);
      //QDP_D_eq_zero(vdest[mu], subset);
      //TRACE;
      //QDP_M_eq_zero(flw->dbllinks[mu], subset);
      //QDP_ColorMatrix *m = QDP_create_M();
      //QDP_M_eq_M(m, flw->dbllinks[mu], subset);
      //QDP_destroy_M(m);
      //TRACE;
      //QDP_D_eq_D(vdest[mu], dtemp[ntmp][mu], subset);
      //TRACE;
      QDP_D_vpeq_spproj_M_times_D(vdest+mu, flw->dbllinks+mu, dtemp[ntmp]+mu,
				  dir+mu, sgn+mu, subset, QOP_wilson_nvec);
      //QDP_D_vpeq_spproj_M_times_D(vdest+mu, flw->dbllinks+mu, dtemp[ntmp],
      //		          dir+mu, sgn+mu, subset, QOP_wilson_nvec);
      //TRACE;
    }
  } else {
    for(mu=0; mu<8; mu+=QOP_wilson_nvec) {
      TRACE;
      QDP_D_vpeq_sprecon_M_times_H(vdest+mu, flw->dbllinks+mu, htemp[ntmp]+mu,
				   dir+mu, sgn+mu, subset, QOP_wilson_nvec);
      TRACE;
    }
  }

  TRACE;
  if(shiftd_style(QOP_wilson_style)) {
    for(mu=0; mu<8; mu++) {
      QDP_discard_D(dtemp[ntmp][mu]);
    }
  } else {
    for(mu=0; mu<8; mu++) {
      QDP_discard_H(htemp[ntmp][mu]);
    }
  }
  TRACE;
}

#if QOP_Colors == 3

// ---------------------------------------------------------- :
// new fermilab action IFLA -- added by bugra --------------- :
// New name is Ok-action ------------------------------------ :
// ---------------------------------------------------------- :
// ---------------------------------------------------------- :
static void
wilson_okaction_full_testtadpole(QOP_FermionLinksWilson *flw,
                                 QDP_DiracFermion *dest,
                                 QDP_DiracFermion *src,
                                 int sign,
                                 QOP_wilson_ifla_coeffs_t *coeffs,
                                 QOP_evenodd_t eo, int n);
void
QOP_wilson_ifla_dslash(QOP_info_t *info,
                       QOP_FermionLinksWilson *flw,
                       REAL kappa,
                       int sign,
                       QOP_wilson_ifla_coeffs_t *coeffs,
                       QOP_DiracFermion *out,
                       QOP_DiracFermion *in,
                       QOP_evenodd_t eo_out,
                       QOP_evenodd_t eo_in)
{
  //printf("This is the function : (QOP_wilson_ifla_dslash)\n");
  //printf("Currently being tested **\n");
  QOP_wilson_ifla_dslash_qdp(info,flw,kappa,sign,coeffs,out->df,in->df,eo_out,eo_in);
};

void QOP_wilson_ifla_dslash_qdp(QOP_info_t *info,
                                QOP_FermionLinksWilson *flw,
                                REAL kappa,
                                int  sign,
                                QOP_wilson_ifla_coeffs_t *coeffs,
                                QDP_DiracFermion *out,
                                QDP_DiracFermion *in,
                                QOP_evenodd_t eo_out,
                                QOP_evenodd_t eo_in)
{
#if 0
  // test version
  wilson_ifla_full_tad(flw,out,in,sign,coeffs,2,2);
  //wilson_okaction_full_4(flw,out,in,sign,coeffs,2,2);
#else
  // new version correct c_5 term
  wilson_okaction_full_testtadpole(flw,out,in,sign,coeffs,2,2);
#endif
};

// ****************************************************************
// ****************************************************************
//    _____.   -----.  .      .-----.  -----. .    .  |
//  ||     |  |        |      |     | |       |    |  |     
//  ||     |--.-----.  |      |-----| .-----. |----|  .-----.
//  ||     |        |  |      |     |       | |    |  |     |
//  ||-----.  .-----.   .----- .     . .-----. .    .  .-----.
// ****************************************************************
// ****************************************************************

static void
wilson_okaction_full_testtadpole(QOP_FermionLinksWilson *flw,
				 QDP_DiracFermion *result,
				 QDP_DiracFermion *source,
				 int sign,
				 QOP_wilson_ifla_coeffs_t *coeffs,
				 QOP_evenodd_t eo,
				 int n)
{

  //printf("LATEST V.4 FULL OK_ACTION\n");
  /* Tadpole improved version   */
  /* Here is the tadpole factor */
  /* Hard wired for now         */
  /* -------------------------- */
  static int dir;
  /* -------------------------- */
  QLA_Real tpf      = 1.0/(coeffs->u0);
  //warning: unused variable 'sff' ??
  QLA_Real sff      = 1e10;

  //printf("Tadpole Factor u0 = %e\n",tpf);
  /* Coefficients ------------- */
  QLA_Real Kappa   = coeffs->kapifla;
  QLA_Real r_s     = coeffs->r_s;
  QLA_Real zeta    = coeffs->zeta;
  QLA_Real c_E     = coeffs->c_E;
  QLA_Real c_B     = coeffs->c_B;
  /* Improvement Coefficients */
  QLA_Real c_1     = coeffs->c_1;
  QLA_Real c_2     = coeffs->c_2;
  QLA_Real c_3     = coeffs->c_3;
  QLA_Real c_4     = coeffs->c_4;
  QLA_Real c_5     = coeffs->c_5;
  QLA_Real c_EE    = coeffs->c_EE;
  /* ------------------------ */


  QLA_Real slc1    = 0.5;

#if 0
  printf("&&IFLA COEFFICIENTS : \n");
  printf("kappa_wilson = %e\t r_s = %e\t zeta = %e\n",Kappa,r_s,zeta);
  printf("c_E          = %e\t c_B = %e\t c_EE = %e\n",c_E,c_B,c_EE);
  printf("c_1          = %e\t c_2 = %e\t c_3  = %e\n",c_1,c_2,c_3);
  printf("c_4          = %e\t c_5 = %e\t          \n",c_4,c_5);
#endif


  /* Gamma Matrix Indices in QDP ============================ */

  //warning: variable 'bidx' set but not used ??
  QLA_Int gidx[4],aidx[4],bidx[4];

  gidx[0] = 1;  /* gamma[X]   = QDP_Gamma[1]   */
  gidx[1] = 2;  /* gamma[Y]   = QDP_Gamma[2]   */
  gidx[2] = 4;  /* gamma[Z]   = QDP_Gamma[4]   */
  gidx[3] = 8;  /* gamma[0]   = QDP_Gamma[8]   */

  aidx[0] = 9;  /* alpha[0]   = -QDP_Gamma[9]  */
  aidx[1] = 10; /* alpha[1]   = -QDP_Gamma[10] */
  aidx[2] = 12; /* alpha[2]   = -QDP_Gamma[12] */

  bidx[0] = 3;  /* i*Sigma[0] = QDP_Gamma[3]   */
  bidx[1] = 5;  /* i*Sigma[1] = QDP_Gamma[5]   */
  bidx[2] = 6;  /* i*Simga[2] = QDP_Gamma[6]   */
  /* --------------------------------------------------------- */

  /* DUMMY FIELDS ============================================ */
  int mu;
  QDP_DiracFermion *tempD1,*tempD2,*tempD3,*tempD4;
  QDP_DiracFermion *tempM,*tempP;
  QDP_DiracFermion *gamD;
  QDP_ColorMatrix  *tempG1,*tempG2,*tempG3;
  QDP_DiracFermion *psi_up[4];
  QDP_DiracFermion *psi_dw[4];
  QDP_ColorMatrix *f12,*f02,*f01;
  

  tempD1       = QDP_create_D();
  tempD2       = QDP_create_D();
  tempD3       = QDP_create_D();
  tempD4       = QDP_create_D();
  tempP        = QDP_create_D();
  tempM        = QDP_create_D();
  gamD         = QDP_create_D();
  tempG1       = QDP_create_M();
  tempG2       = QDP_create_M();
  tempG3       = QDP_create_M();
  f01          = QDP_create_M();
  f02          = QDP_create_M();
  f12          = QDP_create_M();

  /* --------------------------------------------------------- */
  
  /* Reading the gauge fields from FermionLinksWilson */
  QDP_ColorMatrix  *gauge[4];

  //  Rescaling the fields back
  static QLA_Real scale1 = -2.0;
  for(mu = 0;mu<4; mu++){
    gauge[mu] = QDP_create_M();
    //gauge[mu] = flw->links[mu];
    QDP_M_eq_r_times_M(gauge[mu],&scale1,flw->links[mu],QDP_all);
    // Here I scale with the tadpole factor 
    /*
    QDP_M_eq_r_times_M(gauge[mu],&tpf,gauge[mu],QDP_all);
    */
  };

  //Kappa = Kappa*tpf;

  /* This is 1/[ 2*Kappa_Wilson ] */
  QLA_Real m4 = 0.5/Kappa;

  //printf("1/(2*Kappa) = %e\n",m4);

  /*^^ result = (1/2*Kappa)*Source  */
  QDP_D_eq_zero(result,QDP_all);
  QDP_D_eq_zero(gamD,QDP_all);

#if 0
  static QLA_DiracFermion *qladf;
  static int st,nc,ns;
  static QLA_Real c1,c2;
  qladf = QDP_expose_D(source);
  printf("Exposing Dirac Fermion :: source \n");  
  for(st=1;st<2;st++){
    for(nc=0;nc<3;nc++){
      for(ns=0;ns<1;ns++){
	c1=QLA_real(QLA_elem_D(qladf[st],nc,ns));
	c2=QLA_imag(QLA_elem_D(qladf[st],nc,ns));
	printf("site : %d  color : %d  spin  : %d = (%e,%e)\n",st,nc,ns,c1,c2);
      };
    };
    printf("***********************************\n");
  };
  QDP_reset_D(source);
#endif
  // ------------------------------------------------
  //QDP_D_eq_r_times_D(source,&sff,source,QDP_all);



  //QLA_Real kapifla = m4+18.0*c_4; definition!!
  QDP_D_eq_r_times_D(result,&m4,source,QDP_all);
  
  /*^^ result -= [ D-Slash ]*Source */
  for(dir=0;dir<4;dir++){
    psi_up[dir] = QDP_create_D();
    psi_dw[dir] = QDP_create_D();
  };

  QLA_Real kat_a = 0.5*r_s*zeta+4.0*c_4;
  QLA_Real kat_b = 0.5*zeta-(c_1)-(6.0*c_2);
  
  
  //printf("r^primes_s = %e\n", kat_a);
  //printf("zeta^prime = %e\n", kat_b);
  

  for(dir=0;dir<4;dir++){

    QDP_D_eq_M_times_sD(psi_up[dir],gauge[dir],source,QDP_neighbor[dir],QDP_forward,QDP_all);
    QDP_D_eq_Ma_times_D(tempD1,gauge[dir],source,QDP_all);
    QDP_D_eq_sD(psi_dw[dir],tempD1,QDP_neighbor[dir],QDP_backward,QDP_all);
    
    if(dir!=3){
      QDP_D_eq_D_plus_D(tempD1,psi_up[dir],psi_dw[dir],QDP_all);
      QDP_D_eq_r_times_D(tempD1,&kat_a,tempD1,QDP_all);
      
      QDP_D_eq_D_minus_D(tempD2,psi_up[dir],psi_dw[dir],QDP_all);
      QDP_D_eq_gamma_times_D(tempD3,tempD2,gidx[dir],QDP_all);
      //
      if(c_3!=0){
	QDP_D_peq_D(gamD,tempD3,QDP_all); // \Gamma.D.Psi(x) for c_3
      };
      //
      QDP_D_eq_r_times_D(tempD3,&kat_b,tempD3,QDP_all);
      QDP_D_meq_D(tempD1,tempD3,QDP_all);
    }
    else {
      /* This is the time component */
      QDP_D_eq_D_plus_D( tempD1,psi_up[dir],psi_dw[dir],QDP_all);
      QDP_D_eq_D_minus_D(tempD2,psi_up[dir],psi_dw[dir],QDP_all);
      QDP_D_eq_gamma_times_D(tempD3,tempD2,gidx[dir],QDP_all);
      QDP_D_meq_D(tempD1,tempD3,QDP_all);
      QDP_D_eq_r_times_D(tempD1,&slc1,tempD1,QDP_all);
    };
    QDP_D_meq_D(result,tempD1,QDP_all);
  };

  
  /* ---------- up to here is standard -------------- */
 
  /* ------------------------------------------------ */
  /* ------- start of c_1+c_2+c4 diagonal terms ----- */
  /* ------------------------------------------------ */
  if((c_1!=0)||(c_2!=0)||(c_4!=0)){

    QLA_Real qc     = (0.5*c_1+c_2)*tpf;
    
    QDP_DiracFermion *u0u0,*u1u1,*u2u2,*d0d0,*d1d1,*d2d2;
    
    u0u0 = QDP_create_D();
    u1u1 = QDP_create_D();
    u2u2 = QDP_create_D();
    d0d0 = QDP_create_D();
    d1d1 = QDP_create_D();
    d2d2 = QDP_create_D();
    
    if((c_1!=0)||(c_2!=0)){
      // Create U_i(x)U_j(x+i)Psi(x+i+j)
      QDP_D_eq_M_times_sD(u0u0,gauge[0],psi_up[0],QDP_neighbor[0],QDP_forward,QDP_all);
      QDP_D_eq_M_times_sD(u1u1,gauge[1],psi_up[1],QDP_neighbor[1],QDP_forward,QDP_all);
      QDP_D_eq_M_times_sD(u2u2,gauge[2],psi_up[2],QDP_neighbor[2],QDP_forward,QDP_all);
      // Create Ud_i(x-i)Ud_j(x-i-j)Psi(x-i-j)
      QDP_D_eq_Ma_times_D(tempD1,gauge[0],psi_dw[0],QDP_all);
      QDP_D_eq_sD(d0d0,tempD1,QDP_neighbor[0],QDP_backward,QDP_all);
      QDP_D_eq_Ma_times_D(tempD1,gauge[1],psi_dw[1],QDP_all);
      QDP_D_eq_sD(d1d1,tempD1,QDP_neighbor[1],QDP_backward,QDP_all);
      QDP_D_eq_Ma_times_D(tempD1,gauge[2],psi_dw[2],QDP_all);
      QDP_D_eq_sD(d2d2,tempD1,QDP_neighbor[2],QDP_backward,QDP_all);
      // Construct and Add :
      QDP_D_eq_D_minus_D(tempD1,u0u0,d0d0,QDP_all);
      QDP_D_eq_gamma_times_D(tempD2,tempD1,gidx[0],QDP_all);
      QDP_D_eq_D_minus_D(tempD1,u1u1,d1d1,QDP_all);
      QDP_D_eq_gamma_times_D(tempD3,tempD1,gidx[1],QDP_all);
      QDP_D_eq_D_plus_D(tempP,tempD2,tempD3,QDP_all);
      QDP_D_eq_D_minus_D(tempD1,u2u2,d2d2,QDP_all);
      QDP_D_eq_gamma_times_D(tempD2,tempD1,gidx[2],QDP_all);
      QDP_D_peq_D(tempP,tempD2,QDP_all);
      QDP_D_eq_r_times_D(tempP,&qc,tempP,QDP_all);
      QDP_D_peq_D(result,tempP,QDP_all);
    };
    //  
    if(c_4!=0){
      QLA_Real tadc4 = c_4*tpf;
      QDP_D_eq_D_plus_D(tempD1,u0u0,d0d0,QDP_all);
      QDP_D_eq_D_plus_D(tempD2,u1u1,d1d1,QDP_all);
      QDP_D_eq_D_plus_D(tempP,tempD1,tempD2,QDP_all);
      QDP_D_eq_D_plus_D(tempD1,u2u2,d2d2,QDP_all);
      QDP_D_peq_D(tempP,tempD1,QDP_all);
      QDP_D_eq_r_times_D(tempP,&tadc4,tempP,QDP_all);
      QDP_D_peq_D(result,tempP,QDP_all);
    };
    
    QDP_destroy_D(u0u0);
    QDP_destroy_D(u1u1);
    QDP_destroy_D(u2u2);
    QDP_destroy_D(d0d0);
    QDP_destroy_D(d1d1);
    QDP_destroy_D(d2d2);
    
  };
  /* ------------------------------------------------ */
  /* --------- end of c_1+c_2+c4 diagonal terms ----- */
  /* ------------------------------------------------ */


  /* ------------------------------------------------ */
  /* ------- start of c_2 non-diagonal terms -------- */
  /* ------------------------------------------------ */

  if(c_2!=0){
    QLA_Real c2half = (0.5*c_2)*tpf;
    //1
    QDP_DiracFermion *s0,*s1,*s2,*d0,*d1,*d2;
    s0 = QDP_create_D();
    s1 = QDP_create_D();
    s2 = QDP_create_D();
    d0 = QDP_create_D();
    d1 = QDP_create_D();
    d2 = QDP_create_D();
    // -------------------------------------------------
    QDP_D_eq_D_plus_D( s0,psi_up[0],psi_dw[0],QDP_all);
    QDP_D_eq_D_plus_D( s1,psi_up[1],psi_dw[1],QDP_all);
    QDP_D_eq_D_plus_D( s2,psi_up[2],psi_dw[2],QDP_all);
    QDP_D_eq_D_minus_D(d0,psi_up[0],psi_dw[0],QDP_all);
    QDP_D_eq_D_minus_D(d1,psi_up[1],psi_dw[1],QDP_all);
    QDP_D_eq_D_minus_D(d2,psi_up[2],psi_dw[2],QDP_all);
    // -------------------------------------------------
    // 1
    QDP_D_eq_D_plus_D(tempP,s1,s2,QDP_all);
    QDP_D_eq_M_times_sD(tempD1,gauge[0],tempP,QDP_neighbor[0],QDP_forward,QDP_all);
    QDP_D_eq_Ma_times_D(tempD3,gauge[0],tempP,QDP_all);
    QDP_D_eq_sD(tempD2,tempD3,QDP_neighbor[0],QDP_backward,QDP_all);
    QDP_D_eq_D_minus_D(tempP,tempD1,tempD2,QDP_all); //D_0(S_1+S_2)
    //---
    QDP_D_eq_M_times_sD(tempD1,gauge[1],d0,QDP_neighbor[1],QDP_forward,QDP_all);
    QDP_D_eq_M_times_sD(tempD2,gauge[2],d0,QDP_neighbor[2],QDP_forward,QDP_all);
    QDP_D_eq_D_plus_D(tempM,tempD1,tempD2,QDP_all);
    QDP_D_eq_Ma_times_D(tempD3,gauge[1],d0,QDP_all);
    QDP_D_eq_sD(tempD1,tempD3,QDP_neighbor[1],QDP_backward,QDP_all);
    QDP_D_eq_Ma_times_D(tempD3,gauge[2],d0,QDP_all);
    QDP_D_eq_sD(tempD2,tempD3,QDP_neighbor[2],QDP_backward,QDP_all);
    QDP_D_peq_D(tempD1,tempD2,QDP_all);
    QDP_D_peq_D(tempM,tempD1,QDP_all);
    QDP_D_peq_D(tempP,tempM,QDP_all);
    QDP_D_eq_r_times_D(tempP,&c2half,tempP,QDP_all);
    QDP_D_eq_gamma_times_D(tempM,tempP,gidx[0],QDP_all);
    QDP_D_peq_D(result,tempM,QDP_all);
    // 2
    QDP_D_eq_D_plus_D(tempP,s0,s2,QDP_all);
    QDP_D_eq_M_times_sD(tempD1,gauge[1],tempP,QDP_neighbor[1],QDP_forward,QDP_all);
    QDP_D_eq_Ma_times_D(tempD3,gauge[1],tempP,QDP_all);
    QDP_D_eq_sD(tempD2,tempD3,QDP_neighbor[1],QDP_backward,QDP_all);
    QDP_D_eq_D_minus_D(tempP,tempD1,tempD2,QDP_all); //D_1(S_0+S_2)
    //---
    QDP_D_eq_M_times_sD(tempD1,gauge[0],d1,QDP_neighbor[0],QDP_forward,QDP_all);
    QDP_D_eq_M_times_sD(tempD2,gauge[2],d1,QDP_neighbor[2],QDP_forward,QDP_all);
    QDP_D_eq_D_plus_D(tempM,tempD1,tempD2,QDP_all);
    QDP_D_eq_Ma_times_D(tempD3,gauge[0],d1,QDP_all);
    QDP_D_eq_sD(tempD1,tempD3,QDP_neighbor[0],QDP_backward,QDP_all);
    QDP_D_eq_Ma_times_D(tempD3,gauge[2],d1,QDP_all);
    QDP_D_eq_sD(tempD2,tempD3,QDP_neighbor[2],QDP_backward,QDP_all);
    QDP_D_peq_D(tempD1,tempD2,QDP_all);
    QDP_D_peq_D(tempM,tempD1,QDP_all);
    QDP_D_peq_D(tempP,tempM,QDP_all);
    QDP_D_eq_r_times_D(tempP,&c2half,tempP,QDP_all);
    QDP_D_eq_gamma_times_D(tempM,tempP,gidx[1],QDP_all);
    QDP_D_peq_D(result,tempM,QDP_all);
    // 3
    QDP_D_eq_D_plus_D(tempP,s0,s1,QDP_all);
    QDP_D_eq_M_times_sD(tempD1,gauge[2],tempP,QDP_neighbor[2],QDP_forward,QDP_all);
    QDP_D_eq_Ma_times_D(tempD3,gauge[2],tempP,QDP_all);
    QDP_D_eq_sD(tempD2,tempD3,QDP_neighbor[2],QDP_backward,QDP_all);
    QDP_D_eq_D_minus_D(tempP,tempD1,tempD2,QDP_all); //D_2(S_0+S_1)
    //---
    QDP_D_eq_M_times_sD(tempD1,gauge[0],d2,QDP_neighbor[0],QDP_forward,QDP_all);
    QDP_D_eq_M_times_sD(tempD2,gauge[1],d2,QDP_neighbor[1],QDP_forward,QDP_all);
    QDP_D_eq_D_plus_D(tempM,tempD1,tempD2,QDP_all);
    QDP_D_eq_Ma_times_D(tempD3,gauge[0],d2,QDP_all);
    QDP_D_eq_sD(tempD1,tempD3,QDP_neighbor[0],QDP_backward,QDP_all);
    QDP_D_eq_Ma_times_D(tempD3,gauge[1],d2,QDP_all);
    QDP_D_eq_sD(tempD2,tempD3,QDP_neighbor[1],QDP_backward,QDP_all);
    QDP_D_peq_D(tempD1,tempD2,QDP_all);
    QDP_D_peq_D(tempM,tempD1,QDP_all);
    QDP_D_peq_D(tempP,tempM,QDP_all);
    QDP_D_eq_r_times_D(tempP,&c2half,tempP,QDP_all);
    QDP_D_eq_gamma_times_D(tempM,tempP,gidx[2],QDP_all);
    QDP_D_peq_D(result,tempM,QDP_all);
    

    // ---------------------------------------------------
    QDP_destroy_D(s0);
    QDP_destroy_D(s1);
    QDP_destroy_D(s2);
    QDP_destroy_D(d0);
    QDP_destroy_D(d1);
    QDP_destroy_D(d2);
    
  };
  
  
/* ------------------------------------------------ */
/* ------- end of c_2 non-diagonal terms ---------- */
/* ------------------------------------------------ */
 
/* ------------------------------------------------ */
/* ------- start of c_E and c_EE terms --- -------- */
/* ------------------------------------------------ */
 
 if((c_E!=0)||(c_EE!=0)){
   
   QLA_Real cEhalf   = (0.5*c_E*zeta)*tpf*tpf*tpf;
   QLA_Real cE2half  = (0.5*c_EE)*tpf*tpf*tpf*tpf;
   
   QDP_ColorMatrix  *e0,*e1,*e2;
   
   e0 = QDP_create_M();
   e1 = QDP_create_M();
   e2 = QDP_create_M();

   f_mu_nu(e0,1.0,gauge,3,0); // E_0 = F_{30} :: Alpha[0] = -Qamma[9]
   f_mu_nu(e1,1.0,gauge,3,1); // E_1 = F_{31} :: Alpha[1] = -Qamma[10]
   f_mu_nu(e2,1.0,gauge,3,2); // E_2 = F_{32} :: Alpha[2] = -Qamma[12]
   // tempD4 = [U_3(x).Psi(x+3)-Ud_3(x-3).Psi(x-3)]
   QDP_D_eq_D_minus_D(tempD4,psi_up[3],psi_dw[3],QDP_all);
   // --------------------------------------------------- 
   /* C_E TERM */
   QDP_D_eq_M_times_D(tempD1,e0,source,QDP_all); //E_0 Psi(x)
   QDP_D_eq_gamma_times_D(tempP,tempD1,aidx[0],QDP_all);// +Qop[9]E_0Psi(x)  
   QDP_D_eq_M_times_D(tempD2,e1,source,QDP_all); //E_1 Psi(x)
   QDP_D_eq_gamma_times_D(tempM,tempD2,aidx[1],QDP_all);// +Qop[10]E_1Psi(x)
   QDP_D_peq_D(tempP,tempM,QDP_all); // (Qop[9]E_0+Qop[10]E_1)Psi(x)
   QDP_D_eq_M_times_D(tempD3,e2,source,QDP_all); // E_2 Psi(x)
   QDP_D_eq_gamma_times_D(tempM,tempD3,aidx[2],QDP_all); // +Qop[12]E_2Psi(x)
   QDP_D_peq_D(tempP,tempM,QDP_all); // (Qop[9]E_0+Qop[10]E_1+Qop[12]E_2)Psi(x)
   // \alpha.E.Psi(x) = -tempP
   QDP_D_eq_r_times_D(tempM,&cEhalf,tempP,QDP_all);
   QDP_D_peq_D(result,tempM,QDP_all); // plus becaue of - sign from the op //
   // --------------------------------------------------
   /* c_EE TERM */
   if(c_EE!=0){
     QDP_D_eq_M_times_sD(tempD1,gauge[3],tempP,QDP_neighbor[3],QDP_forward,QDP_all);
     QDP_D_eq_Ma_times_D(tempD3,gauge[3],tempP,QDP_all);
     QDP_D_eq_sD(tempD2,tempD3,QDP_neighbor[3],QDP_backward,QDP_all);
     QDP_D_meq_D(tempD1,tempD2,QDP_all);
     QDP_D_eq_gamma_times_D(tempM,tempD1,8,QDP_all);//-Gam[4]D_4\alphaE
     // --------------------------------------
     QDP_D_eq_gamma_times_D(tempD2,tempD4,gidx[0],QDP_all);
     QDP_D_eq_M_times_D(tempD3,e0,tempD2,QDP_all);
     QDP_D_peq_D(tempM,tempD3,QDP_all);
     //
     QDP_D_eq_gamma_times_D(tempD2,tempD4,gidx[1],QDP_all);
     QDP_D_eq_M_times_D(tempD3,e1,tempD2,QDP_all);
     QDP_D_peq_D(tempM,tempD3,QDP_all);
     //
     QDP_D_eq_gamma_times_D(tempD2,tempD4,gidx[2],QDP_all);
     QDP_D_eq_M_times_D(tempD3,e2,tempD2,QDP_all);
     QDP_D_peq_D(tempM,tempD3,QDP_all);
     // ----
     QDP_D_eq_r_times_D(tempM,&cE2half,tempM,QDP_all);
     QDP_D_meq_D(result,tempM,QDP_all);
     
   };
   QDP_destroy_M(e0);
   QDP_destroy_M(e1);
   QDP_destroy_M(e2);
  
 };
 /* ------------------------------------------------ */
 /* --------- end of c_E and c_EE terms ------------ */
 /* ------------------------------------------------ */


 
 
 /* ------------------------------------------------ */
 /* ------- start of c_B+c_3+c+5 ------ ------------ */
 /* ------------------------------------------------ */ 
 
 f_mu_nu(f12,1.0,gauge,1,2);
 f_mu_nu(f02,1.0,gauge,0,2); //*****
 f_mu_nu(f01,1.0,gauge,0,1); 

 // LOOP NO : 8
 //******if((c_B!=0)||(c_3!=0)){
   
   QLA_Real cBhalf = (0.5*c_B*zeta+8.0*c_5)*tpf*tpf*tpf;
   QLA_Real c3half = (0.5*c_3)*tpf*tpf*tpf*tpf;
   
   //printf("cBhalf = %e\n",cBhalf);
   //printf("c3half = %e\n",c3half);

   
   QDP_DiracFermion *b0psi;
   QDP_DiracFermion *b1psi;
   QDP_DiracFermion *b2psi;
   QDP_DiracFermion *sigB;
   b0psi = QDP_create_D();
   b1psi = QDP_create_D();
   b2psi = QDP_create_D();
   sigB  = QDP_create_D();

   // tempG1 = F_{12} = B_0
   // b0psi  = B_0 Psi(x)
   // tempD1 = Gamma[6].B_0.Psi(x) = (+)iSigma[0].B_0.Psi(x)
   /* f_mu_nu(f12,1.0,gauge,1,2); */
   QDP_D_eq_M_times_D(b0psi,f12,source,QDP_all);
   QDP_D_eq_gamma_times_D(tempD1,b0psi,6,QDP_all);

   // tempG2 = F_{20} = B_1
   // b1psi  = B_1 Psi(x)
   // tempD2 = Gamma[5].B_1.Psi(x) = (-)iSigma[1].B_1.Psi(x)
   /* f_mu_nu(f02,1.0,gauge,0,2); */ //*****
   QDP_D_eq_M_times_D(b1psi,f02,source,QDP_all);
   QDP_D_eq_gamma_times_D(tempD2,b1psi,5,QDP_all);
   
   // tempG3 = F_{01} = B_2
   // b2psi  = B_2 Psi(x)
   // tempD3 = Gamma[3].B_2.Psi(x) = (+)iSigma[2].B_2.Psi(x)
   /* f_mu_nu(f01,1.0,gauge,0,1); */
   QDP_D_eq_M_times_D(b2psi,f01,source,QDP_all);
   QDP_D_eq_gamma_times_D(tempD3,b2psi,3,QDP_all);
   
   // tempM = tempD1 - tempD2 Because : iSigma[1] = -Qamma[5]
   // sigB  = tempM  + tempD3
   QDP_D_eq_D_plus_D(tempM,tempD1,tempD2,QDP_all); //**** minus->plus
   QDP_D_eq_D_plus_D(sigB,tempM,tempD3,QDP_all);
   //
   QDP_D_eq_r_times_D(tempP,&cBhalf,sigB,QDP_all);
   // minus sign comes from the -c_B/2\Sigma.B term 
   QDP_D_meq_D(result,tempP,QDP_all);
    
   QDP_destroy_D(b0psi);
   QDP_destroy_D(b1psi);
   QDP_destroy_D(b2psi);
  
   // Now the c_3 part 
   //*****if(c3half!=0){
     // iSigma.B Gamma.D Psi(x)
     // 1 : tempG1 = F_{12}
     QDP_D_eq_M_times_D(tempD1,f12,gamD,QDP_all);
     QDP_D_eq_gamma_times_D(tempP,tempD1,6,QDP_all);
     // 2 : tempG2 = F_{20}
     QDP_D_eq_M_times_D(tempD1,f02,gamD,QDP_all);
     QDP_D_eq_gamma_times_D(tempD2,tempD1,5,QDP_all);
     QDP_D_peq_D(tempP,tempD2,QDP_all); //meq->peq
     //3
     QDP_D_eq_M_times_D(tempD1,f01,gamD,QDP_all);
     QDP_D_eq_gamma_times_D(tempD2,tempD1,3,QDP_all);
     QDP_D_peq_D(tempP,tempD2,QDP_all);
     //
     // gamma.D Sigma.B Psi(x)
     for(dir=0;dir<3;dir++){
       QDP_D_eq_M_times_sD(tempD1,gauge[dir],sigB,QDP_neighbor[dir],QDP_forward,QDP_all);
       QDP_D_eq_Ma_times_D(tempD3,gauge[dir],sigB,QDP_all);
       QDP_D_eq_sD(tempD2,tempD3,QDP_neighbor[dir],QDP_backward,QDP_all);
       QDP_D_meq_D(tempD1,tempD2,QDP_all);
       QDP_D_eq_gamma_times_D(tempD2,tempD1,gidx[dir],QDP_all);
       QDP_D_peq_D(tempP,tempD2,QDP_all);
     };
     //
     QDP_D_eq_r_times_D(tempP,&c3half,tempP,QDP_all);
     QDP_D_peq_D(result,tempP,QDP_all);
     //
     //*****}; // end of c3half
   
     //*****}; // end of LOOP 8
 
 
 // I no longer need tempP and tempM
 // empty memory
 //QDP_destroy_D(tempP);
     QDP_destroy_D(sigB);
     QDP_destroy_D(tempM);
     QDP_destroy_D(gamD);

 
 // Below is the new c_5 term where the three and five
 // link terms are seperated and multiplied with the 
 // appropriate power of tadpole factor. Keeping it separete for now.
 // Factor of 2 coming from \Delta is observed in c_B calculation above.
 // ::::::
 QLA_Real oneeight = +0.125;
 QLA_Real two      = +2.0;
 
 QLA_Real c5u3     = c_5*tpf*tpf;
 QLA_Real c5u5     = c_5*tpf*tpf*tpf*tpf;

 QDP_ColorMatrix *mat;
 mat = QDP_create_M();
 
 //1
#if 1
 //f_mu_nu(f12,1,gauge,1,2);
 
 // (I) i\Sigma[0][ B_0 (u1+d1+u2+d2) + (u1+d1+u2+d2) B_0 ] psi(x)
 // i\Sigma[0] = QOP_Gamma[6] and B_0 = F_{12} 
 // ##############################################################
 // ##############################################################
 // ##############################################################
 // LINKS-SEPARATED VERSION 
 // ###########################################################
 // M = -U_2(x)U_1(x+2)Ud_2(x+1)+Ud_2(x-2)U_1(x-2)U_2(x+1-2)
 QDP_M_eq_sM(tempG1,gauge[1],QDP_neighbor[2],QDP_forward,QDP_all);
 QDP_M_eq_sM(tempG2,gauge[2],QDP_neighbor[1],QDP_forward,QDP_all);
 QDP_M_eq_M_times_Ma(tempG3,tempG1,tempG2,QDP_all);
 QDP_M_eq_M_times_M(tempG1,gauge[2],tempG3,QDP_all);
 //---
 QDP_M_eq_Ma_times_M(tempG2,gauge[2],gauge[1],QDP_all);
 QDP_M_eq_M_times_sM(tempG3,tempG2,gauge[2],QDP_neighbor[1],QDP_forward,QDP_all);
 QDP_M_eq_sM(mat,tempG3,QDP_neighbor[2],QDP_backward,QDP_all);
 //---
 QDP_M_meq_M(mat,tempG1,QDP_all);
 QDP_M_eq_r_times_M(mat,&oneeight,mat,QDP_all); // times 2
 // ###########################################################
 // u1
 QDP_M_eq_M_times_Ma(tempG1,mat,gauge[1],QDP_all);
 QDP_M_eq_M_minus_M(tempG2,f12,tempG1,QDP_all);
 // R :QDP_D_eq_M_times_sD(tempD1,gauge[1],source,QDP_neighbor[1],QDP_forward,QDP_all);
 // W : tempD1 --> psi_up[1]
 QDP_D_eq_M_times_D(tempD2,tempG2,psi_up[1],QDP_all); // YES
 // d1
 QDP_M_eq_Ma_times_M(tempG1,mat,gauge[1],QDP_all);
 QDP_M_eq_sM(tempG2,tempG1,QDP_neighbor[1],QDP_backward,QDP_all);
 QDP_M_eq_M_plus_M(tempG1,f12,tempG2,QDP_all);
 
 //R:QDP_D_eq_Ma_times_D(tempD1,gauge[1],source,QDP_all);
 //R:QDP_D_eq_M_times_sD(tempD3,tempG1,tempD1,QDP_neighbor[1],QDP_backward,QDP_all);
 QDP_D_eq_M_times_D(tempD3,tempG1,psi_dw[1],QDP_all); // YES
 QDP_D_peq_D(tempD2,tempD3,QDP_all);
  //u1
 QDP_M_eq_Ma_times_M(tempG1,gauge[1],mat,QDP_all);
 QDP_M_eq_sM(tempG2,tempG1,QDP_neighbor[1],QDP_backward,QDP_all);
 QDP_M_eq_M_minus_M(tempG3,f12,tempG2,QDP_all);
 QDP_M_eq_M_times_sM(tempG1,gauge[1],tempG3,QDP_neighbor[1],QDP_forward,QDP_all);
 QDP_D_eq_M_times_sD(tempD3,tempG1,source,QDP_neighbor[1],QDP_forward,QDP_all);
 QDP_D_peq_D(tempD2,tempD3,QDP_all);
  //d1
 QDP_M_eq_M_times_Ma(tempG1,gauge[1],mat,QDP_all);
 QDP_M_eq_M_plus_M(tempG2,f12,tempG1,QDP_all);
 QDP_M_eq_Ma_times_M(tempG1,gauge[1],tempG2,QDP_all);
 QDP_D_eq_M_times_D(tempD1,tempG1,source,QDP_all);
 QDP_D_eq_sD(tempD3,tempD1,QDP_neighbor[1],QDP_backward,QDP_all);
 //
 QDP_D_peq_D(tempD2,tempD3,QDP_all); // all 5-links
  //
 QDP_D_eq_r_times_D(tempD2,&c5u5,tempD2,QDP_all);
 QDP_D_eq_gamma_times_D(tempD4,tempD2,6,QDP_all);
 QDP_D_peq_D(result,tempD4,QDP_all); // all 5-links added to result
 //
 // u1 M(x)Psi(x+1)
 QDP_D_eq_M_times_sD(tempD4,mat,source,QDP_neighbor[1],QDP_forward,QDP_all);
 //d1
 QDP_D_eq_Ma_times_D(tempD1,mat,source,QDP_all); // Md(x)Psi(x)
 // Md(x-1)Psi(x-1)
 QDP_D_eq_sD(tempD3,tempD1,QDP_neighbor[1],QDP_backward,QDP_all);
 QDP_D_meq_D(tempD4,tempD3,QDP_all);
 QDP_D_eq_r_times_D(tempD4,&two,tempD4,QDP_all);
 //
 QDP_D_eq_r_times_D(tempD4,&c5u3,tempD4,QDP_all);
 QDP_D_eq_gamma_times_D(tempD3,tempD4,6,QDP_all);
 QDP_D_peq_D(result,tempD3,QDP_all); // all 3-links added to result
 
 // ###########################################################
 QDP_M_eq_sM(tempG1,gauge[2],QDP_neighbor[1],QDP_forward,QDP_all);
 QDP_M_eq_sM(tempG2,gauge[1],QDP_neighbor[2],QDP_forward,QDP_all);
 QDP_M_eq_M_times_Ma(tempG3,tempG1,tempG2,QDP_all);
 QDP_M_eq_M_times_M(mat,gauge[1],tempG3,QDP_all);
 //---
 QDP_M_eq_Ma_times_M(tempG2,gauge[1],gauge[2],QDP_all);
 QDP_M_eq_M_times_sM(tempG3,tempG2,gauge[1],QDP_neighbor[2],QDP_forward,QDP_all);
 QDP_M_eq_sM(tempG1,tempG3,QDP_neighbor[1],QDP_backward,QDP_all);
 //---
 QDP_M_meq_M(mat,tempG1,QDP_all);
 QDP_M_eq_r_times_M(mat,&oneeight,mat,QDP_all);
 // ###########################################################
 // u2
 QDP_M_eq_M_times_Ma(tempG1,mat,gauge[2],QDP_all);
 QDP_M_eq_M_minus_M(tempG2,f12,tempG1,QDP_all);
 // R : QDP_D_eq_M_times_sD(tempD1,gauge[2],source,QDP_neighbor[2],QDP_forward,QDP_all);
 // W : tempD1 = psi_up[2]-----------V
 QDP_D_eq_M_times_D(tempD2,tempG2,psi_up[2],QDP_all); // YES
 // d2
 QDP_M_eq_Ma_times_M(tempG1,mat,gauge[2],QDP_all);
 QDP_M_eq_sM(tempG2,tempG1,QDP_neighbor[2],QDP_backward,QDP_all);
 QDP_M_eq_M_plus_M(tempG1,f12,tempG2,QDP_all);

 //R: QDP_D_eq_Ma_times_D(tempD1,gauge[2],source,QDP_all);
 //R: QDP_D_eq_M_times_sD(tempD3,tempG1,tempD1,QDP_neighbor[2],QDP_backward,QDP_all);
 // W : V
 QDP_D_eq_M_times_D(tempD3,tempG1,psi_dw[2],QDP_all); // YES
 QDP_D_peq_D(tempD2,tempD3,QDP_all);
 //u2
 QDP_M_eq_Ma_times_M(tempG1,gauge[2],mat,QDP_all);
 QDP_M_eq_sM(tempG2,tempG1,QDP_neighbor[2],QDP_backward,QDP_all);
 QDP_M_eq_M_minus_M(tempG3,f12,tempG2,QDP_all);
 QDP_M_eq_M_times_sM(tempG1,gauge[2],tempG3,QDP_neighbor[2],QDP_forward,QDP_all);
 QDP_D_eq_M_times_sD(tempD3,tempG1,source,QDP_neighbor[2],QDP_forward,QDP_all);
 QDP_D_peq_D(tempD2,tempD3,QDP_all);
 //d2
 QDP_M_eq_M_times_Ma(tempG1,gauge[2],mat,QDP_all);
 QDP_M_eq_M_plus_M(tempG2,f12,tempG1,QDP_all);
 QDP_M_eq_Ma_times_M(tempG1,gauge[2],tempG2,QDP_all);
 QDP_D_eq_M_times_D(tempD1,tempG1,source,QDP_all);
 QDP_D_eq_sD(tempD3,tempD1,QDP_neighbor[2],QDP_backward,QDP_all);
 //
 QDP_D_peq_D(tempD2,tempD3,QDP_all); // all 5-links
 QDP_D_eq_r_times_D(tempD2,&c5u5,tempD2,QDP_all);
 QDP_D_eq_gamma_times_D(tempD3,tempD2,6,QDP_all);
 QDP_D_peq_D(result,tempD3,QDP_all); // all 5-links added to result
 //
 // u1
 QDP_D_eq_M_times_sD(tempD4,mat,source,QDP_neighbor[2],QDP_forward,QDP_all);
 //d1
 QDP_D_eq_Ma_times_D(tempD1,mat,source,QDP_all);
 QDP_D_eq_sD(tempD3,tempD1,QDP_neighbor[2],QDP_backward,QDP_all);
 QDP_D_meq_D(tempD4,tempD3,QDP_all);
 QDP_D_eq_r_times_D(tempD4,&two,tempD4,QDP_all);
 //
 QDP_D_eq_r_times_D(tempD4,&c5u3,tempD4,QDP_all);
 QDP_D_eq_gamma_times_D(tempD2,tempD4,6,QDP_all);
 QDP_D_peq_D(result,tempD2,QDP_all); // all 3-links added to result

 // I no longer need F12
 QDP_destroy_M(f12);
#endif

 //2
#if 1
 // ##############################################################
 // ##############################################################
 // ##############################################################
 // (I) i\Sigma[1][ B_1 (u0+d0+u2+d2) + (u0+d0+u2+d2) B_0 ] psi(x)
 // i\Sigma[1] = -QOP_Gamma[5] and B_1 = -F_{02} 
 // ##############################################################
 // ##############################################################
 // ##############################################################
 // LINKS-SEPARATED VERSION 
 // ###########################################################
 // M = Ud_2(x-2)U_0(x-0)U_2(x+0-2)-U_2(x)U_0(x+2)Ud_2(x+0)

 
 //f_mu_nu(f02,1,gauge,0,2);
 
 QDP_M_eq_sM(tempG1,gauge[0],QDP_neighbor[2],QDP_forward,QDP_all);
 QDP_M_eq_sM(tempG2,gauge[2],QDP_neighbor[0],QDP_forward,QDP_all);
 QDP_M_eq_M_times_Ma(tempG3,tempG1,tempG2,QDP_all);
 QDP_M_eq_M_times_M(tempG1,gauge[2],tempG3,QDP_all);
 //---
 QDP_M_eq_Ma_times_M(tempG2,gauge[2],gauge[0],QDP_all);
 QDP_M_eq_M_times_sM(tempG3,tempG2,gauge[2],QDP_neighbor[0],QDP_forward,QDP_all);
 QDP_M_eq_sM(mat,tempG3,QDP_neighbor[2],QDP_backward,QDP_all);
 //---
 QDP_M_meq_M(mat,tempG1,QDP_all);
 QDP_M_eq_r_times_M(mat,&oneeight,mat,QDP_all);
 // ###########################################################
 // u1
 QDP_M_eq_M_times_Ma(tempG1,mat,gauge[0],QDP_all);
 QDP_M_eq_M_minus_M(tempG2,f02,tempG1,QDP_all);
 //R: QDP_D_eq_M_times_sD(tempD1,gauge[0],source,QDP_neighbor[0],QDP_forward,QDP_all);
 //W: tempD1 = psi_up[0]-----------V
 QDP_D_eq_M_times_D(tempD2,tempG2,psi_up[0],QDP_all); //YES
  // d1
  QDP_M_eq_Ma_times_M(tempG1,mat,gauge[0],QDP_all);
  QDP_M_eq_sM(tempG2,tempG1,QDP_neighbor[0],QDP_backward,QDP_all);
  QDP_M_eq_M_plus_M(tempG1,f02,tempG2,QDP_all);
  
  //R:QDP_D_eq_Ma_times_D(tempD1,gauge[0],source,QDP_all);
  //R:QDP_D_eq_M_times_sD(tempD3,tempG1,tempD1,QDP_neighbor[0],QDP_backward,QDP_all);
  //W: tempD1 = psi_dw[0]---^
  QDP_D_eq_M_times_D(tempD3,tempG1,psi_dw[0],QDP_all); //YES
  QDP_D_peq_D(tempD2,tempD3,QDP_all);
  //u1
  QDP_M_eq_Ma_times_M(tempG1,gauge[0],mat,QDP_all);
  QDP_M_eq_sM(tempG2,tempG1,QDP_neighbor[0],QDP_backward,QDP_all);
  QDP_M_eq_M_minus_M(tempG3,f02,tempG2,QDP_all);
  QDP_M_eq_M_times_sM(tempG1,gauge[0],tempG3,QDP_neighbor[0],QDP_forward,QDP_all);
  QDP_D_eq_M_times_sD(tempD3,tempG1,source,QDP_neighbor[0],QDP_forward,QDP_all);
  QDP_D_peq_D(tempD2,tempD3,QDP_all);
  //d1
  QDP_M_eq_M_times_Ma(tempG1,gauge[0],mat,QDP_all);
  QDP_M_eq_M_plus_M(tempG2,f02,tempG1,QDP_all);
  QDP_M_eq_Ma_times_M(tempG1,gauge[0],tempG2,QDP_all);
  QDP_D_eq_M_times_D(tempD1,tempG1,source,QDP_all);
  QDP_D_eq_sD(tempD3,tempD1,QDP_neighbor[0],QDP_backward,QDP_all);
  //
  QDP_D_peq_D(tempD2,tempD3,QDP_all); // all 5-links
  //
  QDP_D_eq_r_times_D(tempD2,&c5u5,tempD2,QDP_all);
  QDP_D_eq_gamma_times_D(tempD4,tempD2,5,QDP_all);
  QDP_D_peq_D(result,tempD4,QDP_all); // 5-links added to result
  //
  // u1
  QDP_D_eq_M_times_sD(tempD4,mat,source,QDP_neighbor[0],QDP_forward,QDP_all);
  //d1
  QDP_D_eq_Ma_times_D(tempD1,mat,source,QDP_all);
  QDP_D_eq_sD(tempD3,tempD1,QDP_neighbor[0],QDP_backward,QDP_all);
  QDP_D_meq_D(tempD4,tempD3,QDP_all);
  QDP_D_eq_r_times_D(tempD4,&two,tempD4,QDP_all);
  //
  QDP_D_eq_r_times_D(tempD4,&c5u3,tempD4,QDP_all);
 
  QDP_D_eq_gamma_times_D(tempD3,tempD4,5,QDP_all);
  QDP_D_peq_D(result,tempD3,QDP_all);
  
  // ###########################################################
  // ###########################################################
  // ###########################################################
  
  QDP_M_eq_sM(tempG1,gauge[2],QDP_neighbor[0],QDP_forward,QDP_all);
  QDP_M_eq_sM(tempG2,gauge[0],QDP_neighbor[2],QDP_forward,QDP_all);
  QDP_M_eq_M_times_Ma(tempG3,tempG1,tempG2,QDP_all);
  QDP_M_eq_M_times_M(mat,gauge[0],tempG3,QDP_all);
  //---
  QDP_M_eq_Ma_times_M(tempG2,gauge[0],gauge[2],QDP_all);
  QDP_M_eq_M_times_sM(tempG3,tempG2,gauge[0],QDP_neighbor[2],QDP_forward,QDP_all);
  QDP_M_eq_sM(tempG1,tempG3,QDP_neighbor[0],QDP_backward,QDP_all);
  //---
  QDP_M_meq_M(mat,tempG1,QDP_all);
  QDP_M_eq_r_times_M(mat,&oneeight,mat,QDP_all);
  // ###########################################################
  // u2
  QDP_M_eq_M_times_Ma(tempG1,mat,gauge[2],QDP_all);
  QDP_M_eq_M_minus_M(tempG2,f02,tempG1,QDP_all);
  //R:QDP_D_eq_M_times_sD(tempD1,gauge[2],source,QDP_neighbor[2],QDP_forward,QDP_all);
  // W: tempD1 = psi_up[2]-----------V
  QDP_D_eq_M_times_D(tempD2,tempG2,psi_up[2],QDP_all); //YES
  // d2
  QDP_M_eq_Ma_times_M(tempG1,mat,gauge[2],QDP_all);
  QDP_M_eq_sM(tempG2,tempG1,QDP_neighbor[2],QDP_backward,QDP_all);
  QDP_M_eq_M_plus_M(tempG1,f02,tempG2,QDP_all);
  //R:QDP_D_eq_Ma_times_D(tempD1,gauge[2],source,QDP_all);
  //R:QDP_D_eq_M_times_sD(tempD3,tempG1,tempD1,QDP_neighbor[2],QDP_backward,QDP_all);
  // W:
  QDP_D_eq_M_times_D(tempD3,tempG1,psi_dw[2],QDP_all); //YES
  QDP_D_peq_D(tempD2,tempD3,QDP_all);
  //u2
  QDP_M_eq_Ma_times_M(tempG1,gauge[2],mat,QDP_all);
  QDP_M_eq_sM(tempG2,tempG1,QDP_neighbor[2],QDP_backward,QDP_all);
  QDP_M_eq_M_minus_M(tempG3,f02,tempG2,QDP_all);
  QDP_M_eq_M_times_sM(tempG1,gauge[2],tempG3,QDP_neighbor[2],QDP_forward,QDP_all);
  QDP_D_eq_M_times_sD(tempD3,tempG1,source,QDP_neighbor[2],QDP_forward,QDP_all);
  QDP_D_peq_D(tempD2,tempD3,QDP_all);
  //d2
  QDP_M_eq_M_times_Ma(tempG1,gauge[2],mat,QDP_all);
  QDP_M_eq_M_plus_M(tempG2,f02,tempG1,QDP_all);
  QDP_M_eq_Ma_times_M(tempG1,gauge[2],tempG2,QDP_all);
  QDP_D_eq_M_times_D(tempD1,tempG1,source,QDP_all);
  QDP_D_eq_sD(tempD3,tempD1,QDP_neighbor[2],QDP_backward,QDP_all);
  //
  QDP_D_peq_D(tempD2,tempD3,QDP_all); // all 5-links
  QDP_D_eq_r_times_D(tempD2,&c5u5,tempD2,QDP_all);
  QDP_D_eq_gamma_times_D(tempD3,tempD2,5,QDP_all);
  QDP_D_peq_D(result,tempD3,QDP_all); // 5 links added to the result 
  //
  // u1
  QDP_D_eq_M_times_sD(tempD4,mat,source,QDP_neighbor[2],QDP_forward,QDP_all);
  //d1
  QDP_D_eq_Ma_times_D(tempD1,mat,source,QDP_all);
  QDP_D_eq_sD(tempD3,tempD1,QDP_neighbor[2],QDP_backward,QDP_all);
  QDP_D_meq_D(tempD4,tempD3,QDP_all);
  QDP_D_eq_r_times_D(tempD4,&two,tempD4,QDP_all);
  //
  QDP_D_eq_r_times_D(tempD4,&c5u3,tempD4,QDP_all);
  QDP_D_eq_gamma_times_D(tempD2,tempD4,5,QDP_all);
  QDP_D_peq_D(result,tempD2,QDP_all);
  
  // I no longer need f_02
  QDP_destroy_M(f02);

#endif

  //3
#if 1

  // ##############################################################
  // ##############################################################
  // ##############################################################
  // (I) i\Sigma[2][ B_2 (u0+d0+u1+d1) + (u0+d0+u1+d1) B_2 ] psi(x)
  // i\Sigma[2] = +QOP_Gamma[3] and B_1 = +F_{01} 
  // ##############################################################
  // ##############################################################
  // ##############################################################
  // LINKS-SEPARATED VERSION 
  // ###########################################################
  // M = 

  //f_mu_nu(f01,1,gauge,0,1);

  QDP_M_eq_sM(tempG1,gauge[0],QDP_neighbor[1],QDP_forward,QDP_all);
  QDP_M_eq_sM(tempG2,gauge[1],QDP_neighbor[0],QDP_forward,QDP_all);
  QDP_M_eq_M_times_Ma(tempG3,tempG1,tempG2,QDP_all);
  QDP_M_eq_M_times_M(tempG1,gauge[1],tempG3,QDP_all);
  //---
  QDP_M_eq_Ma_times_M(tempG2,gauge[1],gauge[0],QDP_all);
  QDP_M_eq_M_times_sM(tempG3,tempG2,gauge[1],QDP_neighbor[0],QDP_forward,QDP_all);
  QDP_M_eq_sM(mat,tempG3,QDP_neighbor[1],QDP_backward,QDP_all);
  //---
  QDP_M_meq_M(mat,tempG1,QDP_all);
  QDP_M_eq_r_times_M(mat,&oneeight,mat,QDP_all);
  // ###########################################################
  // u1
  QDP_M_eq_M_times_Ma(tempG1,mat,gauge[0],QDP_all);
  QDP_M_eq_M_minus_M(tempG2,f01,tempG1,QDP_all);
  //R:QDP_D_eq_M_times_sD(tempD1,gauge[0],source,QDP_neighbor[0],QDP_forward,QDP_all);
  //W:tempD1 = psi_up[0]------------V
  QDP_D_eq_M_times_D(tempD2,tempG2,psi_up[0],QDP_all); //YES
  // d1
  QDP_M_eq_Ma_times_M(tempG1,mat,gauge[0],QDP_all);
  QDP_M_eq_sM(tempG2,tempG1,QDP_neighbor[0],QDP_backward,QDP_all);
  QDP_M_eq_M_plus_M(tempG1,f01,tempG2,QDP_all);
  //R:QDP_D_eq_Ma_times_D(tempD1,gauge[0],source,QDP_all);
  //R:QDP_D_eq_M_times_sD(tempD3,tempG1,tempD1,QDP_neighbor[0],QDP_backward,QDP_all);
  //W: tempD1 = psi_dw[0];
  QDP_D_eq_M_times_D(tempD3,tempG1,psi_dw[0],QDP_all); //YES
 QDP_D_peq_D(tempD2,tempD3,QDP_all);
  //u1
  QDP_M_eq_Ma_times_M(tempG1,gauge[0],mat,QDP_all);
  QDP_M_eq_sM(tempG2,tempG1,QDP_neighbor[0],QDP_backward,QDP_all);
  QDP_M_eq_M_minus_M(tempG3,f01,tempG2,QDP_all);
  QDP_M_eq_M_times_sM(tempG1,gauge[0],tempG3,QDP_neighbor[0],QDP_forward,QDP_all);
  QDP_D_eq_M_times_sD(tempD3,tempG1,source,QDP_neighbor[0],QDP_forward,QDP_all);
  QDP_D_peq_D(tempD2,tempD3,QDP_all);
  //d1
  QDP_M_eq_M_times_Ma(tempG1,gauge[0],mat,QDP_all);
  QDP_M_eq_M_plus_M(tempG2,f01,tempG1,QDP_all);
  QDP_M_eq_Ma_times_M(tempG1,gauge[0],tempG2,QDP_all);
  QDP_D_eq_M_times_D(tempD1,tempG1,source,QDP_all);
  QDP_D_eq_sD(tempD3,tempD1,QDP_neighbor[0],QDP_backward,QDP_all);
  //
  QDP_D_peq_D(tempD2,tempD3,QDP_all); // all 5-links
  //
  QDP_D_eq_r_times_D(tempD2,&c5u5,tempD2,QDP_all);
  QDP_D_eq_gamma_times_D(tempD4,tempD2,3,QDP_all);
  QDP_D_peq_D(result,tempD4,QDP_all); // 5-links added to the result
  //
  // u1
  QDP_D_eq_M_times_sD(tempD4,mat,source,QDP_neighbor[0],QDP_forward,QDP_all);
  //d1
  QDP_D_eq_Ma_times_D(tempD1,mat,source,QDP_all);
  QDP_D_eq_sD(tempD3,tempD1,QDP_neighbor[0],QDP_backward,QDP_all);
  QDP_D_meq_D(tempD4,tempD3,QDP_all);
  QDP_D_eq_r_times_D(tempD4,&two,tempD4,QDP_all);
  //
  QDP_D_eq_r_times_D(tempD4,&c5u3,tempD4,QDP_all);
  QDP_D_eq_gamma_times_D(tempD3,tempD4,3,QDP_all);
  QDP_D_peq_D(result,tempD3,QDP_all); // 3-links added to the result
  
  
  // ###########################################################
  QDP_M_eq_sM(tempG1,gauge[1],QDP_neighbor[0],QDP_forward,QDP_all);
  QDP_M_eq_sM(tempG2,gauge[0],QDP_neighbor[1],QDP_forward,QDP_all);
  QDP_M_eq_M_times_Ma(tempG3,tempG1,tempG2,QDP_all);
  QDP_M_eq_M_times_M(mat,gauge[0],tempG3,QDP_all);
  //---
  QDP_M_eq_Ma_times_M(tempG2,gauge[0],gauge[1],QDP_all);
  QDP_M_eq_M_times_sM(tempG3,tempG2,gauge[0],QDP_neighbor[1],QDP_forward,QDP_all);
  QDP_M_eq_sM(tempG1,tempG3,QDP_neighbor[0],QDP_backward,QDP_all);
  //---
  QDP_M_meq_M(mat,tempG1,QDP_all);
  QDP_M_eq_r_times_M(mat,&oneeight,mat,QDP_all);
  // ###########################################################
  // u2
  QDP_M_eq_M_times_Ma(tempG1,mat,gauge[1],QDP_all);
  QDP_M_eq_M_minus_M(tempG2,f01,tempG1,QDP_all);
  //R:QDP_D_eq_M_times_sD(tempD1,gauge[1],source,QDP_neighbor[1],QDP_forward,QDP_all);
  // W: tempD1 = psi_up[1]-----------V
  QDP_D_eq_M_times_D(tempD2,tempG2,psi_up[1],QDP_all); // YES
  // d2
  QDP_M_eq_Ma_times_M(tempG1,mat,gauge[1],QDP_all);
  QDP_M_eq_sM(tempG2,tempG1,QDP_neighbor[1],QDP_backward,QDP_all);
  QDP_M_eq_M_plus_M(tempG1,f01,tempG2,QDP_all);
  //R:QDP_D_eq_Ma_times_D(tempD1,gauge[1],source,QDP_all);
  //R:QDP_D_eq_M_times_sD(tempD3,tempG1,tempD1,QDP_neighbor[1],QDP_backward,QDP_all);
  //W : tempD1 =psi_dw[1]
  QDP_D_eq_M_times_D(tempD3,tempG1,psi_dw[1],QDP_all); // YES
  QDP_D_peq_D(tempD2,tempD3,QDP_all);
  //u2
  QDP_M_eq_Ma_times_M(tempG1,gauge[1],mat,QDP_all);
  QDP_M_eq_sM(tempG2,tempG1,QDP_neighbor[1],QDP_backward,QDP_all);
  QDP_M_eq_M_minus_M(tempG3,f01,tempG2,QDP_all);
  QDP_M_eq_M_times_sM(tempG1,gauge[1],tempG3,QDP_neighbor[1],QDP_forward,QDP_all);
  QDP_D_eq_M_times_sD(tempD3,tempG1,source,QDP_neighbor[1],QDP_forward,QDP_all);
  QDP_D_peq_D(tempD2,tempD3,QDP_all);
  //d2
  QDP_M_eq_M_times_Ma(tempG1,gauge[1],mat,QDP_all);
  QDP_M_eq_M_plus_M(tempG2,f01,tempG1,QDP_all);
  QDP_M_eq_Ma_times_M(tempG1,gauge[1],tempG2,QDP_all);
  QDP_D_eq_M_times_D(tempD1,tempG1,source,QDP_all);
  QDP_D_eq_sD(tempD3,tempD1,QDP_neighbor[1],QDP_backward,QDP_all);
  //
  QDP_D_peq_D(tempD2,tempD3,QDP_all); // all 5-links
  QDP_D_eq_r_times_D(tempD2,&c5u5,tempD2,QDP_all);
  QDP_D_eq_gamma_times_D(tempD3,tempD2,3,QDP_all);
  QDP_D_peq_D(result,tempD3,QDP_all); // 5-links added
  //
  // u1
  QDP_D_eq_M_times_sD(tempD4,mat,source,QDP_neighbor[1],QDP_forward,QDP_all);
  //d1
  QDP_D_eq_Ma_times_D(tempD1,mat,source,QDP_all);
  QDP_D_eq_sD(tempD3,tempD1,QDP_neighbor[1],QDP_backward,QDP_all);
  QDP_D_meq_D(tempD4,tempD3,QDP_all);
  QDP_D_eq_r_times_D(tempD4,&two,tempD4,QDP_all);
  //
  QDP_D_eq_r_times_D(tempD4,&c5u3,tempD4,QDP_all);
  QDP_D_eq_gamma_times_D(tempD2,tempD4,3,QDP_all);
  QDP_D_peq_D(result,tempD2,QDP_all);

  QDP_destroy_M(f01);
#endif

#if 0
  //static QLA_DiracFermion *qladf;
  //static int st,nc,ns;
  //static QLA_Real c1,c2;
  qladf = QDP_expose_D(result);
  printf("Exposing Dirac Fermion :: result \n");  
  for(st=1;st<2;st++){
    for(nc=0;nc<3;nc++){
      for(ns=0;ns<1;ns++){
	c1=QLA_real(QLA_elem_D(qladf[st],nc,ns));
	c2=QLA_imag(QLA_elem_D(qladf[st],nc,ns));
	printf("site : %d  color : %d  spin  : %d = (%e,%e)\n",st,nc,ns,c1,c2);
      };
    };
    printf("***********************************\n");
  };
  QDP_reset_D(result);
#endif
  // ------------------------------------------------
  //  printf("Tracing my steps..... *** D-slash completed\n");

  
  


 // DESTROYING FIELDS ----------------------------------
 for(dir=0;dir<4;dir++){
 QDP_destroy_D(psi_up[dir]);
 QDP_destroy_D(psi_dw[dir]);
 QDP_destroy_M(gauge[dir]);
 };
 QDP_destroy_M(tempG1);
 QDP_destroy_M(tempG2);
 QDP_destroy_M(tempG3);
 QDP_destroy_D(tempD1);
 QDP_destroy_D(tempD2);
 QDP_destroy_D(tempD3);
 QDP_destroy_D(tempD4);
 QDP_destroy_M(mat);
 
 QDP_destroy_D(tempP);
};

#endif /* QOP_Colors == 3 */
