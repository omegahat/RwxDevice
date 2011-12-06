/*
 An implementation of an R graphics device using the wxWidgets toolkit.
 Copyright Duncan Temple Lang 2006

 Mostly copied from gtkDevice.
*/

/* #define TEST_ALL 0 */

#define CAN_CLIP (Rboolean) 0  /* turn off for now */

#ifdef HAVE_RINT
#define R_rint(x) rint(x)
#else
#define R_rint(x) ((int) x + 0.5)
#endif

#include <wx/wx.h>
#include "wxDevice.h"

#include <R.h>
#include <Rinternals.h>
#include <Rgraphics.h>
//#include <Rdevices.h>
#include <R_ext/GraphicsDevice.h>
#include <R_ext/GraphicsEngine.h>

#define R_wxString(x)  wxString((x), wxConvUTF8)

#define CURSOR		GDK_CROSSHAIR		/* Default cursor */
#define MM_PER_INCH	25.4			/* mm -> inch conversion */

#define IS_100DPI ((int) (1./gtkd->pixelHeight + 0.5) == 100)

static unsigned int adobe_sizes = 0x0403165D;
#define ADOBE_SIZE(I) ((I) > 7 && (I) < 35 && (adobe_sizes & (1<<((I)-8))))

/* routines from here */

/* Device driver actions */
static void WX_Activate(NewDevDesc *dd);
static void WX_Circle(double x, double y, double r,
		       R_GE_gcontext *gc,
		       NewDevDesc *dd);
static void WX_Clip(double x0, double x1, double y0, double y1, 
		     NewDevDesc *dd);
static void WX_Close(NewDevDesc *dd);
static void WX_Deactivate(NewDevDesc *dd);
static void WX_Hold(NewDevDesc *dd);
static Rboolean WX_Locator(double *x, double *y, NewDevDesc *dd);
static void WX_Line(double x1, double y1, double x2, double y2,
		     R_GE_gcontext *gc,
		     NewDevDesc *dd);
static void WX_MetricInfo(int c,
			   R_GE_gcontext *gc,
			   double* ascent, double* descent,
			   double* width, NewDevDesc *dd);
static void WX_Mode(int mode, NewDevDesc *dd);
static void WX_NewPage(R_GE_gcontext *gc, NewDevDesc *dd);
static void WX_Polygon(int n, double *x, double *y, 
			R_GE_gcontext *gc,
			NewDevDesc *dd);
static void WX_Polyline(int n, double *x, double *y, 
			 R_GE_gcontext *gc,
			 NewDevDesc *dd);
static void WX_Rect(double x0, double y0, double x1, double y1,
		     R_GE_gcontext *gc,
		     NewDevDesc *dd);
static void WX_Size(double *left, double *right,
		     double *bottom, double *top,
		     NewDevDesc *dd);
static double WX_StrWidth(char *str,
			   R_GE_gcontext *gc,
			   NewDevDesc *dd);
static void WX_Text(double x, double y, char *str, 
		     double rot, double hadj, 
		     R_GE_gcontext *gc,
		     NewDevDesc *dd);
static wxWindow *WX_Open(NewDevDesc*, wxDesc*, char*, double, double);

static int initialize(NewDevDesc *dd);

#define RWX_DEBUG 0

#if RWX_DEBUG
#define ANNOUNCE(id) fprintf(stderr, #id"\n");fflush(stderr);
#else
#define ANNOUNCE(id) ;
#endif


#define MIN(a, b)  ((a) < (b) ? (a) : (b))

#define SET_DC_FONT(gtkd, f) \
       if(gtkd->drawingContext) {\
           gtkd->drawingContext->SetFont( f ? *f : *(gtkd->font) ); \
       }

/* Make certain that the pen in our R graphics device
   is active in the wxWidgets drawing context (wxDC) */
#define RESET_PEN(x) \
          (x)->drawingContext->SetPen(*(x)->pen);

#define RESET_BRUSH(x) \
          (x)->drawingContext->SetBrush(*(x)->brush);

#define RESET_SIZE(gtkd)    if(gtkd->drawing) gtkd->drawing->GetClientSize(&gtkd->windowWidth, &gtkd->windowHeight)

#define CHECK_wxDESC \
   !gtkd->drawing || !gtkd->drawingContext


IMPLEMENT_DYNAMIC_CLASS(RwxCanvas, wxWindow);


/* Pixel Dimensions (Inches) */
static void getScreenDimensions(wxDesc *gtkd)
{
    wxCoord w, h, w_mm, h_mm;

    wxScreenDC dcScreen;
    wxSize sz_ppi, sz_mm, sz;

#if 0
    dcScreen.GetSizeMM(&w_mm, &h_mm);
    dcScreen.GetSize(&w, &h);
    gtkd->pixelWidth = (((double) w_mm)/ ((double) w))/ (double) MM_PER_INCH;
    gtkd->pixelHeight = (((double) h_mm)/ ((double) h))/ (double) MM_PER_INCH;
#else
#if 0
    sz_mm = dcScreen.GetSizeMM();
    sz = dcScreen.GetSize();
    gtkd->pixelWidth = (((double) sz_mm.x)/ ((double) sz.x))/ (double) MM_PER_INCH;
    gtkd->pixelHeight = (((double) sz_mm.y)/ ((double) sz.y))/ (double) MM_PER_INCH;
#else
    sz_ppi = dcScreen.GetPPI();
    gtkd->pixelWidth = 1.0 / (double) sz_ppi.x;
    gtkd->pixelHeight = 1.0 / (double) sz_ppi.y;
#endif
#endif

}

/* font stuff */

static const char *fontname_R6 = "-*-helvetica-%s-%s-*-*-%d-*-*-*-*-*-*-*";
static const char *symbolname	 = "-adobe-symbol-*-*-*-*-%d-*-*-*-*-*-*-*";

