#include "MainFrame.h"
#include "DicomScanner.h"
#include "DicomUtils.h"
#include "TagDialog.h"

#include <wx/button.h>
#include <wx/dirdlg.h>
#include <wx/filedlg.h>
#include <wx/gauge.h>
#include <wx/listctrl.h>
#include <wx/msgdlg.h>
#include <wx/sizer.h>
#include <wx/splitter.h>
#include <wx/statbox.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/thread.h>
#include <wx/wfstream.h>

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
    ID_EXPORT
};

wxString W(const std::string& text) {
    return wxString::FromUTF8(text.c_str());
}

std::string U8(const wxString& text) {
    const wxScopedCharBuffer buffer = text.ToUTF8();
    return buffer.data() ? std::string(buffer.data()) : std::string{};
}

wxStaticText* AddValueRow(wxWindow* parent, wxFlexGridSizer* grid, const wxString& label) {
    auto* key = new wxStaticText(parent, wxID_ANY, label);
    wxFont f = key->GetFont();
    f.MakeBold();
    key->SetFont(f);
    auto* value = new wxStaticText(parent, wxID_ANY, "-");
    value->SetLabelText("-");
    grid->Add(key, 0, wxALIGN_TOP | wxRIGHT | wxBOTTOM, 8);
    grid->Add(value, 1, wxEXPAND | wxBOTTOM, 8);
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
    : wxFrame(nullptr, wxID_ANY, "ElementValueInspector (EVI) v0.2.5", wxDefaultPosition, wxSize(1500, 920))
{
    BuildUi();
    BindEvents();
    Centre();
}

MainFrame::~MainFrame() {
    StopWorker();
}

void MainFrame::BuildUi() {
    auto* rootPanel = new wxPanel(this);
    auto* root = new wxBoxSizer(wxVERTICAL);

    auto* title = new wxStaticText(rootPanel, wxID_ANY, "ElementValueInspector (EVI)");
    wxFont titleFont = title->GetFont();
    titleFont.SetPointSize(titleFont.GetPointSize() + 5);
    titleFont.MakeBold();
    title->SetFont(titleFont);
    root->Add(title, 0, wxLEFT | wxRIGHT | wxTOP, 12);
    root->Add(new wxStaticText(rootPanel, wxID_ANY,
        "ElementValueInspector fuer DICOM-Studien: Tags suchen und Elementwerte nach Serien/Instanzen anzeigen."),
        0, wxLEFT | wxRIGHT | wxBOTTOM, 12);

    auto* sourceBox = new wxStaticBoxSizer(wxHORIZONTAL, rootPanel, "Studie / Ordner");
    folder_ = new wxTextCtrl(sourceBox->GetStaticBox(), wxID_ANY);
    folder_->SetHint("Ordner mit der DICOM-Studie auswaehlen ...");
    auto* browse = new wxButton(sourceBox->GetStaticBox(), ID_CHOOSE_FOLDER, "Ordner auswaehlen ...");
    sourceBox->Add(folder_, 1, wxEXPAND | wxALL, 6);
    sourceBox->Add(browse, 0, wxALL, 6);
    root->Add(sourceBox, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);

    auto* tagBox = new wxStaticBoxSizer(wxHORIZONTAL, rootPanel, "DICOM-Tag");
    tag_ = new wxTextCtrl(tagBox->GetStaticBox(), wxID_ANY, "(0018,1030)");
    tag_->SetHint("(0018,1030), 00181030 oder ProtocolName");
    auto* chooseTag = new wxButton(tagBox->GetStaticBox(), ID_CHOOSE_TAG, "Tag auswaehlen ...");
    tagInfo_ = new wxStaticText(tagBox->GetStaticBox(), wxID_ANY, "ProtocolName");
    start_ = new wxButton(tagBox->GetStaticBox(), ID_START_SCAN, "Studie durchsuchen");
    cancel_ = new wxButton(tagBox->GetStaticBox(), ID_CANCEL_SCAN, "Abbrechen");
    cancel_->Enable(false);
    tagBox->Add(tag_, 1, wxEXPAND | wxALL, 6);
    tagBox->Add(chooseTag, 0, wxALL, 6);
    tagBox->Add(tagInfo_, 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, 10);
    tagBox->Add(start_, 0, wxALL, 6);
    tagBox->Add(cancel_, 0, wxALL, 6);
    root->Add(tagBox, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);

    auto* progressRow = new wxBoxSizer(wxHORIZONTAL);
    gauge_ = new wxGauge(rootPanel, wxID_ANY, 100);
    status_ = new wxStaticText(rootPanel, wxID_ANY, "Bereit.");
    progressRow->Add(gauge_, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, 10);
    progressRow->Add(status_, 2, wxALIGN_CENTER_VERTICAL);
    root->Add(progressRow, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);

    auto* studyBox = new wxStaticBoxSizer(wxVERTICAL, rootPanel, "Studieninformationen");
    auto* studyGrid = new wxFlexGridSizer(2, 8, 12);
    studyGrid->AddGrowableCol(1, 1);
    patientName_ = AddValueRow(studyBox->GetStaticBox(), studyGrid, "Patient:");
    patientId_ = AddValueRow(studyBox->GetStaticBox(), studyGrid, "Patient ID:");
    accession_ = AddValueRow(studyBox->GetStaticBox(), studyGrid, "Accession Number:");
    studyDate_ = AddValueRow(studyBox->GetStaticBox(), studyGrid, "Study Date:");
    studyDescription_ = AddValueRow(studyBox->GetStaticBox(), studyGrid, "Study Description:");
    studyUid_ = AddValueRow(studyBox->GetStaticBox(), studyGrid, "Study Instance UID:");
    studyBox->Add(studyGrid, 1, wxEXPAND | wxALL, 8);
    root->Add(studyBox, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);

    auto* splitter = new wxSplitterWindow(rootPanel, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                          wxSP_LIVE_UPDATE | wxSP_3D);
    auto* topPanel = new wxPanel(splitter);
    auto* bottomPanel = new wxPanel(splitter);

    auto* seriesSizer = new wxBoxSizer(wxVERTICAL);
    seriesSizer->Add(new wxStaticText(topPanel, wxID_ANY, "Serienuebersicht"), 0, wxALL, 6);
    seriesList_ = new wxListCtrl(topPanel, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                 wxLC_REPORT | wxLC_SINGLE_SEL | wxLC_HRULES | wxLC_VRULES);
    seriesList_->AppendColumn("Serie", wxLIST_FORMAT_LEFT, 75);
    seriesList_->AppendColumn("Modalitaet", wxLIST_FORMAT_LEFT, 85);
    seriesList_->AppendColumn("Serienbeschreibung", wxLIST_FORMAT_LEFT, 270);
    seriesList_->AppendColumn("Protocol Name", wxLIST_FORMAT_LEFT, 220);
    seriesList_->AppendColumn("Body Part", wxLIST_FORMAT_LEFT, 120);
    seriesList_->AppendColumn("Instanzen", wxLIST_FORMAT_RIGHT, 80);
    seriesList_->AppendColumn("Treffer", wxLIST_FORMAT_RIGHT, 70);
    seriesList_->AppendColumn("Series Instance UID", wxLIST_FORMAT_LEFT, 430);
    seriesSizer->Add(seriesList_, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 6);
    topPanel->SetSizer(seriesSizer);

    auto* hitSizer = new wxBoxSizer(wxVERTICAL);
    hitSizer->Add(new wxStaticText(bottomPanel, wxID_ANY, "Treffer / Instanzen"), 0, wxALL, 6);
    hitList_ = new wxListCtrl(bottomPanel, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                              wxLC_REPORT | wxLC_SINGLE_SEL | wxLC_HRULES | wxLC_VRULES);
    hitList_->AppendColumn("Serie", wxLIST_FORMAT_LEFT, 70);
    hitList_->AppendColumn("Instanz", wxLIST_FORMAT_LEFT, 70);
    hitList_->AppendColumn("Modalitaet", wxLIST_FORMAT_LEFT, 80);
    hitList_->AppendColumn("Tag", wxLIST_FORMAT_LEFT, 110);
    hitList_->AppendColumn("Name", wxLIST_FORMAT_LEFT, 210);
    hitList_->AppendColumn("VR", wxLIST_FORMAT_LEFT, 55);
    hitList_->AppendColumn("Wert", wxLIST_FORMAT_LEFT, 330);
    hitList_->AppendColumn("Sequence-Pfad", wxLIST_FORMAT_LEFT, 320);
    hitList_->AppendColumn("Datei", wxLIST_FORMAT_LEFT, 260);
    hitList_->AppendColumn("SOP Instance UID", wxLIST_FORMAT_LEFT, 360);
    hitSizer->Add(hitList_, 1, wxEXPAND | wxLEFT | wxRIGHT, 6);

    auto* hitButtons = new wxBoxSizer(wxHORIZONTAL);
    hitButtons->Add(new wxButton(bottomPanel, ID_SHOW_ALL, "Alle Treffer anzeigen"), 0, wxALL, 6);
    hitButtons->AddStretchSpacer();
    export_ = new wxButton(bottomPanel, ID_EXPORT, "Treffer als CSV exportieren ...");
    export_->Enable(false);
    hitButtons->Add(export_, 0, wxALL, 6);
    hitSizer->Add(hitButtons, 0, wxEXPAND);
    bottomPanel->SetSizer(hitSizer);

    splitter->SplitHorizontally(topPanel, bottomPanel, 300);
    splitter->SetMinimumPaneSize(140);
    root->Add(splitter, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);

    root->Add(new wxStaticText(rootPanel, wxID_ANY,
        "Lokale Verarbeitung. Grosse Binary-/PixelData-Werte werden nicht in die Anzeige geladen."),
        0, wxLEFT | wxRIGHT | wxBOTTOM, 10);

    rootPanel->SetSizer(root);
}

void MainFrame::BindEvents() {
    Bind(wxEVT_BUTTON, &MainFrame::OnChooseFolder, this, ID_CHOOSE_FOLDER);
    Bind(wxEVT_BUTTON, &MainFrame::OnChooseTag, this, ID_CHOOSE_TAG);
    Bind(wxEVT_BUTTON, &MainFrame::OnStart, this, ID_START_SCAN);
    Bind(wxEVT_BUTTON, &MainFrame::OnCancel, this, ID_CANCEL_SCAN);
    Bind(wxEVT_BUTTON, &MainFrame::OnShowAll, this, ID_SHOW_ALL);
    Bind(wxEVT_BUTTON, &MainFrame::OnExport, this, ID_EXPORT);
    seriesList_->Bind(wxEVT_LIST_ITEM_SELECTED, &MainFrame::OnSeriesSelected, this);
    Bind(EVT_SCAN_PROGRESS, &MainFrame::OnProgress, this);
    Bind(EVT_SCAN_COMPLETE, &MainFrame::OnComplete, this);
    Bind(EVT_SCAN_ERROR, &MainFrame::OnScanError, this);
    Bind(wxEVT_CLOSE_WINDOW, &MainFrame::OnClose, this);
}

void MainFrame::OnChooseFolder(wxCommandEvent&) {
    wxDirDialog dialog(this, "DICOM-Studienordner auswaehlen", folder_->GetValue(), wxDD_DEFAULT_STYLE | wxDD_DIR_MUST_EXIST);
    if (dialog.ShowModal() == wxID_OK) folder_->SetValue(dialog.GetPath());
}

void MainFrame::OnChooseTag(wxCommandEvent&) {
    TagDialog dialog(this);
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
        wxMessageBox("Es laeuft bereits eine Suche.", "ElementValueInspector (EVI)", wxOK | wxICON_INFORMATION, this);
        return;
    }

    const wxString folderText = folder_->GetValue();
    if (folderText.empty() || !std::filesystem::is_directory(std::filesystem::path(folderText.ToStdWstring()))) {
        wxMessageBox("Bitte einen gueltigen Studienordner auswaehlen.", "Ordner fehlt", wxOK | wxICON_WARNING, this);
        return;
    }

    std::string error;
    auto target = DicomUtils::ParseTag(U8(tag_->GetValue()), error);
    if (!target) {
        wxMessageBox(W(error), "Ungueltiger DICOM-Tag", wxOK | wxICON_WARNING, this);
        return;
    }

    tagInfo_->SetLabel(W(DicomUtils::TagName(*target)));
    result_.reset();
    seriesList_->DeleteAllItems();
    hitList_->DeleteAllItems();
    gauge_->SetValue(0);
    status_->SetLabel("Suche wird gestartet ...");
    start_->Enable(false);
    cancel_->Enable(true);
    export_->Enable(false);
    cancelRequested_.store(false);

    const std::filesystem::path root(folderText.ToStdWstring());
    const Uint16 group = target->getGroup();
    const Uint16 element = target->getElement();

    worker_ = std::thread([this, root, group, element]() {
        try {
            DicomScanner scanner;
            auto result = scanner.Scan(root, group, element, cancelRequested_,
                [this](std::size_t current, std::size_t total, const std::filesystem::path& path) {
                    auto* event = new wxThreadEvent(EVT_SCAN_PROGRESS);
                    event->SetInt(total ? static_cast<int>((current * 100) / total) : 0);
                    event->SetString(wxString::Format("Datei %zu von %zu: ", current, total)
                        + wxString(path.filename().wstring()));
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
            event->SetString("Unbekannter Fehler beim Durchsuchen der DICOM-Studie.");
            wxQueueEvent(this, event);
        }
    });
}

void MainFrame::OnCancel(wxCommandEvent&) {
    cancelRequested_.store(true);
    cancel_->Enable(false);
    status_->SetLabel("Abbruch angefordert ...");
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
    export_->Enable(result_ && !result_->hits.empty());
    PopulateResult();
}

void MainFrame::OnScanError(wxThreadEvent& event) {
    if (worker_.joinable()) worker_.join();
    start_->Enable(true);
    cancel_->Enable(false);
    status_->SetLabel("Fehler.");
    wxMessageBox(event.GetString(), "Fehler beim DICOM-Scan", wxOK | wxICON_ERROR, this);
}

void MainFrame::PopulateResult() {
    if (!result_) return;
    UpdateStudyLabels();
    PopulateSeries();
    PopulateHits();

    const auto& s = result_->stats;
    if (!s.cancelled) gauge_->SetValue(100);
    const wxString state = s.cancelled ? wxString("Abgebrochen.") : wxString("Fertig.");
    status_->SetLabel(state + wxString::Format(
        " %zu DICOM-Dateien, %zu Dateien mit Tag, %zu Treffer, %zu Serien, %zu Lesefehler.",
        s.dicomFiles, s.filesWithTag, s.tagHits, result_->series.size(), s.readErrors));
}

void MainFrame::UpdateStudyLabels() {
    auto set = [](wxStaticText* label, const std::string& value) {
        label->SetLabel(value.empty() ? wxString("-") : W(value));
    };
    if (!result_ || !result_->hasStudy) {
        set(patientName_, {}); set(patientId_, {}); set(accession_, {}); set(studyDate_, {}); set(studyDescription_, {}); set(studyUid_, {});
        return;
    }
    set(patientName_, result_->study.patientName);
    set(patientId_, result_->study.patientId);
    set(accession_, result_->study.accessionNumber);
    set(studyDate_, result_->study.studyDate);
    set(studyDescription_, result_->study.studyDescription);
    set(studyUid_, result_->study.studyInstanceUid);
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

    wxFileDialog dialog(this, "Treffer als CSV speichern", "", "dicom_tag_treffer.csv",
                        "CSV-Dateien (*.csv)|*.csv", wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dialog.ShowModal() != wxID_OK) return;

    const std::filesystem::path path(dialog.GetPath().ToStdWstring());
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        wxMessageBox("Die CSV-Datei konnte nicht geschrieben werden.", "CSV-Export", wxOK | wxICON_ERROR, this);
        return;
    }

    // UTF-8 BOM for comfortable Excel import on Windows.
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

    wxMessageBox("CSV wurde erfolgreich gespeichert.", "CSV-Export", wxOK | wxICON_INFORMATION, this);
}

void MainFrame::StopWorker() {
    cancelRequested_.store(true);
    if (worker_.joinable()) worker_.join();
}

void MainFrame::OnClose(wxCloseEvent& event) {
    StopWorker();
    event.Skip();
}
