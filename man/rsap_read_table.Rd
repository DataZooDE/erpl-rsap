\name{rsap_read_table}
\alias{rsap_read_table}
\title{Read rows from an ABAP table}
\usage{
rsap_read_table(con, table, fields = character(0), where = character(0),
                max_rows = 0L, skip = 0L, delimiter = "|")
}
\arguments{
\item{con}{an \code{rsap_connection}.}
\item{table}{the ABAP table name.}
\item{fields}{character vector of columns; all of them when omitted.}
\item{where}{character vector of WHERE clauses, ANDed by the server.}
\item{max_rows}{maximum rows to return; 0 for no limit.}
\item{skip}{rows to skip.}
\item{delimiter}{the separator asked of the server. The data is cut by the
offsets the server returns, not split on this, so it cannot corrupt a row.}
}
\value{A \code{data.frame}, one column per requested field.}
\description{
A convenience over \code{RFC_READ_TABLE}, which returns its rows as one
delimited character column plus a description of where each field starts.

Splitting on the delimiter -- as RSAP did -- corrupts any row whose data
contains it. This cuts by offset instead.

When the selected fields exceed \code{RFC_READ_TABLE}'s 512-byte buffer the
server raises \code{DATA_BUFFER_EXCEEDED}, which reaches the caller as an error.
}
\examples{
\dontrun{
rsap_read_table(con, "T000", fields = c("MANDT", "MTEXT"), max_rows = 5)
}
}