static const char *slant[] = {"r", "o"};
static const char *weight[] = {"medium", "bold"};

#include <wx/hashmap.h>
WX_DECLARE_VOIDPTR_HASH_MAP( wxFont, FontHash);

#ifdef HASH_FONTS
static wxHashMap *font_htab = NULL; /* what about a wxHashSet? */
#endif

struct _FontMetricCache {
    int ascent[255];
    int descent[255];
    int width[255];
    int font_ascent;
    int font_descent;
    int max_width;
};

#define SMALLEST 2

static wxFont* RwxLoadFont (wxDesc *gtkd, int face, int size)
{
    wxString fontname;
    wxFont *tmp_font;
    int pixelsize;

    if (face < 1 || face > 5)
	face = 1;

    if (size < SMALLEST) size = SMALLEST;

    /* Here's a 1st class fudge: make sure that the Adobe design sizes
       8, 10, 11, 12, 14, 17, 18, 20, 24, 25, 34 can be obtained via
       an integer "size" at 100 dpi, namely 6, 7, 8, 9, 10, 12, 13,
       14, 17, 18, 24 points. It's almost y = x * 100/72, but not
       quite. The constants were found using lm(). --pd */
    if (IS_100DPI) size = (int) R_rint (size * 1.43 - 0.4);

    /* 'size' is the requested size, 'pixelsize' the size of the
       actually allocated font*/
    pixelsize = size;

    tmp_font = new wxFont(pixelsize, wxFONTFAMILY_SWISS, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);

#if 0
    if(face == 5)
        fontname = wxString::Format(symbolname, pixelsize);
    else
	fontname = wxString::Format(fontname_R6,
				    weight[(face-1)%2],
				    slant[((face-1)/2)%2],
				    pixelsize);

    tmp_font = gdk_font_load(fontname);

    if (!tmp_font) {
	static int near[]=
	{14,14,14,17,17,18,20,20,20,20,24,24,24,25,25,25,25};
	/* 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29  */
	
	/* If ADOBE_SIZE(pixelsize) is true at this point then
	   the user's system does not have the standard ADOBE font set
	   so we just have to use a "fixed" font.
	   If we can't find a "fixed" font then something is seriously
	   wrong */
	if (ADOBE_SIZE (pixelsize)) {
	    tmp_font = gdk_font_load ("fixed");
	    if (!tmp_font)
		error("Could not find any X11 fonts\nCheck that the Font Path is correct.");
	}
	
	if (pixelsize < 8)
	    pixelsize = 8;
	else if (pixelsize == 9)
	    pixelsize = 8;
	else if (pixelsize >= 13 && pixelsize < 30) 
	    pixelsize = near[size-13];
	else
	    pixelsize = 34;
	
	g_free(fontname);
	if(face == 5)
	    fontname = g_strdup_printf(symbolname, pixelsize);
	else
	    fontname = g_strdup_printf(fontname_R6,
				       weight[(face-1)%2],
				       slant[((face-1)/2)%2],
				       pixelsize);	    
	tmp_font = gdk_font_load (fontname);
    }
    
    if(tmp_font) {
#ifdef HASH_FONTS
        font_htab[fontname] = tmp_font;
#endif
	if (fabs( (pixelsize - size)/(double)size ) > 0.2)
	    warning("wxWidgets used font size %d when %d was requested",
		    pixelsize, size);
    }
#endif 

    return tmp_font;
}

static void SetFont(NewDevDesc *dd, int face, int size)
{
    wxFont *tmp_font;
    wxDesc *gtkd = (wxDesc *) dd->deviceSpecific;

    if (face < 1 || face > 5) face = 1;

    if (gtkd->drawingContext && !gtkd->usefixed &&
	(size != gtkd->fontsize	|| face != gtkd->fontface)) {
	tmp_font = RwxLoadFont(gtkd, face, size);
	if(tmp_font) {
	    gtkd->font = tmp_font;
	    gtkd->fontface = face;
	    gtkd->fontsize = size;
            SET_DC_FONT(gtkd, tmp_font);
	} else 
	    error("X11 font at size %d could not be loaded", size);
    }
}


static int SetBaseFont(wxDesc *gtkd)
{
    gtkd->fontface = 1;
    gtkd->fontsize = 12;
    gtkd->usefixed = 0;

#ifdef HASH_FONTS
    if(font_htab == NULL) {
	font_htab = new FontHash(10); /* XXX 10 is a made up number! */
    }
#endif

    gtkd->font = RwxLoadFont (gtkd, gtkd->fontface, gtkd->fontsize);


    if(gtkd->font != NULL) {
        SET_DC_FONT(gtkd, (wxFont *) NULL);
	return 1;
    }

#if 0
    gtkd->usefixed = 1;
    wxFont *tmp = new wxFont(gtkd->fontsize, wxFONTFAMILY_TELETYPE,
                             wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL); /* gdk_font_load ("fixed");*/


    if(tmp != NULL) {
        SET_DC_FONT(gtkd, (wxFont *) tmp);
        SET_DC_FONT(gtkd, (wxFont *) NULL);
	return 1;
    }
#endif

    return 0;
}


/* set the r, g, b, and pixel values of gcol to color */
static void SetColor(wxColour *gcol, int color)
{
    int red, green, blue;

    red = R_RED(color);
    green = R_GREEN(color);
    blue = R_BLUE(color);

    gcol->Set(red, green, blue);
}

