#include "MainFrame.h"
#include "AppIcon.h"
#include "DicomScanner.h"
#include "DicomUtils.h"
#include "HeaderFrame.h"
#include "TagDialog.h"

#include <wx/button.h>
#include <wx/choice.h>
#include <wx/dirdlg.h>
#include <wx/filedlg.h>
#include <wx/font.h>
#include <wx/gauge.h>
#include <wx/listctrl.h>
#include <wx/msgdlg.h>
#include <wx/panel.h>
#include <wx/settings.h>
#include <wx/sizer.h>
#include <wx/splitter.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/thread.h>

#ifdef _WIN32
#include <windows.h>
#include <dwmapi.h>
#include <uxtheme.h>
#endif

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
    ID_LANGUAGE,
    ID_THEME
};

struct Palette {
    wxColour window;
    wxColour card;
    wxColour table;
    wxColour alternate;
    wxColour input;
    wxColour text;
    wxColour muted;
    wxColour accent;
    wxColour separator;
};

Palette BuildThemePalette(UiTheme theme) {
    if (theme == UiTheme::Dark) {
        return {
            wxColour(24, 27, 32),
            wxColour(31, 35, 41),
            wxColour(28, 32, 38),
            wxColour(34, 39, 46),
            wxColour(42, 47, 55),
            wxColour(235, 238, 242),
            wxColour(164, 173, 184),
            wxColour(80, 174, 255),
            wxColour(58, 64, 73)
        };
    }
    return {
        wxColour(244, 246, 249),
        wxColour(255, 255, 255),
        wxColour(255, 255, 255),
        wxColour(248, 250, 252),
        wxColour(255, 255, 255),
        wxColour(30, 35, 42),
        wxColour(100, 109, 120),
        wxColour(0, 120, 212),
        wxColour(224, 229, 235)
    };
}

wxString W(const std::string& text) {
    return wxString::FromUTF8(text.c_str());
}

std::string U8(const wxString& text) {
    const wxScopedCharBuffer buffer = text.ToUTF8();
    return buffer.data() ? std::string(buffer.data()) : std::string{};
}

wxStaticText* AddCompactInfoPair(wxWindow* parent,
                                 wxBoxSizer* row,
                                 wxStaticText*& keyOut,
                                 int proportion,
                                 int minWidth) {
    auto* pair = new wxBoxSizer(wxVERTICAL);

    keyOut = new wxStaticText(parent, wxID_ANY, wxString(L"-"));
    wxFont keyFont = keyOut->GetFont();
    keyFont.SetPointSize(std::max(8, keyFont.GetPointSize() - 1));
    keyFont.MakeBold();
    keyOut->SetFont(keyFont);

    auto* value = new wxStaticText(parent, wxID_ANY, wxString(L"-"), wxDefaultPosition,
                                   wxSize(minWidth, -1), wxST_ELLIPSIZE_END | wxST_NO_AUTORESIZE);
    wxFont valueFont = value->GetFont();
    valueFont.MakeBold();
    value->SetFont(valueFont);
    value->SetMinSize(wxSize(minWidth, -1));

    pair->Add(keyOut, 0, wxBOTTOM, 1);
    pair->Add(value, 0, wxEXPAND);
    row->Add(pair, proportion, wxEXPAND | wxRIGHT, 16);
    return value;
}

std::string CsvEscape(const std::string& value) {
    const bool quote = value.find_first_of(";\"\r\n") != std::string::npos;
    if (!quote) return value;
    std::string out = "\"";
    for (char c : value) {
        if (c == '\"') out += "\"\"";
        else out += c;
    }
    out += '\"';
    return out;
}

#ifdef _WIN32
void ApplyExplorerTheme(wxWindow* window, bool dark) {
    if (!window || !window->GetHandle()) return;
    const wchar_t* themeName = dark ? L"DarkMode_Explorer" : L"Explorer";
    SetWindowTheme(reinterpret_cast<HWND>(window->GetHandle()), themeName, nullptr);
}
#endif

} // namespace

MainFrame::MainFrame()
    : wxFrame(nullptr, wxID_ANY, wxString(L"ElementValueInspector (EVI)"),
              wxDefaultPosition, wxSize(1500, 920))
{
    ApplyEviAppIcon(this);
    SetMinSize(wxSize(1180, 720));
    SetFont(wxFont(wxFontInfo(10).FaceName(wxString(L"Segoe UI"))));
    BuildUi();
    BindEvents();
    ApplyLanguage();
    ApplyTheme();
    Centre();
}

MainFrame::~MainFrame() {
    StopWorker();
}

wxString MainFrame::L(const wchar_t* de, const wchar_t* en) const {
    return wxString(language_ == UiLanguage::German ? de : en);
}

