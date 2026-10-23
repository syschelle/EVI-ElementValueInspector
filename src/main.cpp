#include "MainFrame.h"

#include <dcmtk/dcmdata/dcdict.h>
#include <wx/wx.h>

class EviApp final : public wxApp {
public:
    bool OnInit() override {
        if (!dcmDataDict.isDictionaryLoaded()) {
            wxMessageBox(
                "Das eingebaute DICOM-Dictionary konnte nicht geladen werden.\n"
                "The embedded DICOM dictionary could not be loaded.\n\n"
                "Der Build muss DCMTK mit DCMTK_DEFAULT_DICT=builtin erzeugen.\n"
                "The build must use DCMTK_DEFAULT_DICT=builtin.",
                "DICOM Dictionary", wxOK | wxICON_ERROR);
            return false;
        }
        auto* frame = new MainFrame();
        frame->Show(true);
        return true;
    }
};

wxIMPLEMENT_APP(EviApp);
