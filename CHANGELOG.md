# Changelog

All notable changes to `rsap`. Calendar versioning, `YYYY.M.D`.

## Unreleased

The first release. Nothing is published yet.

### What works

`rsap_connect` / `rsap_close` / `rsap_alive` / `rsap_ping` / `rsap_info`,
`rsap_call`, and `rsap_read_table`. 41 checks pass live against a4h, and
`examples/run_both.sh` reports **no differences across 47 lines** against SAP's
real library.

### Deliberately unlike RSAP

- **The connection is an external pointer with a finaliser**, not an integer
  carrying an attribute. A forgotten `rsap_close` still ends the conversation
  when the object is collected.
- **`DATE` is a `Date`**, not eight bare digits, and an initial date is `NA`
  rather than `"00000000"`.
- **`rsap_read_table` cuts by the offsets the server returns.** RSAP split
  `RFC_READ_TABLE`'s delimited `WA` column, so any value containing the
  delimiter silently corrupted the row and every type was lost.
- **No dependencies.** RSAP needed `yaml` and `reshape`; the latter has since
  been retired from CRAN.
- **Errors are conditions carrying the key and code as attributes**, so a
  caller switches on `FU_NOT_FOUND` rather than matching a translated message.

### A heuristic that had to go

The first `rsap_read_table` warned when a row was shorter than the sum of its
field widths, reading that as `RFC_READ_TABLE`'s 512-byte truncation. It fired
on every ordinary read: the server right-trims `WA`, so a short row is the
normal case. Truncation is the server's to report, and it does, as
`DATA_BUFFER_EXCEEDED`.

### What `R CMD check` says, and why two findings stay

Exactly two, and both are the reason CRAN is not the channel:

- **NOTE: this is not a source package** -- it bundles `libsapnwrfc.a`, which is
  the whole point.
- **WARNING: the compiled code contains `abort`** -- from the Rust staticlib.
  Worth knowing rather than hiding: a panic in the protocol implementation ends
  the R process rather than raising an R condition.

CI asserts *at most one* WARNING/ERROR, so a third finding fails the run rather
than blending in.

### Not on CRAN

CRAN forbids binary code in source packages and licences outside its approved
list, so publishing there would mean open-sourcing the protocol implementation
rather than just this wrapper. Distribution is `remotes::install_github()`,
which is to R what `go get` is to Go. RSAP was itself removed from CRAN in 2018.