/* set the line type */
static void SetLineType(NewDevDesc *dd, int newlty, double nlwd)
{
    int i, j, newlwd;
    wxDesc *gtkd = (wxDesc *) dd->deviceSpecific;

    newlwd = (int) nlwd;
    if(newlty != gtkd->lty || newlwd != gtkd->lwd) {
	gtkd->lty = newlty;
	gtkd->lwd = newlwd;

	if(newlty == 0) {
	    if(newlwd <= 1)
		newlwd = 0;

	    gtkd->pen->SetStyle(gtkd->lty); /* SOLID, DOT, LONG_DASH, SHORT_DASH, DOT_DASH */
	    gtkd->pen->SetWidth(newlwd);
	}
	else {
	    if(newlwd < 1)
		newlwd = 1;

            wxDash* dashes = new wxDash[8];

	    for(i = 0; (i < 8) && (newlty != 0); i++) {
		j = newlty & 15;
		if(j == 0) j = 1;
		j = j * newlwd;
		if(j > 255) j = 255;
		dashes[i] = j;
		newlty = newlty >> 4;
	    }

	    /* set dashes */
            gtkd->pen->SetDashes(i, dashes);
            gtkd->pen->SetJoin(wxJOIN_ROUND);
            gtkd->pen->SetCap(wxCAP_BUTT);
            gtkd->pen->SetWidth(newlwd);
            gtkd->pen->SetStyle(wxSHORT_DASH); /* GDK_LINE_ON_OFF_DASH */
	}
    }
}


/* Called in OnPaint() when we are about to draw. */
static int initialize(NewDevDesc *dd, wxDC *dc)
{
    wxDesc *gtkd;
    gtkd = (wxDesc *) dd->deviceSpecific;
    if(gtkd == NULL) return(FALSE);
    if(gtkd->drawing == NULL) return(FALSE);

    /* create gc */
//    if(gtkd->drawingContext)
//      delete gtkd->drawingContext;

#ifdef RESET_DRAWING_CONTEXT    
    if(!gtkd->drawingContext)
#endif
        gtkd->drawingContext = dc; 

    /* set the cursor */
/*XXX
    gtkd->gcursor = gdk_cursor_new(GDK_CROSSHAIR);
    gdk_window_set_cursor(gtkd->drawing->window, gtkd->gcursor);
*/

   gtkd->drawingContext->Clear();

    /* set window bg */
    /* Does this brush disappear when we exit this routine. */
    wxBrush bgBrush = *wxTRANSPARENT_BRUSH;
    if(gtkd->gcol_bg)
        bgBrush.SetColour(*gtkd->gcol_bg);
    gtkd->drawingContext->SetBackground(bgBrush);

    /*
    if(gtkd->gcol_bg)  {
        gtkd->pen->SetColour(*gtkd->gcol_bg);
    }
    */

   RESET_PEN(gtkd);
   RESET_BRUSH(gtkd);

   dc->SetFont(*gtkd->font);

    /* create offscreen drawable */
#ifdef USE_PIXMAP
    if(gtkd->windowWidth > 0 && gtkd->windowHeight > 0) {
	gtkd->pixmap = gdk_pixmap_new(gtkd->drawing->window,
				      gtkd->windowWidth, gtkd->windowHeight,
				      -1);
	gdk_draw_rectangle(gtkd->pixmap, gtkd->wgc, TRUE, 0, 0,
			   gtkd->windowWidth, gtkd->windowHeight);
    }
#endif


/* The next two bits SetColor and pixmap material were taken
   from WX_Open to here to be shared by the embedding device
   Could be moved to wxDeviceDriver if not to be repeated
   at every new page
*/
    /* setup background color */
    SetColor(gtkd->gcol_bg, R_RGB(255, 255, 255)); /* FIXME canvas color */

    return FALSE;
}

void 
RwxCanvas::SizeEvent(wxSizeEvent &ev)
{
ANNOUNCE(SizeEvent);

    if(dd == NULL) return;
    if(gtkd == NULL) return;

    if(ev.GetEventObject() != this)
        return;

    wxSize s = ev.GetSize();
    s = GetClientSize();
    computeSize(s);
    this->Refresh();

#if 0
    wxDC *dc = new wxClientDC(this);
    if(dc) {
      initialize((NewDevDesc*) dd, dc);
      dc->Clear();
      gtkd->drawingContext = NULL;
      delete dc;
    }
#endif
}

void
RwxCanvas::computeSize(const wxSize s)
{
    int w = s.GetWidth();
    int h = s.GetHeight();
    /* check for resize */
    if(((gtkd->windowWidth != w) || (gtkd->windowHeight != h))) {
	gtkd->windowWidth = w;
	gtkd->windowHeight = h;

	gtkd->resize = TRUE;
        Resize();
    }
}

static void WX_resize(NewDevDesc *dd);


void
RwxCanvas::OnPaint(wxPaintEvent &ev)
{
ANNOUNCE(OnPaint)

    update();
}

void RwxCanvas::update()
{
    if(!gtkd)
        return;

    wxPen *open = gtkd->pen;
    wxBrush  *obrush = gtkd->brush;

    wxPaintDC pdc(this);
    wxClientDC dc(this);

    gtkd->drawingContext = &dc;

    gtkd->brush = (wxBrush *) &(dc.GetBrush());
    gtkd->pen = (wxPen*) &(dc.GetPen());

    RESET_SIZE(gtkd);
    initialize((NewDevDesc *) dd, &dc);

    if(gtkd->resize != 0) {
	WX_resize((NewDevDesc *) dd); 
    }
#ifdef USE_PIXMAP
    else if (gtkd->pixmap) {
	gdk_draw_pixmap(gtkd->drawing->window, gtkd->wgc, gtkd->pixmap,
			event->area.x, event->area.y, event->area.x, event->area.y,
			event->area.width, event->area.height);
    }
#endif
    else {
      GEplayDisplayList((GEDevDesc*) GEgetDevice(Rf_ndevNumber((pDevDesc) dd)));
    }

    gtkd->drawingContext = NULL;
    gtkd->brush = obrush;
    gtkd->pen = open;

}

