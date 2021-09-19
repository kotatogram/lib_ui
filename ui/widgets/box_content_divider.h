// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include "ui/rp_widget.h"

namespace Ui {

class BoxContentDivider : public Ui::RpWidget {
public:
	BoxContentDivider(QWidget *parent);
	BoxContentDivider(
		QWidget *parent,
		int height);
	BoxContentDivider(
		QWidget *parent,
		int height,
		const style::color &bg);

protected:
	void paintEvent(QPaintEvent *e) override;

private:
	const style::color &_bg;

};

} // namespace Ui
