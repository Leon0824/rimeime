// vim: set sts=2 sw=2 et:
// encoding: utf-8
//
// Copyleft 2011 RIME Developers
// License: GPLv3
// 
// 2011-04-06 Zou xu <zouivex@gmail.com>
// 2011-04-08 GONG Chen <chen.sst@gmail.com>
//

#include <gtest/gtest.h>
#include <rime/component.h>
#include <rime/config.h>
#include "../src/yaml_config.h"

using namespace rime;

TEST(RimeConfigTest, ConfigCreation) {
  // registration
  Component::Register("test_config", new YamlConfigComponent("."));
  // finding component
  Config::Component *cc = Config::Find("test_config");
  EXPECT_TRUE(cc);
  // creation
  scoped_ptr<Config> config(cc->Create("test.yaml"));
  EXPECT_TRUE(config);
}
