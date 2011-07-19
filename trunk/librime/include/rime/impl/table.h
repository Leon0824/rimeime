// vim: set sts=2 sw=2 et:
// encoding: utf-8
//
// Copyleft 2011 RIME Developers
// License: GPLv3
// 
// 2011-07-01 GONG Chen <chen.sst@gmail.com>
//

#ifndef RIME_TABLE_H_
#define RIME_TABLE_H_

#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>
#include <boost/interprocess/containers/vector.hpp>
#include <rime/common.h>
#include <rime/impl/mapped_file.h>
#include <rime/impl/vocabulary.h>

namespace rime {

namespace table {

typedef Array<String> Syllabary;

typedef int32_t SyllableId;

struct Entry {
  String text;
  float weight;
};

struct IndexLv2;
struct IndexLv3;

struct IndexNode {
  List<Entry> entries;
  OffsetPtr<IndexLv2> next_level;
};

typedef Array<IndexNode> Index;

struct IndexNodeLv2 {
  SyllableId key;
  List<Entry> entries;
  OffsetPtr<IndexLv3> next_level;
};

struct IndexLv2 : Array<IndexNodeLv2> {};

typedef Array<SyllableId> Code;

struct CodeMapping {
  OffsetPtr<Entry> entry;
  Code code;
};

struct IndexNodeLv3 {
  SyllableId key;
  List<Entry> entries;
  List<CodeMapping> code_map;
};

struct IndexLv3 : Array<IndexNodeLv3> {};

struct Metadata {
  static const int kFormatMaxLength = 32;
  char format[kFormatMaxLength];
  int num_syllables;
  int num_entries;
  OffsetPtr<Syllabary> syllabary;
  OffsetPtr<Index> index;
};

}  // namespace table

class Table : public MappedFile {
 public:
  typedef std::pair<table::Entry*, size_t> Cluster;
  
  Table(const std::string &file_name)
      : MappedFile(file_name), index_(NULL), syllabary_(NULL), metadata_(NULL) {}
  
  bool Load();
  bool Save();
  bool Build(const Syllabary &syllabary, const Vocabulary &vocabulary, size_t num_entries);
  const char* GetSyllableById(int syllable_id);
  const Cluster GetEntries(int syllable_id);
  
 private:
  table::Metadata *metadata_;
  table::Syllabary *syllabary_;
  table::Index *index_;
};

}  // namespace rime

#endif  // RIME_TABLE_H_
