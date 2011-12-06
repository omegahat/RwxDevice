#ifndef R_DEV_WX_H
#define R_DEV_WX_H

#include <wx/wx.h>

extern "C" {
#include <R.h>
#include <Rinternals.h>
//#include <Rgraphics.h>
//#include <Rdevices.h>
#include <R_ext/GraphicsEngine.h>
#include <R_ext/GraphicsDevice.h>
}

/*
  There are two forms of "asynchronous"/non-procedural callbacks.
  One comes from the R graphics engine invoking one of the registered
  C routines for this device. The other is wxWidgets events that call
  explicit C++ methods in the RwxCanvas or RwxDeviceFrame.
  Both "callbacks" need access to information about the device.
*/

typedef struct {
    /* R Graphics Parameters */
    /* Local device copy so that we can detect */
    /* when parameter changes. */

    double cex;				/* Character expansion */
    double srt;				/* String rotation */

    /* gint bg; */                      /* Background */
    int fill;
//    int col;

    int lty, lwd;                      /* line params */

    /* Driver Specific  - borrowed from gtkDevice. */
    int resize;			/* Window resized */

    int windowWidth;			/* Window width (pixels) */
    int windowHeight;			/* Window height (pixels) */

    int fontface;			/* Typeface */
    int fontsize;			/* Size in points */
    bool usefixed;


     /* wxWidgets specific. */
    wxFont *font;
    wxPen *pen;
    wxBrush  *brush;

    wxColour *gcol_bg;
    
    wxFrame  *window;			/* Graphics frame */
    wxWindow *drawing;                  /* Drawable window */
    wxDC     *drawingContext;
 
    double pixelWidth, pixelHeight;

#if 0
    GdkPixmap *pixmap;                  /* Backing store */
    GdkRectangle clip;
    GdkCursor *gcursor;
#endif


} wxDesc;

/*
 A regular wxWindow that can be used as an R graphics device.
 It has knowledge about the R device (DevDesc which is a NewDevDesc) 
 and its own local state (wxDesc)
*/
class RwxCanvas : public wxWindow
{
      public:
         RwxCanvas(wxWindow *parent, wxDesc *local, pDevDesc dev) : 
	     wxWindow(parent, wxID_ANY, wxDefaultPosition, wxSize(450, 500))
         {
	     SetDevInfo(local, dev);
	     deviceNumber = -1;
         };
         RwxCanvas(wxWindow *parent, wxSize size = wxDefaultSize) : wxWindow(parent, wxID_ANY, wxDefaultPosition, size) { 
                    SetDevInfo((wxDesc *) NULL, (pDevDesc) NULL); 
		    deviceNumber = -1;
	 };

         RwxCanvas() { 
	     SetDevInfo((wxDesc *) NULL, (pDevDesc) NULL); 
	     deviceNumber = -1;
	 };

         bool closeDevice();

	 void SizeEvent(wxSizeEvent &ev);

	 void SetDevInfo(wxDesc *info, pDevDesc dev) {
   	       gtkd = info;
	       dd = dev;
	       if(!gtkd || !gtkd->drawing)
		   return;
	       gtkd->drawing->GetClientSize(&gtkd->windowWidth, &gtkd->windowHeight);
	 }


	 void Resize() {
	     if (gtkd->resize != 0) {
		 pDevDesc odd = (pDevDesc) dd;
		 odd->left = 0.0;
		 odd->right = gtkd->windowWidth;
		 odd->bottom = gtkd->windowHeight;
		 odd->top = 0.0;
		 gtkd->resize = 0;
	     }
	 }

	 void update();
	 void computeSize(const wxSize s);

	 void SetDeviceNumber(int n) {
	     deviceNumber = n;
	 }

	 int GetDeviceNumber() {
	     return(deviceNumber);
	 }

	 bool IsRDevice() {
	     return(deviceNumber > 0);
	 };

      public: /* XXX Make private */
         wxDesc *gtkd;
	 pDevDesc dd;

      public:
        void OnPaint(wxPaintEvent &event);
	void OnSize(wxSizeEvent &event);

      protected:
	int deviceNumber;

      private:

      DECLARE_EVENT_TABLE();
      DECLARE_DYNAMIC_CLASS(RwxCanvas);
};


/*
  Top-level frame for containing an R graphics device.
  Can use a regular wxFrame or RwxFrame.
*/
class RwxDeviceFrame : public wxFrame
{
  public:
  RwxDeviceFrame(wxString &title, wxPoint &location, wxSize &size) 
     : wxFrame(NULL, wxID_ANY, title, location, size, wxDEFAULT_FRAME_STYLE | wxFULL_REPAINT_ON_RESIZE) {

  };
  void OnCloseWindow(wxCloseEvent &event);
  void OnExit(wxCommandEvent &event);

  
  protected:
   RwxCanvas *canvas;
    
  DECLARE_EVENT_TABLE();
};

/* Declarations of different routines that we use to create the device 
   i) from an existing widget
  ii) from scratch as a stand-alone window.
*/
typedef Rboolean (*WxDeviceCreateFun)(pDevDesc dev, const char *display, wxWindow *widget, double width, double height, double pointsize);
extern Rboolean wxDeviceFromWidget(pDevDesc odd, char *title, wxWindow *, double width, double height, double pointsize);
extern Rboolean wxDeviceDriver(pDevDesc odd, const char *title, wxWindow *, double width, double height, double pointsize);


#endif
