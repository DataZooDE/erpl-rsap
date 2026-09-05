# The live harness. There are no mocks: a compatibility claim proved against a
# fake is not a compatibility claim.
#
# Without SAP_USER/SAP_PASSWD the tests skip -- and say so, because a skip must
# never read as a pass. Checked in both directions: with credentials they must
# actually run.

live_configured <- function() {
  nzchar(Sys.getenv("SAP_USER")) && nzchar(Sys.getenv("SAP_PASSWD"))
}

live_params <- function(...) {
  base <- list(
    ashost = Sys.getenv("SAP_ASHOST", "localhost"),
    sysnr  = Sys.getenv("SAP_SYSNR", "00"),
    client = Sys.getenv("SAP_CLIENT", "001"),
    user   = Sys.getenv("SAP_USER"),
    passwd = Sys.getenv("SAP_PASSWD"),
    lang   = Sys.getenv("SAP_LANG", "EN")
  )
  utils::modifyList(base, list(...))
}

.checks <- 0L

expect <- function(condition, what) {
  .checks <<- .checks + 1L
  if (!isTRUE(condition)) stop("FAILED: ", what, call. = FALSE)
  cat("  ok  ", what, "\n", sep = "")
}

expect_equal <- function(actual, expected, what) {
  expect(identical(actual, expected),
         paste0(what, " (got ", deparse(actual), ", wanted ", deparse(expected), ")"))
}

expect_error <- function(expr, pattern, what) {
  err <- tryCatch({ force(expr); NULL }, error = function(e) e)
  if (is.null(err)) stop("FAILED: ", what, " -- no error raised", call. = FALSE)
  if (!grepl(pattern, conditionMessage(err))) {
    stop("FAILED: ", what, " -- message was: ", conditionMessage(err), call. = FALSE)
  }
  .checks <<- .checks + 1L
  cat("  ok  ", what, "\n", sep = "")
}

run_suite <- function(name, body) {
  if (!live_configured()) {
    cat("SKIP(live): SAP_USER/SAP_PASSWD unset -- live test not run\n")
    return(invisible(NULL))
  }
  cat(name, "\n", sep = "")
  body()
  cat(sprintf("%s: %d checks passed\n", name, .checks))
}
