#include "MainFrame.h"
#include "DicomScanner.h"
#include "DicomUtils.h"
#include "TagDialog.h"

#include <wx/button.h>
#include <wx/choice.h>
#include <wx/dirdlg.h>
#include <wx/filedlg.h>
#include <wx/gauge.h>
#include <wx/listctrl.h>
#include <wx/msgdlg.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/splitter.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/thread.h>

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <sstream>

wxDEFINE_EVENT(EVT_SCAN_PROGRESS, wxThreadEvent);
wxDEFINE_EVENT(EVT_SCAN_COMPLETE, wxThreadEvent);
wxDEFINE_EVENT(EVT_SCAN_ERROR, wxThreadEvent);

namespace {

enum {
    ID_CHOOSE_FOLDER = wxID_HIGHEST + 100,
    ID_CHOOSE_TAG,
    ID_START_SCAN,
    ID_CANCEL_SCAN,
    ID_SHOW_ALL,
    ID_EXPORT,
    ID_LANGUAGE
};

wxString W(const std::string& text) {
    return wxString::FromUTF8(text.c_str());
}

std::string U8(const wxString& text) {
    const wxScopedCharBuffer buffer = text.ToUTF8();
    return buffer.data() ? std::string(buffer.data()) : std::string{};
}

wxStaticText* AddInfoPair(wxWindow* parent,
                          wxFlexGridSizer* grid,
                          wxStaticText*& keyOut,
                          int valueWidth = 250) {
    keyOut = new wxStaticText(parent, wxID_ANY, "-");
    wxFont keyFont = keyOut->GetFont();
    keyFont.MakeBold();
    keyOut->SetFont(keyFont);
    keyOut->SetForegroundColour(wxColour(80, 84, 89));

    auto* value = new wxStaticText(parent, wxID_ANY, "-", wxDefaultPosition,
                                   wxSize(valueWidth, -1), wxST_ELLIPSIZE_END | wxST_NO_AUTORESIZE);
    value->SetMinSize(wxSize(valueWidth, -1));

    auto* pair = new wxBoxSizer(wxVERTICAL);
    pair->Add(keyOut, 0, wxBOTTOM, 2);
    pair->Add(value, 0, wxEXPAND);
    grid->Add(pair, 1, wxEXPAND | wxRIGHT | wxBOTTOM, 14);
    return value;
}

std::string CsvEscape(const std::string& value) {
    bool quote = value.find_first_of(";\"\r\n") != std::string::npos;
    if (!quote) return value;
    std::string out = "\"";
    for (char c : value) {
        if (c == '\"') out += "\"\"";
        else out += c;
    }
    out += '\"';
    return out;
}

} // namespace

MainFrame::MainFrame()
    : wxFrame(nullptr, wxID_ANY, "ElementValueInspector (EVI) v0.2.8",
              wxDefaultPosition, wxSize(1500, 900))
{
    BuildUi();
    BindEvents();
    ApplyLanguage();
    ApplyModernStyle();
    Centre();
}

MainFrame::~MainFrame() {
    StopWorker();
}

wxString MainFrame::L(const char* de, const char* en) const {
    return wxString::FromUTF8(language_ == UiLanguage::German ? de : en);
}

