\name{asWxDevice}
\alias{asWxDevice}
%- Also NEED an '\alias' for EACH other topic documented here.
\title{ ~~function to do ... ~~ }
\description{
  ~~ A concise (1-5 lines) description of what the function does. ~~
}
\usage{
asWxDevice(widget, size = c(7, 7), pointsize = 12, title = "")
}
%- maybe also 'usage' for other objects documented here.
\arguments{
  \item{widget}{ ~~Describe \code{widget} here~~ }
  \item{size}{ ~~Describe \code{size} here~~ }
  \item{pointsize}{ ~~Describe \code{pointsize} here~~ }
  \item{title}{ ~~Describe \code{title} here~~ }
}
\details{
  ~~ If necessary, more details than the description above ~~
}
\value{
  ~Describe the value returned
  If it is a LIST, use
  \item{comp1 }{Description of 'comp1'}
  \item{comp2 }{Description of 'comp2'}
  ...
}
\references{ ~put references to the literature/web site here ~ }
\author{ ~~who you are~~ }
\note{ ~~further notes~~ 

 ~Make other sections like Warning with \section{Warning }{....} ~
}
\seealso{ ~~objects to See Also as \code{\link{help}}, ~~~ }
\examples{
##---- Should be DIRECTLY executable !! ----
##-- ==>  Define data, use random,
##--	or do  help(data=index)  for the standard data sets.

## The function is currently defined as
function (widget, size = c(7, 7), pointsize = 12, title = "") 
{
    .Call(R_as_wxDevice, widget, as.numeric(size), as.numeric(pointsize), 
        as.character(title))
  }
}
% Add one or more standard keywords, see file 'KEYWORDS' in the
% R documentation directory.
\keyword{ ~kwd1 }
\keyword{ ~kwd2 }% __ONLY ONE__ keyword per line
