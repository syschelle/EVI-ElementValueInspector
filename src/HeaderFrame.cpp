#include "HeaderFrame.h"
#include "DicomUtils.h"

#include <dcmtk/dcmdata/dcfilefo.h>
#include <dcmtk/dcmdata/dcelem.h>
#include <dcmtk/dcmdata/dcvr.h>
#include <dcmtk/dcmdata/dcitem.h>
#include <dcmtk/dcmdata/dcmetinf.h>
#include <dcmtk/dcmdata/dcsequen.h>

#include <wx/button.h>
#include <wx/font.h>
#include <wx/msgdlg.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

#ifdef _WIN32
#include <windows.h>
#include <dwmapi.h>
#include <uxtheme.h>
#endif

#include <algorithm>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace {

wxString W(const std::string& text) {
    return wxString::FromUTF8(text.c_str());
}

std::string U8(const wxString& text) {
    const wxScopedCharBuffer buffer = text.ToUTF8();
    return buffer.data() ? std::string(buffer.data()) : std::string{};
}

wxString LowerText(wxString text) {
    text.MakeLower();
    return text;
}

std::vector<std::string> SplitSequencePath(const std::string& path) {
    std::vector<std::string> parts;
    if (path.empty()) return parts;

    constexpr const char* delimiter = " > ";
    std::size_t start = 0;
    while (start <= path.size()) {
        const std::size_t end = path.find(delimiter, start);
        if (end == std::string::npos) {
            if (start < path.size()) parts.push_back(path.substr(start));
            break;
        }
        if (end > start) parts.push_back(path.substr(start, end - start));
        start = end + 3;
    }
    return parts;
}

std::string JoinPath(const std::string& left, const std::string& right) {
    if (left.empty()) return right;
    if (right.empty()) return left;
    return left + " > " + right;
}

#ifdef _WIN32
void ApplyNativeDarkMode(wxWindow* window, bool dark) {
    if (!window || !window->GetHandle()) return;
    HWND hwnd = reinterpret_cast<HWND>(window->GetHandle());
    const BOOL value = dark ? TRUE : FALSE;
    constexpr DWORD kImmersiveDarkMode = 20;
    DwmSetWindowAttribute(hwnd, kImmersiveDarkMode, &value, sizeof(value));
}
#endif

} // namespace

HeaderFrame::HeaderFrame(wxWindow* parent,
                         const ScanHit& sourceHit,
                         UiLanguage language,
                         UiTheme theme)
    : wxFrame(parent, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(1320, 820),
              wxDEFAULT_FRAME_STYLE | wxCLIP_CHILDREN),
      language_(language),
      theme_(theme),
      filePath_(std::filesystem::path(W(sourceHit.fullFile).ToStdWstring())),
      initialTag_(sourceHit.tag),
      initialValue_(sourceHit.value),
      initialSequencePath_(sourceHit.sequencePath),
      seriesNumber_(sourceHit.seriesNumber),
      instanceNumber_(sourceHit.instanceNumber),
      modality_(sourceHit.modality)
{
    SetMinSize(wxSize(900, 580));
    SetFont(wxFont(wxFontInfo(10).FaceName(wxString(L"Segoe UI"))));
    BuildUi();
    LoadHeader();
    PopulateTree();
    ApplyTheme();
    CentreOnParent();

    CallAfter([this]() {
        SelectInitialMatch();
        search_->SetFocus();
    });
}

wxString HeaderFrame::L(const wchar_t* de, const wchar_t* en) const {
    return wxString(language_ == UiLanguage::German ? de : en);
}

