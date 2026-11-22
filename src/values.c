/*
 * Values: ABAP containers <-> R objects, both directions.
 *
 * Driven by the metadata, never by inspecting the value: only the interface
 * says whether a list is a structure or a table, and only the field type says
 * whether eight characters are a date or a number.
 *
 * Deliberately better than RSAP's mapping in two places. DATE becomes a `Date`
 * rather than eight bare digits, and a table becomes a `data.frame` built from
 * the *typed* fields rather than by splitting a delimited string -- which is
 * what made RSAP's reader corrupt any row containing its own delimiter.
 */

#include "rsap.h"
#include <limits.h>
#include <math.h>

/* Fixed-width ABAP text is blank-padded to its declared length; the padding is
 * not data. Leading blanks are kept -- they can be significant. */
static SEXP rstrip(SEXP charsxp)
{
    const char *p = CHAR(charsxp);
    int n = (int)strlen(p);
    while (n > 0 && (p[n - 1] == ' ' || p[n - 1] == '\0')) n--;
    return mkCharLenCE(p, n, CE_UTF8);
}

static SEXP uc_slice(const SAP_UC *buf, unsigned units)
{
    SAP_UC *tmp = (SAP_UC *)R_alloc(units + 1, sizeof(SAP_UC));
    memcpy(tmp, buf, units * sizeof(SAP_UC));
    tmp[units] = 0;
    return rsap_r_from_uc(tmp);
}

/* An all-zero or all-blank ABAP date is the initial value, which is not a
 * date. NA is the honest answer; a Date would invent one. */
static int uc_initial(const SAP_UC *buf, unsigned units)
{
    unsigned i;
    for (i = 0; i < units; i++) {
        if (buf[i] != (SAP_UC)' ' && buf[i] != (SAP_UC)'0' && buf[i] != 0) return 0;
    }
    return 1;
}

static SEXP as_date(SEXP text)
{
    SEXP call, out;
    PROTECT(call = lang3(install("as.Date"), text, mkString("%Y%m%d")));
    out = PROTECT(eval(call, R_BaseEnv));
    UNPROTECT(2);
    return out;
}

static SEXP structure_to_r(RFC_STRUCTURE_HANDLE row, RFC_TYPE_DESC_HANDLE ty);
static SEXP table_to_r(RFC_TABLE_HANDLE table, RFC_TYPE_DESC_HANDLE ty);

