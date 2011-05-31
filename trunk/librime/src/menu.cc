// vim: set sts=2 sw=2 et:
// encoding: utf-8
//
// Copyleft 2011 RIME Developers
// License: GPLv3
// 
// 2011-05-29 GONG Chen <chen.sst@gmail.com>
//
#include <rime/menu.h>
#include <rime/translation.h>

namespace rime {

void Menu::AddTranslation(shared_ptr<Translation> translation) {
  translations_.push_back(translation);
}

void Menu::Prepare(int candidate_count) {
  // TODO:

}

Page* Menu::CreatePage(int page_size, int page_no) {
  // TODO:
  return NULL;
}

}  // namespace rime