void HeaderFrame::BuildUi() {
    SetTitle(L(L"DICOM-Header – ElementValueInspector", L"DICOM Header – ElementValueInspector"));

    rootPanel_ = new wxPanel(this);
    auto* root = new wxBoxSizer(wxVERTICAL);

    title_ = new wxStaticText(rootPanel_, wxID_ANY, L(L"DICOM-Header", L"DICOM Header"));
    wxFont titleFont = title_->GetFont();
    titleFont.SetPointSize(titleFont.GetPointSize() + 5);
    titleFont.MakeBold();
    title_->SetFont(titleFont);
    root->Add(title_, 0, wxLEFT | wxRIGHT | wxTOP, 18);

    const wxString series = seriesNumber_.empty() ? wxString(L"-") : W(seriesNumber_);
    const wxString instance = instanceNumber_.empty() ? wxString(L"-") : W(instanceNumber_);
    const wxString modality = modality_.empty() ? wxString(L"-") : W(modality_);
    context_ = new wxStaticText(rootPanel_, wxID_ANY,
        wxString::Format(L(L"Serie %s · Instanz %s · %s", L"Series %s · Instance %s · %s"),
                         series.c_str(), instance.c_str(), modality.c_str()));
    root->Add(context_, 0, wxLEFT | wxRIGHT | wxTOP, 18);

    file_ = new wxStaticText(rootPanel_, wxID_ANY, wxString(filePath_.wstring()), wxDefaultPosition,
                             wxDefaultSize, wxST_ELLIPSIZE_MIDDLE);
    file_->SetToolTip(wxString(filePath_.wstring()));
    root->Add(file_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP | wxBOTTOM, 18);

    auto* searchRow = new wxBoxSizer(wxHORIZONTAL);
    searchLabel_ = new wxStaticText(rootPanel_, wxID_ANY, L(L"Im Header suchen", L"Search header"));
    search_ = new wxTextCtrl(rootPanel_, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize,
                             wxTE_PROCESS_ENTER);
    search_->SetHint(L(L"Tagnummer, Tagname, VR, Wert oder Sequence-Pfad",
                       L"Tag number, tag name, VR, value or sequence path"));
    findNext_ = new wxButton(rootPanel_, wxID_ANY, L(L"Weiter", L"Find next"));
    findNext_->SetMinSize(wxSize(110, 32));
    searchRow->Add(searchLabel_, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 10);
    searchRow->Add(search_, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, 10);
    searchRow->Add(findNext_, 0, wxALIGN_CENTER_VERTICAL);
    root->Add(searchRow, 0, wxEXPAND | wxLEFT | wxRIGHT, 18);

    hint_ = new wxStaticText(rootPanel_, wxID_ANY,
        L(L"Sequenzen sind gruppiert und aufklappbar. Die Suche gilt nur für dieses Header-Fenster und verändert die ursprüngliche Studiensuche nicht.",
          L"Sequences are grouped and expandable. This search applies only to this header window and never changes the original study search."));
    root->Add(hint_, 0, wxLEFT | wxRIGHT | wxTOP | wxBOTTOM, 18);

    tree_ = new wxTreeListCtrl(rootPanel_, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTL_DEFAULT_STYLE);
    tree_->AppendColumn(wxString(L"Tag"), 180, wxALIGN_LEFT, wxCOL_RESIZABLE);
    tree_->AppendColumn(L(L"Name", L"Name"), 280, wxALIGN_LEFT, wxCOL_RESIZABLE);
    tree_->AppendColumn(wxString(L"VR"), 65, wxALIGN_LEFT, wxCOL_RESIZABLE);
    tree_->AppendColumn(L(L"Wert", L"Value"), 470, wxALIGN_LEFT, wxCOL_RESIZABLE);
    tree_->AppendColumn(L(L"Sequence-Pfad", L"Sequence path"), 360, wxALIGN_LEFT, wxCOL_RESIZABLE);
    root->Add(tree_, 1, wxEXPAND | wxLEFT | wxRIGHT, 18);

    status_ = new wxStaticText(rootPanel_, wxID_ANY, wxEmptyString);
    root->Add(status_, 0, wxLEFT | wxRIGHT | wxTOP | wxBOTTOM, 18);

    rootPanel_->SetSizer(root);

    search_->Bind(wxEVT_TEXT_ENTER, &HeaderFrame::OnSearch, this);
    findNext_->Bind(wxEVT_BUTTON, &HeaderFrame::OnFindNext, this);
}