SEXP rsap_field_to_r(DATA_CONTAINER_HANDLE h, const SAP_UC *name,
                     RFCTYPE type, unsigned ulen, RFC_TYPE_DESC_HANDLE ty)
{
    RFC_ERROR_INFO info;
    memset(&info, 0, sizeof(info));

    switch (type) {
        case RFCTYPE_CHAR: {
            unsigned chars = ulen / 2;
            SAP_UC *buf = (SAP_UC *)R_alloc(chars + 1, sizeof(SAP_UC));
            if (RfcGetChars(h, name, buf, chars, &info) != RFC_OK) {
                rsap_raise(&info, "get chars");
            }
            return ScalarString(rstrip(uc_slice(buf, chars)));
        }
        case RFCTYPE_NUM: {
            /* Leading zeros are part of the value -- a document number, not an
             * integer -- so NUM stays text. */
            unsigned chars = ulen / 2;
            SAP_UC *buf = (SAP_UC *)R_alloc(chars + 1, sizeof(SAP_UC));
            if (RfcGetNum(h, name, buf, chars, &info) != RFC_OK) {
                rsap_raise(&info, "get num");
            }
            return ScalarString(uc_slice(buf, chars));
        }
        case RFCTYPE_STRING:
        case RFCTYPE_UTCLONG: {
            unsigned len = 0, capacity, written = 0;
            SAP_UC *buf;
            if (RfcGetStringLength(h, name, &len, &info) != RFC_OK) {
                rsap_raise(&info, "string length");
            }
            capacity = len + 1;
            buf = (SAP_UC *)R_alloc(capacity, sizeof(SAP_UC));
            memset(&info, 0, sizeof(info));
            if (RfcGetString(h, name, buf, capacity, &written, &info) != RFC_OK) {
                rsap_raise(&info, "get string");
            }
            buf[written] = 0;
            return ScalarString(rsap_r_from_uc(buf));
        }
        case RFCTYPE_BCD:
        case RFCTYPE_DECF16:
        case RFCTYPE_DECF34: {
            /* Through text, then to double. R has no exact decimal, so the
             * text is kept as an attribute: a caller that needs every digit of
             * a 34-digit DECFLOAT can still read it. */
            unsigned len = 0, capacity, written = 0;
            SAP_UC *buf;
            SEXP text, out;
            if (RfcGetStringLength(h, name, &len, &info) != RFC_OK) {
                rsap_raise(&info, "decimal length");
            }
            capacity = len + 1;
            buf = (SAP_UC *)R_alloc(capacity, sizeof(SAP_UC));
            memset(&info, 0, sizeof(info));
            if (RfcGetString(h, name, buf, capacity, &written, &info) != RFC_OK) {
                rsap_raise(&info, "get decimal");
            }
            buf[written] = 0;
            text = PROTECT(ScalarString(rsap_r_from_uc(buf)));
            out = PROTECT(ScalarReal(written == 0 ? 0.0 : atof(CHAR(STRING_ELT(text, 0)))));
            setAttrib(out, install("exact"), text);
            UNPROTECT(2);
            return out;
        }
        case RFCTYPE_INT: {
            RFC_INT v = 0;
            if (RfcGetInt(h, name, &v, &info) != RFC_OK) rsap_raise(&info, "get int");
            return ScalarInteger((int)v);
        }
        case RFCTYPE_INT1: {
            RFC_INT1 v = 0;
            if (RfcGetInt1(h, name, &v, &info) != RFC_OK) rsap_raise(&info, "get int1");
            return ScalarInteger((int)v);
        }
        case RFCTYPE_INT2: {
            RFC_INT2 v = 0;
            if (RfcGetInt2(h, name, &v, &info) != RFC_OK) rsap_raise(&info, "get int2");
            return ScalarInteger((int)v);
        }
        case RFCTYPE_INT8: {
            /* R has no 64-bit integer, so a double -- exact to 2^53, which is
             * where R itself stops being exact too. */
            RFC_INT8 v = 0;
            if (RfcGetInt8(h, name, &v, &info) != RFC_OK) rsap_raise(&info, "get int8");
            return ScalarReal((double)v);
        }
        case RFCTYPE_FLOAT: {
            RFC_FLOAT v = 0;
            if (RfcGetFloat(h, name, &v, &info) != RFC_OK) rsap_raise(&info, "get float");
            return ScalarReal(v);
        }
        case RFCTYPE_DATE: {
            SAP_UC buf[9];
            SEXP text;
            memset(buf, 0, sizeof(buf));
            if (RfcGetDate(h, name, buf, &info) != RFC_OK) rsap_raise(&info, "get date");
            if (uc_initial(buf, 8)) {
                SEXP na = PROTECT(ScalarReal(NA_REAL));
                setAttrib(na, R_ClassSymbol, mkString("Date"));
                UNPROTECT(1);
                return na;
            }
            text = PROTECT(ScalarString(uc_slice(buf, 8)));
            { SEXP d = as_date(text); UNPROTECT(1); return d; }
        }
        case RFCTYPE_TIME: {
            /* HHMMSS. A bare time has no date to anchor a POSIXct to, and
             * inventing today's would be inventing information. */
            SAP_UC buf[7];
            memset(buf, 0, sizeof(buf));
            if (RfcGetTime(h, name, buf, &info) != RFC_OK) rsap_raise(&info, "get time");
            return ScalarString(uc_slice(buf, 6));
        }
        case RFCTYPE_BYTE: {
            SAP_RAW *buf = (SAP_RAW *)R_alloc(ulen + 1, 1);
            SEXP out;
            if (RfcGetBytes(h, name, buf, ulen, &info) != RFC_OK) {
                rsap_raise(&info, "get bytes");
            }
            out = PROTECT(allocVector(RAWSXP, ulen));
            memcpy(RAW(out), buf, ulen);
            UNPROTECT(1);
            return out;
        }
        case RFCTYPE_XSTRING: {
            unsigned len = 0, written = 0;
            SAP_RAW *buf;
            SEXP out;
            if (RfcGetStringLength(h, name, &len, &info) != RFC_OK) {
                rsap_raise(&info, "xstring length");
            }
            buf = (SAP_RAW *)R_alloc(len + 1, 1);
            memset(&info, 0, sizeof(info));
            if (RfcGetXString(h, name, buf, len, &written, &info) != RFC_OK) {
                rsap_raise(&info, "get xstring");
            }
            out = PROTECT(allocVector(RAWSXP, written));
            memcpy(RAW(out), buf, written);
            UNPROTECT(1);
            return out;
        }
        case RFCTYPE_STRUCTURE: {
            RFC_STRUCTURE_HANDLE sub = NULL;
            if (RfcGetStructure(h, name, &sub, &info) != RFC_OK) {
                rsap_raise(&info, "get structure");
            }
            return structure_to_r(sub, ty);
        }
        case RFCTYPE_TABLE: {
            RFC_TABLE_HANDLE sub = NULL;
            if (RfcGetTable(h, name, &sub, &info) != RFC_OK) {
                rsap_raise(&info, "get table");
            }
            return table_to_r(sub, ty);
        }
        default:
            Rf_error("unsupported ABAP type %d", (int)type);
    }
    return R_NilValue;
}

