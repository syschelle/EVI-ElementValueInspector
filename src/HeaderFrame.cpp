#include "HeaderFrame.h"
#include "AppIcon.h"
#include "DicomUtils.h"

#include <dcmtk/dcmdata/dcfilefo.h>
#include <dcmtk/dcmdata/dcelem.h>
#include <dcmtk/dcmdata/dcvr.h>
#include <dcmtk/dcmdata/dcitem.h>
#include <dcmtk/dcmdata/dcmetinf.h>
#include <dcmtk/dcmdata/dcsequen.h>

#include <wx/button.h>
#include <wx/clipbrd.h>
#include <wx/dataobj.h>
#include <wx/dataview.h>
#include <wx/font.h>
#include <wx/menu.h>
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
#include <array>
#include <memory>
#include <stdexcept>
#include <unordered_map>
#include <utility>
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

class MultiLineTextRenderer final : public wxDataViewCustomRenderer {
public:
    MultiLineTextRenderer()
        : wxDataViewCustomRenderer("string", wxDATAVIEW_CELL_INERT, wxALIGN_LEFT | wxALIGN_TOP) {}

    bool Render(wxRect rect, wxDC* dc, int state) override {
        rect.Deflate(GetView()->FromDIP(4), GetView()->FromDIP(2));
        RenderText(value_, 0, rect, dc, state);
        return true;
    }

    wxSize GetSize() const override {
        wxString text = value_;
        text.Replace("\r\n", "\n");
        text.Replace("\r", "\n");

        const int lineCount = std::max(1, static_cast<int>(text.Freq('\n')) + 1);
        wxSize size = GetTextExtent(text.empty() ? wxString(L" ") : text);
        const int baseLineHeight = std::max(GetTextExtent(wxString(L"Ag")).GetHeight(), size.GetHeight());
        size.SetHeight(baseLineHeight * lineCount + GetView()->FromDIP(6));
        size.SetWidth(size.GetWidth() + GetView()->FromDIP(8));
        return size;
    }

    bool SetValue(const wxVariant& value) override {
        value_ = value.GetString();
        value_.Replace("\r\n", "\n");
        value_.Replace("\r", "\n");
        return true;
    }

    bool GetValue(wxVariant& value) const override {
        value = value_;
        return true;
    }

#if wxUSE_ACCESSIBILITY
    wxString GetAccessibleDescription() const override {
        return value_;
    }
#endif

private:
    wxString value_;
};

struct HeaderTreeNode {
    HeaderTreeNode* parent{};
    std::vector<std::unique_ptr<HeaderTreeNode>> children;
    std::array<wxString, 5> cells;
    bool forceContainer{};
};

class HeaderTreeModel final : public wxDataViewModel {
public:
    HeaderTreeModel()
        : root_(std::make_unique<HeaderTreeNode>()) {
        root_->forceContainer = true;
    }

    HeaderTreeNode* Append(HeaderTreeNode* parent,
                           std::array<wxString, 5> cells,
                           bool forceContainer = false) {
        if (!parent) parent = root_.get();
        auto node = std::make_unique<HeaderTreeNode>();
        node->parent = parent;
        node->cells = std::move(cells);
        node->forceContainer = forceContainer;
        HeaderTreeNode* raw = node.get();
        parent->children.push_back(std::move(node));
        return raw;
    }

    HeaderTreeNode* Root() const { return root_.get(); }

    unsigned int GetColumnCount() const override { return 5; }

    wxString GetColumnType(unsigned int) const override { return wxString(L"string"); }

    void GetValue(wxVariant& variant, const wxDataViewItem& item, unsigned int col) const override {
        const auto* node = Node(item);
        if (!node || col >= node->cells.size()) {
            variant = wxString{};
            return;
        }
        variant = node->cells[col];
    }

    bool SetValue(const wxVariant&, const wxDataViewItem&, unsigned int) override { return false; }

    bool IsContainer(const wxDataViewItem& item) const override {
        if (!item.IsOk()) return true;
        const auto* node = Node(item);
        return node && (node->forceContainer || !node->children.empty());
    }

    bool HasContainerColumns(const wxDataViewItem&) const override { return true; }

    wxDataViewItem GetParent(const wxDataViewItem& item) const override {
        const auto* node = Node(item);
        if (!node || !node->parent || node->parent == root_.get()) return wxDataViewItem{};
        return wxDataViewItem(node->parent);
    }

    unsigned int GetChildren(const wxDataViewItem& item, wxDataViewItemArray& children) const override {
        const HeaderTreeNode* node = item.IsOk() ? Node(item) : root_.get();
        if (!node) return 0;
        for (const auto& child : node->children) {
            children.Add(wxDataViewItem(child.get()));
        }
        return static_cast<unsigned int>(node->children.size());
    }

private:
    static HeaderTreeNode* Node(const wxDataViewItem& item) {
        return item.IsOk() ? static_cast<HeaderTreeNode*>(item.GetID()) : nullptr;
    }

