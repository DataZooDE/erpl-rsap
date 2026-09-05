# Releasing rsap

## The gate

Live tests are not run in CI — there is no ABAP system there — so they are the
release gate and they are run by hand:

```bash
R CMD INSTALL --library=/tmp/rlib .
cd tests
for t in test-*.R; do
  SAP_USER=DEVELOPER SAP_PASSWD=… R_LIBS=/tmp/rlib Rscript "$t" || exit 1
done
```

Every file must report its check count. A `SKIP(live)` line here means the
credentials did not reach the tests, which is a failed gate, not a pass.

Then the differential, which is the stronger claim:

```bash
SAPNWRFC_HOME=/opt/nwrfcsdk SAP_USER=DEVELOPER SAP_PASSWD=… examples/run_both.sh
```

It must report **no differences**. It builds the same package against SAP's real
library and against ours, so a difference is the protocol implementation's.

## Packaging

```bash
scripts/build-gorfc-libs.sh          # all seven archives, from the repo root
R CMD build --no-build-vignettes .
R CMD check --no-manual rsap_*.tar.gz
```

`R CMD check` reports exactly **two** findings, both expected:

- a NOTE that this is not a source package — it bundles `libsapnwrfc.a`
- a WARNING that the compiled code contains `abort` — from the Rust staticlib

A third finding is a real one. CI asserts at most one WARNING/ERROR for exactly
that reason.

## Distribution

Not CRAN. CRAN forbids binary code in source packages and licences outside its
approved list, so publishing there would mean open-sourcing the protocol
implementation rather than this wrapper. RSAP was itself removed from CRAN in
2018; its users are on GitHub.

The public repository **is** the distribution, as it is for the Go binding:

1. Assemble `DataZooDE/erpl-rsap` with the package plus all seven archives
   under `src/lib/<platform>/`.
2. Before the first release it must carry `README.md`, `CHANGELOG.md`,
   `MIGRATION.md`, `LICENSE`, `NOTICE`, `examples/` and
   `.github/ISSUE_TEMPLATE/` — `erpl-node-rfc-issues` was created and left
   empty once, and an install link that leads to a blank page reads as
   abandoned.
3. Tag `rsap-vYYYY.M.D`.
4. Verify from a clean library, which is the only check that exercises what a
   user actually gets:

```r
remotes::install_github("DataZooDE/erpl-rsap")
library(rsap); rsap_info(rsap_connect(...))$sysId
```

Pushing the tag is the irreversible act.
