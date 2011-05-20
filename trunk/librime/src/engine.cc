// vim: set sts=2 sw=2 et:
// encoding: utf-8
//
// Copyleft 2011 RIME Developers
// License: GPLv3
// 
// 2011-04-24 GONG Chen <chen.sst@gmail.com>
//
#include <cctype>
#include <string>
#include <boost/bind.hpp>
#include <boost/foreach.hpp>
#include <rime/context.h>
#include <rime/dictionary.h>
#include <rime/engine.h>
#include <rime/key_event.h>
#include <rime/processor.h>
#include <rime/schema.h>
#include <rime/segmentation.h>
#include <rime/segmentor.h>

namespace rime {

Engine::Engine() : schema_(new Schema), context_(new Context) {
  EZLOGGERFUNCTRACKER;
  // receive context notifications
  context_->input_change_notifier().connect(
      boost::bind(&Engine::OnInputChange, this, _1));     
  context_->commit_notifier().connect(
      boost::bind(&Engine::OnCommit, this, _1));
}

Engine::~Engine() {
  EZLOGGERFUNCTRACKER;
  processors_.clear();
  segmentors_.clear();
  dictionaries_.clear();
}

bool Engine::ProcessKeyEvent(const KeyEvent &key_event) {
  EZLOGGERVAR(key_event);
  BOOST_FOREACH(shared_ptr<Processor> &p, processors_) {
    Processor::Result ret = p->ProcessKeyEvent(key_event);
    if (ret == Processor::kRejected) return false;
    if (ret == Processor::kAccepted) return true;
  }
  return false;
}

void Engine::OnInputChange(Context *ctx) {
  Segmentation segmentation(ctx->input());
  int start_pos = 0;
  while (!segmentation.HasFinished()) {
    // recognize a segment by calling the segmentors in turn
    BOOST_FOREACH(shared_ptr<Segmentor> &s, segmentors_) {
      if (!s->Proceed(&segmentation))
        break;
    }
    // move on to the next segment
    if (!segmentation.Forward())
      break;
  }
}

void Engine::OnCommit(Context *ctx) {
  const std::string commit_text = ctx->GetCommitText();
  sink_(commit_text);
}

void Engine::set_schema(Schema *schema) {
  schema_.reset(schema);
  InitializeComponents();
}

void Engine::InitializeComponents() {
  if (!schema_)
    return;
  Config *config = schema_->config();
  // create processors
  shared_ptr<ConfigList> processor_list(
      config->GetList("engine/processors"));
  if (processor_list) {
    size_t n = processor_list->size();
    for (size_t i = 0; i < n; ++i) {
      std::string klass;
      if (!processor_list->GetAt(i)->GetString(&klass))
        continue;
      Processor::Component *c = Processor::Require(klass);
      if (!c) {
        EZLOGGERPRINT("error creating processor: '%s'", klass.c_str());
      }
      else {
        shared_ptr<Processor> p(c->Create(this));
        processors_.push_back(p);
      }
    }
  }
  // create segmentors
  shared_ptr<ConfigList> segmentor_list(
      config->GetList("engine/segmentors"));
  if (segmentor_list) {
    size_t n = segmentor_list->size();
    for (size_t i = 0; i < n; ++i) {
      std::string klass;
      if (!segmentor_list->GetAt(i)->GetString(&klass))
        continue;
      Segmentor::Component *c = Segmentor::Require(klass);
      if (!c) {
        EZLOGGERPRINT("error creating segmentor: '%s'", klass.c_str());
      }
      else {
        shared_ptr<Segmentor> s(c->Create(this));
        segmentors_.push_back(s);
      }
    }
  }
  // create dictionaries
  shared_ptr<ConfigList> dictionary_list(
      config->GetList("engine/dictionaries"));
  if (dictionary_list) {
    size_t n = dictionary_list->size();
    for (size_t i = 0; i < n; ++i) {
      std::string klass;
      if (!dictionary_list->GetAt(i)->GetString(&klass))
        continue;
      Dictionary::Component *c = Dictionary::Require(klass);
      if (!c) {
        EZLOGGERPRINT("error creating dictionary: '%s'", klass.c_str());
      }
      else {
        shared_ptr<Dictionary> d(c->Create(this));
        dictionaries_.push_back(d);
      }
    }
  }
}

}  // namespace rime
