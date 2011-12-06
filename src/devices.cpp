#include "wxDevice.h"


//#include <Rgraphics.h>
//#include <Rdevices.h>
#include <R_ext/GraphicsDevice.h>
#include <R_ext/GraphicsEngine.h>


#ifdef BEGIN_SUSPEND_INTERRUPTS
#undef BEGIN_SUSPEND_INTERRUPTS
#endif
#define BEGIN_SUSPEND_INTERRUPTS

#ifdef END_SUSPEND_INTERRUPTS
#undef END_SUSPEND_INTERRUPTS
#endif
#define END_SUSPEND_INTERRUPTS



#ifdef __cplusplus
extern "C" {
#endif
   SEXP R_create_wxDevice(SEXP dims, SEXP r_title, SEXP pointSize);
   SEXP R_as_wxDevice(SEXP r_widget, SEXP dims, SEXP pointSize, SEXP r_title);
#ifdef __cplusplus
}
#endif

//extern WxDeviceCreateFun wxDeviceDriver;

static GEDevDesc *
createWxDevice(const char *title, wxWindow *widget, double width, double height, double ps, 
                WxDeviceCreateFun init_fun)
{
    GEDevDesc *dd;
    NewDevDesc *dev;


    R_CheckDeviceAvailable();
    BEGIN_SUSPEND_INTERRUPTS {
	/* Allocate and initialize the device driver data */
	if (!(dev = (NewDevDesc *) calloc(1, sizeof(NewDevDesc))))
	    return (GEDevDesc *) NULL;
	/* Do this for early redraw attempts */
#if 0
	dev->displayList = R_NilValue;
#endif
	if (! init_fun (dev, title, widget, width, height, ps)) {
	    free(dev);
	    PROBLEM  "unable to start wx device" ERROR;
	}
//	gsetVar(install(".Device"), mkString(widget ? "embedded_wxDevice" : "wxDevice"), R_NilValue);
	dd = GEcreateDevDesc(dev);
#if 0
        dd->newDevStruct = 1;
#endif
	GEaddDevice2(dd, widget ? "embedded_wxDevice" : "wxDevice");
	//GEinitDisplayList(dd);
    } END_SUSPEND_INTERRUPTS;

    return(dd);
}


SEXP
R_create_wxDevice(SEXP dims, SEXP r_title, SEXP pointSize)
{
    char *title;
    const char *tmp;
    void *vmax;
    double height, width, ps;

    vmax = vmaxget();
    tmp = CHAR(STRING_ELT(r_title, 0));
    title = R_alloc(strlen(tmp) + 1, sizeof(char));
    strcpy(title, tmp);
    width = REAL(dims)[0];
    height = REAL(dims)[1];
    if (width <= 0 || height <= 0) {
	PROBLEM "wxWidgets device: invalid width or height" ERROR;
    }
    ps = REAL(pointSize)[0];
 
    createWxDevice(title, (wxWindow *) NULL, width, height, ps, wxDeviceDriver);

    vmaxset(vmax);
    return R_NilValue; /*XXX*/
}

int
addToEventHandler()
{

    return(0);
}


#include "RwxUtils.h"
#include "EventLoop.h"

/*
 This takes 
*/
SEXP
R_as_wxDevice(SEXP r_widget, SEXP dims, SEXP pointSize, SEXP r_title)
{
 SEXP ans;
 int status;
 wxWindow *widget;
 char *title;
 void *vmax;
 double height, width, ps;
 
 vmax = vmaxget();
 ps = REAL(pointSize)[0];
 width = REAL(dims)[0];
 height = REAL(dims)[1]; 

 widget = (wxWindow *) R_get_wxWidget_Ref(r_widget, "wxWindow");

#if 1
 title = NULL;
#else
 tmp = CHAR(STRING_ELT(r_title, 0));
 title = R_alloc(strlen(tmp) + 1, sizeof(char));
 strcpy(title, tmp);
#endif

 status = createWxDevice(title, widget, width, height, ps, wxDeviceDriver) != NULL;

/* Not needed !*/
 if(0) {
   WaitSizeEventLoop loop(widget);
   loop.Run();
 }

 ans = ScalarLogical(status);

 return(ans);
}


extern "C"
SEXP
R_RwxCanvas_new(SEXP r_parent, SEXP r_size)
{
  wxWindow *parent;
  RwxCanvas *ans;
  wxSize size = wxDefaultSize;

  if(LENGTH(r_size)) 
      size.SetDefaults(wxSize(INTEGER(r_size)[0], INTEGER(r_size)[1]));

  parent = (wxWindow *) R_get_wxWidget_Ref(r_parent, "wxWindow");
  ans = new RwxCanvas(parent, size);
  return(R_make_wxWidget_Ref(ans, "RwxCanvas"));
}


extern "C"
SEXP
R_set_DeviceNumber(SEXP r_widget, SEXP r_devNum)
{
    RwxCanvas *widget = (RwxCanvas *) R_get_wxWidget_Ref(r_widget, "RwxCanvas");
    int devNum = INTEGER(r_devNum)[0];
    widget->SetDeviceNumber(devNum);
    return(R_NilValue);
}

extern "C"
SEXP
R_get_DeviceNumber(SEXP r_widget)
{
    RwxCanvas *widget = (RwxCanvas *) R_get_wxWidget_Ref(r_widget, "RwxCanvas");
    return(ScalarInteger(widget->GetDeviceNumber()));
}

extern "C"
SEXP R_RwxCanvas_IsRDevice(SEXP r_widget)
{
    RwxCanvas *widget = (RwxCanvas *) R_get_wxWidget_Ref(r_widget, "RwxCanvas");  
    bool ans = widget->IsRDevice();
    return(ScalarLogical(ans));
}
