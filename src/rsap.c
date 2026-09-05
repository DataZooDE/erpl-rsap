/*
 * rsap -- SAP RFC for R, with no SAP NW RFC SDK.
 *
 * Written independently. RSAP, the abandoned predecessor, declares GPL-3 in its
 * DESCRIPTION while its C source carries "All rights reserved" and no grant at
 * all, so no code from it was read while implementing this; see NOTICE.
 *
 * Plain C against R's .Call interface -- no Rcpp, which would be a dependency
 * for very little. The C ABI on the other side is the one
 * `crates/erpl-proto-nwrfc` implements, so the same source builds against SAP's
 * real library and the differential compiles it both ways.
 */

#include "rsap.h"

/* ------------------------------------------------------------------ */
/* UTF-16 <-> R strings                                                */
/* ------------------------------------------------------------------ */

/* R's own iconv does the conversion, not the SDK's RfcUTF8ToSAPUC pair: their
 * buffer-size contract is subtle enough that getting it slightly wrong
 * corrupts the heap, which is exactly what the Ruby binding's differential
 * caught. Every other binding here converts in its own runtime too. */

SAP_UC *rsap_uc_from_r_cstr(const char *utf8, R_xlen_t *out_units)
{
    size_t bytes = strlen(utf8);
    SAP_UC *buf = (SAP_UC *)R_alloc(bytes + 1, sizeof(SAP_UC));
    size_t i = 0, n = 0;

    /* UTF-8 -> UTF-16, with astral planes as surrogate pairs. */
    while (i < bytes) {
        unsigned char c = (unsigned char)utf8[i];
        unsigned int cp;
        int extra;
        if (c < 0x80)        { cp = c;          extra = 0; }
        else if (c < 0xE0)   { cp = c & 0x1F;   extra = 1; }
        else if (c < 0xF0)   { cp = c & 0x0F;   extra = 2; }
        else                 { cp = c & 0x07;   extra = 3; }
        if (i + (size_t)extra >= bytes + 1) break;
        for (int k = 0; k < extra; k++) {
            cp = (cp << 6) | ((unsigned char)utf8[i + 1 + k] & 0x3F);
        }
        i += (size_t)extra + 1;
        if (cp >= 0x10000) {
            cp -= 0x10000;
            buf[n++] = (SAP_UC)(0xD800 + (cp >> 10));
            buf[n++] = (SAP_UC)(0xDC00 + (cp & 0x3FF));
        } else {
            buf[n++] = (SAP_UC)cp;
        }
    }
    buf[n] = 0;
    if (out_units) *out_units = (R_xlen_t)n;
    return buf;
}

SEXP rsap_r_from_uc(const SAP_UC *uc)
{
    size_t units = 0, out = 0, i;
    char *buf;
    SEXP s;

    if (uc == NULL) return mkCharCE("", CE_UTF8);
    while (uc[units] != 0) units++;
    if (units == 0) return mkCharCE("", CE_UTF8);

    buf = (char *)R_alloc(units * 4 + 1, 1);
    for (i = 0; i < units; i++) {
        unsigned int cp = uc[i];
        if (cp >= 0xD800 && cp <= 0xDBFF && i + 1 < units &&
            uc[i + 1] >= 0xDC00 && uc[i + 1] <= 0xDFFF) {
            cp = 0x10000 + ((cp - 0xD800) << 10) + (uc[i + 1] - 0xDC00);
            i++;
        }
        if (cp < 0x80) {
            buf[out++] = (char)cp;
        } else if (cp < 0x800) {
            buf[out++] = (char)(0xC0 | (cp >> 6));
            buf[out++] = (char)(0x80 | (cp & 0x3F));
        } else if (cp < 0x10000) {
            buf[out++] = (char)(0xE0 | (cp >> 12));
            buf[out++] = (char)(0x80 | ((cp >> 6) & 0x3F));
            buf[out++] = (char)(0x80 | (cp & 0x3F));
        } else {
            buf[out++] = (char)(0xF0 | (cp >> 18));
            buf[out++] = (char)(0x80 | ((cp >> 12) & 0x3F));
            buf[out++] = (char)(0x80 | ((cp >> 6) & 0x3F));
            buf[out++] = (char)(0x80 | (cp & 0x3F));
        }
    }
    buf[out] = 0;
    s = mkCharLenCE(buf, (int)out, CE_UTF8);
    return s;
}

/* ------------------------------------------------------------------ */
/* Errors                                                              */
/* ------------------------------------------------------------------ */

/* The library's own message, with its key and code attached as attributes so a
 * caller can switch on them instead of matching translated text. */
