library(rsap)
source("live.R")

run_suite("connection", function() {
  conn <- do.call(rsap_connect, live_params())
  expect(rsap_alive(conn), "a fresh connection is alive")
  expect(isTRUE(rsap_ping(conn)), "it answers a ping")

  info <- rsap_info(conn)
  expect_equal(info$sysId, "A4H", "it reports the system id")
  expect_equal(info$client, "001", "it reports the client")
  # The one-character SAP form, not the ISO code. The Go binding shipped `EN`
  # here where the reference gives `E`; see CLAUDE.md.
  expect_equal(info$language, "E", "language is the one-character SAP form")
  expect_equal(info$isoLanguage, "EN", "isoLanguage is the ISO code")

  rsap_close(conn)
  expect(!rsap_alive(conn), "it is not alive after close")
  expect(is.null(rsap_close(conn)) || TRUE, "close is idempotent")

  expect_error(rsap_ping(conn), "closed", "a closed connection refuses work")

  expect_error(
    do.call(rsap_connect, live_params(user = "NOBODY", passwd = "definitely not it")),
    "Name or password",
    "bad credentials are refused with the server's own message"
  )
})
