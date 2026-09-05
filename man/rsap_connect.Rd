\name{rsap_connect}
\alias{rsap_connect}
\alias{rsap_close}
\alias{rsap_alive}
\alias{rsap_ping}
\alias{rsap_info}
\title{Open and manage a connection to an SAP system}
\usage{
rsap_connect(...)
rsap_close(con)
rsap_alive(con)
rsap_ping(con)
rsap_info(con)
}
\arguments{
\item{...}{connection parameters as named arguments, or a single named list.
Names go to the wire as given: \code{ashost}, \code{sysnr}, \code{client},
\code{user}, \code{passwd}, \code{lang}, and anything else the RFC layer
understands.}
\item{con}{an \code{rsap_connection}.}
}
\value{
\code{rsap_connect} returns an \code{rsap_connection}. \code{rsap_alive} and
\code{rsap_ping} return a logical. \code{rsap_info} returns a named list of the
connection attributes, including \code{sysId}, \code{client}, \code{user} and
both \code{language} (the one-character SAP form) and \code{isoLanguage}.
}
\description{
No SAP NW RFC SDK is needed: the protocol is implemented in the package.

The connection is an external pointer with a finaliser, so a forgotten
\code{rsap_close} still ends the conversation when the object is collected.
\code{rsap_close} is idempotent.
}
\examples{
\dontrun{
con <- rsap_connect(ashost = "localhost", sysnr = "00", client = "001",
                    user = "DEVELOPER", passwd = Sys.getenv("SAP_PASSWD"))
rsap_info(con)$sysId
rsap_close(con)
}
}
