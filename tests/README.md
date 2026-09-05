# Live tests

There are no mocks. The reference's own suites talk to a real ABAP system, and
a compatibility claim proved against a fake is not a compatibility claim.

```bash
R CMD INSTALL --library=/tmp/rlib ..
SAP_USER=DEVELOPER SAP_PASSWD=… R_LIBS=/tmp/rlib Rscript test-connection.R
```

Without `SAP_USER`/`SAP_PASSWD` each file prints `SKIP(live):` and exits 0 --
and that is checked in both directions, because printing the line without
actually skipping was a real bug in the Node binding.
