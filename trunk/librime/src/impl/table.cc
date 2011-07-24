// vim: set sts=2 sw=2 et:
// encoding: utf-8
//
// Copyleft 2011 RIME Developers
// License: GPLv3
// 
// 2011-07-02 GONG Chen <chen.sst@gmail.com>
//
#include <cstring>
#include <boost/foreach.hpp>
#include <rime/impl/table.h>


namespace rime {

const char kTableFormat[] = "Rime::Table/0.9";

bool Table::Load() {
  EZLOGGERPRINT("Load file: %s", file_name().c_str());

  if (IsOpen())
    Close();
  
  if (!OpenReadOnly()) {
    EZLOGGERPRINT("Error opening table file '%s'.",
                  file_name().c_str());
    return false;
  }

  metadata_ = Find<table::Metadata>(0);
  if (!metadata_) {
    EZLOGGERPRINT("Metadata not found.");
    return false;
  }
  syllabary_ = metadata_->syllabary.get();
  if (!syllabary_) {
    EZLOGGERPRINT("Syllabary not found.");
    return false;
  }
  index_ = metadata_->index.get();
  if (!index_) {
    EZLOGGERPRINT("Table index not found.");
    return false;
  }
  return true;
}

bool Table::Save() {
  EZLOGGERPRINT("Save file: %s", file_name().c_str());

  if (!index_) {
    EZLOGGERPRINT("Error: the table has not been constructed!");
    return false;
  }

  return ShrinkToFit();
}

bool Table::Build(const Syllabary &syllabary, const Vocabulary &vocabulary, size_t num_entries) {
  size_t num_syllables = syllabary.size();
  size_t estimated_file_size = 32 * num_syllables + 128 * num_entries;
  EZLOGGERVAR(num_syllables);
  EZLOGGERVAR(num_entries);
  EZLOGGERVAR(estimated_file_size);
  if (!Create(estimated_file_size)) {
    EZLOGGERPRINT("Error creating table file '%s'.", file_name().c_str());
    return false;
  }

  EZLOGGERPRINT("Creating metadata.");
  metadata_ = Allocate<table::Metadata>();
  if (!metadata_) {
    EZLOGGERPRINT("Error creating metadata in file '%s'.", file_name().c_str());
    return false;
  }
  std::strncpy(metadata_->format, kTableFormat, table::Metadata::kFormatMaxLength);
  metadata_->num_syllables = num_syllables;
  metadata_->num_entries = num_entries;

  EZLOGGERPRINT("Creating syllabary.");
  syllabary_ = CreateArray<String>(num_syllables);
  if (!syllabary_) {
    EZLOGGERPRINT("Error creating syllabary.");
    return false;
  }
  else {
    size_t i = 0;
    BOOST_FOREACH(const std::string &syllable, syllabary) {
      CopyString(syllable, &syllabary_->at[i++]);
    }
  }
  metadata_->syllabary = syllabary_;

  EZLOGGERPRINT("Creating table index.");
  index_ = BuildIndex(vocabulary, num_syllables);
  if (!index_) {
    EZLOGGERPRINT("Error creating table index.");
    return false;
  }
  metadata_->index = index_;
  
  return true;
}

table::Index* Table::BuildIndex(const Vocabulary &vocabulary, size_t num_syllables) {
  table::Index *index = CreateArray<table::IndexNode>(num_syllables);
  if (!index) {
    return NULL;
  }
  BOOST_FOREACH(const Vocabulary::value_type &v, vocabulary) {
    int syllable_id = v.first;
    EZLOGGERVAR(syllable_id);
    table::IndexNode &lv1_node(index->at[syllable_id]);
    const DictEntryList &entries(v.second.entries);
    if (!BuildEntries(entries, &lv1_node.entries)) {
        return NULL;
    }
    if (v.second.next_level) {
      Code code;
      code.push_back(syllable_id);
      table::IndexLv2 *index_lv2 = BuildIndexLv2(code, *v.second.next_level);
      if (!index_lv2) {
        return NULL;
      }
      lv1_node.next_level = index_lv2;
    }
  }
  return index;
}

table::IndexLv2* Table::BuildIndexLv2(const Code &prefix, const Vocabulary &vocabulary) {
  table::IndexLv2 *index = reinterpret_cast<table::IndexLv2*>(
      CreateArray<table::IndexNodeLv2>(vocabulary.size()));
  if (!index) {
    return NULL;
  }
  size_t count = 0;
  BOOST_FOREACH(const Vocabulary::value_type &v, vocabulary) {
    int syllable_id = v.first;
    EZLOGGERVAR(syllable_id);
    table::IndexNodeLv2 &lv2_node(index->at[count++]);
    lv2_node.key = syllable_id;
    const DictEntryList &entries(v.second.entries);
    if (!BuildEntries(entries, &lv2_node.entries)) {
        return NULL;
    }
    if (v.second.next_level) {
      // TODO: build lv3 index...
    }
  }
  return index;
}

bool Table::BuildEntries(const DictEntryList &src, List<table::Entry> *dest) {
  if (!dest)
    return false;
  dest->size = src.size();
  dest->at = Allocate<table::Entry>(src.size());
  if (!dest->at) {
    EZLOGGERPRINT("Error creating table entries; file size: %u.", file_size());
    return false;
  }
  size_t i = 0;
  for (std::vector<DictEntry>::const_iterator d = src.begin(); d != src.end(); ++d, ++i) {
    table::Entry &e(dest->at[i]);
    if (!CopyString(d->text, &e.text)) {
      EZLOGGERPRINT("Error creating table entry '%s'; file size: %u.",
                    d->text.c_str(), file_size());
      return false;
    }
    e.weight = static_cast<float>(d->weight);
  }
  return true;
}

const char* Table::GetSyllableById(int syllable_id) {
  if (!syllabary_ ||
      syllable_id < 0 ||
      syllable_id >= syllabary_->size)
    return NULL;
  return syllabary_->at[syllable_id].c_str();
}

const Table::Cluster Table::GetEntries(int syllable_id) {
  if (!index_ || syllable_id < 0 || syllable_id >= index_->size)
    return Table::Cluster();
  List<table::Entry> &entries(index_->at[syllable_id].entries);
  return std::make_pair(entries.at.get(), entries.size);
}

}  // namespace rime
