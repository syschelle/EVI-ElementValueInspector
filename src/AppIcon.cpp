#include "AppIcon.h"

#include <wx/icon.h>
#include <wx/toplevel.h>

void ApplyEviAppIcon(wxTopLevelWindow* window) {
    if (!window) return;

#ifdef _WIN32
    wxIcon icon(wxString(L"EVI_APP_ICON"), wxBITMAP_TYPE_ICO_RESOURCE);
    if (icon.IsOk()) {
        window->SetIcon(icon);
    }
#else
    (void)window;
#endif
}