void MainFrame::BuildUi() {
    rootPanel_ = new wxPanel(this);
    auto* root = new wxBoxSizer(wxVERTICAL);

    // Compact application header.
    headerPanel_ = new wxPanel(rootPanel_);
    auto* headerSizer = new wxBoxSizer(wxHORIZONTAL);
    auto* identity = new wxBoxSizer(wxVERTICAL);
    auto* titleRow = new wxBoxSizer(wxHORIZONTAL);

    title_ = new wxStaticText(headerPanel_, wxID_ANY, wxString(L"ElementValueInspector"));
    wxFont titleFont = title_->GetFont();
    titleFont.SetPointSize(titleFont.GetPointSize() + 6);
    titleFont.MakeBold();
    title_->SetFont(titleFont);
    titleRow->Add(title_, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 10);

    version_ = new wxStaticText(headerPanel_, wxID_ANY, wxString(L"EVI 0.2.16"));
    wxFont versionFont = version_->GetFont();
    versionFont.SetPointSize(std::max(8, versionFont.GetPointSize() - 1));
    versionFont.MakeBold();
    version_->SetFont(versionFont);
    titleRow->Add(version_, 0, wxALIGN_CENTER_VERTICAL);
    identity->Add(titleRow, 0, wxBOTTOM, 2);

    subtitle_ = new wxStaticText(headerPanel_, wxID_ANY, wxEmptyString);
    identity->Add(subtitle_, 0);
    headerSizer->Add(identity, 1, wxALIGN_CENTER_VERTICAL);

    themeLabel_ = new wxStaticText(headerPanel_, wxID_ANY, wxEmptyString);
    themeChoice_ = new wxChoice(headerPanel_, ID_THEME);
    themeChoice_->Append(wxString(L"Hell"));
    themeChoice_->Append(wxString(L"Dunkel"));
    themeChoice_->SetSelection(0);

    languageLabel_ = new wxStaticText(headerPanel_, wxID_ANY, wxEmptyString);
    languageChoice_ = new wxChoice(headerPanel_, ID_LANGUAGE);
    languageChoice_->Append(wxString(L"Deutsch"));
    languageChoice_->Append(wxString(L"English"));
    languageChoice_->SetSelection(0);

    headerSizer->Add(themeLabel_, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
    headerSizer->Add(themeChoice_, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 18);
    headerSizer->Add(languageLabel_, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
    headerSizer->Add(languageChoice_, 0, wxALIGN_CENTER_VERTICAL);
    headerPanel_->SetSizer(headerSizer);
    root->Add(headerPanel_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP | wxBOTTOM, 16);

    // One flat search card with two compact rows.
    searchCard_ = new wxPanel(rootPanel_);
    auto* searchSizer = new wxBoxSizer(wxVERTICAL);
    auto* sourceRow = new wxBoxSizer(wxHORIZONTAL);
    auto* tagRow = new wxBoxSizer(wxHORIZONTAL);

    sourceTitle_ = new wxStaticText(searchCard_, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(105, -1));
    wxFont sectionFont = sourceTitle_->GetFont();
    sectionFont.MakeBold();
    sourceTitle_->SetFont(sectionFont);
    folder_ = new wxTextCtrl(searchCard_, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
    browse_ = new wxButton(searchCard_, ID_CHOOSE_FOLDER, wxEmptyString);
    browse_->SetMinSize(wxSize(150, 34));
    sourceRow->Add(sourceTitle_, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 10);
    sourceRow->Add(folder_, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
    sourceRow->Add(browse_, 0, wxALIGN_CENTER_VERTICAL);

    tagTitle_ = new wxStaticText(searchCard_, wxID_ANY, wxString(L"DICOM Tag"), wxDefaultPosition, wxSize(105, -1));
    tagTitle_->SetFont(sectionFont);
    tag_ = new wxTextCtrl(searchCard_, wxID_ANY, wxString(L"(0018,1030)"), wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
    chooseTag_ = new wxButton(searchCard_, ID_CHOOSE_TAG, wxEmptyString);
    chooseTag_->SetMinSize(wxSize(150, 34));
    tagInfo_ = new wxStaticText(searchCard_, wxID_ANY, wxString(L"ProtocolName"), wxDefaultPosition, wxSize(190, -1), wxST_ELLIPSIZE_END);
    start_ = new wxButton(searchCard_, ID_START_SCAN, wxEmptyString);
    start_->SetMinSize(wxSize(150, 34));
    start_->SetDefault();
    cancel_ = new wxButton(searchCard_, ID_CANCEL_SCAN, wxEmptyString);
    cancel_->SetMinSize(wxSize(105, 34));
    cancel_->Enable(false);

    tagRow->Add(tagTitle_, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 10);
    tagRow->Add(tag_, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
    tagRow->Add(chooseTag_, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 12);
    tagRow->Add(tagInfo_, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 12);
    tagRow->Add(start_, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
    tagRow->Add(cancel_, 0, wxALIGN_CENTER_VERTICAL);

    searchSizer->Add(sourceRow, 0, wxEXPAND | wxALL, 12);
    searchSizer->Add(tagRow, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);
    searchCard_->SetSizer(searchSizer);
    root->Add(searchCard_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 16);

    auto* progressRow = new wxBoxSizer(wxHORIZONTAL);
    gauge_ = new wxGauge(rootPanel_, wxID_ANY, 100, wxDefaultPosition, wxSize(-1, 12));
    status_ = new wxStaticText(rootPanel_, wxID_ANY, wxEmptyString);
    progressRow->Add(gauge_, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, 12);
    progressRow->Add(status_, 2, wxALIGN_CENTER_VERTICAL);
    root->Add(progressRow, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);

    // Very compact study strip: one row, no tall group box.
    studyCard_ = new wxPanel(rootPanel_);
    auto* studyRow = new wxBoxSizer(wxHORIZONTAL);
    studySectionTitle_ = new wxStaticText(studyCard_, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(72, -1));
    wxFont studyTitleFont = studySectionTitle_->GetFont();
    studyTitleFont.MakeBold();
    studySectionTitle_->SetFont(studyTitleFont);
    studyRow->Add(studySectionTitle_, 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, 12);

    patientName_ = AddCompactInfoPair(studyCard_, studyRow, patientNameLabel_, 2, 130);
    patientId_ = AddCompactInfoPair(studyCard_, studyRow, patientIdLabel_, 1, 100);
    studyDate_ = AddCompactInfoPair(studyCard_, studyRow, studyDateLabel_, 1, 90);
    accession_ = AddCompactInfoPair(studyCard_, studyRow, accessionLabel_, 1, 120);
    studyDescription_ = AddCompactInfoPair(studyCard_, studyRow, studyDescriptionLabel_, 3, 210);
    studyUid_ = AddCompactInfoPair(studyCard_, studyRow, studyUidLabel_, 3, 260);
    studyCard_->SetSizer(studyRow);
    root->Add(studyCard_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);

    // Main content area: clean list views without old-fashioned grid lines.
    splitter_ = new wxSplitterWindow(rootPanel_, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxSP_LIVE_UPDATE);
    seriesPanel_ = new wxPanel(splitter_);
    hitsPanel_ = new wxPanel(splitter_);

    auto* seriesSizer = new wxBoxSizer(wxVERTICAL);
    seriesTitle_ = new wxStaticText(seriesPanel_, wxID_ANY, wxEmptyString);
    seriesTitle_->SetFont(sectionFont);
    seriesSizer->Add(seriesTitle_, 0, wxLEFT | wxRIGHT | wxTOP | wxBOTTOM, 10);
    seriesList_ = new wxListCtrl(seriesPanel_, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                 wxLC_REPORT | wxLC_SINGLE_SEL);
    seriesList_->AppendColumn(wxEmptyString, wxLIST_FORMAT_LEFT, 70);
    seriesList_->AppendColumn(wxEmptyString, wxLIST_FORMAT_LEFT, 85);
    seriesList_->AppendColumn(wxEmptyString, wxLIST_FORMAT_LEFT, 270);
    seriesList_->AppendColumn(wxEmptyString, wxLIST_FORMAT_LEFT, 210);
    seriesList_->AppendColumn(wxEmptyString, wxLIST_FORMAT_LEFT, 130);
    seriesList_->AppendColumn(wxEmptyString, wxLIST_FORMAT_RIGHT, 80);
    seriesList_->AppendColumn(wxEmptyString, wxLIST_FORMAT_RIGHT, 70);
    seriesList_->AppendColumn(wxEmptyString, wxLIST_FORMAT_LEFT, 420);
    seriesSizer->Add(seriesList_, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);
    seriesPanel_->SetSizer(seriesSizer);

    auto* hitSizer = new wxBoxSizer(wxVERTICAL);
    hitsTitle_ = new wxStaticText(hitsPanel_, wxID_ANY, wxEmptyString);
    hitsTitle_->SetFont(sectionFont);
    hitSizer->Add(hitsTitle_, 0, wxLEFT | wxRIGHT | wxTOP | wxBOTTOM, 10);
    hitList_ = new wxListCtrl(hitsPanel_, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                              wxLC_REPORT | wxLC_SINGLE_SEL);
    hitList_->AppendColumn(wxEmptyString, wxLIST_FORMAT_LEFT, 65);
    hitList_->AppendColumn(wxEmptyString, wxLIST_FORMAT_LEFT, 70);
    hitList_->AppendColumn(wxEmptyString, wxLIST_FORMAT_LEFT, 80);
    hitList_->AppendColumn(wxEmptyString, wxLIST_FORMAT_LEFT, 105);
    hitList_->AppendColumn(wxEmptyString, wxLIST_FORMAT_LEFT, 210);
    hitList_->AppendColumn(wxEmptyString, wxLIST_FORMAT_LEFT, 50);
    hitList_->AppendColumn(wxEmptyString, wxLIST_FORMAT_LEFT, 330);
    hitList_->AppendColumn(wxEmptyString, wxLIST_FORMAT_LEFT, 300);
    hitList_->AppendColumn(wxEmptyString, wxLIST_FORMAT_LEFT, 250);
    hitList_->AppendColumn(wxEmptyString, wxLIST_FORMAT_LEFT, 350);
    hitSizer->Add(hitList_, 1, wxEXPAND | wxLEFT | wxRIGHT, 10);

    auto* hitButtons = new wxBoxSizer(wxHORIZONTAL);
    showAll_ = new wxButton(hitsPanel_, ID_SHOW_ALL, wxEmptyString);
    showAll_->SetMinSize(wxSize(150, 32));
    export_ = new wxButton(hitsPanel_, ID_EXPORT, wxEmptyString);
    export_->SetMinSize(wxSize(190, 32));
    export_->Enable(false);
    hitButtons->Add(showAll_, 0, wxLEFT | wxTOP | wxBOTTOM, 10);
    hitButtons->AddStretchSpacer();
    hitButtons->Add(export_, 0, wxRIGHT | wxTOP | wxBOTTOM, 10);
    hitSizer->Add(hitButtons, 0, wxEXPAND);
    hitsPanel_->SetSizer(hitSizer);

    splitter_->SplitHorizontally(seriesPanel_, hitsPanel_, 225);
    splitter_->SetMinimumPaneSize(130);
    splitter_->SetSashGravity(0.35);
    root->Add(splitter_, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);

    footer_ = new wxStaticText(rootPanel_, wxID_ANY, wxEmptyString);
    root->Add(footer_, 0, wxLEFT | wxRIGHT | wxBOTTOM, 12);

    rootPanel_->SetSizer(root);
}

void MainFrame::BindEvents() {
    Bind(wxEVT_BUTTON, &MainFrame::OnChooseFolder, this, ID_CHOOSE_FOLDER);
    Bind(wxEVT_BUTTON, &MainFrame::OnChooseTag, this, ID_CHOOSE_TAG);
    Bind(wxEVT_BUTTON, &MainFrame::OnStart, this, ID_START_SCAN);
    Bind(wxEVT_BUTTON, &MainFrame::OnCancel, this, ID_CANCEL_SCAN);
    Bind(wxEVT_BUTTON, &MainFrame::OnShowAll, this, ID_SHOW_ALL);
    Bind(wxEVT_BUTTON, &MainFrame::OnExport, this, ID_EXPORT);
    Bind(wxEVT_CHOICE, &MainFrame::OnLanguageChanged, this, ID_LANGUAGE);
    Bind(wxEVT_CHOICE, &MainFrame::OnThemeChanged, this, ID_THEME);
    tag_->Bind(wxEVT_TEXT_ENTER, &MainFrame::OnStart, this);
    folder_->Bind(wxEVT_TEXT_ENTER, &MainFrame::OnStart, this);
    seriesList_->Bind(wxEVT_LIST_ITEM_SELECTED, &MainFrame::OnSeriesSelected, this);
    hitList_->Bind(wxEVT_LIST_ITEM_ACTIVATED, &MainFrame::OnHitActivated, this);
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
    SetTitle(wxString(L"ElementValueInspector (EVI) 0.2.16"));

    subtitle_->SetLabel(L(
        L"DICOM-Studien durchsuchen, Tags finden und Elementwerte übersichtlich anzeigen.",
        L"Inspect DICOM studies, find tags and review element values clearly."));
    languageLabel_->SetLabel(L(L"Sprache", L"Language"));
    themeLabel_->SetLabel(L(L"Darstellung", L"Appearance"));

    const int themeSelection = themeChoice_->GetSelection();
    themeChoice_->Clear();
    themeChoice_->Append(L(L"Hell", L"Light"));
    themeChoice_->Append(L(L"Dunkel", L"Dark"));
    themeChoice_->SetSelection(themeSelection < 0 ? 0 : themeSelection);

    sourceTitle_->SetLabel(L(L"Studienordner", L"Study folder"));
    folder_->SetHint(L(L"Ordner mit der DICOM-Studie auswählen ...", L"Select the folder containing the DICOM study ..."));
    browse_->SetLabel(L(L"Ordner auswählen", L"Choose folder"));
    tagTitle_->SetLabel(L(L"DICOM-Tag", L"DICOM tag"));
    tag_->SetHint(L(L"(0018,1030), 00181030 oder ProtocolName", L"(0018,1030), 00181030 or ProtocolName"));
    chooseTag_->SetLabel(L(L"Tag auswählen", L"Select tag"));
    start_->SetLabel(L(L"Studie durchsuchen", L"Scan study"));
    cancel_->SetLabel(L(L"Abbrechen", L"Cancel"));

    studySectionTitle_->SetLabel(L(L"STUDIE", L"STUDY"));
    patientNameLabel_->SetLabel(L(L"Patient", L"Patient"));
    patientIdLabel_->SetLabel(L(L"Patienten-ID", L"Patient ID"));
    accessionLabel_->SetLabel(L(L"Accession", L"Accession"));
    studyDateLabel_->SetLabel(L(L"Datum", L"Date"));
    studyDescriptionLabel_->SetLabel(L(L"Beschreibung", L"Description"));
    studyUidLabel_->SetLabel(L(L"Study UID", L"Study UID"));

    seriesTitle_->SetLabel(L(L"Serien", L"Series"));
    hitsTitle_->SetLabel(L(L"Treffer und Instanzen", L"Matches and instances"));
    showAll_->SetLabel(L(L"Alle Treffer", L"Show all"));
    export_->SetLabel(L(L"CSV exportieren", L"Export CSV"));
    footer_->SetLabel(L(
        L"Alle DICOM-Daten werden ausschließlich lokal verarbeitet. PixelData und große Binärwerte werden nicht in die Ergebnisanzeige geladen.",
        L"All DICOM data is processed locally. Pixel Data and large binary values are not loaded into the result view."));

    SetColumnTitle(seriesList_, 0, L(L"Serie", L"Series"));
    SetColumnTitle(seriesList_, 1, L(L"Modalität", L"Modality"));
    SetColumnTitle(seriesList_, 2, L(L"Serienbeschreibung", L"Series description"));
    SetColumnTitle(seriesList_, 3, L(L"Protokollname", L"Protocol name"));
    SetColumnTitle(seriesList_, 4, L(L"Körperregion", L"Body part"));
    SetColumnTitle(seriesList_, 5, L(L"Instanzen", L"Instances"));
    SetColumnTitle(seriesList_, 6, L(L"Treffer", L"Matches"));
    SetColumnTitle(seriesList_, 7, wxString(L"Series Instance UID"));

    SetColumnTitle(hitList_, 0, L(L"Serie", L"Series"));
    SetColumnTitle(hitList_, 1, L(L"Instanz", L"Instance"));
    SetColumnTitle(hitList_, 2, L(L"Modalität", L"Modality"));
    SetColumnTitle(hitList_, 3, wxString(L"Tag"));
    SetColumnTitle(hitList_, 4, L(L"Name", L"Name"));
    SetColumnTitle(hitList_, 5, wxString(L"VR"));
    SetColumnTitle(hitList_, 6, L(L"Wert", L"Value"));
    SetColumnTitle(hitList_, 7, L(L"Sequence-Pfad", L"Sequence path"));
    SetColumnTitle(hitList_, 8, L(L"Datei", L"File"));
    SetColumnTitle(hitList_, 9, wxString(L"SOP Instance UID"));

    hitList_->SetToolTip(L(
        L"Doppelklick auf einen Treffer öffnet den vollständigen DICOM-Header dieser Instanz.",
        L"Double-click a match to open the complete DICOM header for that instance."));

    if (result_) UpdateStatusSummary();
    else status_->SetLabel(L(L"Bereit.", L"Ready."));

    rootPanel_->Layout();
    Layout();
}

void MainFrame::ApplyTheme() {
    const Palette p = BuildThemePalette(theme_);
    SetBackgroundColour(p.window);
    rootPanel_->SetBackgroundColour(p.window);
    headerPanel_->SetBackgroundColour(p.window);
    searchCard_->SetBackgroundColour(p.card);
    studyCard_->SetBackgroundColour(p.card);
    seriesPanel_->SetBackgroundColour(p.card);
    hitsPanel_->SetBackgroundColour(p.card);
    splitter_->SetBackgroundColour(p.separator);

    const auto setText = [&](wxStaticText* control, const wxColour& colour) {
        if (control) control->SetForegroundColour(colour);
    };

    setText(title_, p.text);
    setText(version_, p.accent);
    setText(subtitle_, p.muted);
    setText(languageLabel_, p.muted);
    setText(themeLabel_, p.muted);
    setText(sourceTitle_, p.text);
    setText(tagTitle_, p.text);
    setText(tagInfo_, p.muted);
    setText(status_, p.muted);
    setText(studySectionTitle_, p.accent);
    setText(patientNameLabel_, p.muted);
    setText(patientIdLabel_, p.muted);
    setText(accessionLabel_, p.muted);
    setText(studyDateLabel_, p.muted);
    setText(studyDescriptionLabel_, p.muted);
    setText(studyUidLabel_, p.muted);
    setText(patientName_, p.text);
    setText(patientId_, p.text);
    setText(accession_, p.text);
    setText(studyDate_, p.text);
    setText(studyDescription_, p.text);
    setText(studyUid_, p.text);
    setText(seriesTitle_, p.text);
    setText(hitsTitle_, p.text);
    setText(footer_, p.muted);

    for (wxTextCtrl* control : {folder_, tag_}) {
        control->SetBackgroundColour(p.input);
        control->SetForegroundColour(p.text);
    }
    for (wxChoice* control : {languageChoice_, themeChoice_}) {
        control->SetBackgroundColour(p.input);
        control->SetForegroundColour(p.text);
    }

    seriesList_->SetBackgroundColour(p.table);
    seriesList_->SetForegroundColour(p.text);
    hitList_->SetBackgroundColour(p.table);
    hitList_->SetForegroundColour(p.text);

#ifdef _WIN32
    const bool dark = theme_ == UiTheme::Dark;
    ApplyExplorerTheme(seriesList_, dark);
    ApplyExplorerTheme(hitList_, dark);
    for (wxWindow* control : {
             static_cast<wxWindow*>(folder_), static_cast<wxWindow*>(tag_),
             static_cast<wxWindow*>(languageChoice_), static_cast<wxWindow*>(themeChoice_),
             static_cast<wxWindow*>(browse_), static_cast<wxWindow*>(chooseTag_),
             static_cast<wxWindow*>(start_), static_cast<wxWindow*>(cancel_),
             static_cast<wxWindow*>(showAll_), static_cast<wxWindow*>(export_),
             static_cast<wxWindow*>(gauge_)}) {
        ApplyExplorerTheme(control, dark);
    }
#endif

    ApplyListRowTheme(seriesList_);
    ApplyListRowTheme(hitList_);
    ApplyNativeChrome();

    rootPanel_->Refresh();
    searchCard_->Refresh();
    studyCard_->Refresh();
    seriesPanel_->Refresh();
    hitsPanel_->Refresh();
    seriesList_->Refresh();
    hitList_->Refresh();
    Layout();
}

void MainFrame::ApplyNativeChrome() {
#ifdef _WIN32
    HWND hwnd = reinterpret_cast<HWND>(GetHandle());
    if (!hwnd) return;

    const BOOL dark = theme_ == UiTheme::Dark ? TRUE : FALSE;
    constexpr DWORD kImmersiveDarkMode = 20;
    DwmSetWindowAttribute(hwnd, kImmersiveDarkMode, &dark, sizeof(dark));

    // Rounded corners on Windows 11; harmlessly ignored on older systems.
    constexpr DWORD kWindowCornerPreference = 33;
    constexpr DWORD kRound = 2;
    DwmSetWindowAttribute(hwnd, kWindowCornerPreference, &kRound, sizeof(kRound));
#endif
}

void MainFrame::ApplyListRowTheme(wxListCtrl* list) {
    if (!list) return;
    const Palette p = BuildThemePalette(theme_);
    for (long row = 0; row < list->GetItemCount(); ++row) {
        list->SetItemBackgroundColour(row, (row % 2) ? p.alternate : p.table);
        list->SetItemTextColour(row, p.text);
    }
}

void MainFrame::OnLanguageChanged(wxCommandEvent&) {
    language_ = languageChoice_->GetSelection() == 1 ? UiLanguage::English : UiLanguage::German;
    ApplyLanguage();
}

void MainFrame::OnThemeChanged(wxCommandEvent&) {
    theme_ = themeChoice_->GetSelection() == 1 ? UiTheme::Dark : UiTheme::Light;
    ApplyTheme();
}

void MainFrame::OnChooseFolder(wxCommandEvent&) {
    wxDirDialog dialog(this, L(L"DICOM-Studienordner auswählen", L"Select DICOM study folder"),
                       folder_->GetValue(), wxDD_DEFAULT_STYLE | wxDD_DIR_MUST_EXIST);
    if (dialog.ShowModal() == wxID_OK) folder_->SetValue(dialog.GetPath());
}

void MainFrame::OnChooseTag(wxCommandEvent&) {
    TagDialog dialog(this, language_, theme_);
    if (dialog.ShowModal() == wxID_OK && dialog.HasSelection()) {
        const auto selected = dialog.SelectedTag();
        std::ostringstream exact;
        exact << '(' << std::uppercase << std::hex << std::setfill('0')
              << std::setw(4) << selected.group << ',' << std::setw(4) << selected.element << ')';
        tag_->SetValue(W(exact.str()));
        tagInfo_->SetLabel(W(selected.name + "  VR=" + selected.vr + "  VM=" + selected.vm));
        tagInfo_->SetToolTip(tagInfo_->GetLabel());
    }
}

void MainFrame::OnStart(wxCommandEvent&) { StartScan(); }

void MainFrame::StartScan() {
    if (worker_.joinable()) {
        wxMessageBox(L(L"Es läuft bereits eine Suche.", L"A scan is already running."),
                     wxString(L"ElementValueInspector (EVI)"), wxOK | wxICON_INFORMATION, this);
        return;
    }

    const wxString folderText = folder_->GetValue();
    if (folderText.empty() || !std::filesystem::is_directory(std::filesystem::path(folderText.ToStdWstring()))) {
        wxMessageBox(L(L"Bitte einen gültigen Studienordner auswählen.", L"Please select a valid study folder."),
                     L(L"Ordner fehlt", L"Folder missing"), wxOK | wxICON_WARNING, this);
        return;
    }

    std::string error;
    auto target = DicomUtils::ParseTag(U8(tag_->GetValue()), error, language_);
    if (!target) {
        wxMessageBox(W(error), L(L"Ungültiger DICOM-Tag", L"Invalid DICOM tag"), wxOK | wxICON_WARNING, this);
        return;
    }

    tagInfo_->SetLabel(W(DicomUtils::TagName(*target)));
    tagInfo_->SetToolTip(tagInfo_->GetLabel());
    result_.reset();
    seriesList_->DeleteAllItems();
    hitList_->DeleteAllItems();
    gauge_->SetValue(0);
    status_->SetLabel(L(L"Suche wird gestartet ...", L"Starting scan ..."));
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
                        ? wxString::Format(L"Datei %zu von %zu: ", current, total)
                        : wxString::Format(L"File %zu of %zu: ", current, total);
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
                ? wxString(L"Unbekannter Fehler beim Durchsuchen der DICOM-Studie.")
                : wxString(L"Unknown error while scanning the DICOM study."));
            wxQueueEvent(this, event);
        }
    });
}

void MainFrame::OnCancel(wxCommandEvent&) {
    cancelRequested_.store(true);
    cancel_->Enable(false);
    status_->SetLabel(L(L"Abbruch angefordert ...", L"Cancellation requested ..."));
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
    status_->SetLabel(L(L"Fehler.", L"Error."));
    wxMessageBox(event.GetString(), L(L"Fehler beim DICOM-Scan", L"DICOM scan error"), wxOK | wxICON_ERROR, this);
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
        const wxString state = s.cancelled ? wxString(L"Abgebrochen.") : wxString(L"Fertig.");
        status_->SetLabel(state + wxString::Format(
            L" %zu DICOM-Dateien, %zu Dateien mit Tag, %zu Treffer, %zu Serien, %zu Lesefehler.",
            s.dicomFiles, s.filesWithTag, s.tagHits, result_->series.size(), s.readErrors));
    } else {
        const wxString state = s.cancelled ? wxString(L"Cancelled.") : wxString(L"Done.");
        status_->SetLabel(state + wxString::Format(
            L" %zu DICOM files, %zu files with tag, %zu matches, %zu series, %zu read errors.",
            s.dicomFiles, s.filesWithTag, s.tagHits, result_->series.size(), s.readErrors));
    }
}

void MainFrame::SetStudyValue(wxStaticText* label, const std::string& value) {
    const wxString text = value.empty() ? wxString(L"-") : W(value);
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
    studyCard_->Layout();
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
        seriesList_->SetItem(row, 5, wxString::Format(L"%zu", s->instances.size()));
        seriesList_->SetItem(row, 6, wxString::Format(L"%zu", s->hits));
        seriesList_->SetItem(row, 7, W(s->seriesInstanceUid));
        visibleSeriesKeys_.push_back(s->seriesKey);
    }
    ApplyListRowTheme(seriesList_);
    seriesList_->Thaw();
}

void MainFrame::PopulateHits(const std::string& seriesKey) {
    hitList_->Freeze();
    hitList_->DeleteAllItems();
    visibleHitIndices_.clear();
    if (!result_) { hitList_->Thaw(); return; }

    std::vector<std::size_t> sortedHits;
    sortedHits.reserve(result_->hits.size());
    for (std::size_t index = 0; index < result_->hits.size(); ++index) {
        const auto& h = result_->hits[index];
        if (!seriesKey.empty() && h.seriesKey != seriesKey) continue;
        sortedHits.push_back(index);
    }

    const auto parseDicomInteger = [](const std::string& value, long long& parsed) {
        const auto first = value.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) return false;
        const auto last = value.find_last_not_of(" \t\r\n");
        const std::string trimmed = value.substr(first, last - first + 1);

        std::size_t pos = 0;
        try {
            parsed = std::stoll(trimmed, &pos, 10);
        } catch (...) {
            return false;
        }
        return pos == trimmed.size();
    };

    const auto numericLess = [&](const std::string& left, const std::string& right) {
        long long l = 0;
        long long r = 0;
        const bool leftNumeric = parseDicomInteger(left, l);
        const bool rightNumeric = parseDicomInteger(right, r);

        if (leftNumeric && rightNumeric && l != r) return l < r;
        if (leftNumeric != rightNumeric) return leftNumeric;
        return left < right;
    };

    std::stable_sort(sortedHits.begin(), sortedHits.end(), [&](std::size_t leftIndex, std::size_t rightIndex) {
        const auto& a = result_->hits[leftIndex];
        const auto& b = result_->hits[rightIndex];
        if (a.seriesNumber != b.seriesNumber) {
            return numericLess(a.seriesNumber, b.seriesNumber);
        }
        if (a.instanceNumber != b.instanceNumber) {
            return numericLess(a.instanceNumber, b.instanceNumber);
        }
        return a.relativeFile < b.relativeFile;
    });

    for (const auto hitIndex : sortedHits) {
        const auto& h = result_->hits[hitIndex];
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
        visibleHitIndices_.push_back(hitIndex);
    }
    ApplyListRowTheme(hitList_);
    hitList_->Thaw();
}

