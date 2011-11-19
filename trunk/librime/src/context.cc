// vim: set sts=2 sw=2 et:
// encoding: utf-8
//
// Copyleft 2011 RIME Developers
// License: GPLv3
//
// 2011-05-08 GONG Chen <chen.sst@gmail.com>
//
#include <rime/candidate.h>
#include <rime/context.h>
#include <rime/composition.h>
#include <rime/menu.h>
#include <rime/segmentation.h>

namespace rime {

Context::Context() : composition_(new Composition) {
}

Context::~Context() {
}

bool Context::Commit() {
  if (composition_->empty())
    return false;
  commit_notifier_(this);
  Clear();
  return true;
}

const std::string Context::GetCommitText() const {
  return composition_->GetCommitText();
}

bool Context::IsComposing() const {
  return !input_.empty();
}

bool Context::PushInput(char ch) {
  input_.push_back(ch);
  update_notifier_(this);
  return true;
}

bool Context::PopInput() {
  if (input_.empty())
    return false;
  input_.resize(input_.size() - 1);
  update_notifier_(this);
  return true;
}

void Context::Clear() {
  input_.clear();
  update_notifier_(this);
}

bool Context::Select(size_t index) {
  if (composition_->empty())
    return false;
  Segment &seg(composition_->back());
  shared_ptr<Candidate> cand(seg.GetCandidateAt(index));
  if (cand) {
    seg.selected_index = index;
    seg.status = Segment::kSelected;
    EZLOGGERPRINT("Selected: '%s', index = %d.",
                  cand->text().c_str(), index);
    select_notifier_(this);
    return true;
  }
  return false;
}

bool Context::ConfirmCurrentSelection() {
  if (composition_->empty())
    return false;
  Segment &seg(composition_->back());
  shared_ptr<Candidate> cand(seg.GetSelectedCandidate());
  if (cand) {
    seg.status = Segment::kSelected;
    EZLOGGERPRINT("Confirmed: '%s', selected_index = %d.",
                  cand->text().c_str(), seg.selected_index);
    select_notifier_(this);
    return true;
  }
  return false;
}

bool Context::ConfirmPreviousSelection() {
  for (Composition::reverse_iterator it = composition_->rbegin();
       it != composition_->rend(); ++it) {
    if (it->status > Segment::kSelected) return false;
    if (it->status == Segment::kSelected) {
      it->status = Segment::kConfirmed;
      return true;
    }
  }
  return false;
}

bool Context::ReopenPreviousSegment() {
  if (composition_->empty())
    return false;
  Segment &last(composition_->back());
  if (last.start == last.end) {
    composition_->pop_back();
    if (!composition_->empty()) {
      composition_->back().status = Segment::kVoid;
    }
    update_notifier_(this);
    return true;
  }
  return false;
}

bool Context::ReopenPreviousSelection() {
  for (Composition::reverse_iterator it = composition_->rbegin();
       it != composition_->rend(); ++it) {
    if (it->status > Segment::kSelected) return false;
    if (it->status == Segment::kSelected) {
      it->status = Segment::kVoid;
      while (it != composition_->rbegin())
        composition_->pop_back();
      update_notifier_(this);
      return true;
    }
  }
  return false;
}

void Context::set_composition(Composition *comp) {
  if (composition_.get() != comp)
    composition_.reset(comp);
  // TODO: notification
}

void Context::set_input(const std::string &value) {
  input_ = value;
  update_notifier_(this);
}

Composition* Context::composition() {
  return composition_.get();
}

const Composition* Context::composition() const {
  return composition_.get();
}

}  // namespace rime
