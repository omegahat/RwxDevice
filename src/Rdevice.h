#include <R.h>
#include <Rinternals.h>
#include <Rgraphics.h>
#include <Rdevices.h>
#include <R_ext/GraphicsDevice.h>
#include <R_ext/GraphicsEngine.h>

#define BEGIN_SUSPEND_INTERRUPTS
#define END_SUSPEND_INTERRUPTS

class RGraphicsDevice {

   public:

     virtual Rboolean locator(double *x, double *y, NewDevDesc *dd) = 0;
     virtual void close(NewDevDesc *dd) = 0;
     virtual void activate(NewDevDesc *dd) = 0;
     virtual void deactivate(NewDevDesc *dd) = 0;

     virtual void size(double *left, double *right,
                       double *bottom, double *top,
                       NewDevDesc *dd) = 0;

     virtual void clip(double x0, double x1, double y0, double y1,
                        NewDevDesc *dd) = 0;

     virtual double strWidth(char *str, R_GE_gcontext *gc, NewDevDesc *dd) = 0;

     virtual void text(double x, double y, char *str,
                        double rot, double hadj,
                        R_GE_gcontext *gc, 
                        NewDevDesc *dd) = 0;

     virtual void rect(double x0, double y0, double x1, double y1,
		       R_GE_gcontext *gc,
		       NewDevDesc *dd) = 0;

     virtual void circle(double x0, double y0, double r,
		       R_GE_gcontext *gc,
		       NewDevDesc *dd) = 0;

     virtual void line(double x1, double y1, double x2, double y2,
			  R_GE_gcontext *gc,
			  NewDevDesc *dd) = 0;

     virtual void polyline(int n, double *x, double *y, 
			   R_GE_gcontext *gc,
			   NewDevDesc *dd) = ;

     virtual void WX_Polygon(int n, double *x, double *y, 
			     R_GE_gcontext *gc,
			     NewDevDesc *dd) = 0;

     virtual Rboolean WX_Locator(double *x, double *y, NewDevDesc *dd) = 0;

     virtual void WX_Mode(int mode, NewDevDesc *dd) = 0;


     virtual void WX_Hold(NewDevDesc *dd)  {    };

     virtual void WX_MetricInfo(int c,
			   R_GE_gcontext *gc,
			   double* ascent, double* descent,
			   double* width, NewDevDesc *dd) = 0;

};
