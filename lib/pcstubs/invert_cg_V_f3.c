#define QDP_Precision 'F'
#define QDP_Nc 3
#define QOP_Precision 'F'
#define QOP_Nc 3
#include <qop_internal.h>
#include <generic_V.h>

#ifdef HAVE_BLAS
#include "invert_cg_blas_p.c"
#else
#include "invert_cg_p.c"
#endif
