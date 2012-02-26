#pragma once

namespace rime {
	class SwitcherSettings;
}

class UIStyleSettings;

class Configurator
{
public:
	explicit Configurator();

	int Run(bool installing);
	int UpdateWorkspace(bool report_errors = false);

protected:
	bool ConfigureSwitcher(rime::SwitcherSettings* settings);
	bool ConfigureUI(UIStyleSettings* settings);
};
