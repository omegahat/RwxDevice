library(RwxWidgets)

# with the run = TRUE, this currently crashes because
# the drawingContext in the graphics device structure has
# not been initialized.

RApp(OnInit =
     function(app) {
       library(RwxDevice);
       wx()
       print(.Devices)
       plot(1:10)       
       library(lattice)
       d = data.frame(x = runif(300), y = runif(300), type = rep(factor(c("A", "B", "C")), 100))
       print(xyplot(y ~ x | type, d))

       TRUE
     }, run = TRUE)

wxEventLoop()