void HeaderFrame::LoadHeader() {
    DcmFileFormat dcm;
    // Read metadata values up to 1 MiB directly. Larger binary values and Pixel Data
    // remain deferred/omitted by SafeElementValue().
    OFCondition status = DicomUtils::LoadDicomFile(dcm, filePath_, 1024U * 1024U);
    if (status.bad()) {
        throw std::runtime_error(status.text());
    }

#ifdef DCMTK_ENABLE_CHARSET_CONVERSION
    dcm.convertToUTF8();
#endif

    CollectRows(dcm.getDataset(), {}, false);
    CollectRows(dcm.getMetaInfo(), language_ == UiLanguage::German
        ? "Datei-Metainformationen"
        : "File Meta Information", true);
}

void HeaderFrame::CollectRows(DcmItem* container, const std::string& prefix, bool fileMeta) {
    if (!container) return;

    DcmStack stack;
    while (container->nextObject(stack, OFTrue).good()) {
        DcmObject* object = stack.top();
        if (!object) continue;

        // Items are traversal containers rather than DICOM data elements.
        if (dynamic_cast<DcmElement*>(object) == nullptr) continue;

        const DcmTagKey key = object->getTag().getTagKey();
        HeaderRow row;
        row.group = key.getGroup();
        row.element = key.getElement();
        row.tag = DicomUtils::FormatTag(key);
        row.name = DicomUtils::TagName(key);
        row.vr = DcmVR(object->getVR()).getVRName();
        row.value = DicomUtils::SafeElementValue(object, language_);
        row.sequencePath = DicomUtils::SequencePath(stack, prefix);
        row.fileMeta = fileMeta;
        row.sequence = dynamic_cast<DcmSequenceOfItems*>(object) != nullptr;
        rows_.push_back(std::move(row));
    }
}

