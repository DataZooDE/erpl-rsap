\name{rsap_call}
\alias{rsap_call}
\title{Call an ABAP function module}
\usage{
rsap_call(con, name, ...)
}
\arguments{
\item{con}{an \code{rsap_connection}.}
\item{name}{the function module name.}
\item{...}{parameters as named arguments. Structures are named lists; tables
are lists of named lists, or a \code{data.frame}.}
}
\value{
A named list of the exporting, changing and table parameters. Structures come
back as named lists and tables as \code{data.frame}s. Importing parameters are
left out: they are the caller's own input echoed back.
}
\description{
A parameter the interface does not declare is an error rather than being
ignored, so a typo does not silently send an empty call.
}
\section{Errors}{
Failures are conditions of class \code{rsap_error} carrying \code{key},
\code{code}, \code{group} and the T100 fields as attributes. Switch on the key:
the message of a declared exception is the T100 identity rendered, not text you
can match.
}
\examples{
\dontrun{
res <- rsap_call(con, "STFC_CONNECTION", REQUTEXT = "hello")
res$ECHOTEXT
}
}
