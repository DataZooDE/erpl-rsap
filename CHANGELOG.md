# Changelog

All notable changes to `rsap`. Calendar versioning, `YYYY.M.D`.

## 2026.11.22

Built on the current shim. `examples/run_both.sh` against SAP's library: **no
differences across 47 lines**; 69 live checks pass against a4h (kernel 816).

- **An SAP system with an instance number other than 00 is reached.** The
  September build announced instance 00 whatever was configured, so the gateway
  refused the partner or, on a host that also runs an instance 00, logged on to
  the wrong system.
- **A call longer than 60 seconds no longer fails** and drops the connection;
  SAP's library completes it. Logon stays bounded at 60 s.
- **The connection lives exactly as long as with SAP's library**, measured: a
  `MESSAGE`-raised failure ends it, a declared exception does not, a short dump
  is found on the next call. Short dumps are reported as runtime failures
  (code 3), not as messages.
- **A signal during a socket read no longer costs the connection.**
- `DESTINATION 'BACK'` callbacks, `RfcCancel`, and `RFC_BUFFER_TOO_SMALL` with
  the required length, as SAP's library reports it.
- Integers are range-checked before any conversion (`1e100` was undefined
  behaviour), and `NA`, fractions and out-of-range values are refused instead
  of wrapping. An INT4 of -2147483648 reads back as `NA` (it *is* R's
  `NA_integer_`; see MIGRATION.md).
- `DATE`/`TIME` accept exactly eight/six ASCII digits; `NA` or `""` sends the
  initial value, so a structure read back with an initial date goes back
  unchanged.
- A value refused while converting no longer leaks the call's function
  container (measured: 17.8 MB over 20,000 refusals before, none after).

## 2026.9.5

The first release.

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
