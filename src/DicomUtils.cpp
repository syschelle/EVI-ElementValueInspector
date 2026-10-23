#include "DicomUtils.h"

#include <dcmtk/dcmdata/dcdict.h>
#include <dcmtk/dcmdata/dcdicent.h>
#include <dcmtk/dcmdata/dcdeftag.h>
#include <dcmtk/dcmdata/dcsequen.h>
#include <dcmtk/dcmdata/dcvr.h>

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <sstream>

namespace {

std::string Trim(std::string value) {
    auto notSpace = [](unsigned char c) { return !std::isspace(c); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), notSpace));
    value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
    return value;
}

bool IsHex8(const std::string& value) {
    if (value.size() != 8) return false;
    return std::all_of(value.begin(), value.end(), [](unsigned char c) { return std::isxdigit(c) != 0; });
}

std::string FormatVm(const DcmDictEntry& entry) {
    const int min = entry.getVMMin();
    const int max = entry.getVMMax();
    if (max == DcmVariableVM) return std::to_string(min) + "-n";
    if (min == max) return std::to_string(min);
    return std::to_string(min) + "-" + std::to_string(max);
}

std::string FormatDictionaryTag(const DcmDictEntry& entry) {
    std::ostringstream out;
    out << '(' << std::uppercase << std::hex << std::setfill('0') << std::setw(4) << entry.getGroup();
    if (entry.isRepeatingGroup()) {
        out << '-' << std::setw(4) << entry.getUpperGroup();
    }
    out << ',' << std::setw(4) << entry.getElement();
    if (entry.isRepeatingElement()) {
        out << '-' << std::setw(4) << entry.getUpperElement();
    }
    out << ')';
    return out.str();
}

} // namespace