static SEXP structure_to_r(RFC_STRUCTURE_HANDLE row, RFC_TYPE_DESC_HANDLE ty)
{
    RFC_ERROR_INFO info;
    unsigned count = 0, i;
    SEXP out, nms;

    memset(&info, 0, sizeof(info));
    if (RfcGetFieldCount(ty, &count, &info) != RFC_OK) rsap_raise(&info, "field count");

    out = PROTECT(allocVector(VECSXP, count));
    nms = PROTECT(allocVector(STRSXP, count));
    for (i = 0; i < count; i++) {
        RFC_FIELD_DESC fd;
        memset(&fd, 0, sizeof(fd));
        memset(&info, 0, sizeof(info));
        if (RfcGetFieldDescByIndex(ty, i, &fd, &info) != RFC_OK) {
            rsap_raise(&info, "field description");
        }
        SET_STRING_ELT(nms, i, rsap_r_from_uc(fd.name));
        SET_VECTOR_ELT(out, i,
                       rsap_field_to_r(row, fd.name, fd.type, fd.ucLength,
                                       fd.typeDescHandle));
    }
    setAttrib(out, R_NamesSymbol, nms);
    UNPROTECT(2);
    return out;
}

/* A data.frame built column-wise from the typed fields.
 *
 * RSAP produced its data.frame by splitting RFC_READ_TABLE's delimited `WA`
 * column, which corrupts any row containing the delimiter and loses every
 * type. Reading the fields directly has neither problem. */
static SEXP table_to_r(RFC_TABLE_HANDLE table, RFC_TYPE_DESC_HANDLE ty)
{
    RFC_ERROR_INFO info;
    unsigned rows = 0, count = 0, i, c;
    SEXP out, nms, rownames;

    memset(&info, 0, sizeof(info));
    if (RfcGetRowCount(table, &rows, &info) != RFC_OK) rsap_raise(&info, "row count");
    memset(&info, 0, sizeof(info));
    if (RfcGetFieldCount(ty, &count, &info) != RFC_OK) rsap_raise(&info, "field count");

    out = PROTECT(allocVector(VECSXP, count));
    nms = PROTECT(allocVector(STRSXP, count));

    for (c = 0; c < count; c++) {
        RFC_FIELD_DESC fd;
        SEXP column;
        memset(&fd, 0, sizeof(fd));
        memset(&info, 0, sizeof(info));
        if (RfcGetFieldDescByIndex(ty, c, &fd, &info) != RFC_OK) {
            rsap_raise(&info, "field description");
        }
        SET_STRING_ELT(nms, c, rsap_r_from_uc(fd.name));

        column = PROTECT(allocVector(VECSXP, rows));
        for (i = 0; i < rows; i++) {
            RFC_STRUCTURE_HANDLE row;
            memset(&info, 0, sizeof(info));
            if (RfcMoveTo(table, i, &info) != RFC_OK) rsap_raise(&info, "move to row");
            row = RfcGetCurrentRow(table, &info);
            if (row == NULL) rsap_raise(&info, "current row");
            SET_VECTOR_ELT(column, i,
                           rsap_field_to_r(row, fd.name, fd.type, fd.ucLength,
                                           fd.typeDescHandle));
        }
        /* Each column is simplified to an atomic vector where every cell is a
         * length-one scalar of one type, which is every case but a nested
         * table or structure. */
        {
            SEXP call = PROTECT(lang3(install("unlist"), column, ScalarLogical(FALSE)));
            SET_VECTOR_ELT(out, c, eval(call, R_BaseEnv));
            UNPROTECT(1);
        }
        UNPROTECT(1);
    }

    rownames = PROTECT(allocVector(INTSXP, rows));
    for (i = 0; i < rows; i++) INTEGER(rownames)[i] = (int)(i + 1);

    setAttrib(out, R_NamesSymbol, nms);
    setAttrib(out, R_RowNamesSymbol, rownames);
    setAttrib(out, R_ClassSymbol, mkString("data.frame"));
    UNPROTECT(3);
    return out;
}

