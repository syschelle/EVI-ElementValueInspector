#pragma once

class wxTopLevelWindow;

// Applies the embedded EVI application icon to a top-level window.
// On Windows the icon is compiled into evi.exe as the EVI_APP_ICON resource.
void ApplyEviAppIcon(wxTopLevelWindow* window);
