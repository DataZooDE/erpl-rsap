# rsap

**SAP RFC client for R, with no SAP NW RFC SDK.**

Call ABAP function modules from R without the licence-gated SDK download, ICU,
or `LD_LIBRARY_PATH`. The RFC protocol is implemented in Rust and ships
prebuilt, so installing compiles a few hundred lines of C and nothing else.

```r
remotes::install_github("DataZooDE/erpl-rsap")
```

```r
library(rsap)

con <- rsap_connect(ashost = "localhost", sysnr = "00", client = "001",
                    user = "DEVELOPER", passwd = Sys.getenv("SAP_PASSWD"), lang = "EN")

res <- rsap_call(con, "STFC_CONNECTION", REQUTEXT = "hello")
res$ECHOTEXT

clients <- rsap_read_table(con, "T000", fields = c("MANDT", "MTEXT"))
head(clients)

rsap_close(con)
```

| | RSAP | rsap |
|---|---|---|
| SAP NW RFC SDK | required (licence-gated download) | **not needed** |
| ICU, `LD_LIBRARY_PATH` | required | **not needed** |
| Perl, for a UTF-16 literal fixup at build time | required on Linux | **not needed** |
| `reshape` (retired from CRAN) | required | **not needed** — no dependencies at all |
| Last release | 2018; removed from CRAN | active |

## Supported platforms

Prebuilt archives ship for four:

| | |
|---|---|
| Linux | x86_64, arm64 (glibc) |
| macOS | x86_64, arm64 |

**Not yet: Windows and musl/Alpine.** The archives exist -- they are built for
the Go binding -- but neither is wired into `configure` or exercised by CI, and
shipping a platform nobody has run is worse than not claiming it. If you need
one, say so in an issue.

## Not on CRAN, and why

CRAN forbids two things this package needs: *"Source packages may not contain
any form of binary executable code"* (the prebuilt archive), and licences
outside its approved list (BUSL-1.1). Getting onto CRAN would mean publishing
the whole protocol implementation as open source, not just this wrapper.

`remotes::install_github()` is to R what `go get` is to Go, and RSAP's own users
are on GitHub anyway — CRAN removed it in 2018.

## Values

| ABAP | R |
|---|---|
| `CHAR`, `STRING` | `character`, trailing blanks trimmed on fixed-width fields |
| `NUM` | `character`, leading zeros kept — they are part of the value |
| `INT1`, `INT2`, `INT4` | `integer` |
| `INT8` | `numeric` — R has no 64-bit integer |
| `FLOAT` | `numeric` |
| `BCD`, `DECFLOAT16/34` | `numeric`, with the exact text kept in an `exact` attribute |
| `DATE` | `Date` (`NA` when initial), not eight bare digits |
| `TIME` | `character` `"HHMMSS"` — a bare time has no date to anchor to |
| `BYTE`, `XSTRING` | `raw` |
| structure | named `list` |
| table | `data.frame` |

## `rsap_read_table`, done properly

`RFC_READ_TABLE` returns its rows as one delimited character column plus a
description of where each field starts. RSAP split that column on the
delimiter, so any value containing the delimiter silently corrupted the row and
every type was lost.

`rsap_read_table` cuts by the **offsets the server itself returns**, so there is
nothing to collide with.

## Errors

Conditions of class `rsap_error`, carrying the library's own key and code as
attributes — so you can switch on them rather than match translated text.

```r
err <- tryCatch(rsap_call(con, "NOPE"), error = function(e) e)
attr(err, "key")   # "FU_NOT_FOUND"
attr(err, "code")  # 5
```

The *message* of a declared exception is the T100 identity rendered
(`"ID:FL Type:E Number:046 …"`), which is why the name lives in the key.

## How correctness is established

`examples/run_both.sh` builds this package **twice** — once against our
`libsapnwrfc.a`, once against SAP's real `libsapnwrfc.so` — runs one program
under each, and diffs. The R and C source is identical on both sides, so a
difference is unambiguously in the protocol implementation. It currently
reports **no differences across 47 lines**.

## The same thing for other languages

| | Package | Replaces |
|---|---|---|
| **R** | `rsap` | the abandoned RSAP |
| **Ruby** | `erpl-rubyrfc` | the abandoned `sapnwrfc` gem |
| **Python** | [`erpl-pyrfc`](https://pypi.org/project/erpl-pyrfc/) | SAP's PyRFC |
| **Go** | [`github.com/DataZooDE/erpl-gorfc`](https://pkg.go.dev/github.com/DataZooDE/erpl-gorfc) | SAP's gorfc |
| **Node.js** | [`erpl-node-rfc`](https://www.npmjs.com/package/erpl-node-rfc) | SAP's node-rfc |

More at [erpl.io](https://erpl.io).

## Development

```bash
R CMD INSTALL --library=/tmp/rlib .
cd tests && SAP_USER=… SAP_PASSWD=… R_LIBS=/tmp/rlib Rscript test-invoke.R
```

The tests talk to a real ABAP system; there are no mocks, because a
compatibility claim proved against a fake is not one. Without
`SAP_USER`/`SAP_PASSWD` they print `SKIP(live):` and skip.

## Licence

BUSL-1.1. See [LICENSE](LICENSE) and [NOTICE](NOTICE) — no upstream code was
adapted here, and why it could not have been.