void MainFrame::BuildUi() {
    rootPanel_ = new wxPanel(this);
    auto* root = new wxBoxSizer(wxVERTICAL);

    // Header with app identity and language selector.
    auto* header = new wxPanel(rootPanel_);
    auto* headerSizer = new wxBoxSizer(wxHORIZONTAL);
    auto* identity = new wxBoxSizer(wxVERTICAL);

    title_ = new wxStaticText(header, wxID_ANY, "ElementValueInspector (EVI)");
    wxFont titleFont = title_->GetFont();
    titleFont.SetPointSize(titleFont.GetPointSize() + 5);
    titleFont.MakeBold();
    title_->SetFont(titleFont);
    identity->Add(title_, 0, wxBOTTOM, 3);

    subtitle_ = new wxStaticText(header, wxID_ANY, "");
    subtitle_->SetForegroundColour(wxColour(90, 95, 100));
    identity->Add(subtitle_, 0);

    headerSizer->Add(identity, 1, wxALIGN_CENTER_VERTICAL);
    languageLabel_ = new wxStaticText(header, wxID_ANY, "");
    languageChoice_ = new wxChoice(header, ID_LANGUAGE);
    languageChoice_->Append("Deutsch");
    languageChoice_->Append("English");
    languageChoice_->SetSelection(0);
    headerSizer->Add(languageLabel_, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
    headerSizer->Add(languageChoice_, 0, wxALIGN_CENTER_VERTICAL);
    header->SetSizer(headerSizer);
    root->Add(header, 0, wxEXPAND | wxALL, 14);

    // Source card.
    auto* sourceCard = new wxPanel(rootPanel_);
    auto* sourceSizer = new wxBoxSizer(wxVERTICAL);
    sourceTitle_ = new wxStaticText(sourceCard, wxID_ANY, "");
    wxFont sectionFont = sourceTitle_->GetFont();
    sectionFont.MakeBold();
    sourceTitle_->SetFont(sectionFont);
    sourceSizer->Add(sourceTitle_, 0, wxLEFT | wxRIGHT | wxTOP, 12);
    auto* sourceRow = new wxBoxSizer(wxHORIZONTAL);
    folder_ = new wxTextCtrl(sourceCard, wxID_ANY);
    browse_ = new wxButton(sourceCard, ID_CHOOSE_FOLDER, "");
    sourceRow->Add(folder_, 1, wxEXPAND | wxRIGHT, 8);
    sourceRow->Add(browse_, 0);
    sourceSizer->Add(sourceRow, 0, wxEXPAND | wxALL, 12);
    sourceCard->SetSizer(sourceSizer);
    root->Add(sourceCard, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 14);

    // Tag/search card.
    auto* tagCard = new wxPanel(rootPanel_);
    auto* tagSizer = new wxBoxSizer(wxVERTICAL);
    tagTitle_ = new wxStaticText(tagCard, wxID_ANY, "DICOM-Tag");
    tagTitle_->SetFont(sectionFont);
    tagSizer->Add(tagTitle_, 0, wxLEFT | wxRIGHT | wxTOP, 12);

    auto* tagRow = new wxBoxSizer(wxHORIZONTAL);
    tag_ = new wxTextCtrl(tagCard, wxID_ANY, "(0018,1030)");
    chooseTag_ = new wxButton(tagCard, ID_CHOOSE_TAG, "");
    tagInfo_ = new wxStaticText(tagCard, wxID_ANY, "ProtocolName");
    tagInfo_->SetForegroundColour(wxColour(82, 94, 110));
    start_ = new wxButton(tagCard, ID_START_SCAN, "");
    cancel_ = new wxButton(tagCard, ID_CANCEL_SCAN, "");
    cancel_->Enable(false);
    tagRow->Add(tag_, 1, wxEXPAND | wxRIGHT, 8);
    tagRow->Add(chooseTag_, 0, wxRIGHT, 10);
    tagRow->Add(tagInfo_, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 14);
    tagRow->Add(start_, 0, wxRIGHT, 8);
    tagRow->Add(cancel_, 0);
    tagSizer->Add(tagRow, 0, wxEXPAND | wxALL, 12);
    tagCard->SetSizer(tagSizer);
    root->Add(tagCard, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);

    auto* progressRow = new wxBoxSizer(wxHORIZONTAL);
    gauge_ = new wxGauge(rootPanel_, wxID_ANY, 100);
    status_ = new wxStaticText(rootPanel_, wxID_ANY, "");
    progressRow->Add(gauge_, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, 12);
    progressRow->Add(status_, 2, wxALIGN_CENTER_VERTICAL);
    root->Add(progressRow, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);

    // Compact study card: 3 items per row instead of a tall vertical block.
    auto* studyCard = new wxPanel(rootPanel_);
    auto* studySizer = new wxBoxSizer(wxVERTICAL);
    studySectionTitle_ = new wxStaticText(studyCard, wxID_ANY, "");
    studySectionTitle_->SetFont(sectionFont);
    studySizer->Add(studySectionTitle_, 0, wxLEFT | wxRIGHT | wxTOP, 12);

    auto* studyGrid = new wxFlexGridSizer(3, 10, 16);
    studyGrid->AddGrowableCol(0, 1);
    studyGrid->AddGrowableCol(1, 1);
    studyGrid->AddGrowableCol(2, 1);
    patientName_ = AddInfoPair(studyCard, studyGrid, patientNameLabel_, 230);
    patientId_ = AddInfoPair(studyCard, studyGrid, patientIdLabel_, 180);
    studyDate_ = AddInfoPair(studyCard, studyGrid, studyDateLabel_, 180);
    accession_ = AddInfoPair(studyCard, studyGrid, accessionLabel_, 230);
    studyDescription_ = AddInfoPair(studyCard, studyGrid, studyDescriptionLabel_, 300);
    studyUid_ = AddInfoPair(studyCard, studyGrid, studyUidLabel_, 390);
    studySizer->Add(studyGrid, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP | wxBOTTOM, 12);
    studyCard->SetSizer(studySizer);
    root->Add(studyCard, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);

    auto* splitter = new wxSplitterWindow(rootPanel_, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                          wxSP_LIVE_UPDATE | wxSP_BORDER);
    auto* topPanel = new wxPanel(splitter);
    auto* bottomPanel = new wxPanel(splitter);
    topPanel->SetBackgroundColour(wxColour(255, 255, 255));
    bottomPanel->SetBackgroundColour(wxColour(255, 255, 255));

    auto* seriesSizer = new wxBoxSizer(wxVERTICAL);
    seriesTitle_ = new wxStaticText(topPanel, wxID_ANY, "");
    seriesTitle_->SetFont(sectionFont);
    seriesSizer->Add(seriesTitle_, 0, wxLEFT | wxRIGHT | wxTOP | wxBOTTOM, 8);
    seriesList_ = new wxListCtrl(topPanel, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                 wxLC_REPORT | wxLC_SINGLE_SEL | wxLC_HRULES | wxLC_VRULES);
    seriesList_->AppendColumn("", wxLIST_FORMAT_LEFT, 70);
    seriesList_->AppendColumn("", wxLIST_FORMAT_LEFT, 85);
    seriesList_->AppendColumn("", wxLIST_FORMAT_LEFT, 260);
    seriesList_->AppendColumn("Protocol Name", wxLIST_FORMAT_LEFT, 210);
    seriesList_->AppendColumn("Body Part", wxLIST_FORMAT_LEFT, 115);
    seriesList_->AppendColumn("", wxLIST_FORMAT_RIGHT, 80);
    seriesList_->AppendColumn("", wxLIST_FORMAT_RIGHT, 70);
    seriesList_->AppendColumn("Series Instance UID", wxLIST_FORMAT_LEFT, 420);
    seriesSizer->Add(seriesList_, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);
    topPanel->SetSizer(seriesSizer);

    auto* hitSizer = new wxBoxSizer(wxVERTICAL);
    hitsTitle_ = new wxStaticText(bottomPanel, wxID_ANY, "");
    hitsTitle_->SetFont(sectionFont);
    hitSizer->Add(hitsTitle_, 0, wxLEFT | wxRIGHT | wxTOP | wxBOTTOM, 8);
    hitList_ = new wxListCtrl(bottomPanel, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                              wxLC_REPORT | wxLC_SINGLE_SEL | wxLC_HRULES | wxLC_VRULES);
    hitList_->AppendColumn("", wxLIST_FORMAT_LEFT, 65);
    hitList_->AppendColumn("", wxLIST_FORMAT_LEFT, 70);
    hitList_->AppendColumn("", wxLIST_FORMAT_LEFT, 80);
    hitList_->AppendColumn("Tag", wxLIST_FORMAT_LEFT, 105);
    hitList_->AppendColumn("Name", wxLIST_FORMAT_LEFT, 205);
    hitList_->AppendColumn("VR", wxLIST_FORMAT_LEFT, 50);
    hitList_->AppendColumn("", wxLIST_FORMAT_LEFT, 320);
    hitList_->AppendColumn("", wxLIST_FORMAT_LEFT, 300);
    hitList_->AppendColumn("", wxLIST_FORMAT_LEFT, 250);
    hitList_->AppendColumn("SOP Instance UID", wxLIST_FORMAT_LEFT, 350);
    hitSizer->Add(hitList_, 1, wxEXPAND | wxLEFT | wxRIGHT, 8);

    auto* hitButtons = new wxBoxSizer(wxHORIZONTAL);
    showAll_ = new wxButton(bottomPanel, ID_SHOW_ALL, "");
    hitButtons->Add(showAll_, 0, wxALL, 8);
    hitButtons->AddStretchSpacer();
    export_ = new wxButton(bottomPanel, ID_EXPORT, "");
    export_->Enable(false);
    hitButtons->Add(export_, 0, wxALL, 8);
    hitSizer->Add(hitButtons, 0, wxEXPAND);
    bottomPanel->SetSizer(hitSizer);

    splitter->SplitHorizontally(topPanel, bottomPanel, 215);
    splitter->SetMinimumPaneSize(125);
    root->Add(splitter, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);

    footer_ = new wxStaticText(rootPanel_, wxID_ANY, "");
    footer_->SetForegroundColour(wxColour(95, 99, 104));
    root->Add(footer_, 0, wxLEFT | wxRIGHT | wxBOTTOM, 12);

    rootPanel_->SetSizer(root);
}

void MainFrame::ApplyModernStyle() {
    const wxColour appBg(242, 244, 247);
    const wxColour cardBg(255, 255, 255);
    rootPanel_->SetBackgroundColour(appBg);

    for (wxWindow* child : rootPanel_->GetChildren()) {
        if (auto* panel = dynamic_cast<wxPanel*>(child)) panel->SetBackgroundColour(cardBg);
    }

    seriesList_->SetBackgroundColour(wxColour(255, 255, 255));
    hitList_->SetBackgroundColour(wxColour(255, 255, 255));
    Refresh();
}

void MainFrame::BindEvents() {
    Bind(wxEVT_BUTTON, &MainFrame::OnChooseFolder, this, ID_CHOOSE_FOLDER);
    Bind(wxEVT_BUTTON, &MainFrame::OnChooseTag, this, ID_CHOOSE_TAG);
    Bind(wxEVT_BUTTON, &MainFrame::OnStart, this, ID_START_SCAN);
    Bind(wxEVT_BUTTON, &MainFrame::OnCancel, this, ID_CANCEL_SCAN);
    Bind(wxEVT_BUTTON, &MainFrame::OnShowAll, this, ID_SHOW_ALL);
    Bind(wxEVT_BUTTON, &MainFrame::OnExport, this, ID_EXPORT);
    Bind(wxEVT_CHOICE, &MainFrame::OnLanguageChanged, this, ID_LANGUAGE);
    seriesList_->Bind(wxEVT_LIST_ITEM_SELECTED, &MainFrame::OnSeriesSelected, this);
    Bind(EVT_SCAN_PROGRESS, &MainFrame::OnProgress, this);
    Bind(EVT_SCAN_COMPLETE, &MainFrame::OnComplete, this);
    Bind(EVT_SCAN_ERROR, &MainFrame::OnScanError, this);
    Bind(wxEVT_CLOSE_WINDOW, &MainFrame::OnClose, this);
}

void MainFrame::SetColumnTitle(wxListCtrl* list, int column, const wxString& title) {
    wxListItem item;
    item.SetMask(wxLIST_MASK_TEXT);
    item.SetText(title);
    list->SetColumn(column, item);
}

void MainFrame::ApplyLanguage() {
    SetTitle(wxString("ElementValueInspector (EVI) v0.2.8 — ") +
        (language_ == UiLanguage::German ? "Deutsch" : "English"));

    subtitle_->SetLabel(L(
        "DICOM-Studien durchsuchen, Tags finden und Elementwerte nach Serien und Instanzen anzeigen.",
        "Inspect DICOM studies, find tags and display element values by series and instance."));
    languageLabel_->SetLabel(L("Sprache:", "Language:"));
    sourceTitle_->SetLabel(L("Studie / Ordner", "Study / Folder"));
    folder_->SetHint(L("Ordner mit der DICOM-Studie auswählen ...", "Select folder containing the DICOM study ..."));
    browse_->SetLabel(L("Ordner auswählen ...", "Choose folder ..."));
    tagTitle_->SetLabel("DICOM-Tag");
    tag_->SetHint(L("(0018,1030), 00181030 oder ProtocolName", "(0018,1030), 00181030 or ProtocolName"));
    chooseTag_->SetLabel(L("Tag auswählen ...", "Select tag ..."));
    start_->SetLabel(L("Studie durchsuchen", "Scan study"));
    cancel_->SetLabel(L("Abbrechen", "Cancel"));
    studySectionTitle_->SetLabel(L("Studieninformationen", "Study information"));
    patientNameLabel_->SetLabel(L("Patient", "Patient"));
    patientIdLabel_->SetLabel(L("Patient ID", "Patient ID"));
    accessionLabel_->SetLabel(L("Accession Number", "Accession Number"));
    studyDateLabel_->SetLabel(L("Study Date", "Study Date"));
    studyDescriptionLabel_->SetLabel(L("Study Description", "Study Description"));
    studyUidLabel_->SetLabel("Study Instance UID");
    seriesTitle_->SetLabel(L("Serienübersicht", "Series overview"));
    hitsTitle_->SetLabel(L("Treffer / Instanzen", "Matches / Instances"));
    showAll_->SetLabel(L("Alle Treffer anzeigen", "Show all matches"));
    export_->SetLabel(L("Treffer als CSV exportieren ...", "Export matches as CSV ..."));
    footer_->SetLabel(L(
        "Lokale Verarbeitung. Große Binary-/PixelData-Werte werden nicht in die Anzeige geladen.",
        "Local processing only. Large binary/Pixel Data values are not loaded into the display."));

    SetColumnTitle(seriesList_, 0, L("Serie", "Series"));
    SetColumnTitle(seriesList_, 1, L("Modalität", "Modality"));
    SetColumnTitle(seriesList_, 2, L("Serienbeschreibung", "Series Description"));
    SetColumnTitle(seriesList_, 3, "Protocol Name");
    SetColumnTitle(seriesList_, 4, "Body Part");
    SetColumnTitle(seriesList_, 5, L("Instanzen", "Instances"));
    SetColumnTitle(seriesList_, 6, L("Treffer", "Matches"));
    SetColumnTitle(seriesList_, 7, "Series Instance UID");

    SetColumnTitle(hitList_, 0, L("Serie", "Series"));
    SetColumnTitle(hitList_, 1, L("Instanz", "Instance"));
    SetColumnTitle(hitList_, 2, L("Modalität", "Modality"));
    SetColumnTitle(hitList_, 3, "Tag");
    SetColumnTitle(hitList_, 4, "Name");
    SetColumnTitle(hitList_, 5, "VR");
    SetColumnTitle(hitList_, 6, L("Wert", "Value"));
    SetColumnTitle(hitList_, 7, L("Sequence-Pfad", "Sequence Path"));
    SetColumnTitle(hitList_, 8, L("Datei", "File"));
    SetColumnTitle(hitList_, 9, "SOP Instance UID");

    if (result_) UpdateStatusSummary();
    else status_->SetLabel(L("Bereit.", "Ready."));

    rootPanel_->Layout();
    Layout();
}

void MainFrame::OnLanguageChanged(wxCommandEvent&) {
    language_ = languageChoice_->GetSelection() == 1 ? UiLanguage::English : UiLanguage::German;
    ApplyLanguage();
}

void MainFrame::OnChooseFolder(wxCommandEvent&) {
    wxDirDialog dialog(this, L("DICOM-Studienordner auswählen", "Select DICOM study folder"),
                       folder_->GetValue(), wxDD_DEFAULT_STYLE | wxDD_DIR_MUST_EXIST);
    if (dialog.ShowModal() == wxID_OK) folder_->SetValue(dialog.GetPath());
}

void MainFrame::OnChooseTag(wxCommandEvent&) {
    TagDialog dialog(this, language_);
    if (dialog.ShowModal() == wxID_OK && dialog.HasSelection()) {
        const auto selected = dialog.SelectedTag();
        std::ostringstream exact;
        exact << '(' << std::uppercase << std::hex << std::setfill('0')
              << std::setw(4) << selected.group << ',' << std::setw(4) << selected.element << ')';
        tag_->SetValue(W(exact.str()));
        tagInfo_->SetLabel(W(selected.name + "  VR=" + selected.vr + "  VM=" + selected.vm));
    }
}

void MainFrame::OnStart(wxCommandEvent&) { StartScan(); }

void MainFrame::StartScan() {
    if (worker_.joinable()) {
        wxMessageBox(L("Es läuft bereits eine Suche.", "A scan is already running."),
                     "ElementValueInspector (EVI)", wxOK | wxICON_INFORMATION, this);
        return;
    }

    const wxString folderText = folder_->GetValue();
    if (folderText.empty() || !std::filesystem::is_directory(std::filesystem::path(folderText.ToStdWstring()))) {
        wxMessageBox(L("Bitte einen gültigen Studienordner auswählen.", "Please select a valid study folder."),
                     L("Ordner fehlt", "Folder missing"), wxOK | wxICON_WARNING, this);
        return;
    }

    std::string error;
    auto target = DicomUtils::ParseTag(U8(tag_->GetValue()), error, language_);
    if (!target) {
        wxMessageBox(W(error), L("Ungültiger DICOM-Tag", "Invalid DICOM tag"), wxOK | wxICON_WARNING, this);
        return;
    }

    tagInfo_->SetLabel(W(DicomUtils::TagName(*target)));
    result_.reset();
    seriesList_->DeleteAllItems();
    hitList_->DeleteAllItems();
    gauge_->SetValue(0);
    status_->SetLabel(L("Suche wird gestartet ...", "Starting scan ..."));
    start_->Enable(false);
    cancel_->Enable(true);
    export_->Enable(false);
    languageChoice_->Enable(false);
    cancelRequested_.store(false);

    const UiLanguage scanLanguage = language_;
    const std::filesystem::path root(folderText.ToStdWstring());
    const Uint16 group = target->getGroup();
    const Uint16 element = target->getElement();

    worker_ = std::thread([this, root, group, element, scanLanguage]() {
        try {
            DicomScanner scanner;
            auto result = scanner.Scan(root, group, element, cancelRequested_, scanLanguage,
                [this, scanLanguage](std::size_t current, std::size_t total, const std::filesystem::path& path) {
                    auto* event = new wxThreadEvent(EVT_SCAN_PROGRESS);
                    event->SetInt(total ? static_cast<int>((current * 100) / total) : 0);
                    const wxString prefix = scanLanguage == UiLanguage::German
                        ? wxString::Format("Datei %zu von %zu: ", current, total)
                        : wxString::Format("File %zu of %zu: ", current, total);
                    event->SetString(prefix + wxString(path.filename().wstring()));
                    wxQueueEvent(this, event);
                });

            auto* event = new wxThreadEvent(EVT_SCAN_COMPLETE);
            event->SetPayload(result);
            wxQueueEvent(this, event);
        } catch (const std::exception& ex) {
            auto* event = new wxThreadEvent(EVT_SCAN_ERROR);
            event->SetString(W(ex.what()));
            wxQueueEvent(this, event);
        } catch (...) {
            auto* event = new wxThreadEvent(EVT_SCAN_ERROR);
            event->SetString(scanLanguage == UiLanguage::German
                ? "Unbekannter Fehler beim Durchsuchen der DICOM-Studie."
                : "Unknown error while scanning the DICOM study.");
            wxQueueEvent(this, event);
        }
    });
}

void MainFrame::OnCancel(wxCommandEvent&) {
    cancelRequested_.store(true);
    cancel_->Enable(false);
    status_->SetLabel(L("Abbruch angefordert ...", "Cancellation requested ..."));
}

void MainFrame::OnProgress(wxThreadEvent& event) {
    gauge_->SetValue(std::clamp(event.GetInt(), 0, 100));
    status_->SetLabel(event.GetString());
}

void MainFrame::OnComplete(wxThreadEvent& event) {
    if (worker_.joinable()) worker_.join();
    result_ = event.GetPayload<std::shared_ptr<ScanResult>>();
    start_->Enable(true);
    cancel_->Enable(false);
    languageChoice_->Enable(true);
    export_->Enable(result_ && !result_->hits.empty());
    PopulateResult();
}

void MainFrame::OnScanError(wxThreadEvent& event) {
    if (worker_.joinable()) worker_.join();
    start_->Enable(true);
    cancel_->Enable(false);
    languageChoice_->Enable(true);
    status_->SetLabel(L("Fehler.", "Error."));
    wxMessageBox(event.GetString(), L("Fehler beim DICOM-Scan", "DICOM scan error"), wxOK | wxICON_ERROR, this);
}

void MainFrame::PopulateResult() {
    if (!result_) return;
    UpdateStudyLabels();
    PopulateSeries();
    PopulateHits();
    UpdateStatusSummary();
}

void MainFrame::UpdateStatusSummary() {
    if (!result_) return;
    const auto& s = result_->stats;
    if (!s.cancelled) gauge_->SetValue(100);
    if (language_ == UiLanguage::German) {
        const wxString state = s.cancelled ? "Abgebrochen." : "Fertig.";
        status_->SetLabel(state + wxString::Format(
            " %zu DICOM-Dateien, %zu Dateien mit Tag, %zu Treffer, %zu Serien, %zu Lesefehler.",
            s.dicomFiles, s.filesWithTag, s.tagHits, result_->series.size(), s.readErrors));
    } else {
        const wxString state = s.cancelled ? "Cancelled." : "Done.";
        status_->SetLabel(state + wxString::Format(
            " %zu DICOM files, %zu files with tag, %zu matches, %zu series, %zu read errors.",
            s.dicomFiles, s.filesWithTag, s.tagHits, result_->series.size(), s.readErrors));
    }
}

void MainFrame::SetStudyValue(wxStaticText* label, const std::string& value) {
    const wxString text = value.empty() ? wxString("-") : W(value);
    label->SetLabel(text);
    label->SetToolTip(text);
}

void MainFrame::UpdateStudyLabels() {
    if (!result_ || !result_->hasStudy) {
        SetStudyValue(patientName_, {});
        SetStudyValue(patientId_, {});
        SetStudyValue(accession_, {});
        SetStudyValue(studyDate_, {});
        SetStudyValue(studyDescription_, {});
        SetStudyValue(studyUid_, {});
        return;
    }
    SetStudyValue(patientName_, result_->study.patientName);
    SetStudyValue(patientId_, result_->study.patientId);
    SetStudyValue(accession_, result_->study.accessionNumber);
    SetStudyValue(studyDate_, result_->study.studyDate);
    SetStudyValue(studyDescription_, result_->study.studyDescription);
    SetStudyValue(studyUid_, result_->study.studyInstanceUid);
}

void MainFrame::PopulateSeries() {
    seriesList_->Freeze();
    seriesList_->DeleteAllItems();
    visibleSeriesKeys_.clear();
    if (!result_) { seriesList_->Thaw(); return; }

    std::vector<const SeriesInfo*> sorted;
    sorted.reserve(result_->series.size());
    for (const auto& [key, series] : result_->series) sorted.push_back(&series);
    std::sort(sorted.begin(), sorted.end(), [](const SeriesInfo* a, const SeriesInfo* b) {
        try {
            return std::stoi(a->seriesNumber) < std::stoi(b->seriesNumber);
        } catch (...) {
            return a->seriesNumber < b->seriesNumber;
        }
    });

    for (const auto* s : sorted) {
        const long row = seriesList_->InsertItem(seriesList_->GetItemCount(), W(s->seriesNumber));
        seriesList_->SetItem(row, 1, W(s->modality));
        seriesList_->SetItem(row, 2, W(s->seriesDescription));
        seriesList_->SetItem(row, 3, W(s->protocolName));
        seriesList_->SetItem(row, 4, W(s->bodyPart));
        seriesList_->SetItem(row, 5, wxString::Format("%zu", s->instances.size()));
        seriesList_->SetItem(row, 6, wxString::Format("%zu", s->hits));
        seriesList_->SetItem(row, 7, W(s->seriesInstanceUid));
        visibleSeriesKeys_.push_back(s->seriesKey);
    }
    seriesList_->Thaw();
}

void MainFrame::PopulateHits(const std::string& seriesKey) {
    hitList_->Freeze();
    hitList_->DeleteAllItems();
    if (!result_) { hitList_->Thaw(); return; }

    for (const auto& h : result_->hits) {
        if (!seriesKey.empty() && h.seriesKey != seriesKey) continue;
        const long row = hitList_->InsertItem(hitList_->GetItemCount(), W(h.seriesNumber));
        hitList_->SetItem(row, 1, W(h.instanceNumber));
        hitList_->SetItem(row, 2, W(h.modality));
        hitList_->SetItem(row, 3, W(h.tag));
        hitList_->SetItem(row, 4, W(h.tagName));
        hitList_->SetItem(row, 5, W(h.vr));
        hitList_->SetItem(row, 6, W(h.value));
        hitList_->SetItem(row, 7, W(h.sequencePath));
        hitList_->SetItem(row, 8, W(h.relativeFile));
        hitList_->SetItem(row, 9, W(h.sopInstanceUid));
    }
    hitList_->Thaw();
}

void MainFrame::OnSeriesSelected(wxListEvent& event) {
    const long row = event.GetIndex();
    if (row >= 0 && static_cast<std::size_t>(row) < visibleSeriesKeys_.size()) {
        PopulateHits(visibleSeriesKeys_[static_cast<std::size_t>(row)]);
    }
}

void MainFrame::OnShowAll(wxCommandEvent&) {
    for (long i = 0; i < seriesList_->GetItemCount(); ++i) seriesList_->SetItemState(i, 0, wxLIST_STATE_SELECTED);
    PopulateHits();
}

void MainFrame::OnExport(wxCommandEvent&) { ExportCsv(); }

void MainFrame::ExportCsv() {
    if (!result_ || result_->hits.empty()) return;

    wxFileDialog dialog(this, L("Treffer als CSV speichern", "Save matches as CSV"), "", "dicom_tag_matches.csv",
                        "CSV (*.csv)|*.csv", wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dialog.ShowModal() != wxID_OK) return;

    const std::filesystem::path path(dialog.GetPath().ToStdWstring());
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        wxMessageBox(L("Die CSV-Datei konnte nicht geschrieben werden.", "The CSV file could not be written."),
                     L("CSV-Export", "CSV export"), wxOK | wxICON_ERROR, this);
        return;
    }

    out.write("\xEF\xBB\xBF", 3);
    out << "PatientName;PatientID;AccessionNumber;StudyDate;StudyDescription;StudyInstanceUID;"
           "SeriesNumber;Modality;SeriesInstanceUID;InstanceNumber;SOPInstanceUID;Tag;TagName;VR;Value;SequencePath;RelativeFile;FullFile\r\n";

    for (const auto& h : result_->hits) {
        const auto seriesIt = result_->series.find(h.seriesKey);
        const std::string seriesUid = seriesIt == result_->series.end() ? "" : seriesIt->second.seriesInstanceUid;
        out << CsvEscape(result_->study.patientName) << ';'
            << CsvEscape(result_->study.patientId) << ';'
            << CsvEscape(result_->study.accessionNumber) << ';'
            << CsvEscape(result_->study.studyDate) << ';'
            << CsvEscape(result_->study.studyDescription) << ';'
            << CsvEscape(result_->study.studyInstanceUid) << ';'
            << CsvEscape(h.seriesNumber) << ';'
            << CsvEscape(h.modality) << ';'
            << CsvEscape(seriesUid) << ';'
            << CsvEscape(h.instanceNumber) << ';'
            << CsvEscape(h.sopInstanceUid) << ';'
            << CsvEscape(h.tag) << ';'
            << CsvEscape(h.tagName) << ';'
            << CsvEscape(h.vr) << ';'
            << CsvEscape(h.value) << ';'
            << CsvEscape(h.sequencePath) << ';'
            << CsvEscape(h.relativeFile) << ';'
            << CsvEscape(h.fullFile) << "\r\n";
    }

    wxMessageBox(L("CSV wurde erfolgreich gespeichert.", "CSV was saved successfully."),
                 L("CSV-Export", "CSV export"), wxOK | wxICON_INFORMATION, this);
}

void MainFrame::StopWorker() {
    cancelRequested_.store(true);
    if (worker_.joinable()) worker_.join();
}

void MainFrame::OnClose(wxCloseEvent& event) {
    StopWorker();
    event.Skip();
}
