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
#include <rime/common.h>
#include <rime/composition.h>
#include <rime/context.h>
#include <rime/engine.h>
#include <rime/key_event.h>
#include <rime/menu.h>
#include <rime/processor.h>
#include <rime/schema.h>
#include <rime/segmentation.h>
#include <rime/segmentor.h>
#include <rime/translation.h>
#include <rime/translator.h>

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
  translators_.clear();
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
  const std::string &input(ctx->input());
  EZLOGGERVAR(input);
  CalculateSegmentation(ctx);
  TranslateSegments(ctx);
}

void Engine::CalculateSegmentation(Context *ctx) {
  EZLOGGERFUNCTRACKER;
  Segmentation *segmentation = new Segmentation(ctx->input());
  while (!segmentation->HasFinished()) {
    int start_pos = segmentation->GetCurrentPosition();
    // recognize a segment by calling the segmentors in turn
    BOOST_FOREACH(shared_ptr<Segmentor> &s, segmentors_) {
      if (!s->Proceed(segmentation))
        break;
    }
    // move onto the next segment
    segmentation->Forward();
    EZLOGGERVAR(*segmentation);
    // no advancement
    if (start_pos == segmentation->GetCurrentPosition())
      break;
  }
  ctx->set_segmentation(segmentation);
}

void Engine::TranslateSegments(Context *ctx) {
  EZLOGGERFUNCTRACKER;
  if (!ctx || !ctx->segmentation())
    return;
  // TODO: keep user confirmed segments
  ctx->composition()->clear();
  BOOST_FOREACH(const Segment &segment, ctx->segmentation()->segments()) {
    int len = segment.end - segment.start;
    const std::string input = ctx->input().substr(segment.start, len);
    EZLOGGERPRINT("Translating segment '%s'", input.c_str());
    shared_ptr<Menu> menu(new Menu);
    BOOST_FOREACH(shared_ptr<Translator> translator, translators_) {
      shared_ptr<Translation> translation(translator->Query(input, segment));
      if (!translation)
        continue;
      if (translation->exhausted()) {
        EZLOGGERPRINT("Oops, got a futile translation.");
        continue;
      }
      menu->AddTranslation(translation);
    }
    Selection selection;
    selection.manner = Selection::kGuess;
    selection.index = 0;
    selection.menu = menu;
    ctx->composition()->push_back(selection);
  }
}

void Engine::OnCommit(Context *ctx) {
  const std::string commit_text = ctx->GetCommitText();
  EZLOGGERVAR(commit_text);
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
  // create translators
  shared_ptr<ConfigList> translator_list(
      config->GetList("engine/translators"));
  if (translator_list) {
    size_t n = translator_list->size();
    for (size_t i = 0; i < n; ++i) {
      std::string klass;
      if (!translator_list->GetAt(i)->GetString(&klass))
        continue;
      Translator::Component *c = Translator::Require(klass);
      if (!c) {
        EZLOGGERPRINT("error creating translator: '%s'", klass.c_str());
      }
      else {
        shared_ptr<Translator> d(c->Create(this));
        translators_.push_back(d);
      }
    }
  }
}

}  // namespace rime
