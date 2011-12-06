/* Not needed !*/
#include <wx/evtloop.h>
class WaitSizeEventLoop : public wxEventLoop
{
public:
    WaitSizeEventLoop(wxWindow *target) {
        SetTarget(target);
    }
    
    int Run() {
        while(Pending()) {
            Dispatch();
            if(checkFinished()) {
                return(0);
	    }
        }
	return(0);
    }

    void SetTarget(wxWindow *target) {
        _target = target;
        _target->GetClientSize(&w, &h);
    }
    bool checkFinished() {
        int nw, nh;
        _target->GetClientSize(&nw, &nh);        
	bool ans = nw != w || nh != h;
	if(ans)
	    fprintf(stderr, "Changed size\n");
	else
	    fprintf(stderr, "next\n");
        return(ans);
    }
    
protected:
    int w, h;
    wxWindow *_target;
};
