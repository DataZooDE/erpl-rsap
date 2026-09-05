#ifndef RSAP_H
#define RSAP_H

#include <R.h>
#include <Rinternals.h>
#include <R_ext/Rdynload.h>
#include <string.h>
#include <stdlib.h>

#include "sapnwrfc.h"

/* UTF-16 <-> R. Both allocate with R_alloc, so R frees them. */
SAP_UC *rsap_uc_from_r_cstr(const char *utf8, R_xlen_t *out_units);
SAP_UC *rsap_uc_from_r(SEXP str, R_xlen_t *out_units);
SEXP rsap_r_from_uc(const SAP_UC *uc);

/* Raises an R condition carrying the library's key, code and group. */
void rsap_raise(const RFC_ERROR_INFO *info, const char *doing);

/* Coercions used by the value layer. */
SEXP rsap_as_text(SEXP value);
SEXP rsap_as_date_text(SEXP value);

/* One field, both directions. `ty` is read only for STRUCTURE and TABLE. */
SEXP rsap_field_to_r(DATA_CONTAINER_HANDLE h, const SAP_UC *name,
                     RFCTYPE type, unsigned ulen, RFC_TYPE_DESC_HANDLE ty);
void rsap_field_from_r(DATA_CONTAINER_HANDLE h, const SAP_UC *name,
                       RFCTYPE type, unsigned ulen, RFC_TYPE_DESC_HANDLE ty,
                       SEXP value);

RFC_CONNECTION_HANDLE rsap_conn_handle(SEXP ptr);

#endif
