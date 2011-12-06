wx =
function(title = "wxDevice", size = c(7, 7), pointSize = 12)
{
  .Call("R_create_wxDevice", as.numeric(size), as.character(title), as.numeric(pointSize))
}  


asWxDevice = asGraphicsDevice =
function(widget, size = c(7, 7), pointsize = 12, title = "")  
{
  .Call(R_as_wxDevice, widget, as.numeric(size), as.numeric(pointsize), as.character(title))
  .Call(R_set_DeviceNumber, widget, dev.cur())
}

# setClass("RwxCanvas", contains = "wxFrame")
setClass("wxCanvas", contains = "wxWindow")
setClass("RwxCanvas", contains = "wxCanvas")

RwxCanvas =
function(parent = Rframe("R wxWidgets Graphics Device"), size = c(400, 400))
{
   #XXX check parent.

  if(!is.null(size))
    size = as.integer(size)

  .Call("R_RwxCanvas_new", parent, size)
}

RwxCanvas_IsRDevice =
function(this)
{
  .Call("R_RwxCanvas_IsRDevice", this)
}  
  

#
# This method allows us to say things like
#  dev$plot(1:10)
#  dev$hist(x)
# and so on. It intercepts all the plotting functions
# (and others) and sets the active graphics device to this one
# and unsets it at the end of the function call.
#
setMethod("$", "RwxCanvas",
          function(x, name) {
               # Next method!!!!
              funNames = paste(getWxClassInfo(x), name, sep = "_")
              e = sapply(funNames, exists)
              if(any(e)) {
                 f = get(funNames[e][1])
                 return(function(...)  f(x, ...))
              } else {
                f = get(name)
              }
            
            function(...)  {

              orig = dev.cur()
              target = .Call(R_get_DeviceNumber, x)
              if(target != orig) {
                on.exit(dev.set(orig))
                dev.set(target)
              }
              
              f(...) 
            }
          })