void 
RwxDeviceFrame::OnCloseWindow(wxCloseEvent &ev)
{
#if 0
    /* The this has already gone if this comes from a window frame event.*/
    if(canvas)
        canvas->closeDevice();
#endif
}


/* create window etc */
static wxWindow * WX_Open(pDevDesc dd, wxDesc *gtkd, const char *dsp, double w, double h)
{
ANNOUNCE(WX_Open)

    int iw, ih;

    /* initialise pointers */
    gtkd->drawingContext = NULL;
//XXX    gtkd->gcursor = NULL;

/*XXX initialize color  */
    /* create window etc */
    getScreenDimensions(gtkd);
    gtkd->windowWidth = iw = (int) (w / gtkd->pixelWidth);
    gtkd->windowHeight = ih = (int) (h / gtkd->pixelHeight);


    wxString title = R_wxString(dsp);
    wxPoint pos = wxPoint(10, 10);
    wxSize dims = wxSize(iw, ih);

    RwxDeviceFrame *frame = new RwxDeviceFrame(title, pos, dims);
    RwxCanvas *canvas = new RwxCanvas(frame, gtkd, (pDevDesc) dd);

    frame->Show(true);

    gtkd->drawing = canvas;
    gtkd->window = frame;

    /* drawingarea properties */
    gtkd->drawing->SetSize(iw, ih);

    return canvas;
}

static double 
WX_StrWidth(const char *str,
            R_GE_gcontext *gc,
            pDevDesc dd)
{
    int size;
    wxDesc *gtkd = (wxDesc *) dd->deviceSpecific;

    size = (int) (gc->cex * gc->ps + 0.5);
    SetFont(dd, gc->fontface, size);

    wxCoord w = 0;
    wxDC *dc;
    if(gtkd->drawingContext)
        dc = gtkd->drawingContext;
    else {
        dc = new wxScreenDC();
        dc->SetFont(*(gtkd->font));
    }

    dc->GetTextExtent(R_wxString(str), &w, (wxCoord *) NULL);
    if(!gtkd->drawingContext)
        delete dc;

    return (double) w;
}

static void WX_MetricInfo(int c,
			   R_GE_gcontext *gc,
			   double* ascent, double* descent,
			   double* width, pDevDesc dd)
{

ANNOUNCE(MetricInfo)

    int size;
    int lbearing, rbearing, iascent, idescent;
    wxCoord iwidth, iheight;
    int maxwidth;
    char tmp[2];
    wxDesc *gtkd = (wxDesc *) dd->deviceSpecific;

    size = (int) (gc->cex * gc->ps + 0.5);
    SetFont(dd, gc->fontface, size);

    if(c == 0) {
	maxwidth = 0;

	for(c = 0; c <= 255; c++) {
            wxCoord idescent;
	    snprintf(tmp, 2, "%c", (char) c);
            gtkd->drawingContext->GetTextExtent(R_wxString(tmp), &iwidth, &iheight, &idescent);

	    if (iwidth > maxwidth)
		maxwidth = iwidth;
	}

	*ascent = (double) iheight; //gtkd->font->ascent;
	*descent = idescent;
	*width = (double) maxwidth;
    }
    else {
	snprintf(tmp, 2, "%c", (char) c);

            gtkd->drawingContext->GetTextExtent(R_wxString(tmp), &iwidth, NULL, &idescent);

	*ascent = (double) size; /* iheight     was size; */
	*descent = (double) idescent;
	*width = (double) iwidth;
	/* This formula does NOT work for spaces
	*width = (double) (lbearing+rbearing);
	*/
    }

#if 0
    fprintf(stderr, "<MetricInfo>: ascent = %lf, descent = %lf, width = %lf\n", *ascent, *descent, *width);
#endif

}

/* set clipping */
static void WX_Clip(double x0, double x1, double y0, double y1, pDevDesc dd)
{
#if RWX_DEBUG
fprintf(stderr, "Clip %lf %lf %lf %lf\n", x0, y0, x1, y1);
#endif
//XXX  return;

    wxDesc *gtkd = (wxDesc *) dd->deviceSpecific;
    int x, clipLeft, clipRigth, clip_width, clip_height, clip_y;

    dd->clipLeft = (int) MIN(x0, x1);
    clip_width = abs( (int) x0 - (int) x1) + 1;

    dd->clipBottom = clip_y  = (int) MIN(y0, y1);
    clip_height = abs( (int) y0 - (int) y1) + 1;
    dd->clipTop = dd->clipBottom + clip_height;


    if(gtkd->drawingContext && 0) {
        gtkd->drawingContext->DestroyClippingRegion();
        gtkd->drawingContext->SetClippingRegion(clipLeft,
                                                clip_y,
                                                clip_width,
                                                clip_height);
    }
}

static void WX_Size(double *left, double *right,
		     double *bottom, double *top,
		     NewDevDesc *dd)
{
    wxDesc *gtkd = (wxDesc *) dd->deviceSpecific;

    *left = 0.0;
    *right =  gtkd->windowWidth;
    *bottom = gtkd->windowHeight;
    *top = 0.0;
}