/* ------------------------------------------------------------------ */
/* R -> container                                                      */
/* ------------------------------------------------------------------ */

static void structure_from_r(RFC_STRUCTURE_HANDLE row, RFC_TYPE_DESC_HANDLE ty, SEXP value);

static void table_from_r(RFC_TABLE_HANDLE table, RFC_TYPE_DESC_HANDLE ty, SEXP value)
{
    RFC_ERROR_INFO info;
    R_xlen_t n, i;

    if (!isVectorList(value)) {
        Rf_error("a table parameter needs a list of rows or a data.frame");
    }
    if (inherits(value, "data.frame")) {
        /* A data.frame is a list of columns; rows are read across them. */
        R_xlen_t cols = XLENGTH(value);
        R_xlen_t rows = cols > 0 ? XLENGTH(VECTOR_ELT(value, 0)) : 0;
        SEXP nms = getAttrib(value, R_NamesSymbol);
        for (i = 0; i < rows; i++) {
            RFC_STRUCTURE_HANDLE row;
            R_xlen_t c;
            SEXP one = PROTECT(allocVector(VECSXP, cols));
            for (c = 0; c < cols; c++) {
                SEXP col = VECTOR_ELT(value, c);
                SET_VECTOR_ELT(one, c, isVectorList(col)
                                           ? VECTOR_ELT(col, i)
                                           : Rf_lengthgets(col, 0) /* placeholder */);
                if (!isVectorList(col)) {
                    /* Take element i of an atomic column. */
                    SEXP call = PROTECT(lang3(install("[["), col, ScalarInteger((int)i + 1)));
                    SET_VECTOR_ELT(one, c, eval(call, R_BaseEnv));
                    UNPROTECT(1);
                }
            }
            setAttrib(one, R_NamesSymbol, nms);
            memset(&info, 0, sizeof(info));
            row = RfcAppendNewRow(table, &info);
            if (row == NULL) rsap_raise(&info, "append row");
            structure_from_r(row, ty, one);
            UNPROTECT(1);
        }
        return;
    }

    n = XLENGTH(value);
    for (i = 0; i < n; i++) {
        RFC_STRUCTURE_HANDLE row;
        memset(&info, 0, sizeof(info));
        row = RfcAppendNewRow(table, &info);
        if (row == NULL) rsap_raise(&info, "append row");
        structure_from_r(row, ty, VECTOR_ELT(value, i));
    }
}

/* One element of an R vector as a scalar, so a caller may pass either a
 * length-one vector or a list element. */
static SEXP scalar_of(SEXP value)
{
    if (isVectorList(value) && XLENGTH(value) == 1) return VECTOR_ELT(value, 0);
    return value;
}

/* RfcSetDate and RfcSetTime read a fixed eight and six UTF-16 units with no
 * length of their own, so the width is checked here, in units: strlen counted
 * UTF-8 bytes, and "\u00e4\u00e4\u00e4\u00e4" is eight bytes but four
 * units, so the library read past the converted text and sent whatever
 * followed it. Only ASCII digits are accepted, which makes bytes and units
 * agree by construction. NULL when the text is not `width` digits; the caller
 * raises, after releasing what it protected -- this function leaves the
 * protect stack alone. The buffer is R_alloc'd and released with the call. */
static SAP_UC *rsap_digits(SEXP text, R_xlen_t width)
{
    R_xlen_t units = 0, i;
    SAP_UC *uc;
    if (TYPEOF(text) != STRSXP || XLENGTH(text) < 1 || STRING_ELT(text, 0) == NA_STRING) return NULL;
    uc = rsap_uc_from_r(text, &units);
    if (units != width) return NULL;
    for (i = 0; i < width; i++) {
        if (uc[i] < '0' || uc[i] > '9') return NULL;
    }
    return uc;
}

/* Whether a DATE or TIME value means "initial": NA of any type, or "". An
 * initial DATE is read back as NA, so a structure read from the system must
 * be sendable again without the caller rewriting its dates. */
