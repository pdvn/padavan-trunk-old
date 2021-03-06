/*
 * Please do not edit this file.
 * It was generated using rpcgen.
 */

#include "nlm_sm_inter.h"

bool_t
xdr_nlm_sm_notify (XDR *xdrs, nlm_sm_notify *objp)
{
	register int32_t *buf;

	int i;
	 if (!xdr_string (xdrs, &objp->mon_name, SM_MAXSTRLEN))
		 return FALSE;
	 if (!xdr_int (xdrs, &objp->state))
		 return FALSE;
	 if (!xdr_opaque (xdrs, objp->priv, SM_PRIV_SIZE))
		 return FALSE;
	return TRUE;
}