static void WX_resize(NewDevDesc *dd)
{
    wxDesc *gtkd = (wxDesc *) dd->deviceSpecific;

    if (gtkd->resize != 0) {
	dd->left = 0.0;
	dd->right = gtkd->windowWidth;
	dd->bottom = gtkd->windowHeight;
	dd->top = 0.0;
	gtkd->resize = 0;

#ifdef USE_PIXMAP
	if(gtkd->pixmap)
	    gdk_pixmap_unref(gtkd->pixmap);
#endif
	if(gtkd->windowWidth > 0 && gtkd->windowHeight > 0) {
#ifdef USE_PIXMAP
	    gtkd->pixmap = gdk_pixmap_new(gtkd->drawing->window,
					  gtkd->windowWidth, gtkd->windowHeight,
					  -1);
#endif
	    if(gtkd->drawingContext) {
		gtkd->pen->SetColour(*gtkd->gcol_bg);
                RESET_PEN(gtkd);
                gtkd->drawingContext->DestroyClippingRegion();
#ifdef USE_PIXMAP
		gdk_draw_rectangle(gtkd->pixmap, gtkd->wgc, TRUE, 0, 0,
				   gtkd->windowWidth, gtkd->windowHeight);
#endif
	    }
	}
	GEplayDisplayList((GEDevDesc*) GEgetDevice(Rf_ndevNumber((pDevDesc)dd)));
    }
}

/* clear the drawing area */
static void WX_NewPage(R_GE_gcontext *gc,
			pDevDesc dd)
{
ANNOUNCE(NewPage)
    wxDesc *gtkd;

    if(dd == NULL) return;

    gtkd = (wxDesc *) dd->deviceSpecific;

    RESET_SIZE(gtkd);


//XXX free the existing context if present.
    wxDC *dc;
    gtkd->drawingContext = dc = new wxClientDC(gtkd->drawing); /* tried wxScreenDC() */
    if(gtkd->drawingContext) {
        gtkd->drawingContext->DestroyClippingRegion(); 
        gtkd->drawingContext->SetBackground(*wxLIGHT_GREY_BRUSH);
        gtkd->drawingContext->Clear();
    }
    
    RwxCanvas *canvas;
    canvas = (RwxCanvas *) gtkd->drawing;
//    canvas->update();
    canvas->computeSize(canvas->GetClientSize());
    gtkd->drawingContext = dc;
}

/* kill off the window etc */
static void WX_Close(pDevDesc dd)
{
    wxDesc *gtkd = (wxDesc *) dd->deviceSpecific;

    if(gtkd->window)
	delete gtkd->window;

    gtkd->window = NULL;

#ifdef USE_PIXMAP
    if(gtkd->pixmap) 
	delete gtkd->drawingContext;
#endif
    gtkd->drawingContext = NULL;


    delete gtkd;
}

#define title_text_inactive "R graphics wx device %d"
#define title_text_active "R graphics wx device %d - Active"

static void WX_Activate(pDevDesc dd)
{
    wxDesc *gtkd;
    int devnum;
    char title_text[100];//XXX big enough?

    gtkd = (wxDesc *) dd->deviceSpecific;
    if(gtkd == NULL) return;
    if(gtkd->window == NULL) return;
    if(CHECK_wxDESC)
	return;

    devnum = Rf_ndevNumber((pDevDesc) dd) + 1;

//XXX make certain we don't overflow here.
    sprintf(title_text, title_text_active, devnum);


    gtkd->window->SetTitle(R_wxString(title_text));
}

static void WX_Deactivate(pDevDesc dd)
{
    wxDesc *gtkd;
    int devnum;
    char title_text[100];

    gtkd = (wxDesc *) dd->deviceSpecific;
    if(gtkd == NULL) return;

    if(!gtkd->window)
        return;
/*
    if(CHECK_wxDESC)
	return;
*/

    devnum = Rf_ndevNumber((pDevDesc)dd) + 1;
    
    sprintf(title_text, title_text_inactive, devnum);

    gtkd->window->SetTitle(R_wxString(title_text));
}



static void WX_Rect(double x0, double y0, double x1, double y1,
		     R_GE_gcontext *gc,
		     pDevDesc dd)
{
#if RWX_DEBUG
fprintf(stderr, "Rect filled  (%lf, %lf) (%lf, %lf)\n", x0, y0, x1, y1);
#endif
    double tmp;
    wxDesc *gtkd = (wxDesc *) dd->deviceSpecific;
    wxColour gcol_fill, gcol_outline;


    if(CHECK_wxDESC)
	return;

    if(x0 > x1) {
	tmp = x0;
	x0 = x1;
	x1 = tmp;
    }
    if(y0 > y1) {
	tmp = y0;
	y0 = y1;
	y1 = tmp;
    }


    if (R_OPAQUE(gc->fill)) {
#if RWX_DEBUG
fprintf(stderr, "  filled\n");
#endif
	SetLineType(dd, gc->lty, gc->lwd);
	SetColor(&gcol_fill, gc->fill);
        wxBrush brush;
	brush.SetColour(gcol_fill);
        gtkd->drawingContext->SetBrush(brush);

	gtkd->drawingContext->DrawRectangle(
			   (wxCoord) x0, (wxCoord) y0,
			   (wxCoord) x1 - (wxCoord) x0,
			   (wxCoord) y1 - (wxCoord) y0);
#ifdef USE_PIXMAP
	gdk_draw_rectangle(gtkd->pixmap,
			   gtkd->wgc, TRUE,
			   (int) x0, (int) y0,
			   (int) x1 - (int) x0,
			   (int) y1 - (int) y0);
#endif
         
        gtkd->brush = (wxBrush *) wxTRANSPARENT_BRUSH;
        RESET_BRUSH(gtkd);
    }
    if (R_OPAQUE(gc->col)) {
	SetColor(&gcol_outline, gc->col);
	gtkd->pen->SetColour(gcol_outline);
        RESET_PEN(gtkd);
        RESET_BRUSH(gtkd);

	SetLineType(dd, gc->lty, gc->lwd);

	gtkd->drawingContext->DrawRectangle(
			   (wxCoord) x0, (wxCoord) y0,
			   (wxCoord) x1 - (wxCoord) x0,
			   (wxCoord) y1 - (wxCoord) y0);
#ifdef USE_PIXMAP
	gdk_draw_rectangle(gtkd->pixmap,
			   gtkd->wgc, FALSE,
			   (int) x0, (int) y0,
			   (int) x1 - (int) x0,
			   (int) y1 - (int) y0);
#endif
    }
}

