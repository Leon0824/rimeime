// vim: set sts=2 sw=2 et:
// encoding: utf-8
//
// Copyleft 2011 RIME Developers
// License: GPLv3
// 
// 2011-06-27 GONG Chen <chen.sst@gmail.com>
//
#ifndef RIME_MAPPED_FILE_H_
#define RIME_MAPPED_FILE_H_

#include <boost/interprocess/managed_mapped_file.hpp>
#include <boost/utility.hpp>
#include <rime/common.h>

namespace rime {

class MappedFile : boost::noncopyable {
 public:
  explicit MappedFile(const std::string &file_name);
  virtual ~MappedFile();
  
  bool Create(size_t size);
  bool OpenOrCreate(size_t size);
  bool OpenReadOnly();
  void Close();
  bool IsOpen() const;

  // should be called when the file is not mapped.
  bool Grow(size_t size);
  bool ShrinkToFit();

  // seems hard to provide a complete set of object creators for various types.
  // so, let's make this class the base of your particular mapped file.
  
 protected:
  std::string file_name_;
  scoped_ptr<boost::interprocess::managed_mapped_file> file_;
};

}  // namespace rime

#endif  // RIME_MAPPED_FILE_H_