static int rsap_is_initial(SEXP value)
{
    if (XLENGTH(value) < 1) return 1;
    switch (TYPEOF(value)) {
        case STRSXP:  return STRING_ELT(value, 0) == NA_STRING || CHAR(STRING_ELT(value, 0))[0] == 0;
        case LGLSXP:  return LOGICAL(value)[0] == NA_LOGICAL;
        case INTSXP:  return INTEGER(value)[0] == NA_INTEGER;
        case REALSXP: return ISNAN(REAL(value)[0]);
        default:      return 0;
    }
}

static const SAP_UC initial_digits[9] = { '0', '0', '0', '0', '0', '0', '0', '0', 0 };

/* An integer for a field of the given range, refused before it is narrowed:
 * `(RFC_INT1)256` is 0, sent without a word. NA, non-finite and fractional
 * values are refused too, rather than becoming whatever asInteger makes of
 * them. The range is checked on the double *before* any cast: converting a
 * double outside the target type's range (1e100 to a long) is undefined
 * behaviour in C, not a refusal. `hi_excl` is one past the maximum, so the
 * INT8 bound, 2^63, is exact as a double. */
static long long rsap_int_in_range(SEXP value, double lo, double hi_excl, const char *type)
{
    double d;
    if (XLENGTH(value) < 1) Rf_error("an %s field needs a number", type);
    d = asReal(value);
    if (ISNAN(d) || !R_FINITE(d)) Rf_error("an %s field cannot be NA or infinite", type);
    if (d < lo || d >= hi_excl) {
        Rf_error("%g is out of range for %s (%.0f..%.0f)", d, type, lo, hi_excl - 1);
    }
    if (trunc(d) != d) Rf_error("an %s field needs a whole number, got %g", type, d);
    return (long long)d;
}

void rsap_field_from_r(DATA_CONTAINER_HANDLE h, const SAP_UC *name,
                       RFCTYPE type, unsigned ulen, RFC_TYPE_DESC_HANDLE ty,
                       SEXP value)
{
    RFC_ERROR_INFO info;
    memset(&info, 0, sizeof(info));

    if (isNull(value)) return;   /* leave the field initial */
    /* Only for scalars. A structure with one field is a length-one list, and
     * unwrapping it here threw its names away -- the field then arrived as a
     * bare value and the structure looked unnamed. */
    if (type != RFCTYPE_STRUCTURE && type != RFCTYPE_TABLE) {
        value = scalar_of(value);
    }

    switch (type) {
        case RFCTYPE_CHAR:
        case RFCTYPE_NUM: {
            R_xlen_t units = 0;
            SAP_UC *uc = rsap_uc_from_r(rsap_as_text(value), &units);
            RFC_RC rc = (type == RFCTYPE_CHAR)
                ? RfcSetChars(h, name, uc, (unsigned)units, &info)
                : RfcSetNum(h, name, uc, (unsigned)units, &info);
            if (rc != RFC_OK) rsap_raise(&info, "set chars");
            return;
        }
        case RFCTYPE_STRING:
        case RFCTYPE_UTCLONG:
        case RFCTYPE_BCD:
        case RFCTYPE_DECF16:
        case RFCTYPE_DECF34: {
            /* Decimals go out as text for the same reason they come back with
             * their text kept: a double would round what ABAP stores exactly. */
            R_xlen_t units = 0;
            SAP_UC *uc = rsap_uc_from_r(rsap_as_text(value), &units);
            if (RfcSetString(h, name, uc, (unsigned)units, &info) != RFC_OK) {
                rsap_raise(&info, "set string");
            }
            return;
        }
        case RFCTYPE_INT:
            if (RfcSetInt(h, name, (RFC_INT)rsap_int_in_range(value, -2147483648.0, 2147483648.0, "INT4"), &info) != RFC_OK) {
                rsap_raise(&info, "set int");
            }
            return;
        case RFCTYPE_INT1:
            /* Checked before narrowing: a cast would send 256 as 0, silently. */
            if (RfcSetInt1(h, name, (RFC_INT1)rsap_int_in_range(value, 0.0, 256.0, "INT1"), &info) != RFC_OK) {
                rsap_raise(&info, "set int1");
            }
            return;
        case RFCTYPE_INT2:
            if (RfcSetInt2(h, name, (RFC_INT2)rsap_int_in_range(value, -32768.0, 32768.0, "INT2"), &info) != RFC_OK) {
                rsap_raise(&info, "set int2");
            }
            return;
        case RFCTYPE_INT8:
            if (RfcSetInt8(h, name, (RFC_INT8)rsap_int_in_range(value, -9223372036854775808.0, 9223372036854775808.0, "INT8"), &info) != RFC_OK) {
                rsap_raise(&info, "set int8");
            }
            return;
        case RFCTYPE_FLOAT:
            if (RfcSetFloat(h, name, (RFC_FLOAT)asReal(value), &info) != RFC_OK) {
                rsap_raise(&info, "set float");
            }
            return;
        case RFCTYPE_DATE: {
            const SAP_UC *uc = initial_digits;
            RFC_RC rc;
            if (!rsap_is_initial(value)) {
                SEXP text = PROTECT(rsap_as_date_text(value));
                uc = rsap_digits(text, 8);
                UNPROTECT(1);
                if (uc == NULL) Rf_error("a DATE needs a Date, eight digits, NA or \"\"");
            }
            rc = RfcSetDate(h, name, uc, &info);
            if (rc != RFC_OK) rsap_raise(&info, "set date");
            return;
        }
        case RFCTYPE_TIME: {
            const SAP_UC *uc = initial_digits;
            RFC_RC rc;
            if (!rsap_is_initial(value)) {
                SEXP text = PROTECT(rsap_as_text(value));
                uc = rsap_digits(text, 6);
                UNPROTECT(1);
                if (uc == NULL) Rf_error("a TIME needs six digits, NA or \"\"");
            }
            rc = RfcSetTime(h, name, uc, &info);
            if (rc != RFC_OK) rsap_raise(&info, "set time");
            return;
        }
        case RFCTYPE_BYTE:
        case RFCTYPE_XSTRING: {
            if (TYPEOF(value) != RAWSXP) Rf_error("a binary field needs a raw vector");
            if (type == RFCTYPE_BYTE) {
                if (RfcSetBytes(h, name, RAW(value), (unsigned)XLENGTH(value), &info) != RFC_OK) {
                    rsap_raise(&info, "set bytes");
                }
            } else if (RfcSetXString(h, name, RAW(value), (unsigned)XLENGTH(value), &info) != RFC_OK) {
                rsap_raise(&info, "set xstring");
            }
            return;
        }
        case RFCTYPE_STRUCTURE: {
            RFC_STRUCTURE_HANDLE sub = NULL;
            if (RfcGetStructure(h, name, &sub, &info) != RFC_OK) {
                rsap_raise(&info, "structure for writing");
            }
            structure_from_r(sub, ty, value);
            return;
        }
        case RFCTYPE_TABLE: {
            RFC_TABLE_HANDLE sub = NULL;
            if (RfcGetTable(h, name, &sub, &info) != RFC_OK) {
                rsap_raise(&info, "table for writing");
            }
            table_from_r(sub, ty, value);
            return;
        }
        default:
            Rf_error("unsupported ABAP type %d", (int)type);
    }
    (void)ulen;
}