static void WX_Circle(double x, double y, double r,
		       R_GE_gcontext *gc,
		       pDevDesc dd)
{
ANNOUNCE(Circle)
    wxColour gcol_fill, gcol_outline;
    int ix, iy, ir;
    wxDesc *gtkd = (wxDesc *) dd->deviceSpecific;

    if(CHECK_wxDESC)
	return;

#if 0
    ix = x - r;
    iy = y - r;
#endif
    ir = 2 * floor(r + 0.5);

    if (R_OPAQUE(gc->fill)) {
	SetColor(&gcol_fill, gc->fill);
	gtkd->brush->SetColour(gcol_fill);
        RESET_BRUSH(gtkd);
	gtkd->drawingContext->DrawCircle((wxCoord) x, (wxCoord) y, (wxCoord) ir); /* Try DrawEllipse and DrawEllipticArc */
/*XXX Pixmap */

    }
    if (R_OPAQUE(gc->col)) {
	SetColor(&gcol_outline, gc->col);
        gtkd->pen->SetColour(gcol_outline);
        RESET_PEN(gtkd);
	SetLineType(dd, gc->lty, gc->lwd);
	gtkd->drawingContext->DrawCircle((wxCoord) x, (wxCoord) y, (wxCoord) ir); /* Try DrawEllipse and DrawEllipticArc */
#ifdef USE_PIXMAP
/*XXX pixmap */
#endif
    }
}

static void WX_Line(double x1, double y1, double x2, double y2,
		     R_GE_gcontext *gc,
		     pDevDesc dd)
{
ANNOUNCE(Line)
    wxDesc *gtkd = (wxDesc *) dd->deviceSpecific;
    wxColour gcol_fill;
    int ix1, iy1, ix2, iy2;

    if(!gtkd->drawing || !gtkd->drawingContext)
	return;

    ix1 = (int) x1;  iy1 = (int) y1;
    ix2 = (int) x2;  iy2 = (int) y2;

    if (R_OPAQUE(gc->col)) {
	SetColor(&gcol_fill, gc->col);
	gtkd->pen->SetColour(gcol_fill);
        RESET_PEN(gtkd);
	SetLineType(dd, gc->lty, gc->lwd);
	gtkd->drawingContext->DrawLine(ix1, iy1, ix2, iy2);
/*XXX pixmap */
    } 

}

static void WX_Polyline(int n, double *x, double *y, 
			 R_GE_gcontext *gc,
			 pDevDesc dd)
{
ANNOUNCE(Polyline)
    wxDesc *gtkd = (wxDesc *) dd->deviceSpecific;
    wxColour gcol_fill;
    wxPoint *points;
    int i;

    if(!gtkd->drawing || !gtkd->drawingContext)
	return;

    points = new wxPoint[n];

    for(i = 0; i < n; i++) {
	points[i].x = (int) x[i];
	points[i].y = (int) y[i];
    }

    if (R_OPAQUE(gc->col)) {
	SetColor(&gcol_fill, gc->col);
	gtkd->pen->SetColour(gcol_fill);
	RESET_PEN(gtkd); //XXX brush
	SetLineType(dd, gc->lty, gc->lwd);

	gtkd->drawingContext->DrawLines(n, points);
/* pixmap */
    }

    free(points);
}

static void WX_Polygon(int n, double *x, double *y, 
			R_GE_gcontext *gc,
			pDevDesc dd)
{
ANNOUNCE(Polygon)
    wxDesc *gtkd = (wxDesc *) dd->deviceSpecific;
    wxColour gcol_fill, gcol_outline;
    wxPoint *points;
    int i;
 
    if(!gtkd->drawing || !gtkd->drawingContext)
	return;

    points = new wxPoint[n];

    for(i = 0; i < n; i++) {
	points[i].x = (int) x[i];
	points[i].y = (int) y[i];
    }

    if (R_OPAQUE(gc->fill)) {
	SetColor(&gcol_fill, gc->fill);
	gtkd->brush->SetColour(gcol_fill);
	RESET_BRUSH(gtkd);
        
        gtkd->drawingContext->DrawPolygon(n, points);
#ifdef USE_PIXMAP
	gdk_draw_polygon(gtkd->pixmap,
			 gtkd->wgc, TRUE, points, n);
#endif

	gtkd->brush = (wxBrush *) wxTRANSPARENT_BRUSH;
	RESET_BRUSH(gtkd);
    }
    if (R_OPAQUE(gc->col)) {
	SetColor(&gcol_outline, gc->col);
        gtkd->pen->SetColour(gcol_outline);
        RESET_PEN(gtkd);

	SetLineType(dd, gc->lty, gc->lwd);

	gtkd->drawingContext->DrawPolygon(n, points);
#ifdef USE_PIXMAP
	gdk_draw_polygon(gtkd->pixmap,
			 gtkd->wgc, FALSE, points, n);
#endif
    }

    free(points);
}

