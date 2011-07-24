// vim: set sts=2 sw=2 et:
// encoding: utf-8
//
// Copyleft 2011 RIME Developers
// License: GPLv3
// 
// 2011-07-05 GONG Chen <chen.sst@gmail.com>
//
#include <list>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/foreach.hpp>
#include <boost/filesystem.hpp>
#include <yaml-cpp/yaml.h>
#include <rime/common.h>
#include <rime/impl/dictionary.h>

namespace {

struct RawDictEntry {
  rime::RawCode raw_code;
  std::string text;
  double weight;
};

typedef std::list<rime::shared_ptr<RawDictEntry> > RawDictEntryList;

struct Chunk {
  rime::Code code;
  const rime::table::Entry *entries;
  size_t size;
  size_t cursor;
  size_t consumed_input_length;

  Chunk() : entries(NULL), size(0), cursor(0), consumed_input_length(0) {}
  Chunk(const rime::Code &c, const rime::Table::Cluster &s, size_t len)
      : code(c), entries(s.first), size(s.second),
        cursor(0), consumed_input_length(len) {}
};
    
}  // namespace

namespace rime {

class DictEntryCollector : public std::list<Chunk> {
 public:
  DictEntryCollector() {}
  ~DictEntryCollector() {}
};


const std::string RawCode::ToString() const {
  return boost::join(*this, " ");
}

void RawCode::FromString(const std::string &code) {
  boost::split(*dynamic_cast<std::vector<std::string> *>(this),
               code,
               boost::algorithm::is_space(),
               boost::algorithm::token_compress_on);
}

bool DictEntry::operator< (const DictEntry& other) const {
  // Sort different entries sharing the same code by weight desc.
  if (weight != other.weight)
    return weight > other.weight;
  return text < other.text;
}

DictEntryIterator::DictEntryIterator()
    : collector_(new DictEntryCollector), entry_() {
}

DictEntryIterator::DictEntryIterator(const DictEntryIterator &other)
    : collector_(other.collector_), entry_(other.entry_) {
}

bool DictEntryIterator::exhausted() const {
  return collector_->empty();
}

shared_ptr<DictEntry> DictEntryIterator::Peek() {
  if (collector_->empty()) {
    EZLOGGERPRINT("Oops!");
    return shared_ptr<DictEntry>();
  }
  if (!entry_) {
    const Chunk &chunk(collector_->front());
    entry_.reset(new DictEntry);
    const table::Entry &e(chunk.entries[chunk.cursor]);
    EZLOGGERPRINT("Creating temporary dict entry '%s'.", e.text.c_str());
    entry_->code = chunk.code;
    entry_->text = e.text.c_str();
    entry_->weight = e.weight;
    entry_->consumed_input_length = chunk.consumed_input_length;
  }
  return entry_;
}

bool DictEntryIterator::Next() {
  if (collector_->empty()) {
    return false;
  }
  Chunk &chunk(collector_->front());
  if (++chunk.cursor >= chunk.size) {
    collector_->pop_front();
  }
  entry_.reset();
  return true;
}

void DictEntryIterator::AddChunk(const Code &code,
                                 const Table::Cluster &cluster,
                                 size_t consumed_input_length) {
  EZLOGGERPRINT("Add chunk: %d entries, len = %d.",
                cluster.second, consumed_input_length);
  collector_->push_back(Chunk(code, cluster, consumed_input_length));
}

Dictionary::Dictionary(const std::string &name)
    : name_(name), loaded_(false) {
  prism_.reset(new Prism(name_ + ".prism.bin"));
  table_.reset(new Table(name_ + ".table.bin"));
}

Dictionary::~Dictionary() {
  if (loaded_) {
    Unload();
  }
}

DictEntryIterator Dictionary::Lookup(const std::string &str_code) {
  EZLOGGERVAR(str_code);
  DictEntryIterator result;
  if (!loaded_)
    return result;
  std::vector<Prism::Match> keys;
  prism_->CommonPrefixSearch(str_code, &keys);
  EZLOGGERPRINT("found %u matching keys thru the prism.", keys.size());
  Code code;
  code.resize(1);
  BOOST_REVERSE_FOREACH(Prism::Match &match, keys) {
    int syllable_id = match.value;
    code[0] = syllable_id;
    const Table::Cluster cluster(table_->GetEntries(syllable_id));
    if (cluster.first) {
      result.AddChunk(code, cluster, match.length);
    }
  }
  return result;
}

DictEntryIterator Dictionary::PredictiveLookup(const std::string &str_code) {
  EZLOGGERVAR(str_code);
  DictEntryIterator result;
  if (!loaded_)
    return result;
  const size_t kExpandSearchLimit = 0;  // unlimited!
  std::vector<Prism::Match> keys;
  prism_->ExpandSearch(str_code, &keys, kExpandSearchLimit);
  EZLOGGERPRINT("found %u matching keys thru the prism.", keys.size());
  Code code;
  code.resize(1);
  BOOST_FOREACH(Prism::Match &match, keys) {
    int syllable_id = match.value;
    code[0] = syllable_id;
    const Table::Cluster cluster(table_->GetEntries(syllable_id));
    if (cluster.first) {
      result.AddChunk(code, cluster, str_code.length());
    }
  }
  return result;
}

bool Dictionary::Compile(const std::string &source_file) {
  EZLOGGERFUNCTRACKER;
  YAML::Node doc;
  {
    std::ifstream fin(source_file.c_str());
    YAML::Parser parser(fin);
    parser.GetNextDocument(doc);
  }
  if (doc.Type() != YAML::NodeType::Map) {
    return false;
  }
  std::string dict_name;
  std::string dict_version;
  {
    const YAML::Node *name_node = doc.FindValue("name");
    const YAML::Node *version_node = doc.FindValue("version");
    if (!name_node || !version_node) {
      return false;
    }
    *name_node >> dict_name;
    *version_node >> dict_version;
  }
  EZLOGGERVAR(dict_name);
  EZLOGGERVAR(dict_version);
  // read entries
  const YAML::Node *entries = doc.FindValue("entries");
  if (!entries ||
      entries->Type() != YAML::NodeType::Sequence) {
    return false;
  }
  
  RawDictEntryList raw_entries;
  int entry_count = 0;
  Syllabary syllabary;
  for (YAML::Iterator it = entries->begin(); it != entries->end(); ++it) {
    if (it->Type() != YAML::NodeType::Sequence) {
      EZLOGGERPRINT("Invalid entry %d.", entry_count);
      continue;
    }
    // read a dict entry
    std::string word;
    std::string str_code;
    double weight = 1.0;
    if (it->size() < 2) {
      EZLOGGERPRINT("Invalid entry %d.", entry_count);
      continue;
    }
    (*it)[0] >> word;
    (*it)[1] >> str_code;
    if (it->size() > 2) {
      (*it)[2] >> weight;
    }
    shared_ptr<RawDictEntry> e(new RawDictEntry);
    e->raw_code.FromString(str_code);
    BOOST_FOREACH(const std::string &s, e->raw_code) {
      if (syllabary.find(s) == syllabary.end())
        syllabary.insert(s);
    }
    e->text.swap(word);
    e->weight = weight;
    raw_entries.push_back(e);
    ++entry_count;
  }
  EZLOGGERVAR(entry_count);
  EZLOGGERVAR(syllabary.size());
  // build prism
  {
    prism_->Remove();
    if (!prism_->Build(syllabary) ||
        !prism_->Save()) {
      return false;
    }
  }
  // build table
  {
    std::map<std::string, int> syllable_to_id;
    int syllable_id = 0;
    BOOST_FOREACH(const std::string &s, syllabary) {
      syllable_to_id[s] = syllable_id++;
    }
    Vocabulary vocabulary;
    BOOST_FOREACH(const shared_ptr<RawDictEntry> &e, raw_entries) {
      Code code;
      BOOST_FOREACH(const std::string &s, e->raw_code) {
        code.push_back(syllable_to_id[s]);
      }
      Code index_code;
      code.CreateIndex(&index_code);
      DictEntryList *ls = vocabulary.LocateEntries(index_code);
      if (!ls) {
        EZLOGGERPRINT("Error locating entries in vocabulary.");
        continue;
      }
      ls->resize(ls->size() + 1);
      DictEntry &d(ls->back());
      d.code.swap(code);
      d.text.swap(e->text);
      d.weight = e->weight;
    }
    vocabulary.SortHomophones();
    table_->Remove();
    if (!table_->Build(syllabary, vocabulary, entry_count) ||
        !table_->Save()) {
      return false;
    }  
  }
  return true;
}

bool Dictionary::Decode(const Code &code, RawCode *result) {
  if (!result || !table_)
    return false;
  result->clear();
  BOOST_FOREACH(int c, code) {
    const char *s = table_->GetSyllableById(c);
    if (!s)
      return false;
    result->push_back(s);
  }
  return true;
}

bool Dictionary::Exists() const {
  return boost::filesystem::exists(prism_->file_name()) &&
         boost::filesystem::exists(table_->file_name());
}

bool Dictionary::Load() {
  EZLOGGERFUNCTRACKER;
  if (!prism_->Load() || !table_->Load()) {
    EZLOGGERPRINT("Error loading dictionary '%s'.", name_.c_str());
    prism_->Close();
    table_->Close();
    loaded_ = false;
  }
  else {
    loaded_ = true;
  }
  return loaded_;
}

bool Dictionary::Unload() {
  prism_->Close();
  table_->Close();
  loaded_ = false;
  return true;
}

}  // namespace rime
