library(rsap)
source("live.R")

run_suite("read_table", function() {
  conn <- do.call(rsap_connect, live_params())
  on.exit(rsap_close(conn))

  df <- rsap_read_table(conn, "T000", fields = c("MANDT", "MTEXT"), max_rows = 5)
  expect(is.data.frame(df), "it returns a data.frame")
  expect_equal(names(df), c("MANDT", "MTEXT"), "with the requested columns")
  expect(nrow(df) > 0L, "and some rows")
  expect(is.character(df$MANDT), "text columns are character")
  expect("000" %in% df$MANDT || "001" %in% df$MANDT, "carrying real clients")

  all_cols <- rsap_read_table(conn, "T000", max_rows = 1)
  expect(ncol(all_cols) > 2L, "asking for no fields returns them all")

  few <- rsap_read_table(conn, "T000", max_rows = 2)
  expect(nrow(few) <= 2L, "max_rows is respected")

  # RSAP split RFC_READ_TABLE's delimited WA column with no escaping, so any
  # value containing the delimiter silently corrupted the row. This asks the
  # server for the field boundaries instead, so there is no delimiter to
  # collide with -- and a delimiter that would collide is refused outright.
  expect_error(rsap_read_table(conn, "T000", delimiter = ""),
               "delimiter", "an empty delimiter is refused")

  # As for an unknown function module: the message is the T100 identity
  # rendered, so the name to switch on lives in the key.
  nt <- tryCatch(rsap_read_table(conn, "NO_SUCH_TABLE_AT_ALL"),
                 error = function(e) e)
  expect(inherits(nt, "rsap_error"), "an unknown table raises")
  expect_equal(attr(nt, "key"), "TABLE_NOT_AVAILABLE",
               "carrying TABLE_NOT_AVAILABLE as the key")

  # Spec PRO-12 again: TABLE_NOT_AVAILABLE is a declared exception, so the
  # conversation survives it.
  again <- rsap_read_table(conn, "T000", fields = "MANDT", max_rows = 1)
  expect(nrow(again) >= 1L, "the connection still works afterwards")
})