static void WX_Text(double x, double y, const char *str, 
		     double rot, double hadj, 
		     R_GE_gcontext *gc,
		     pDevDesc dd)
{
#if RWX_DEBUG
fprintf(stderr, "Text %s  (%lf, %lf)\n", str, x, y);
#endif

    wxDesc *gtkd = (wxDesc *) dd->deviceSpecific;
    wxColour gcol_fill;
    int size;
    double rrot = DEG2RAD * rot;


    if(!gtkd->drawing || !gtkd->drawingContext)
	return;
    size = (int) (gc->cex * gc->ps + 0.5);

    SetFont(dd, gc->fontface, size);
    SET_DC_FONT(gtkd, (wxFont *) NULL);

    if (R_OPAQUE(gc->col)) {
        wxString w_str; 
	w_str = R_wxString(str);

	SetColor(&gcol_fill, gc->col);
	gtkd->pen->SetColour(gcol_fill);
	gtkd->pen->SetColour(wxT("black"));
        RESET_PEN(gtkd);

        wxCoord h, d;
        gtkd->drawingContext->GetTextExtent(w_str, NULL, &h, &d);

        if(rot == 0) {
            y = y - h + d/2.; /* - Half of h (h/2.) is too low.
                                 - h is too high  */
        } else if(rot == 90) {
            x = x - h/2;
        }
	gtkd->drawingContext->DrawRotatedText(w_str, (wxCoord) x, (wxCoord) y, rot);
/*XX Same to pixmap */
    }
}

#ifdef TEST_ALL
typedef struct _WX_locator_info WX_locator_info;

struct _WX_locator_info {
    guint x;
    guint y;
    gboolean button1;
};

static void locator_button_press(GtkWidget *widget,
				 GdkEventButton *event,
				 gpointer user_data)
{
    WX_locator_info *info;

    info = (WX_locator_info *) user_data;

    info->x = event->x;
    info->y = event->y;
    if(event->button == 1)
	info->button1 = TRUE;
    else
	info->button1 = FALSE;

    gtk_main_quit();
}

static Rboolean WX_Locator(double *x, double *y, pDevDesc dd)
{
    wxDesc *gtkd = (wxDesc *) dd->deviceSpecific;
    WX_locator_info *info;
    guint handler_id;
    gboolean button1;

    info = g_new(WX_locator_info, 1);

    /* Flush any pending events */
    while(gtk_events_pending())
	gtk_main_iteration();

    /* connect signal */
    handler_id = gtk_signal_connect(WX_OBJECT(gtkd->drawing), "button-press-event",
				    (GtkSignalFunc) locator_button_press, (gpointer) info);

    /* run the handler */
    gtk_main();

    *x = (double) info->x;
    *y = (double) info->y;
    button1 = info->button1;

    /* clean up */
    gtk_signal_disconnect(WX_OBJECT(gtkd->drawing), handler_id);
    g_free(info);

    if(button1)
	return TRUE;

    return FALSE;
}
#endif


static void WX_Mode(int mode, pDevDesc dd)
{
#ifdef XSYNC
    if(mode == 0) ; /* gdk_flush();*/
#else
    /* gdk_flush(); */
#endif
}

static void WX_Hold(pDevDesc dd)
{
}