    std::unique_ptr<HeaderTreeNode> root_;
};

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
    ApplyEviAppIcon(this);
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
        L(L"Sequenzen sind gruppiert und aufklappbar. Mehrzeilige Werte vergrößern ihre Zeile automatisch. Rechtsklick oder Strg+C kopiert die aktuelle Zelle (außer VR).",
          L"Sequences are grouped and expandable. Multi-line values automatically increase their row height. Right-click or Ctrl+C copies the current cell (except VR)."));
    root->Add(hint_, 0, wxLEFT | wxRIGHT | wxTOP | wxBOTTOM, 18);

    tree_ = new wxDataViewCtrl(rootPanel_, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                               wxDV_ROW_LINES | wxDV_VARIABLE_LINE_HEIGHT);

    auto* tagColumn = new wxDataViewColumn(wxString(L"Tag"),
        new wxDataViewTextRenderer("string", wxDATAVIEW_CELL_INERT),
        0, 180, wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE);
    tree_->AppendColumn(tagColumn);
    tree_->AppendColumn(new wxDataViewColumn(L(L"Name", L"Name"),
        new wxDataViewTextRenderer("string", wxDATAVIEW_CELL_INERT),
        1, 280, wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE));
    tree_->AppendColumn(new wxDataViewColumn(wxString(L"VR"),
        new wxDataViewTextRenderer("string", wxDATAVIEW_CELL_INERT),
        2, 65, wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE));
    tree_->AppendColumn(new wxDataViewColumn(L(L"Wert", L"Value"),
        new MultiLineTextRenderer(),
        3, 470, wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE));
    tree_->AppendColumn(new wxDataViewColumn(L(L"Sequence-Pfad", L"Sequence path"),
        new wxDataViewTextRenderer("string", wxDATAVIEW_CELL_INERT),
        4, 360, wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE));
    tree_->SetExpanderColumn(tagColumn);
    root->Add(tree_, 1, wxEXPAND | wxLEFT | wxRIGHT, 18);

    status_ = new wxStaticText(rootPanel_, wxID_ANY, wxEmptyString);
    root->Add(status_, 0, wxLEFT | wxRIGHT | wxTOP | wxBOTTOM, 18);

    rootPanel_->SetSizer(root);

    search_->Bind(wxEVT_TEXT_ENTER, &HeaderFrame::OnSearch, this);
    findNext_->Bind(wxEVT_BUTTON, &HeaderFrame::OnFindNext, this);
    tree_->Bind(wxEVT_DATAVIEW_ITEM_CONTEXT_MENU, &HeaderFrame::OnContextMenu, this);
    Bind(wxEVT_MENU, &HeaderFrame::OnCopyCell, this, wxID_COPY);
    Bind(wxEVT_CHAR_HOOK, &HeaderFrame::OnCharHook, this);
}

