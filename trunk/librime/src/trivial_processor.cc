// vim: set sts=2 sw=2 et:
// encoding: utf-8
//
// Copyleft 2011 RIME Developers
// License: GPLv3
// 
// register components
//
// 2011-04-24 GONG Chen <chen.sst@gmail.com>
//

#include <rime/common.h>
#include <rime/context.h>
#include <rime/engine.h>
#include <rime/key_event.h>
#include <rime/key_table.h>
#include "trivial_processor.h"

namespace rime {

// treat printable characters as input
// commit with Return key
bool TrivialProcessor::ProcessKeyEvent(const KeyEvent &key_event) {
  Context *ctx = engine_->context();
  int ch = key_event.keycode();
  if (ch == XK_Return && ctx->IsComposing()) {
    ctx->Commit();
    return true;
  }
  if (ch == XK_BackSpace && ctx->IsComposing()) {
    ctx->PopInput();
    return true;
  }
  if (std::isprint(ch)) {
    ctx->PushInput(key_event.keycode());
    return true;
  }
  // not handled
  return false;
}

}  // namespace rime