typedef void (*VoidFunPtr)();
typedef Rboolean (*BooleanFunPtr)();
/* Device driver entry point */
Rboolean
wxDeviceDriver(pDevDesc odd, const char *title, wxWindow *widget, 
               double width, double height, double pointsize)
{
    pDevDesc dd;
    int ps;
    char tmp[2];
    int c, rbearing, lbearing;
    double max_rbearing, min_lbearing;
    wxDesc *gtkd;

    dd = (pDevDesc) odd;

    if(!(gtkd = (wxDesc *) malloc(sizeof(wxDesc))))
	return FALSE;

    gtkd->drawing = (wxWindow *) NULL;
    gtkd->drawingContext = NULL;
    gtkd->font = NULL;
    gtkd->pen = new wxPen();
    gtkd->brush = new wxBrush();

    gtkd->resize = 0;

    dd->deviceSpecific = (void *) gtkd;

    /* font loading */
    ps = (int) pointsize;
    if(ISNAN(ps) || ps < 6 || ps > 24) ps = 12;
    ps = 2 * (ps / 2);
    gtkd->fontface = -1;
    gtkd->fontsize = -1;
    dd->startfont = 1; 
    dd->startps = ps;
    dd->startcol = R_RGB(0, 0, 0);
    dd->startfill = NA_INTEGER;
    dd->startlty = LTY_SOLID; 
    dd->startgamma = 1;

    /* initialise line params */
    gtkd->lty = -1;
    gtkd->lwd = -1;

    gtkd->gcol_bg = new wxColour(0, 0, 255);

    /* device driver start */
    if(widget != NULL)  {
        gtkd->drawing = widget;
        gtkd->window = NULL;
        getScreenDimensions(gtkd);

        RwxCanvas *p;  /* Set the fields in RwxCanvas */
        p = dynamic_cast<RwxCanvas *>(widget);
        if(p) {
            p->SetDevInfo(gtkd, odd);
        }
        RESET_SIZE(gtkd); /* get the width and height of the widget.*/

    } else if(!(widget = WX_Open(dd, gtkd, title, width, height))) {
	free(gtkd);
	return FALSE;
    }

          /* Set base font in the drawing context */
    if(!SetBaseFont(gtkd)) {
	Rprintf("can't find X11 font\n");
	return FALSE;
    }

    wxDC *dc = new wxClientDC(widget);
    initialize(dd, dc);

//XXX    dd->newDevStruct = 1;

    /* setup data structure */
#if 0 /* This is never needed by the graphics code, except perhaps for cloning!*/
    dd->open = (Rboolean (*)()) WX_Open;
#endif

/* Different instances of the same compiler complain about this*/
#if 1 
#if 0
    dd->close = (void (*)(pDevDesc)) WX_Close;
#else
    dd->close = (void (*)()) WX_Close;
#endif
    dd->activate = (void (*)(pDevDesc)) WX_Activate;
    dd->deactivate = (void (*)(pDevDesc)) WX_Deactivate;
    dd->size = (void (*)(double*, double *, double *, double *,pDevDesc)) WX_Size;
    dd->newPage = (void (*)(R_GE_gcontext*, pDevDesc)) WX_NewPage;
    dd->clip = (void (*)(double, double, double, double, pDevDesc))  WX_Clip;
    dd->strWidth = (double (*)(const char*, R_GE_gcontext*, pDevDesc)) WX_StrWidth;
    dd->text = (void (*)(double, double, const char*, double, double, R_GE_gcontext*, pDevDesc)) WX_Text;
    dd->rect = (void (*)(double, double, double, double, R_GE_gcontext*, pDevDesc)) WX_Rect;
    dd->circle = (void (*)(double, double, double, R_GE_gcontext*, pDevDesc)) WX_Circle;
    dd->line = (void (*)(double, double, double, double, R_GE_gcontext*, pDevDesc)) WX_Line;
    dd->polyline = (void (*)(int, double*, double*, R_GE_gcontext*, pDevDesc)) WX_Polyline;
    dd->polygon = (void (*)(int, double*, double*, R_GE_gcontext*, pDevDesc)) WX_Polygon;
#ifdef USE_LOCATOR
    dd->locator = (BooleanFunPtr) WX_Locator;
#endif
    dd->mode = (void (*)(int, pDevDesc)) WX_Mode;
#if 0
    dd->hold = (void (*)(pDevDesc)) WX_Hold;
#endif
    dd->metricInfo = (void (*)(int, R_GE_gcontext*, double*, double*, double*, pDevDesc)) WX_MetricInfo;
#else
    dd->close = (void (*)()) WX_Close;
    dd->activate = (void (*)()) WX_Activate;
    dd->deactivate = (void (*)()) WX_Deactivate;
    dd->size = (void (*)()) WX_Size;
    dd->newPage = (void (*)()) WX_NewPage;
    dd->clip = (void (*)())  WX_Clip;
    dd->strWidth = (double (*)()) WX_StrWidth;
    dd->text = (void (*)()) WX_Text;
    dd->rect = (void (*)()) WX_Rect;
    dd->circle = (void (*)()) WX_Circle;
    dd->line = (void (*)()) WX_Line;
    dd->polyline = (void (*)()) WX_Polyline;
    dd->polygon = (void (*)()) WX_Polygon;
#ifdef USE_LOCATOR
    dd->locator = (BooleanFunPtr) WX_Locator;
#endif
    dd->mode = (void (*)()) WX_Mode;
    dd->hold = (void (*)()) WX_Hold;
    dd->metricInfo = (void (*)()) WX_MetricInfo;
#endif
/*XX what about dot. */

    dd->left = 0;
    dd->right = gtkd->windowWidth;
    dd->bottom = gtkd->windowHeight;
    dd->top = 0;

/*XXX */
    dd->clipLeft = 0;
    dd->clipRight = 0;
    dd->clipBottom = 0;
    dd->clipTop = 0;

//XXXX
    /* nominal character sizes */
    max_rbearing = 0;
    min_lbearing = 10000; /* just a random big number */
    wxCoord iwidth, iheight, max_height = 0;
#if 0
    wxClientDC dc(gtkd->drawing);
    for(c = 0; c <= 255; c++) {
	snprintf(tmp, 2, "%c", (char) c);
        wxString str = wxString(tmp, wxConvUTF8);
        dc.GetTextExtent(str, &iwidth, &iheight);
	if(lbearing < min_lbearing || c == 0)
	    min_lbearing = lbearing;
        if(iheight > max_height)
            max_height = iheight;
	if(iwidth > max_rbearing)
	    max_rbearing = iwidth;
    }
#else
    max_height = 14;
#endif

    //XXX
    dd->cra[0] = 14; // max_rbearing - min_lbearing;
    dd->cra[1] = (double) max_height; // (double) gtkd->font->ascent + (double) gtkd->font->descent;

    /* character addressing offsets */
    dd->xCharOffset = 0.4900;
    dd->yCharOffset = 0.3333; 
    dd->yLineBias = 0.1; 

    /* inches per raster unit */
    //  getScreenDimensions(gtkd);


    dd->ipr[0] = gtkd->pixelWidth;
    dd->ipr[1] = gtkd->pixelHeight;

    /* device capabilities */
#if 0
    dd->canResizePlot= TRUE;
    dd->canChangeFont= FALSE;
    dd->canRotateText= TRUE;
    dd->canResizeText= TRUE;
#endif
    dd->canClip = CAN_CLIP; 
    dd->canHAdj = 0;/* not better? {0, 0.5, 1} */
    dd->canChangeGamma = FALSE;

    /* gtk device description stuff */
    gtkd->cex = 1.0;
    gtkd->srt = 0.0;
    gtkd->resize = FALSE;

/*XXX Add canGenMouseDown, canGenMouseMove, etc. */
    dd->displayListOn = TRUE;


    /* create offscreen drawable */
#ifdef USE_PIXMAP
    gtkd->pixmap = gdk_pixmap_new(gtkd->drawing->window,
				  gtkd->windowWidth, gtkd->windowHeight,
				  -1);
    gdk_gc_set_foreground(gtkd->wgc, &gtkd->gcol_bg);
    gdk_draw_rectangle(gtkd->pixmap, gtkd->wgc, TRUE, 0, 0,
		       gtkd->windowWidth, gtkd->windowHeight);
#endif


    /* finish */
    return TRUE;
}





bool 
RwxCanvas::closeDevice()
{
     if(!dd)
	 return(false);
     Rf_killDevice(Rf_ndevNumber((pDevDesc) dd));

     return(true);
}

BEGIN_EVENT_TABLE(RwxCanvas, wxWindow)
   EVT_SIZE( RwxCanvas::SizeEvent)
   EVT_PAINT(RwxCanvas::OnPaint)
END_EVENT_TABLE()


BEGIN_EVENT_TABLE(RwxDeviceFrame, wxFrame)
   EVT_CLOSE(  RwxDeviceFrame::OnCloseWindow)
END_EVENT_TABLE()