void HeaderFrame::LoadHeader() {
    DcmFileFormat dcm;
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
    rowItems_.assign(rows_.size(), wxDataViewItem{});

    auto* model = new HeaderTreeModel();
    HeaderTreeNode* datasetRootNode = model->Append(nullptr,
        {L(L"Datensatz", L"Dataset"), L(L"DICOM-Datensatz", L"DICOM dataset"), wxString{}, wxString{}, wxString{}},
        true);
    HeaderTreeNode* metaRootNode = model->Append(nullptr,
        {L(L"Datei-Metainformationen", L"File Meta Information"),
         L(L"Dateiheader (0002,xxxx)", L"File header (0002,xxxx)"), wxString{}, wxString{}, wxString{}},
        true);

    std::unordered_map<std::string, HeaderTreeNode*> sequenceNodes;

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
        HeaderTreeNode* parent = fileMeta ? metaRootNode : datasetRootNode;
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

            HeaderTreeNode* placeholder = model->Append(parent,
                {W(part), L(L"Sequenz", L"Sequence"), wxString(L"SQ"), wxString{}, W(cumulative)},
                true);
            sequenceNodes.emplace(key, placeholder);
            parent = placeholder;
        }
        return parent;
    };

    for (std::size_t i = 0; i < rows_.size(); ++i) {
        const HeaderRow& row = rows_[i];
        const std::string relativePath = relativeSequencePath(row);
        HeaderTreeNode* parent = ensureSequencePath(row.fileMeta, relativePath);

        if (row.sequence) {
            const std::string sequenceComponent = row.tag + " " + row.name;
            const std::string fullPath = JoinPath(relativePath, sequenceComponent);
            const std::string key = std::string(1, row.fileMeta ? 'M' : 'D') + "|" + fullPath;

            HeaderTreeNode* node = nullptr;
            const auto existing = sequenceNodes.find(key);
            if (existing != sequenceNodes.end()) {
                node = existing->second;
                node->cells = {W(row.tag), W(row.name), W(row.vr), W(row.value), W(row.sequencePath)};
                node->forceContainer = true;
            } else {
                node = model->Append(parent,
                    {W(row.tag), W(row.name), W(row.vr), W(row.value), W(row.sequencePath)},
                    true);
                sequenceNodes.emplace(key, node);
            }

            rowItems_[i] = wxDataViewItem(node);
            continue;
        }

        HeaderTreeNode* node = model->Append(parent,
            {W(row.tag), W(row.name), W(row.vr), W(row.value), W(row.sequencePath)});
        rowItems_[i] = wxDataViewItem(node);
    }

    tree_->AssociateModel(model);
    model->DecRef();

    const wxDataViewItem datasetRoot(datasetRootNode);
    const wxDataViewItem metaRoot(metaRootNode);
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
    const wxDataViewItem item = rowItems_[row];
    if (!item.IsOk() || !tree_->GetModel()) return;

    for (wxDataViewItem parent = tree_->GetModel()->GetParent(item);
         parent.IsOk();
         parent = tree_->GetModel()->GetParent(parent)) {
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

wxString HeaderFrame::CopyLabelForColumn(int column) const {
    switch (column) {
        case 0: return L(L"Tag kopieren", L"Copy tag");
        case 1: return L(L"Name kopieren", L"Copy name");
        case 3: return L(L"Wert kopieren", L"Copy value");
        case 4: return L(L"Sequence-Pfad kopieren", L"Copy sequence path");
        default: return L(L"Kopieren", L"Copy");
    }
}

bool HeaderFrame::CopyCell(const wxDataViewItem& item, int column) {
    if (!item.IsOk() || !tree_->GetModel()) return false;
    if (column < 0 || column >= 5 || column == 2) return false; // VR is intentionally not copyable.

    wxVariant value;
    tree_->GetModel()->GetValue(value, item, static_cast<unsigned int>(column));
    const wxString text = value.GetString();
    if (text.empty()) return false;

    if (!wxTheClipboard->Open()) {
        status_->SetLabel(L(L"Zwischenablage konnte nicht geöffnet werden.", L"Could not open the clipboard."));
        return false;
    }

    wxTheClipboard->SetData(new wxTextDataObject(text));
    wxTheClipboard->Flush();
    wxTheClipboard->Close();

    status_->SetLabel(L(L"Zelleninhalt wurde in die Zwischenablage kopiert.",
                         L"Cell content copied to clipboard."));
    return true;
}

bool HeaderFrame::CopyCurrentCell() {
    const wxDataViewItem item = tree_->GetCurrentItem();
    wxDataViewColumn* column = tree_->GetCurrentColumn();
    if (!item.IsOk() || !column) return false;
    const int columnIndex = tree_->GetColumnPosition(column);
    return CopyCell(item, columnIndex);
}

void HeaderFrame::OnSearch(wxCommandEvent&) {
    RunSearch(true);
}

void HeaderFrame::OnFindNext(wxCommandEvent&) {
    RunSearch(true);
}

void HeaderFrame::OnContextMenu(wxDataViewEvent& event) {
    wxDataViewItem item = event.GetItem();
    int column = event.GetColumn();

    if (!item.IsOk() || column < 0) {
        wxDataViewColumn* hitColumn = nullptr;
        tree_->HitTest(event.GetPosition(), item, hitColumn);
        if (hitColumn) column = tree_->GetColumnPosition(hitColumn);
    }

    if (!item.IsOk() || column < 0 || column == 2) return;

    wxVariant value;
    if (!tree_->GetModel()) return;
    tree_->GetModel()->GetValue(value, item, static_cast<unsigned int>(column));
    if (value.GetString().empty()) return;

    contextItem_ = item;
    contextColumn_ = column;
    tree_->Select(item);

    wxMenu menu;
    menu.Append(wxID_COPY, CopyLabelForColumn(column));
    PopupMenu(&menu);
}

void HeaderFrame::OnCopyCell(wxCommandEvent&) {
    if (contextItem_.IsOk() && contextColumn_ >= 0) {
        CopyCell(contextItem_, contextColumn_);
    } else {
        CopyCurrentCell();
    }
}

void HeaderFrame::OnCharHook(wxKeyEvent& event) {
    const int key = event.GetKeyCode();
    if (event.ControlDown() && (key == 'C' || key == 'c')) {
        wxWindow* focus = wxWindow::FindFocus();
        if (focus && focus != search_ && (focus == tree_ || tree_->IsDescendant(focus))) {
            if (CopyCurrentCell()) return;
        }
    }
    event.Skip();
}

void HeaderFrame::ApplyTheme() {
    const bool dark = theme_ == UiTheme::Dark;
    const wxColour background = dark ? wxColour(24, 27, 32) : wxColour(240, 243, 247);
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
    tree_->Refresh();

#ifdef _WIN32
    if (tree_->GetHandle()) {
        SetWindowTheme(reinterpret_cast<HWND>(tree_->GetHandle()), dark ? L"DarkMode_Explorer" : L"Explorer", nullptr);
    }
    ApplyNativeDarkMode(this, dark);
#endif
    Refresh();
}
