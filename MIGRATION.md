# Moving from RSAP

`rsap` is not a drop-in for [RSAP](https://github.com/piersharding/RSAP). The
API is designed for modern R rather than reproducing one whose last release was
2018 and which needs the retired `reshape` package to install at all.

The mapping is small, and everything it changes it changes deliberately.

| RSAP | rsap | why |
|---|---|---|
| `RSAPConnect(...)` | `rsap_connect(...)` | same arguments; also accepts a single named list |
| `RSAPConnect("system.yml")` | — | read the YAML yourself and pass the list. One less dependency, and the file format was never the interesting part |
| `RSAPClose(con)` | `rsap_close(con)` | idempotent; also `close(con)` |
| `RSAPValidHandle(con)` | `rsap_alive(con)` | |
| `RSAPGetInfo(con)` | `rsap_info(con)` | |
| `RSAPInvoke(con, fn, parms)` | `rsap_call(con, fn, ...)` | parameters are named arguments, not a list — though a list still works |
| `RSAPReadTable(con, tbl, ...)` | `rsap_read_table(con, tbl, ...)` | see below |
| `RSAPReadCube`, `RSAPGetCube`, `RSAPListCubes`, `RSAPExecInfoQuery` | — | thin wrappers over BW function modules; call them with `rsap_call` |

## What changed in the values

- **`DATE` is a `Date`**, not eight bare characters. An initial date is `NA`,
  not `"00000000"`.
- **`TIME` stays a string** `"HHMMSS"`. A bare time has no date to anchor a
  `POSIXct` to, and inventing today's would be inventing information.
- **`NUM` stays a string.** Its leading zeros are part of the value.
- **Decimals carry their exact text** in an `exact` attribute, because R has no
  exact decimal type and a 34-digit `DECFLOAT` does not fit a double.
- **Tables are `data.frame`s built from the typed fields**, not from splitting
  a delimited string.

## `RSAPReadTable` was subtly wrong

`RFC_READ_TABLE` returns its rows as one delimited character column (`WA`) plus
a description of where each field starts. RSAP split `WA` on the delimiter with
no escaping, so **any value containing the delimiter shifted every field after
it** — silently, producing plausible-looking wrong data.

`rsap_read_table` cuts by the offsets the server itself returns. There is
nothing to collide with, and the delimiter only ever reaches the server.

## Errors

RSAP raised plain R errors. `rsap` raises conditions of class `rsap_error`
carrying `key`, `code`, `group` and the T100 fields as attributes:

```r
err <- tryCatch(rsap_call(con, "NOPE"), error = function(e) e)
attr(err, "key")    # "FU_NOT_FOUND"
```

Switch on the key, not the message: a declared exception's message is the T100
identity rendered (`"ID:FL Type:E Number:046 …"`), and it is translated.

## Values

- **DATE and TIME** take a `Date`, or exactly eight/six ASCII digits; `NA` or
  `""` sends the initial value, so a structure read back with an initial date
  can be sent again unchanged.
- **Integers are range-checked.** INT1, INT2, INT4 and INT8 refuse a value
  outside their range, `NA`, and a fraction, instead of wrapping or
  truncating it on the way to ABAP.
- **An INT4 holding -2147483648 reads back as `NA`.** That value *is* R's
  `NA_integer_`, so R cannot represent it as an integer. Sending it works;
  read it as a decimal (`P`) field if the minimum matters.

## Installation

No SAP NW RFC SDK, no `--with-nwrfcsdk-*` flags, no `LD_LIBRARY_PATH`, and no
Perl. `remotes::install_github("DataZooDE/erpl-rsap")` compiles a few hundred
lines of C against a bundled archive.