void HeaderFrame::PopulateTree() {
    tree_->Freeze();
    tree_->DeleteAllItems();
    rowItems_.assign(rows_.size(), wxTreeListItem{});

    const wxTreeListItem root = tree_->GetRootItem();
    const wxTreeListItem datasetRoot = tree_->AppendItem(root, L(L"Datensatz", L"Dataset"));
    const wxTreeListItem metaRoot = tree_->AppendItem(root, L(L"Datei-Metainformationen", L"File Meta Information"));

    tree_->SetItemText(datasetRoot, 1, L(L"DICOM-Datensatz", L"DICOM dataset"));
    tree_->SetItemText(metaRoot, 1, L(L"Dateiheader (0002,xxxx)", L"File header (0002,xxxx)"));

    std::unordered_map<std::string, wxTreeListItem> sequenceNodes;

    const std::string metaPrefix = language_ == UiLanguage::German
        ? "Datei-Metainformationen"
        : "File Meta Information";

    auto relativeSequencePath = [&](const HeaderRow& row) {
        std::string path = row.sequencePath;
        if (row.fileMeta) {
            if (path == metaPrefix) {
                path.clear();
            } else {
                const std::string prefix = metaPrefix + " > ";
                if (path.starts_with(prefix)) path.erase(0, prefix.size());
            }
        }
        return path;
    };

    auto ensureSequencePath = [&](bool fileMeta, const std::string& path) {
        wxTreeListItem parent = fileMeta ? metaRoot : datasetRoot;
        if (path.empty()) return parent;

        std::string cumulative;
        const char sectionKey = fileMeta ? 'M' : 'D';
        for (const auto& part : SplitSequencePath(path)) {
            cumulative = JoinPath(cumulative, part);
            const std::string key = std::string(1, sectionKey) + "|" + cumulative;
            const auto found = sequenceNodes.find(key);
            if (found != sequenceNodes.end()) {
                parent = found->second;
                continue;
            }

            // Normally the sequence element itself is encountered before its children.
            // A placeholder keeps malformed/unusual traversal orders grouped correctly.
            const wxTreeListItem placeholder = tree_->AppendItem(parent, W(part));
            tree_->SetItemText(placeholder, 1, L(L"Sequenz", L"Sequence"));
            tree_->SetItemText(placeholder, 2, wxString(L"SQ"));
            sequenceNodes.emplace(key, placeholder);
            parent = placeholder;
        }
        return parent;
    };

    for (std::size_t i = 0; i < rows_.size(); ++i) {
        const HeaderRow& row = rows_[i];
        const std::string relativePath = relativeSequencePath(row);
        wxTreeListItem parent = ensureSequencePath(row.fileMeta, relativePath);

        if (row.sequence) {
            const std::string sequenceComponent = row.tag + " " + row.name;
            const std::string fullPath = JoinPath(relativePath, sequenceComponent);
            const std::string key = std::string(1, row.fileMeta ? 'M' : 'D') + "|" + fullPath;

            wxTreeListItem item;
            const auto existing = sequenceNodes.find(key);
            if (existing != sequenceNodes.end()) {
                item = existing->second;
                tree_->SetItemText(item, 0, W(row.tag));
            } else {
                item = tree_->AppendItem(parent, W(row.tag));
                sequenceNodes.emplace(key, item);
            }

            tree_->SetItemText(item, 1, W(row.name));
            tree_->SetItemText(item, 2, W(row.vr));
            tree_->SetItemText(item, 3, W(row.value));
            tree_->SetItemText(item, 4, W(row.sequencePath));
            rowItems_[i] = item;
            continue;
        }

        const wxTreeListItem item = tree_->AppendItem(parent, W(row.tag));
        tree_->SetItemText(item, 1, W(row.name));
        tree_->SetItemText(item, 2, W(row.vr));
        tree_->SetItemText(item, 3, W(row.value));
        tree_->SetItemText(item, 4, W(row.sequencePath));
        rowItems_[i] = item;
    }

    // Keep the two main sections open while leaving sequences collapsed by default.
    tree_->Expand(datasetRoot);
    tree_->Expand(metaRoot);
    tree_->Thaw();

    status_->SetLabel(language_ == UiLanguage::German
        ? wxString::Format(L"%zu DICOM-Elemente geladen · Sequenzen gruppiert", rows_.size())
        : wxString::Format(L"%zu DICOM elements loaded · sequences grouped", rows_.size()));
}

void HeaderFrame::SelectInitialMatch() {
    if (rows_.empty()) return;

    long fallback = -1;
    long valueFallback = -1;
    long pathFallback = -1;
    long exact = -1;

    for (std::size_t i = 0; i < rows_.size(); ++i) {
        const auto& row = rows_[i];
        if (row.tag != initialTag_) continue;
        if (fallback < 0) fallback = static_cast<long>(i);
        if (valueFallback < 0 && row.value == initialValue_) valueFallback = static_cast<long>(i);
        if (pathFallback < 0 && row.sequencePath == initialSequencePath_) pathFallback = static_cast<long>(i);
        if (row.value == initialValue_ && row.sequencePath == initialSequencePath_) {
            exact = static_cast<long>(i);
            break;
        }
    }

    const long row = exact >= 0 ? exact : (pathFallback >= 0 ? pathFallback : (valueFallback >= 0 ? valueFallback : fallback));
    if (row >= 0) SelectRow(static_cast<std::size_t>(row));
}

bool HeaderFrame::RowMatchesSearch(const HeaderRow& row, const wxString& query) const {
    wxString trimmed = query;
    trimmed.Trim(true).Trim(false);
    if (trimmed.empty()) return false;

    std::string parseError;
    if (const auto tag = DicomUtils::ParseTag(U8(trimmed), parseError, language_)) {
        return row.group == tag->getGroup() && row.element == tag->getElement();
    }

    const wxString needle = LowerText(trimmed);
    wxString haystack = W(row.tag + " " + row.name + " " + row.vr + " " + row.value + " " + row.sequencePath);
    haystack.MakeLower();
    return haystack.Find(needle) != wxNOT_FOUND;
}

