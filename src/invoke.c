/*
 * The call: rsap_call(con, "STFC_CONNECTION", REQUTEXT = "hi") -> named list
 * of the exporting, changing and table parameters.
 */

#include "rsap.h"

/* Directions, from the header. */
#define RSAP_IMPORT 1

/* Everything the call allocated, released however it ends -- including an
 * R error, which longjmps past any cleanup written after the call site. */
typedef struct {
    RFC_FUNCTION_HANDLE func;
} call_state;

static void call_cleanup(void *p)
{
    call_state *st = (call_state *)p;
    RFC_ERROR_INFO info;
    if (st->func) {
        memset(&info, 0, sizeof(info));
        RfcDestroyFunction(st->func, &info);
        st->func = NULL;
    }
}

SEXP C_rsap_call(SEXP ptr, SEXP name, SEXP names, SEXP values)
{
    RFC_CONNECTION_HANDLE conn = rsap_conn_handle(ptr);
    RFC_FUNCTION_DESC_HANDLE desc;
    RFC_ERROR_INFO info;
    SAP_UC *fname;
    unsigned count = 0, i;
    R_xlen_t k, given;
    call_state st = { NULL };
    SEXP out, nms;
    int produced = 0;

    memset(&info, 0, sizeof(info));
    fname = rsap_uc_from_r(name, NULL);
    desc = RfcGetFunctionDesc(conn, fname, &info);
    if (desc == NULL) rsap_raise(&info, "function description");

    memset(&info, 0, sizeof(info));
    st.func = RfcCreateFunction(desc, &info);
    if (st.func == NULL) rsap_raise(&info, "create function");
    memset(&info, 0, sizeof(info));
    if (RfcGetParameterCount(desc, &count, &info) != RFC_OK) {
        call_cleanup(&st);
        rsap_raise(&info, "parameter count");
    }

    /* Inputs. A name the interface does not declare is refused rather than
     * ignored: a typo would otherwise send an empty call and look like a
     * server-side problem. */
    given = isNull(names) ? 0 : XLENGTH(names);
    for (k = 0; k < given; k++) {
        const char *wanted = Rf_translateCharUTF8(STRING_ELT(names, k));
        int found = 0;
        for (i = 0; i < count; i++) {
            RFC_PARAMETER_DESC pd;
            memset(&pd, 0, sizeof(pd));
            memset(&info, 0, sizeof(info));
            if (RfcGetParameterDescByIndex(desc, i, &pd, &info) != RFC_OK) {
                call_cleanup(&st);
                rsap_raise(&info, "parameter description");
            }
            if (strcmp(CHAR(rsap_r_from_uc(pd.name)), wanted) == 0) {
                rsap_field_from_r(st.func, pd.name, pd.type, pd.ucLength,
                                  pd.typeDescHandle, VECTOR_ELT(values, k));
                found = 1;
                break;
            }
        }
        if (!found) {
            char msg[256];
            snprintf(msg, sizeof(msg), "%s has no parameter %s",
                     CHAR(STRING_ELT(name, 0)), wanted);
            call_cleanup(&st);
            Rf_error("%s", msg);
        }
    }

    memset(&info, 0, sizeof(info));
    if (RfcInvoke(conn, st.func, &info) != RFC_OK) {
        RFC_ERROR_INFO failure = info;
        call_cleanup(&st);
        rsap_raise(&failure, "invoke");
    }

    /* Outputs: what the call produced, not what the caller supplied. An IMPORT
     * parameter is the caller's own input echoed back, so it is left out --
     * every other binding here does the same. */
    out = PROTECT(allocVector(VECSXP, count));
    nms = PROTECT(allocVector(STRSXP, count));
    for (i = 0; i < count; i++) {
        RFC_PARAMETER_DESC pd;
        memset(&pd, 0, sizeof(pd));
        memset(&info, 0, sizeof(info));
        if (RfcGetParameterDescByIndex(desc, i, &pd, &info) != RFC_OK) continue;
        if (pd.direction == RSAP_IMPORT) continue;
        SET_STRING_ELT(nms, produced, rsap_r_from_uc(pd.name));
        SET_VECTOR_ELT(out, produced,
                       rsap_field_to_r(st.func, pd.name, pd.type, pd.ucLength,
                                       pd.typeDescHandle));
        produced++;
    }
    /* Rebuilt at the produced size rather than truncated in place: SETLENGTH
     * is not part of R's public API and lies to the garbage collector. */
    {
        SEXP trimmed = PROTECT(allocVector(VECSXP, produced));
        SEXP tnms = PROTECT(allocVector(STRSXP, produced));
        for (i = 0; i < (unsigned)produced; i++) {
            SET_VECTOR_ELT(trimmed, i, VECTOR_ELT(out, i));
            SET_STRING_ELT(tnms, i, STRING_ELT(nms, i));
        }
        setAttrib(trimmed, R_NamesSymbol, tnms);
        call_cleanup(&st);
        UNPROTECT(4);
        return trimmed;
    }
}