void rsap_raise(const RFC_ERROR_INFO *info, const char *doing)
{
    SEXP msg = PROTECT(ScalarString(rsap_r_from_uc(info->message)));
    const char *text = CHAR(STRING_ELT(msg, 0));
    char buffer[1024];

    if (text[0] == '\0') {
        snprintf(buffer, sizeof(buffer), "%s failed (rc %d)", doing, info->code);
    } else {
        snprintf(buffer, sizeof(buffer), "%s", text);
    }
    UNPROTECT(1);
    {
        /* A condition, not a bare message: the key and the code travel as
         * attributes so a caller can switch on them instead of matching
         * translated text. `FU_NOT_FOUND` is the same string in every
         * language; "Function module not found" is not. */
        SEXP cond, cls, msg2, callsxp;
        PROTECT(msg2 = mkString(buffer));
        PROTECT(cond = allocVector(VECSXP, 2));
        SET_VECTOR_ELT(cond, 0, msg2);
        SET_VECTOR_ELT(cond, 1, R_NilValue);
        {
            SEXP nms = PROTECT(allocVector(STRSXP, 2));
            SET_STRING_ELT(nms, 0, mkChar("message"));
            SET_STRING_ELT(nms, 1, mkChar("call"));
            setAttrib(cond, R_NamesSymbol, nms);
            UNPROTECT(1);
        }
        setAttrib(cond, install("key"), ScalarString(rsap_r_from_uc(info->key)));
        setAttrib(cond, install("code"), ScalarInteger(info->code));
        setAttrib(cond, install("group"), ScalarInteger(info->group));
        setAttrib(cond, install("abapMsgClass"), ScalarString(rsap_r_from_uc(info->abapMsgClass)));
        setAttrib(cond, install("abapMsgType"), ScalarString(rsap_r_from_uc(info->abapMsgType)));
        setAttrib(cond, install("abapMsgNumber"), ScalarString(rsap_r_from_uc(info->abapMsgNumber)));
        PROTECT(cls = allocVector(STRSXP, 3));
        SET_STRING_ELT(cls, 0, mkChar("rsap_error"));
        SET_STRING_ELT(cls, 1, mkChar("error"));
        SET_STRING_ELT(cls, 2, mkChar("condition"));
        setAttrib(cond, R_ClassSymbol, cls);
        callsxp = PROTECT(lang2(install("stop"), cond));
        eval(callsxp, R_BaseEnv);
        UNPROTECT(4);
    }
}

SAP_UC *rsap_uc_from_r(SEXP str, R_xlen_t *out_units)
{
    if (TYPEOF(str) != STRSXP || XLENGTH(str) == 0) {
        return rsap_uc_from_r_cstr("", out_units);
    }
    return rsap_uc_from_r_cstr(Rf_translateCharUTF8(STRING_ELT(str, 0)), out_units);
}

/* Anything as the text ABAP wants. Numbers go through `format` with enough
 * digits that a decimal is not rounded on the way out. */
SEXP rsap_as_text(SEXP value)
{
    SEXP call, out;
    if (TYPEOF(value) == STRSXP) return value;
    PROTECT(call = lang3(install("format"), value, ScalarLogical(FALSE)));
    SET_TAG(CDDR(call), install("scientific"));
    out = PROTECT(eval(call, R_BaseEnv));
    out = PROTECT(coerceVector(out, STRSXP));
    UNPROTECT(3);
    return out;
}

/* A Date, or eight digits already. Both are accepted; a Date is the one worth
 * encouraging, and is what this package hands back. */
SEXP rsap_as_date_text(SEXP value)
{
    SEXP call, out;
    if (!inherits(value, "Date")) return rsap_as_text(value);
    PROTECT(call = lang3(install("format"), value, mkString("%Y%m%d")));
    out = PROTECT(eval(call, R_BaseEnv));
    UNPROTECT(2);
    return out;
}

/* ------------------------------------------------------------------ */
/* Connection                                                          */
/* ------------------------------------------------------------------ */

/* An external pointer with a finaliser, not RSAP's integer-with-an-attribute.
 * A forgotten close then still ends the conversation when the object is
 * collected, rather than leaving it open at the gateway. */

static void conn_finalizer(SEXP ptr)
{
    RFC_CONNECTION_HANDLE handle = (RFC_CONNECTION_HANDLE)R_ExternalPtrAddr(ptr);
    RFC_ERROR_INFO info;
    if (handle == NULL) return;
    memset(&info, 0, sizeof(info));
    RfcCloseConnection(handle, &info);   /* best effort: a finaliser cannot fail */
    R_ClearExternalPtr(ptr);
}

static RFC_CONNECTION_HANDLE conn_of(SEXP ptr, int require_open);

RFC_CONNECTION_HANDLE rsap_conn_handle(SEXP ptr)
{
    return conn_of(ptr, 1);
}

static RFC_CONNECTION_HANDLE conn_of(SEXP ptr, int require_open)
{
    RFC_CONNECTION_HANDLE handle;
    if (TYPEOF(ptr) != EXTPTRSXP) Rf_error("not an rsap connection");
    handle = (RFC_CONNECTION_HANDLE)R_ExternalPtrAddr(ptr);
    if (require_open && handle == NULL) Rf_error("the connection is closed");
    return handle;
}