void HeaderFrame::RunSearch(bool advance) {
    wxString query = search_->GetValue();
    query.Trim(true).Trim(false);
    if (query.empty()) {
        searchMatches_.clear();
        lastQuery_.clear();
        UpdateSearchStatus();
        return;
    }

    const bool queryChanged = query != lastQuery_;
    if (queryChanged) {
        lastQuery_ = query;
        searchMatches_.clear();
        for (std::size_t i = 0; i < rows_.size(); ++i) {
            if (RowMatchesSearch(rows_[i], query)) searchMatches_.push_back(i);
        }
        searchPosition_ = 0;
    } else if (advance && !searchMatches_.empty()) {
        searchPosition_ = (searchPosition_ + 1) % searchMatches_.size();
    }

    if (!searchMatches_.empty()) {
        SelectRow(searchMatches_[searchPosition_]);
    }
    UpdateSearchStatus();
}

void HeaderFrame::SelectRow(std::size_t row) {
    if (row >= rowItems_.size()) return;
    const wxTreeListItem item = rowItems_[row];
    if (!item.IsOk()) return;

    // Expand all sequence ancestors so a search hit is always visible.
    const wxTreeListItem root = tree_->GetRootItem();
    for (wxTreeListItem parent = tree_->GetItemParent(item);
         parent.IsOk() && parent != root;
         parent = tree_->GetItemParent(parent)) {
        tree_->Expand(parent);
    }

    tree_->Select(item);
    tree_->EnsureVisible(item);
}

void HeaderFrame::UpdateSearchStatus() {
    if (lastQuery_.empty()) {
        status_->SetLabel(language_ == UiLanguage::German
            ? wxString::Format(L"%zu DICOM-Elemente geladen · Sequenzen gruppiert", rows_.size())
            : wxString::Format(L"%zu DICOM elements loaded · sequences grouped", rows_.size()));
        return;
    }

    if (searchMatches_.empty()) {
        status_->SetLabel(L(L"Keine passenden Header-Elemente gefunden.", L"No matching header elements found."));
        return;
    }

    status_->SetLabel(language_ == UiLanguage::German
        ? wxString::Format(L"Treffer %zu von %zu", searchPosition_ + 1, searchMatches_.size())
        : wxString::Format(L"Match %zu of %zu", searchPosition_ + 1, searchMatches_.size()));
}

void HeaderFrame::OnSearch(wxCommandEvent&) {
    RunSearch(true);
}

void HeaderFrame::OnFindNext(wxCommandEvent&) {
    RunSearch(true);
}

void HeaderFrame::ApplyTheme() {
    const bool dark = theme_ == UiTheme::Dark;
    const wxColour background = dark ? wxColour(24, 27, 32) : wxColour(244, 246, 249);
    const wxColour table = dark ? wxColour(28, 32, 38) : wxColour(255, 255, 255);
    const wxColour input = dark ? wxColour(42, 47, 55) : wxColour(255, 255, 255);
    const wxColour text = dark ? wxColour(235, 238, 242) : wxColour(30, 35, 42);
    const wxColour muted = dark ? wxColour(164, 173, 184) : wxColour(100, 109, 120);

    SetBackgroundColour(background);
    if (rootPanel_) rootPanel_->SetBackgroundColour(background);
    title_->SetForegroundColour(text);
    context_->SetForegroundColour(text);
    file_->SetForegroundColour(muted);
    searchLabel_->SetForegroundColour(text);
    hint_->SetForegroundColour(muted);
    status_->SetForegroundColour(muted);
    search_->SetBackgroundColour(input);
    search_->SetForegroundColour(text);
    tree_->SetBackgroundColour(table);
    tree_->SetForegroundColour(text);

#ifdef _WIN32
    if (tree_->GetHandle()) {
        SetWindowTheme(reinterpret_cast<HWND>(tree_->GetHandle()), dark ? L"DarkMode_Explorer" : L"Explorer", nullptr);
    }
    ApplyNativeDarkMode(this, dark);
#endif
    Refresh();
}
