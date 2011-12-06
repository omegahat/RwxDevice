library(RwxWidgets)

# This is the callback function that draws the plot.
# It is given the radio button widget that was selected to cause the new plot to
# be drawn along with its identifier that was given when the button was add'ed to
# the radioButtonGroup().   Also it receives any additional arguments that were
# specified as part of the ... in the call to radioButtonGroup().
# This avoids the need to have global variables or define this function inside
# another to use lexical scoping to find the variable 'canvas'.
doPlot =
function(ev, canvas)
{
     if(!canvas$IsRDevice()) 
       asWxDevice(canvas)
     
     plot(1:10)
     lines(c(2, 2), c(1, 11))
}


init =
function(app)
{
  library(RwxDevice)

  f = RFrame("Embedded RwxDevice test", size = c(600, 800))
  sz = wxBoxSizer(wxHORIZONTAL)

  canvas = RwxCanvas(f)


   # Now put the components into the frame.
  sz$Add(canvas, 1, wxEXPAND)
  btn = wxButton(f, wxID_ANY, "Draw plot")
  btn$AddCallback(wxEVT_COMMAND_BUTTON_CLICKED, doPlot, canvas)
  sz$Add(btn, 0)

  f$SetSizer(sz)
  sz$SetSizeHints(f)

  f$Show()

  TRUE
}

RApp(OnInit = init, run = TRUE)

