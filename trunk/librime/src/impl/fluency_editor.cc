// vim: set sts=2 sw=2 et:
// encoding: utf-8
//
// Copyleft 2011 RIME Developers
// License: GPLv3
//
// 2011-10-23 GONG Chen <chen.sst@gmail.com>
//
#include <cctype>
#include <rime/common.h>
#include <rime/context.h>
#include <rime/engine.h>
#include <rime/key_event.h>
#include <rime/key_table.h>
#include <rime/impl/fluency_editor.h>

namespace rime {

FluencyEditor::FluencyEditor(Engine *engine) : Processor(engine) {
  engine->context()->set_option("auto_commit", false);
}

// treat printable characters as input
// commit with Return key
Processor::Result FluencyEditor::ProcessKeyEvent(
    const KeyEvent &key_event) {
  if (key_event.release() || key_event.ctrl() || key_event.alt())
    return kRejected;
  Context *ctx = engine_->context();
  int ch = key_event.keycode();
  if (ch == XK_space) {
    if (ctx->IsComposing()) {
      ctx->ConfirmCurrentSelection() || ctx->Commit();
      return kAccepted;
    }
    else {
      return kNoop;
    }
  }
  if (ctx->IsComposing()) {
    if (ch == XK_Return) {
      if (key_event.shift()) {
        engine_->sink()(ctx->GetScriptText());
        ctx->Clear();
      }
      else {
        ctx->Commit();
      }
      return kAccepted;
    }
    if (ch == XK_BackSpace) {
      ctx->ReopenPreviousSegment() ||
          ctx->ReopenPreviousSelection() ||
          ctx->PopInput();
      return kAccepted;
    }
    if (ch == XK_Delete || ch == XK_KP_Delete) {
      if (key_event.shift()) {
        ctx->DeleteCurrentSelection();
      }
      else {
        ctx->DeleteInput();
      }
      return kAccepted;
    }
    if (ch == XK_Escape) {
      if (!ctx->ClearPreviousSegment())
        ctx->Clear();
      return kAccepted;
    }
  }
  if (ch > 0x20 && ch < 0x7f) {
    EZDBGONLYLOGGERPRINT("Add to input: '%c', %d, '%s'",
                         ch, key_event.keycode(), key_event.repr().c_str());
    ctx->PushInput(key_event.keycode());
    ctx->ConfirmPreviousSelection();
    return kAccepted;
  }
  // not handled
  return kNoop;
}

}  // namespace rime