/* A name the structure does not declare is an error, not a silent no-op: a
 * typo would otherwise send an empty structure and look like a server fault. */
static void structure_from_r(RFC_STRUCTURE_HANDLE row, RFC_TYPE_DESC_HANDLE ty, SEXP value)
{
    RFC_ERROR_INFO info;
    unsigned count = 0, i;
    SEXP nms;
    R_xlen_t k;

    if (!isVectorList(value)) Rf_error("a structure parameter needs a named list");
    nms = getAttrib(value, R_NamesSymbol);
    if (isNull(nms)) Rf_error("a structure parameter needs a *named* list");

    memset(&info, 0, sizeof(info));
    if (RfcGetFieldCount(ty, &count, &info) != RFC_OK) rsap_raise(&info, "field count");

    for (k = 0; k < XLENGTH(value); k++) {
        const char *wanted = Rf_translateCharUTF8(STRING_ELT(nms, k));
        int found = 0;
        for (i = 0; i < count; i++) {
            RFC_FIELD_DESC fd;
            memset(&fd, 0, sizeof(fd));
            memset(&info, 0, sizeof(info));
            if (RfcGetFieldDescByIndex(ty, i, &fd, &info) != RFC_OK) {
                rsap_raise(&info, "field description");
            }
            if (strcmp(CHAR(rsap_r_from_uc(fd.name)), wanted) == 0) {
                rsap_field_from_r(row, fd.name, fd.type, fd.ucLength,
                                  fd.typeDescHandle, VECTOR_ELT(value, k));
                found = 1;
                break;
            }
        }
        if (!found) Rf_error("no field %s in this structure", wanted);
    }
}