void MainFrame::OnSeriesSelected(wxListEvent& event) {
    const long row = event.GetIndex();
    if (row >= 0 && static_cast<std::size_t>(row) < visibleSeriesKeys_.size()) {
        PopulateHits(visibleSeriesKeys_[static_cast<std::size_t>(row)]);
    }
}

void MainFrame::OnHitActivated(wxListEvent& event) {
    const long row = event.GetIndex();
    if (!result_ || row < 0 || static_cast<std::size_t>(row) >= visibleHitIndices_.size()) return;

    const auto hitIndex = visibleHitIndices_[static_cast<std::size_t>(row)];
    if (hitIndex >= result_->hits.size()) return;

    try {
        auto* header = new HeaderFrame(this, result_->hits[hitIndex], language_, theme_);
        header->Show();
    } catch (const std::exception& ex) {
        wxMessageBox(W(ex.what()),
                     L(L"DICOM-Header konnte nicht geöffnet werden", L"Could not open DICOM header"),
                     wxOK | wxICON_ERROR, this);
    }
}

void MainFrame::OnShowAll(wxCommandEvent&) {
    for (long i = 0; i < seriesList_->GetItemCount(); ++i) {
        seriesList_->SetItemState(i, 0, wxLIST_STATE_SELECTED);
    }
    PopulateHits();
}

void MainFrame::OnExport(wxCommandEvent&) { ExportCsv(); }

void MainFrame::ExportCsv() {
    if (!result_ || result_->hits.empty()) return;

    wxFileDialog dialog(this, L(L"Treffer als CSV speichern", L"Save matches as CSV"), wxEmptyString,
                        wxString(L"dicom_tag_matches.csv"), wxString(L"CSV (*.csv)|*.csv"),
                        wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dialog.ShowModal() != wxID_OK) return;

    const std::filesystem::path path(dialog.GetPath().ToStdWstring());
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        wxMessageBox(L(L"Die CSV-Datei konnte nicht geschrieben werden.", L"The CSV file could not be written."),
                     L(L"CSV-Export", L"CSV export"), wxOK | wxICON_ERROR, this);
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

    wxMessageBox(L(L"CSV wurde erfolgreich gespeichert.", L"CSV was saved successfully."),
                 L(L"CSV-Export", L"CSV export"), wxOK | wxICON_INFORMATION, this);
}

void MainFrame::StopWorker() {
    cancelRequested_.store(true);
    if (worker_.joinable()) worker_.join();
}

void MainFrame::OnClose(wxCloseEvent& event) {
    StopWorker();
    event.Skip();
}
