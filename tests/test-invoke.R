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

  # RfcSetDate/RfcSetTime read a fixed eight/six UTF-16 units. "\u00e4" x 4 is
  # eight UTF-8 bytes but four units; the byte-counting guard let it through
  # and the library read past the converted text.
  expect_error(rsap_call(conn, "STFC_STRUCTURE",
                         IMPORTSTRUCT = list(RFCDATE = "\u00e4\u00e4\u00e4\u00e4")),
               "eight digits", "a DATE of eight bytes but four characters is refused")
  expect_error(rsap_call(conn, "STFC_STRUCTURE",
                         IMPORTSTRUCT = list(RFCTIME = "\u00e4\u00e4\u00e4")),
               "six digits", "a TIME of six bytes but three characters is refused")
  expect_error(rsap_call(conn, "STFC_STRUCTURE", IMPORTSTRUCT = list(RFCDATE = "2026O905")),
               "eight digits", "a DATE that is not digits is refused")

  # Integers are range-checked before they are narrowed: a cast sent 256 as 0.
  edge <- rsap_call(conn, "STFC_STRUCTURE",
                    IMPORTSTRUCT = list(RFCINT1 = 0L, RFCINT2 = -32768L))
  expect_equal(edge$ECHOSTRUCT$RFCINT1, 0L, "INT1 accepts its minimum")
  expect_equal(edge$ECHOSTRUCT$RFCINT2, -32768L, "INT2 accepts its minimum")
  expect_error(rsap_call(conn, "STFC_STRUCTURE", IMPORTSTRUCT = list(RFCINT1 = 256L)),
               "out of range for INT1", "INT1 refuses 256 instead of sending 0")
  expect_error(rsap_call(conn, "STFC_STRUCTURE", IMPORTSTRUCT = list(RFCINT1 = -1L)),
               "out of range for INT1", "INT1 refuses -1")
  expect_error(rsap_call(conn, "STFC_STRUCTURE", IMPORTSTRUCT = list(RFCINT2 = 32768L)),
               "out of range for INT2", "INT2 refuses 32768")
  expect_error(rsap_call(conn, "STFC_STRUCTURE", IMPORTSTRUCT = list(RFCINT4 = NA_integer_)),
               "NA", "an NA integer is refused")
  expect_error(rsap_call(conn, "STFC_STRUCTURE", IMPORTSTRUCT = list(RFCINT2 = 1.5)),
               "whole number", "a fractional integer is refused")

  # Far outside any C integer: the range is checked before any cast, since
  # converting 1e100 to a C integer type is undefined behaviour, not a refusal.
  for (big in c(1e100, -1e100)) {
    for (f in c("RFCINT1", "RFCINT2", "RFCINT4")) {
      expect_error(rsap_call(conn, "STFC_STRUCTURE",
                             IMPORTSTRUCT = setNames(list(big), f)),
                   "out of range", paste(f, "refuses", big))
    }
  }
  # -2^31 itself is in range and is sent, but R cannot read it back: INT_MIN
  # *is* NA_integer_. So the round trip is checked one above it.
  rsap_call(conn, "STFC_STRUCTURE", IMPORTSTRUCT = list(RFCINT4 = -2147483648))
  expect(TRUE, "INT4 accepts -2^31, given as a double")
  top <- rsap_call(conn, "STFC_STRUCTURE",
                   IMPORTSTRUCT = list(RFCINT1 = 254, RFCINT2 = 32766, RFCINT4 = -2147483647))
  expect_equal(top$ECHOSTRUCT$RFCINT4, -2147483647L, "INT4 round-trips its smallest representable value")
  expect_error(rsap_call(conn, "STFC_STRUCTURE", IMPORTSTRUCT = list(RFCINT4 = 2147483648)),
               "out of range for INT4", "INT4 refuses 2^31")
  expect_error(rsap_call(conn, "STFC_STRUCTURE", IMPORTSTRUCT = list(RFCINT4 = -2147483649)),
               "out of range for INT4", "INT4 refuses -2^31 - 1")

  # An initial DATE comes back as NA, so NA -- and "" -- must go back out as the
  # initial value: a structure read from SAP has to be sendable again unchanged.
  blank <- rsap_call(conn, "STFC_STRUCTURE", IMPORTSTRUCT = list(RFCCHAR4 = "init"))
  expect(is.na(blank$ECHOSTRUCT$RFCDATE), "an initial DATE reads as NA")
  again <- rsap_call(conn, "STFC_STRUCTURE", IMPORTSTRUCT = blank$ECHOSTRUCT)
  expect(is.na(again$ECHOSTRUCT$RFCDATE), "and an NA DATE is sent back as the initial value")
  expect_equal(again$ECHOSTRUCT$RFCTIME, blank$ECHOSTRUCT$RFCTIME, "the TIME round-trips with it")
  empty <- rsap_call(conn, "STFC_STRUCTURE",
                     IMPORTSTRUCT = list(RFCDATE = "", RFCTIME = ""))
  expect(is.na(empty$ECHOSTRUCT$RFCDATE), "an empty DATE is the initial value")
  expect_equal(empty$ECHOSTRUCT$RFCTIME, "000000", "an empty TIME is the initial value")
  nat <- rsap_call(conn, "STFC_STRUCTURE", IMPORTSTRUCT = list(RFCTIME = NA_character_))
  expect_equal(nat$ECHOSTRUCT$RFCTIME, "000000", "an NA TIME is the initial value")

  # A value refused *inside* a conversion raises from deep in the C glue, and
  # the call's function container must be destroyed on that path too (it runs
  # under R_ExecWithCleanup). Measured, not assumed: the refusal happens before
  # anything goes on the wire, so many iterations are cheap, and a container
  # leaked per iteration shows up as resident memory that does not come back.
  rss_kb <- function() {
    line <- grep("^VmRSS:", readLines("/proc/self/status"), value = TRUE)
    as.numeric(gsub("[^0-9]", "", line))
  }
  refuse <- function(n) {
    for (i in seq_len(n)) {
      tryCatch(rsap_call(conn, "STFC_STRUCTURE",
                         IMPORTSTRUCT = list(RFCCHAR4 = "abcd", RFCDATE = "not a date"),
                         RFCTABLE = list(list(RFCCHAR4 = "row1"), list(RFCCHAR4 = "row2"))),
               error = function(e) NULL)
    }
  }
  if (file.exists("/proc/self/status")) {
    refuse(2000L)                     # warm up allocator and metadata caches
    invisible(gc()); before <- rss_kb()
    refuse(20000L)
    invisible(gc()); grown <- rss_kb() - before
    cat("  ..  RSS grew by ", grown, " kB over 20000 refused calls\n", sep = "")
    expect(grown < 8192, "a refused conversion leaks no function container")
  }
  again <- rsap_call(conn, "STFC_CONNECTION", REQUTEXT = "after refusals")
  expect(startsWith(again$ECHOTEXT, "after refusals"), "and the connection still works")

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
