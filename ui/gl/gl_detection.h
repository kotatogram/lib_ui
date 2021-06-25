// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include "base/flags.h"

namespace Ui::GL {

enum class Backend {
	Raster,
	OpenGL,
};

struct Capabilities {
	bool supported = false;
	bool transparency = false;
};

[[nodiscard]] Capabilities CheckCapabilities(QWidget *widget = nullptr);

void ForceDisable(bool disable);

} // namespace Ui::GL
