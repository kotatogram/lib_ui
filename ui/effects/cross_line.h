// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include "styles/style_widgets.h"

namespace Ui {

class CrossLineAnimation {
public:
	CrossLineAnimation(
		const style::CrossLineAnimation &st,
		bool reversed = false,
		float angle = 315);

	void paint(
		QPainter &p,
		QPoint position,
		float64 progress,
		std::optional<QColor> colorOverride = std::nullopt);
	void paint(
		QPainter &p,
		int left,
		int top,
		float64 progress,
		std::optional<QColor> colorOverride = std::nullopt);

	void invalidate();

private:
	void fillFrame(float64 progress, std::optional<QColor> colorOverride);

	const style::CrossLineAnimation &_st;
	const bool _reversed;
	const QPen _transparentPen;
	QPen _strokePen;
	QLineF _line;
	QImage _frame;
	QImage _completeCross;
	QImage _completeCrossOverride;

};

} // namespace Ui
