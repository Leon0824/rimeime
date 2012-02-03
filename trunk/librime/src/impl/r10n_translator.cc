// vim: set sts=2 sw=2 et:
// encoding: utf-8
//
// Copyleft 2011 RIME Developers
// License: GPLv3
//
// Romanization translator
//
// 2011-07-10 GONG Chen <chen.sst@gmail.com>
// 2011-10-06 GONG Chen <chen.sst@gmail.com>  implemented simplistic sentence-making
//
#include <algorithm>
#include <boost/bind.hpp>
#include <boost/foreach.hpp>
#include <rime/composition.h>
#include <rime/candidate.h>
#include <rime/config.h>
#include <rime/context.h>
#include <rime/engine.h>
#include <rime/schema.h>
#include <rime/segmentation.h>
#include <rime/translation.h>
#include <rime/dict/dictionary.h>
#include <rime/algo/syllabifier.h>
#include <rime/dict/user_dictionary.h>
#include <rime/impl/r10n_translator.h>

namespace rime {

namespace {

struct DelimitSyllableState {
  const std::string *input;
  const std::string *delimiters;
  const SyllableGraph *graph;
  const Code *code;
  size_t end_pos;
  std::string output;
};

bool DelimitSyllablesDfs(DelimitSyllableState *state, size_t current_pos, size_t depth) {
  if (depth == state->code->size()) {
    return current_pos == state->end_pos;
  }
  SyllableId syllable_id = state->code->at(depth);
  EdgeMap::const_iterator z = state->graph->edges.find(current_pos);
  if (z == state->graph->edges.end())
    return false;
  BOOST_REVERSE_FOREACH(const EndVertexMap::value_type &y, z->second) {  // favor longer spelling
    size_t end_vertex_pos = y.first;
    if (end_vertex_pos > state->end_pos)
      continue;
    SpellingMap::const_iterator x = y.second.find(syllable_id);
    if (x != y.second.end()) {
      size_t len = state->output.length();
      if (depth > 0 && len > 0 &&
          state->delimiters->find(state->output[len - 1]) == std::string::npos) {
        state->output += state->delimiters->at(0);
      }
      state->output += state->input->substr(current_pos, end_vertex_pos - current_pos);
      if (DelimitSyllablesDfs(state, end_vertex_pos, depth + 1))
        return true;
      state->output.resize(len);
    }
  }
  return false;
}

}  // anonymous namespace

class R10nCandidate;

class R10nTranslation : public Translation {
 public:
  R10nTranslation(const std::string &input, size_t start,
                  R10nTranslator *translator)
      : input_(input), start_(start),
        translator_(translator),
        user_phrase_index_(0) {
    set_exhausted(true);
  }
  bool Evaluate(Dictionary *dict, UserDictionary *user_dict);
  virtual bool Next();
  virtual shared_ptr<Candidate> Peek();

 protected:
  void CheckEmpty();
  const std::string GetPreeditString(const R10nCandidate &cand) const;
  shared_ptr<DictEntry> SimplisticSentenceMaking(Dictionary *dict,
                                                 UserDictionary *user_dict);

  const std::string input_;
  size_t start_;
  R10nTranslator *translator_;
  
  SyllableGraph syllable_graph_;
  shared_ptr<DictEntryCollector> phrase_;
  shared_ptr<UserDictEntryCollector> user_phrase_;
  