SEXP C_rsap_connect(SEXP names, SEXP values)
{
    R_xlen_t n = XLENGTH(names), i;
    RFC_CONNECTION_PARAMETER *params;
    RFC_CONNECTION_HANDLE handle;
    RFC_ERROR_INFO info;
    SEXP ptr;

    if (XLENGTH(values) != n) Rf_error("names and values differ in length");
    params = (RFC_CONNECTION_PARAMETER *)R_alloc(n > 0 ? (size_t)n : 1,
                                                 sizeof(RFC_CONNECTION_PARAMETER));
    for (i = 0; i < n; i++) {
        params[i].name = rsap_uc_from_r_cstr(Rf_translateCharUTF8(STRING_ELT(names, i)), NULL);
        params[i].value = rsap_uc_from_r_cstr(Rf_translateCharUTF8(STRING_ELT(values, i)), NULL);
    }

    memset(&info, 0, sizeof(info));
    handle = RfcOpenConnection(params, (unsigned)n, &info);
    if (handle == NULL) rsap_raise(&info, "open");

    ptr = PROTECT(R_MakeExternalPtr(handle, install("rsap_connection"), R_NilValue));
    R_RegisterCFinalizerEx(ptr, conn_finalizer, TRUE);
    UNPROTECT(1);
    return ptr;
}

SEXP C_rsap_close(SEXP ptr)
{
    RFC_CONNECTION_HANDLE handle = conn_of(ptr, 0);
    RFC_ERROR_INFO info;
    if (handle == NULL) return ScalarLogical(FALSE);   /* idempotent */
    memset(&info, 0, sizeof(info));
    R_ClearExternalPtr(ptr);
    if (RfcCloseConnection(handle, &info) != RFC_OK) rsap_raise(&info, "close");
    return ScalarLogical(TRUE);
}

SEXP C_rsap_alive(SEXP ptr)
{
    return ScalarLogical(conn_of(ptr, 0) != NULL);
}

SEXP C_rsap_ping(SEXP ptr)
{
    RFC_CONNECTION_HANDLE handle = conn_of(ptr, 1);
    RFC_ERROR_INFO info;
    memset(&info, 0, sizeof(info));
    if (RfcPing(handle, &info) != RFC_OK) rsap_raise(&info, "ping");
    return ScalarLogical(TRUE);
}

#define ATTR(field) do {                                    \
    SET_STRING_ELT(vals, k, rsap_r_from_uc(attrs.field));        \
    SET_STRING_ELT(nms, k, mkChar(#field));                 \
    k++;                                                    \
} while (0)

SEXP C_rsap_info(SEXP ptr)
{
    RFC_CONNECTION_HANDLE handle = conn_of(ptr, 1);
    RFC_ATTRIBUTES attrs;
    RFC_ERROR_INFO info;
    SEXP vals, nms, out;
    int k = 0;
    const int fields = 24;

    memset(&attrs, 0, sizeof(attrs));
    memset(&info, 0, sizeof(info));
    if (RfcGetConnectionAttributes(handle, &attrs, &info) != RFC_OK) {
        rsap_raise(&info, "attributes");
    }

    vals = PROTECT(allocVector(STRSXP, fields));
    nms = PROTECT(allocVector(STRSXP, fields));
    ATTR(dest); ATTR(host); ATTR(partnerHost); ATTR(sysNumber); ATTR(sysId);
    ATTR(client); ATTR(user); ATTR(language); ATTR(trace); ATTR(isoLanguage);
    ATTR(codepage); ATTR(partnerCodepage); ATTR(rfcRole); ATTR(type);
    ATTR(partnerType); ATTR(rel); ATTR(partnerRel); ATTR(kernelRel);
    ATTR(cpicConvId); ATTR(progName); ATTR(partnerBytesPerChar);
    ATTR(partnerSystemCodepage); ATTR(partnerIP); ATTR(partnerIPv6);

    setAttrib(vals, R_NamesSymbol, nms);
    out = PROTECT(coerceVector(vals, VECSXP));
    setAttrib(out, R_NamesSymbol, nms);
    UNPROTECT(3);
    return out;
}

/* ------------------------------------------------------------------ */

extern SEXP C_rsap_call(SEXP, SEXP, SEXP, SEXP);

static const R_CallMethodDef call_methods[] = {
    { "rsap_connect", (DL_FUNC)&C_rsap_connect, 2 },
    { "rsap_close",   (DL_FUNC)&C_rsap_close,   1 },
    { "rsap_alive",   (DL_FUNC)&C_rsap_alive,   1 },
    { "rsap_ping",    (DL_FUNC)&C_rsap_ping,    1 },
    { "rsap_info",    (DL_FUNC)&C_rsap_info,    1 },
    { "rsap_call",    (DL_FUNC)&C_rsap_call,    4 },
    { NULL, NULL, 0 }
};

void R_init_rsap(DllInfo *dll)
{
    R_registerRoutines(dll, NULL, call_methods, NULL, NULL);
    R_useDynamicSymbols(dll, FALSE);
}
