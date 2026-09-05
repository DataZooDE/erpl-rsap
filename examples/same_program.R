# One program, run twice: once with the package linked against our
# libsapnwrfc.a, once against SAP's real libsapnwrfc.so. The R and C source is
# identical on both sides, so any difference in this output is the shim's.
#
# Printed field by field. Dumping a list would put its order into the diff as a
# difference that is not one.

suppressMessages(library(rsap))

show <- function(label, value) {
  cat(sprintf("%-34s %s\n", paste0(label, ":"), paste(deparse(value), collapse = " ")))
}

params <- list(
  ashost = Sys.getenv("SAP_ASHOST", "localhost"),
  sysnr  = Sys.getenv("SAP_SYSNR", "00"),
  client = Sys.getenv("SAP_CLIENT", "001"),
  user   = Sys.getenv("SAP_USER"),
  passwd = Sys.getenv("SAP_PASSWD"),
  lang   = Sys.getenv("SAP_LANG", "EN")
)

conn <- do.call(rsap_connect, params)
show("alive", rsap_alive(conn))
show("ping", rsap_ping(conn))

info <- rsap_info(conn)
for (k in c("sysId", "client", "user", "language", "isoLanguage",
            "partnerType", "rfcRole")) {
  show(paste0("attr.", k), info[[k]])
}

r <- rsap_call(conn, "STFC_CONNECTION", REQUTEXT = "differential")
show("STFC_CONNECTION.ECHOTEXT", r$ECHOTEXT)

s <- rsap_call(conn, "STFC_STRUCTURE",
               IMPORTSTRUCT = list(RFCCHAR1 = "a", RFCCHAR2 = "bc",
                                   RFCCHAR4 = "defg", RFCINT1 = 7L,
                                   RFCINT2 = 8L, RFCINT4 = 9L,
                                   RFCFLOAT = 1.5, RFCDATE = "20260905",
                                   RFCTIME = "101112"))
for (f in c("RFCCHAR1", "RFCCHAR2", "RFCCHAR4", "RFCINT1", "RFCINT2",
            "RFCINT4", "RFCFLOAT", "RFCDATE", "RFCTIME")) {
  show(paste0("ECHOSTRUCT.", f), s$ECHOSTRUCT[[f]])
}

rows <- list(list(RFCCHAR4 = "one", RFCINT4 = 1L),
             list(RFCCHAR4 = "two", RFCINT4 = 2L))
t <- rsap_call(conn, "STFC_STRUCTURE",
               IMPORTSTRUCT = list(RFCCHAR4 = "x"), RFCTABLE = rows)
show("RFCTABLE.nrow", nrow(t$RFCTABLE))
show("RFCTABLE.RFCCHAR4", as.character(t$RFCTABLE$RFCCHAR4))
show("RFCTABLE.RFCINT4", as.integer(t$RFCTABLE$RFCINT4))

tbl <- rsap_read_table(conn, "T000", fields = c("MANDT", "MTEXT"), max_rows = 3)
show("read_table.names", names(tbl))
show("read_table.MANDT", tbl$MANDT)

# The error taxonomy: the classification is where the shim was wrong.
for (case in list(list("declared", list(METHOD = "1", MESSAGETYPE = "E")),
                  list("plain", list(METHOD = "2", MESSAGETYPE = "E")),
                  list("dump", list(METHOD = "3", MESSAGETYPE = "E")),
                  list("message", list(MESSAGETYPE = "A")))) {
  label <- case[[1]]
  c2 <- do.call(rsap_connect, params)
  err <- tryCatch(
    do.call(rsap_call, c(list(c2, "RFC_RAISE_ERROR"), case[[2]])),
    error = function(e) e)
  show(paste0("raise.", label, ".key"), attr(err, "key"))
  show(paste0("raise.", label, ".code"), attr(err, "code"))
  show(paste0("raise.", label, ".group"), attr(err, "group"))
  show(paste0("raise.", label, ".msgClass"), attr(err, "abapMsgClass"))
  show(paste0("raise.", label, ".msgNumber"), attr(err, "abapMsgNumber"))
  try(rsap_close(c2), silent = TRUE)
}

nf <- tryCatch(rsap_call(conn, "Z_NO_SUCH_FUNCTION_MODULE_AT_ALL"),
               error = function(e) e)
show("unknown.key", attr(nf, "key"))
show("unknown.code", attr(nf, "code"))

rsap_close(conn)
show("alive after close", rsap_alive(conn))