  DictEntryCollector::reverse_iterator phrase_iter_;
  UserDictEntryCollector::reverse_iterator user_phrase_iter_;
  size_t user_phrase_index_;
  std::set<std::string> candidate_set_;
};

class R10nCandidate : public Candidate {
 public:
  R10nCandidate(size_t start, size_t end,
                const shared_ptr<DictEntry> &entry)
      : Candidate("zh", start, end),
        entry_(entry) {
  }
  virtual const std::string& text() const {
    return entry_->text;
  }
  virtual const std::string comment() const {
    return entry_->comment;
  }
  virtual const std::string preedit() const {
    return entry_->preedit;
  }
  void set_preedit(const std::string &preedit) {
    entry_->preedit = preedit;
  }
  const Code& code() const {
    return entry_->code;
  }
 protected:
  const shared_ptr<DictEntry> entry_;
};

// R10nTranslator implementation

R10nTranslator::R10nTranslator(Engine *engine)
    : Translator(engine),
      enable_completion_(true) {
  if (!engine) return;

  Config *config = engine->schema()->config();
  if (config) {
    config->GetString("speller/delimiter", &delimiters_);
    config->GetBool("translator/enable_completion", &enable_completion_);
    formatter_.Load(config->GetList("translator/preedit_format"));
  }
  if (delimiters_.empty()) {
    delimiters_ = " ";
  }
  
  Dictionary::Component *dictionary = Dictionary::Require("dictionary");
  if (dictionary) {
    dict_.reset(dictionary->Create(engine->schema()));
    if (dict_)
      dict_->Load();
  }

  UserDictionary::Component *user_dictionary = UserDictionary::Require("user_dictionary");
  if (user_dictionary) {
    user_dict_.reset(user_dictionary->Create(engine->schema()));
    if (user_dict_) {
      user_dict_->Load();
      if (dict_)
        user_dict_->Attach(dict_->table(), dict_->prism());
    }
  }

  connection_ = engine->context()->commit_notifier().connect(
      boost::bind(&R10nTranslator::OnCommit, this, _1));
}

R10nTranslator::~R10nTranslator() {
  connection_.disconnect();
}

Translation* R10nTranslator::Query(const std::string &input, const Segment &segment) {
  if (!dict_ || !dict_->loaded())
    return NULL;
  if (!segment.HasTag("abc"))
    return NULL;
  EZDBGONLYLOGGERPRINT("input = '%s', [%d, %d)",
                       input.c_str(), segment.start, segment.end);

  // the translator should survive translations it creates
  R10nTranslation* result(new R10nTranslation(input, segment.start, this));
  if (result && result->Evaluate(dict_.get(), user_dict_.get()))
    return result;
  else
    return NULL;
}

const std::string R10nTranslator::FormatPreedit(const std::string& preedit) {
  std::string result(preedit);
  formatter_.Apply(&result);
  return result;
}

void R10nTranslator::OnCommit(Context *ctx) {
  DictEntry commit_entry;
  BOOST_FOREACH(Composition::value_type &seg, *ctx->composition()) {
    shared_ptr<Candidate> cand = seg.GetSelectedCandidate();
    shared_ptr<UniquifiedCandidate> uniquified = As<UniquifiedCandidate>(cand);
    if (uniquified) cand = uniquified->items().front();
    shared_ptr<ShadowCandidate> shadow = As<ShadowCandidate>(cand);
    if (shadow) cand = shadow->item();
    shared_ptr<R10nCandidate> r10n_cand = As<R10nCandidate>(cand);
    if (r10n_cand) {
      commit_entry.text += r10n_cand->text();
      commit_entry.code.insert(commit_entry.code.end(),
                               r10n_cand->code().begin(), r10n_cand->code().end());
    }
    if ((!r10n_cand || seg.status >= Segment::kConfirmed) &&
        !commit_entry.text.empty()) {
      EZDBGONLYLOGGERVAR(commit_entry.text);
      user_dict_->UpdateEntry(commit_entry, 1);
      commit_entry.text.clear();
      commit_entry.code.clear();
    }
  }
}

// R10nTranslation implementation

bool R10nTranslation::Evaluate(Dictionary *dict, UserDictionary *user_dict) {
  Syllabifier syllabifier(translator_->delimiters(),
                          translator_->enable_completion());
  size_t consumed = syllabifier.BuildSyllableGraph(input_,
                                                   *dict->prism(),
                                                   &syllable_graph_);

  phrase_ = dict->Lookup(syllable_graph_, 0);
  user_phrase_ = user_dict->Lookup(syllable_graph_, 0);
  if (!phrase_ && !user_phrase_)
    return false;
  // make sentences when there is no exact-matching phrase candidate
  size_t translated_len = 0;
  if (phrase_ && !phrase_->empty())
    translated_len = (std::max)(translated_len, phrase_->rbegin()->first);
  if (user_phrase_ && !user_phrase_->empty())
    translated_len = (std::max)(translated_len, user_phrase_->rbegin()->first);
  if (translated_len < consumed &&
      syllable_graph_.edges.size() > 1) {  // at least 2 syllables required
    shared_ptr<DictEntry> sentence = SimplisticSentenceMaking(dict, user_dict);
    if (sentence) {
      if (!user_phrase_) user_phrase_.reset(new UserDictEntryCollector);
      (*user_phrase_)[consumed].push_back(sentence);
    }
  }

  if (phrase_) phrase_iter_ = phrase_->rbegin();
  if (user_phrase_) user_phrase_iter_ = user_phrase_->rbegin();
  CheckEmpty();
  return !exhausted();
}

const std::string R10nTranslation::GetPreeditString(const R10nCandidate &cand) const {
  DelimitSyllableState state;
  state.input = &input_;
  state.delimiters = &translator_->delimiters();
  state.graph = &syllable_graph_;
  state.code = &cand.code();
  state.end_pos = cand.end() - start_;
  bool success = DelimitSyllablesDfs(&state, cand.start() - start_, 0);
  if (success) {
    return translator_->FormatPreedit(state.output);
  }
  else {
    return std::string();
  }
}

bool R10nTranslation::Next() {
  if (exhausted())
    return false;
  do {
    int user_phrase_code_length = 0;
    if (user_phrase_ && user_phrase_iter_ != user_phrase_->rend()) {
      user_phrase_code_length = user_phrase_iter_->first;
    }
    int phrase_code_length = 0;
    if (phrase_ && phrase_iter_ != phrase_->rend()) {
      phrase_code_length = phrase_iter_->first;
    }
    if (user_phrase_code_length > 0 &&
        user_phrase_code_length >= phrase_code_length) {
      DictEntryList &entries(user_phrase_iter_->second);
      candidate_set_.insert(entries[user_phrase_index_]->text);
      if (++user_phrase_index_ >= entries.size()) {
        ++user_phrase_iter_;
        user_phrase_index_ = 0;
      }
    }
    else if (phrase_code_length > 0) {
      DictEntryIterator &iter(phrase_iter_->second);
      candidate_set_.insert(iter.Peek()->text);
      if (!iter.Next()) {
        ++phrase_iter_;
      }
    }
    CheckEmpty();
  }
  while (!exhausted() && /* skip duplicate candidates */
         candidate_set_.find(Peek()->text()) != candidate_set_.end());
  return exhausted();
}

shared_ptr<Candidate> R10nTranslation::Peek() {
  if (exhausted())
    return shared_ptr<Candidate>();
  size_t user_phrase_code_length = 0;
  if (user_phrase_ && user_phrase_iter_ != user_phrase_->rend()) {
    user_phrase_code_length = user_phrase_iter_->first;
  }
  size_t phrase_code_length = 0;
  if (phrase_ && phrase_iter_ != phrase_->rend()) {
    phrase_code_length = phrase_iter_->first;
  }
  shared_ptr<R10nCandidate> cand;
  if (user_phrase_code_length > 0 &&
      user_phrase_code_length >= phrase_code_length) {
    DictEntryList &entries(user_phrase_iter_->second);
    const shared_ptr<DictEntry> &e(entries[user_phrase_index_]);
    EZDBGONLYLOGGERVAR(user_phrase_code_length);
    EZDBGONLYLOGGERVAR(e->text);
    cand.reset(new R10nCandidate(start_,
                                 start_ + user_phrase_code_length,
                                 e));
  }
  else if (phrase_code_length > 0) {
    DictEntryIterator &iter(phrase_iter_->second);
    const shared_ptr<DictEntry> &e(iter.Peek());
    EZDBGONLYLOGGERVAR(phrase_code_length);
    EZDBGONLYLOGGERVAR(e->text);
    cand.reset(new R10nCandidate(start_,
                                 start_ + phrase_code_length,
                                 e));
  }
  if (cand && cand->preedit().empty()) {
    cand->set_preedit(GetPreeditString(*cand));
  }
  return cand;
}

void R10nTranslation::CheckEmpty() {
  set_exhausted((!phrase_ || phrase_iter_ == phrase_->rend()) &&
                (!user_phrase_ || user_phrase_iter_ == user_phrase_->rend()));
}

shared_ptr<DictEntry> R10nTranslation::SimplisticSentenceMaking(Dictionary *dict,
                                                                UserDictionary *user_dict) {
  typedef std::map<int, UserDictEntryCollector> WordGraph;
  const int kMaxSentenceMakingHomophones = 1;  // 20; if we have bigram model...
  const int kMaxSyllablesInSentenceMakingUserPhrases = 5;
  const double kEpsilon = 1e-30;
  const double kPenalty = 1e-8;
  size_t total_length = syllable_graph_.interpreted_length;
  WordGraph graph;
  BOOST_FOREACH(const EdgeMap::value_type &s, syllable_graph_.edges) {
    shared_ptr<UserDictEntryCollector> user_phrase =
        user_dict->Lookup(syllable_graph_, s.first, kMaxSyllablesInSentenceMakingUserPhrases);
    UserDictEntryCollector &u(graph[s.first]);
    if (user_phrase)
      u.swap(*user_phrase);
    shared_ptr<DictEntryCollector> phrase = dict->Lookup(syllable_graph_, s.first);
    if (phrase) {
      // merge lookup results
      BOOST_FOREACH(DictEntryCollector::value_type &t, *phrase) {
        DictEntryList &entries(u[t.first]);
        if (entries.empty()) {
          shared_ptr<DictEntry> e(t.second.Peek());
          entries.push_back(e);
        }
      }
    }
  }
  std::map<int, shared_ptr<DictEntry> > sentence;
  sentence[0].reset(new DictEntry);
  sentence[0]->weight = 1.0;
  // dynamic programming
  BOOST_FOREACH(WordGraph::value_type &w, graph) {
    size_t start_pos = w.first;
    EZDBGONLYLOGGERVAR(start_pos);
    if (sentence.find(start_pos) == sentence.end())
      continue;
    BOOST_FOREACH(UserDictEntryCollector::value_type &x, w.second) {
      size_t end_pos = x.first;
      if (start_pos == 0 && end_pos == total_length)  // exclude single words from the result
        continue;
      EZDBGONLYLOGGERVAR(end_pos);
      DictEntryList &entries(x.second);
      for (size_t i = 0; i < kMaxSentenceMakingHomophones && i < entries.size(); ++i) {
        const shared_ptr<DictEntry> &e(entries[i]);
        shared_ptr<DictEntry> new_sentence(new DictEntry(*sentence[start_pos]));
        new_sentence->code.insert(new_sentence->code.end(), e->code.begin(), e->code.end());
        new_sentence->text.append(e->text);
        new_sentence->weight *= (std::max)(e->weight, kEpsilon) * kPenalty;
        if (sentence.find(end_pos) == sentence.end() ||
            sentence[end_pos]->weight < new_sentence->weight) {
          EZDBGONLYLOGGERPRINT("updated sentence[%d] with '%s', %g",
                               end_pos, new_sentence->text.c_str(), new_sentence->weight);
          sentence[end_pos] = new_sentence;
        }
      }
    }
  }
  if (sentence.find(total_length) == sentence.end())
    return shared_ptr<DictEntry>();
  else
    return sentence[total_length];
}

}  // namespace rime
