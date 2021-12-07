// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include "styles/style_basic.h"

#include <QtCore/QPoint>
#include <QtGui/QPainter>

class Painter : public QPainter {
public:
	explicit Painter(QPaintDevice *device) : QPainter(device) {
	}

	void drawTextLeft(int x, int y, int outerw, const QString &text, int textWidth = -1) {
		QFontMetrics m(fontMetrics());
		auto ascent = (_ascent == 0 ? m.ascent() : _ascent);
		if (style::RightToLeft() && textWidth < 0) textWidth = m.horizontalAdvance(text);
		drawText(style::RightToLeft() ? (outerw - x - textWidth) : x, y + ascent, text);
	}
	void drawTextRight(int x, int y, int outerw, const QString &text, int textWidth = -1) {
		QFontMetrics m(fontMetrics());
		auto ascent = (_ascent == 0 ? m.ascent() : _ascent);
		if (!style::RightToLeft() && textWidth < 0) textWidth = m.horizontalAdvance(text);
		drawText(style::RightToLeft() ? x : (outerw - x - textWidth), y + ascent, text);
	}
	void drawPixmapLeft(int x, int y, int outerw, const QPixmap &pix, const QRect &from) {
		drawPixmap(QPoint(style::RightToLeft() ? (outerw - x - (from.width() / pix.devicePixelRatio())) : x, y), pix, from);
	}
	void drawPixmapLeft(const QPoint &p, int outerw, const QPixmap &pix, const QRect &from) {
		return drawPixmapLeft(p.x(), p.y(), outerw, pix, from);
	}
	void drawPixmapLeft(int x, int y, int w, int h, int outerw, const QPixmap &pix, const QRect &from) {
		drawPixmap(QRect(style::RightToLeft() ? (outerw - x - w) : x, y, w, h), pix, from);
	}
	void drawPixmapLeft(const QRect &r, int outerw, const QPixmap &pix, const QRect &from) {
		return drawPixmapLeft(r.x(), r.y(), r.width(), r.height(), outerw, pix, from);
	}
	void drawPixmapLeft(int x, int y, int outerw, const QPixmap &pix) {
		drawPixmap(QPoint(style::RightToLeft() ? (outerw - x - (pix.width() / pix.devicePixelRatio())) : x, y), pix);
	}
	void drawPixmapLeft(const QPoint &p, int outerw, const QPixmap &pix) {
		return drawPixmapLeft(p.x(), p.y(), outerw, pix);
	}
	void drawPixmapRight(int x, int y, int outerw, const QPixmap &pix, const QRect &from) {
		drawPixmap(QPoint(style::RightToLeft() ? x : (outerw - x - (from.width() / pix.devicePixelRatio())), y), pix, from);
	}
	void drawPixmapRight(const QPoint &p, int outerw, const QPixmap &pix, const QRect &from) {
		return drawPixmapRight(p.x(), p.y(), outerw, pix, from);
	}
	void drawPixmapRight(int x, int y, int w, int h, int outerw, const QPixmap &pix, const QRect &from) {
		drawPixmap(QRect(style::RightToLeft() ? x : (outerw - x - w), y, w, h), pix, from);
	}
	void drawPixmapRight(const QRect &r, int outerw, const QPixmap &pix, const QRect &from) {
		return drawPixmapRight(r.x(), r.y(), r.width(), r.height(), outerw, pix, from);
	}
	void drawPixmapRight(int x, int y, int outerw, const QPixmap &pix) {
		drawPixmap(QPoint(style::RightToLeft() ? x : (outerw - x - (pix.width() / pix.devicePixelRatio())), y), pix);
	}
	void drawPixmapRight(const QPoint &p, int outerw, const QPixmap &pix) {
		return drawPixmapRight(p.x(), p.y(), outerw, pix);
	}

	void setTextPalette(const style::TextPalette &palette) {
		_textPalette = &palette;
	}
	void restoreTextPalette() {
		_textPalette = nullptr;
	}
	const style::TextPalette &textPalette() const {
		return _textPalette ? *_textPalette : st::defaultTextPalette;
	}
	void setFont(const style::font &font) {
		_ascent = font->ascent;
		QPainter::setFont(font->f);
	}

private:
	const style::TextPalette *_textPalette = nullptr;
	int _ascent = 0;

};

class PainterHighQualityEnabler {
public:
	PainterHighQualityEnabler(QPainter &p) : _painter(p) {
		static constexpr QPainter::RenderHint Hints[] = {
			QPainter::Antialiasing,
			QPainter::SmoothPixmapTransform,
			QPainter::TextAntialiasing
		};

		const auto hints = _painter.renderHints();
		for (const auto hint : Hints) {
			if (!(hints & hint)) {
				_hints |= hint;
			}
		}
		if (_hints) {
			_painter.setRenderHints(_hints);
		}
	}

	PainterHighQualityEnabler(
		const PainterHighQualityEnabler &other) = delete;
	PainterHighQualityEnabler &operator=(
		const PainterHighQualityEnabler &other) = delete;

	~PainterHighQualityEnabler() {
		if (_hints && _painter.isActive()) {
			_painter.setRenderHints(_hints, false);
		}
	}

private:
	QPainter &_painter;
	QPainter::RenderHints _hints;

};