namespace DicomUtils {

std::optional<DcmTagKey> ParseTag(const std::string& input, std::string& error, UiLanguage language) {
    std::string text = Trim(input);
    if (text.empty()) {
        error = language == UiLanguage::German ? "Bitte einen DICOM-Tag eingeben oder auswählen." : "Please enter or select a DICOM tag.";
        return std::nullopt;
    }

    if (text.front() == '(' && text.back() == ')') {
        text = text.substr(1, text.size() - 2);
    }

    text.erase(std::remove_if(text.begin(), text.end(), [](unsigned char c) {
        return std::isspace(c) != 0;
    }), text.end());

    if (text.rfind("0x", 0) == 0 || text.rfind("0X", 0) == 0) {
        text.erase(0, 2);
    }

    if (IsHex8(text)) {
        try {
            const auto group = static_cast<Uint16>(std::stoul(text.substr(0, 4), nullptr, 16));
            const auto element = static_cast<Uint16>(std::stoul(text.substr(4, 4), nullptr, 16));
            return DcmTagKey(group, element);
        } catch (...) {
            error = language == UiLanguage::German ? "Der DICOM-Tag konnte nicht gelesen werden." : "The DICOM tag could not be parsed.";
            return std::nullopt;
        }
    }

    std::replace(text.begin(), text.end(), ';', ',');
    std::replace(text.begin(), text.end(), ':', ',');

    DcmTag tag;
    if (DcmTag::findTagFromName(text.c_str(), tag).good()) {
        return tag.getTagKey();
    }

    error = language == UiLanguage::German ? "Unbekannter DICOM-Tag. Beispiele: (0018,1030), 00181030 oder ProtocolName." : "Unknown DICOM tag. Examples: (0018,1030), 00181030 or ProtocolName.";
    return std::nullopt;
}

std::string FormatTag(const DcmTagKey& key) {
    std::ostringstream out;
    out << '(' << std::uppercase << std::hex << std::setfill('0')
        << std::setw(4) << key.getGroup() << ',' << std::setw(4) << key.getElement() << ')';
    return out.str();
}

std::string TagName(const DcmTagKey& key) {
    DcmTag tag(key);
    const char* name = tag.getTagName();
    return name ? std::string(name) : std::string{};
}

std::string GetString(DcmItem* item, const DcmTagKey& key) {
    if (!item) return {};
    OFString value;
    if (item->findAndGetOFStringArray(key, value, OFFalse).good()) {
        return value.c_str();
    }
    return {};
}

std::string SafeElementValue(DcmObject* object, UiLanguage language) {
    if (!object) return {};

    const DcmTagKey key = object->getTag().getTagKey();
    if (key == DCM_PixelData) {
        std::ostringstream out;
        out << (language == UiLanguage::German ? "<PixelData ausgelassen: " : "<Pixel Data omitted: ") << object->getLength() << " Bytes>";
        return out.str();
    }

    if (auto* sequence = dynamic_cast<DcmSequenceOfItems*>(object)) {
        std::ostringstream out;
        out << (language == UiLanguage::German ? "<Sequenz: " : "<Sequence: ") << sequence->card() << " Item(s)>";
        return out.str();
    }

    auto* element = dynamic_cast<DcmElement*>(object);
    if (!element) return {};

    constexpr Uint32 kMaxDisplayValue = 1024 * 1024;
    if (element->getLength() > kMaxDisplayValue) {
        std::ostringstream out;
        out << (language == UiLanguage::German ? "<Wert ausgelassen: " : "<Value omitted: ") << element->getLength() << " Bytes>";
        return out.str();
    }

    OFString value;
    if (element->getOFStringArray(value).good()) {
        std::string text = value.c_str();
        if (text.size() > 4000) {
            text.resize(4000);
            text += " ...";
        }
        return text;
    }

    if (element->getLength() == 0) return {};

    std::ostringstream out;
    out << "<" << element->getLength() << " Bytes>";
    return out.str();
}

std::string SequencePath(const DcmStack& stack, const std::string& prefix) {
    std::vector<std::string> parts;
    if (!prefix.empty()) parts.push_back(prefix);

    for (unsigned long i = 0; i < stack.card(); ++i) {
        DcmObject* obj = stack.elem(i);
        if (!obj || obj == stack.top()) continue;
        if (obj->ident() == EVR_SQ) {
            const DcmTagKey key = obj->getTag().getTagKey();
            parts.push_back(FormatTag(key) + " " + TagName(key));
        }
    }

    std::ostringstream out;
    for (std::size_t i = 0; i < parts.size(); ++i) {
        if (i) out << " > ";
        out << parts[i];
    }
    return out.str();
}

std::vector<TagEntry> LoadDictionary() {
    std::vector<TagEntry> tags;
    const DcmDataDictionary& lockedDict = dcmDataDict.rdlock();
    // DCMTK 3.7.0 exposes the dictionary iterators as non-const methods even
    // though iteration itself does not modify the dictionary. The read lock
    // remains held for the whole enumeration.
    DcmDataDictionary& dict = const_cast<DcmDataDictionary&>(lockedDict);

    for (auto it = dict.normalBegin(); it != dict.normalEnd(); ++it) {
        const DcmDictEntry* entry = *it;
        if (!entry) continue;
        TagEntry tag;
        tag.group = entry->getGroup();
        tag.element = entry->getElement();
        tag.upperGroup = entry->getUpperGroup();
        tag.upperElement = entry->getUpperElement();
        tag.repeating = false;
        tag.tagDisplay = FormatDictionaryTag(*entry);
        tag.name = entry->getTagName() ? entry->getTagName() : "";
        tag.vr = entry->getVR().getVRName();
        tag.vm = FormatVm(*entry);
        tag.version = entry->getStandardVersion() ? entry->getStandardVersion() : "";
        tags.push_back(std::move(tag));
    }

    for (auto it = dict.repeatingBegin(); it != dict.repeatingEnd(); ++it) {
        const DcmDictEntry* entry = *it;
        if (!entry) continue;
        TagEntry tag;
        tag.group = entry->getGroup();
        tag.element = entry->getElement();
        tag.upperGroup = entry->getUpperGroup();
        tag.upperElement = entry->getUpperElement();
        tag.repeating = true;
        tag.tagDisplay = FormatDictionaryTag(*entry);
        tag.name = entry->getTagName() ? entry->getTagName() : "";
        tag.vr = entry->getVR().getVRName();
        tag.vm = FormatVm(*entry);
        tag.version = entry->getStandardVersion() ? entry->getStandardVersion() : "";
        tags.push_back(std::move(tag));
    }

    dcmDataDict.rdunlock();

    std::sort(tags.begin(), tags.end(), [](const TagEntry& a, const TagEntry& b) {
        if (a.group != b.group) return a.group < b.group;
        if (a.element != b.element) return a.element < b.element;
        return a.name < b.name;
    });
    return tags;
}

std::string ToUtf8Path(const std::filesystem::path& path) {
#ifdef _WIN32
    const auto u8 = path.u8string();
    return std::string(reinterpret_cast<const char*>(u8.data()), u8.size());
#else
    return path.string();
#endif
}

OFCondition LoadDicomFile(DcmFileFormat& file, const std::filesystem::path& path, Uint32 maxReadLength) {
#ifdef _WIN32
    OFFilename filename(path.wstring().c_str());
#else
    OFFilename filename(path.string().c_str());
#endif
    return file.loadFile(filename, EXS_Unknown, EGL_noChange, maxReadLength, ERM_autoDetect);
}

} // namespace DicomUtils
