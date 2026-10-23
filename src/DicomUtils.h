#pragma once

#include "Models.h"

#include <dcmtk/dcmdata/dctk.h>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace DicomUtils {

std::optional<DcmTagKey> ParseTag(const std::string& input, std::string& error);
std::string FormatTag(const DcmTagKey& key);
std::string TagName(const DcmTagKey& key);
std::string GetString(DcmItem* item, const DcmTagKey& key);
std::string SafeElementValue(DcmObject* object);
std::string SequencePath(const DcmStack& stack, const std::string& prefix = {});
std::vector<TagEntry> LoadDictionary();
std::string ToUtf8Path(const std::filesystem::path& path);
OFCondition LoadDicomFile(DcmFileFormat& file, const std::filesystem::path& path, Uint32 maxReadLength = 4096);

}
