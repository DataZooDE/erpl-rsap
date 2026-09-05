library(rsap)
source("live.R")

run_suite("invoke", function() {
  conn <- do.call(rsap_connect, live_params())
  on.exit(rsap_close(conn))

  r <- rsap_call(conn, "STFC_CONNECTION", REQUTEXT = "hello R")
  expect(startsWith(r$ECHOTEXT, "hello R"), "a scalar round-trips")
  expect(grepl("SAP", r$RESPTEXT), "the system answers with its own text")

  s <- rsap_call(conn, "STFC_STRUCTURE",
                 IMPORTSTRUCT = list(RFCCHAR4 = "abcd", RFCINT4 = 42L))
  expect(is.list(s$ECHOSTRUCT), "a structure comes back as a list")
  expect_equal(s$ECHOSTRUCT$RFCCHAR4, "abcd", "its char field")
  expect_equal(s$ECHOSTRUCT$RFCINT4, 42L, "its integer field")

  rows <- list(list(RFCCHAR4 = "one", RFCINT4 = 1L),
               list(RFCCHAR4 = "two", RFCINT4 = 2L))
  t <- rsap_call(conn, "STFC_STRUCTURE",
                 IMPORTSTRUCT = list(RFCCHAR4 = "x"), RFCTABLE = rows)
  expect(is.data.frame(t$RFCTABLE), "a table comes back as a data.frame")
  expect(nrow(t$RFCTABLE) >= 2L, "with the rows we sent")
  expect(all(c(1L, 2L) %in% t$RFCTABLE$RFCINT4), "carrying their values")

  # One below each maximum: STFC_STRUCTURE's ABAP increments the integer fields
  # when it builds RESPSTRUCT, so the maximum of each type overflows on the
  # server. SAP's own library gives byte-identical errors on a4h.
  w <- rsap_call(conn, "STFC_STRUCTURE",
                 IMPORTSTRUCT = list(RFCINT1 = 254L, RFCINT2 = 32766L,
                                     RFCINT4 = 2147483646L))
  expect_equal(w$ECHOSTRUCT$RFCINT1, 254L, "INT1 round-trips")
  expect_equal(w$ECHOSTRUCT$RFCINT2, 32766L, "INT2 round-trips")
  expect_equal(w$ECHOSTRUCT$RFCINT4, 2147483646L, "INT4 round-trips")

  f <- rsap_call(conn, "STFC_STRUCTURE", IMPORTSTRUCT = list(RFCFLOAT = 1.25))
  expect(abs(f$ECHOSTRUCT$RFCFLOAT - 1.25) < 1e-9, "a float round-trips")

  d <- rsap_call(conn, "STFC_STRUCTURE",
                 IMPORTSTRUCT = list(RFCDATE = as.Date("2026-09-05"),
                                     RFCTIME = "123456"))
  expect_equal(d$ECHOSTRUCT$RFCDATE, as.Date("2026-09-05"), "DATE is a Date")
  expect_equal(d$ECHOSTRUCT$RFCTIME, "123456", "TIME stays text")

  expect_error(rsap_call(conn, "STFC_CONNECTION", NOT_A_PARAMETER = "x"),
               "NOT_A_PARAMETER", "an undeclared parameter is refused")

  # The *message* of a declared exception is the T100 identity rendered
  # ("ID:FL Type:E Number:046 ..."), so the name to switch on is in the key --
  # which is the whole reason the key travels as its own attribute.
  nf <- tryCatch(rsap_call(conn, "Z_NO_SUCH_FUNCTION_MODULE_AT_ALL"),
                 error = function(e) e)
  expect(inherits(nf, "rsap_error"), "an unknown function module raises")
  expect_equal(attr(nf, "key"), "FU_NOT_FOUND", "carrying FU_NOT_FOUND as the key")

  # Spec PRO-12: a declared exception leaves the conversation usable.
  err <- tryCatch(rsap_call(conn, "RFC_RAISE_ERROR", METHOD = "1", MESSAGETYPE = "E"),
                  error = function(e) e)
  expect(inherits(err, "error"), "a declared exception raises")
  expect_equal(attr(err, "key"), "RAISE_EXCEPTION", "carrying its key")
  again <- rsap_call(conn, "STFC_CONNECTION", REQUTEXT = "still here")
  expect(startsWith(again$ECHOTEXT, "still here"),
         "and the connection still works")
})
