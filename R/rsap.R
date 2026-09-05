#' Open a connection to an SAP system.
#'
#' Connection parameters are named arguments -- `ashost`, `sysnr`, `client`,
#' `user`, `passwd`, `lang`, or any other name the RFC layer understands. Names
#' go to the wire as given.
#'
#' @param ... connection parameters, or a single named list of them.
#' @return an object of class `rsap_connection`.
#' @export
rsap_connect <- function(...) {
  args <- list(...)
  if (length(args) == 1L && is.list(args[[1L]])) args <- args[[1L]]
  if (length(args) == 0L || is.null(names(args)) || any(!nzchar(names(args)))) {
    stop("connection parameters must all be named", call. = FALSE)
  }
  conn <- .Call(C_rsap_connect,
                as.character(names(args)),
                vapply(args, as.character, character(1L)))
  class(conn) <- "rsap_connection"
  conn
}

#' Close a connection. Idempotent.
#' @param con an `rsap_connection`.
#' @export
rsap_close <- function(con) invisible(.Call(C_rsap_close, con))

#' Is the connection still open?
#' @param con an `rsap_connection`.
#' @export
rsap_alive <- function(con) .Call(C_rsap_alive, con)

#' Round-trip the connection.
#' @param con an `rsap_connection`.
#' @export
rsap_ping <- function(con) .Call(C_rsap_ping, con)

#' What the system said about this connection at logon.
#' @param con an `rsap_connection`.
#' @return a named list.
#' @export
rsap_info <- function(con) .Call(C_rsap_info, con)

#' @export
print.rsap_connection <- function(x, ...) {
  if (!rsap_alive(x)) {
    cat("<rsap connection: closed>\n")
    return(invisible(x))
  }
  info <- rsap_info(x)
  cat(sprintf("<rsap connection: %s client %s as %s>\n",
              info$sysId, info$client, info$user))
  invisible(x)
}

#' @export
close.rsap_connection <- function(con, ...) rsap_close(con)

#' Call an ABAP function module.
#'
#' @param con an `rsap_connection`.
#' @param name the function module name.
#' @param ... parameters as named arguments. Structures are named lists, tables
#'   are lists of named lists or a `data.frame`.
#' @return a named list of the exporting, changing and table parameters.
#'   Structures come back as named lists, tables as `data.frame`s.
#' @export
rsap_call <- function(con, name, ...) {
  args <- list(...)
  if (length(args) == 1L && is.null(names(args)) && is.list(args[[1L]])) {
    args <- args[[1L]]
  }
  if (length(args) > 0L &&
      (is.null(names(args)) || any(!nzchar(names(args))))) {
    stop("call parameters must all be named", call. = FALSE)
  }
  .Call(C_rsap_call, con, as.character(name),
        if (length(args)) as.character(names(args)) else character(0L),
        unname(args))
}

#' Read rows from an ABAP table.
#'
#' A convenience over `RFC_READ_TABLE`, which returns its rows as one
#' delimited character column (`WA`) plus a description of where each field
#' starts and how long it is.
#'
#' RSAP split that column on the delimiter, which corrupts any row whose data
#' contains the delimiter -- a description with a semicolon in it, say -- and
#' loses every type. This uses the field offsets the server itself returns, so
#' there is nothing to collide with.
#'
#' @param con an `rsap_connection`.
#' @param table the ABAP table name.
#' @param fields character vector of columns; all of them when omitted.
#' @param where character vector of WHERE clauses, ANDed by the server.
#' @param max_rows maximum rows to return; 0 for no limit.
#' @param skip rows to skip.
#' @param delimiter the field separator asked of the server. Only used to make
#'   the server's own offsets checkable; the data is cut by offset, not split.
#' @return a `data.frame`, one column per requested field.
#' @export
rsap_read_table <- function(con, table, fields = character(0L),
                            where = character(0L), max_rows = 0L,
                            skip = 0L, delimiter = "|") {
  if (!is.character(delimiter) || length(delimiter) != 1L || !nzchar(delimiter)) {
    stop("delimiter must be a single non-empty character", call. = FALSE)
  }

  params <- list(
    QUERY_TABLE = table,
    DELIMITER = delimiter,
    ROWCOUNT = as.integer(max_rows),
    ROWSKIPS = as.integer(skip)
  )
  if (length(fields)) {
    params$FIELDS <- lapply(fields, function(f) list(FIELDNAME = f))
  }
  if (length(where)) {
    params$OPTIONS <- lapply(where, function(w) list(TEXT = w))
  }

  res <- do.call(rsap_call, c(list(con, "RFC_READ_TABLE"), params))

  meta <- res$FIELDS
  data <- res$DATA
  if (!is.data.frame(meta) || nrow(meta) == 0L) {
    return(data.frame())
  }

  offsets <- as.integer(meta$OFFSET)
  lengths <- as.integer(meta$LENGTH)
  names_out <- as.character(meta$FIELDNAME)
  wa <- if (is.data.frame(data) && nrow(data) > 0L) as.character(data$WA) else character(0L)

  # Cut by offset, not by splitting. The server tells us where each field
  # begins and how long it is; that is authoritative and delimiter-proof.
  cols <- lapply(seq_along(names_out), function(i) {
    trimws(substr(wa, offsets[i] + 1L, offsets[i] + lengths[i]), which = "right")
  })
  names(cols) <- names_out

  # No truncation heuristic here. `WA` is right-trimmed by the server, so a row
  # shorter than the sum of the field widths is the normal case, not a
  # truncated one -- guessing from length warned on every ordinary read. When
  # the selected fields genuinely exceed RFC_READ_TABLE's 512-byte buffer the
  # server raises DATA_BUFFER_EXCEEDED, which reaches the caller as an error.

  as.data.frame(cols, stringsAsFactors = FALSE, optional = TRUE)
}
