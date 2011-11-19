// vim: set sts=2 sw=2 et:
// encoding: utf-8
//
// Copyleft 2011 RIME Developers
// License: GPLv3
//
// 2011-06-19 GONG Chen <chen.sst@gmail.com>
//
#ifndef RIME_COMPOSITION_H_
#define RIME_COMPOSITION_H_

#include <string>
#include <rime/segmentation.h>

namespace rime {

struct Preedit {
  std::string text;
  size_t cursor_pos;
  size_t sel_start;
  size_t sel_end;
};

class Composition : public Segmentation {
 public:
  Composition();
  bool HasFinishedComposition() const;
  void GetPreedit(Preedit *preedit) const;
  const std::string GetCommitText() const;
  const std::string GetDebugText() const;
};

}  // namespace rime

#endif  // RIME_COMPOSITION_H_
