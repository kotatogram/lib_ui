// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "ui/effects/ripple_animation.h"

#include "ui/effects/animations.h"
#include "ui/painter.h"
#include "ui/ui_utility.h"
#include "ui/image/image_prepare.h"
#include "styles/style_widgets.h"

namespace Ui {

class RippleAnimation::Ripple {
public:
	Ripple(
		const style::RippleAnimation &st,
		QPoint origin,
		int startRadius,
		const QPixmap &mask,
		Fn<void()> update);
	Ripple(
		const style::RippleAnimation &st,
		const QPixmap &mask,
		Fn<void()> update);

	void paint(
		QPainter &p,
		const QPixmap &mask,
		const QColor *colorOverride);

	void stop();
	void unstop();
	void finish();
	void clearCache();
	bool finished() const {
		return _hiding && !_hide.animating();
	}

private:
	const style::RippleAnimation &_st;
	Fn<void()> _update;

	QPoint _origin;
	int _radiusFrom = 0;
	int _radiusTo = 0;

	bool _hiding = false;
	Ui::Animations::Simple _show;
	Ui::Animations::Simple _hide;
	QPixmap _cache;
	QImage _frame;

};

RippleAnimation::Ripple::Ripple(
	const style::RippleAnimation &st,
	QPoint origin,
	int startRadius,
	const QPixmap &mask,
	Fn<void()> update)
: _st(st)
, _update(update)
, _origin(origin)
, _radiusFrom(startRadius)
, _frame(mask.size(), QImage::Format_ARGB32_Premultiplied) {
	_frame.setDevicePixelRatio(mask.devicePixelRatio());

	const auto pixelRatio = style::DevicePixelRatio();
	QPoint points[] = {
		{ 0, 0 },
		{ _frame.width() / pixelRatio, 0 },
		{ _frame.width() / pixelRatio, _frame.height() / pixelRatio },
		{ 0, _frame.height() / pixelRatio },
	};
	for (auto point : points) {
		accumulate_max(
			_radiusTo,
			style::point::dotProduct(_origin - point, _origin - point));
	}
	_radiusTo = qRound(sqrt(_radiusTo));

	_show.start(_update, 0., 1., _st.showDuration, anim::easeOutQuint);
}

RippleAnimation::Ripple::Ripple(const style::RippleAnimation &st, const QPixmap &mask, Fn<void()> update)
: _st(st)
, _update(update)
, _origin(
	mask.width() / (2 * style::DevicePixelRatio()),
	mask.height() / (2 * style::DevicePixelRatio()))
, _radiusFrom(mask.width() + mask.height())
, _frame(mask.size(), QImage::Format_ARGB32_Premultiplied) {
	_frame.setDevicePixelRatio(mask.devicePixelRatio());
	_radiusTo = _radiusFrom;
	_hide.start(_update, 0., 1., _st.hideDuration);
}

void RippleAnimation::Ripple::paint(
		QPainter &p,
		const QPixmap &mask,
		const QColor *colorOverride) {
	auto opacity = _hide.value(_hiding ? 0. : 1.);
	if (opacity == 0.) {
		return;
	}

	if (_cache.isNull() || colorOverride != nullptr) {
		const auto shown = _show.value(1.);
		Assert(!std::isnan(shown));
		const auto diff = float64(_radiusTo - _radiusFrom);
		Assert(!std::isnan(diff));
		const auto mult = diff * shown;
		Assert(!std::isnan(mult));
		const auto interpolated = _radiusFrom + mult;
		//anim::interpolateF(_radiusFrom, _radiusTo, shown);
		Assert(!std::isnan(interpolated));
		auto radius = int(base::SafeRound(interpolated));
		//anim::interpolate(_radiusFrom, _radiusTo, _show.value(1.));
		_frame.fill(Qt::transparent);
		{
			QPainter p(&_frame);
			p.setPen(Qt::NoPen);
			if (colorOverride) {
				p.setBrush(*colorOverride);
			} else {
				p.setBrush(_st.color);
			}
			{
				PainterHighQualityEnabler hq(p);
				p.drawEllipse(_origin, radius, radius);
			}
			p.setCompositionMode(QPainter::CompositionMode_DestinationIn);
			p.drawPixmap(0, 0, mask);
		}
		if (radius == _radiusTo && colorOverride == nullptr) {
			_cache = PixmapFromImage(std::move(_frame));
		}
	}
	auto saved = p.opacity();
	if (opacity != 1.) p.setOpacity(saved * opacity);
	if (_cache.isNull()) {
		p.drawImage(0, 0, _frame);
	} else {
		p.drawPixmap(0, 0, _cache);
	}
	if (opacity != 1.) p.setOpacity(saved);
}

void RippleAnimation::Ripple::stop() {
	_hiding = true;
	_hide.start(_update, 1., 0., _st.hideDuration);
}

void RippleAnimation::Ripple::unstop() {
	if (_hiding) {
		if (_hide.animating()) {
			_hide.start(_update, 0., 1., _st.hideDuration);
		}
		_hiding = false;
	}
}

void RippleAnimation::Ripple::finish() {
	if (_update) {
		_update();
	}
	_show.stop();
	_hide.stop();
}

void RippleAnimation::Ripple::clearCache() {
	_cache = QPixmap();
}

RippleAnimation::RippleAnimation(
	const style::RippleAnimation &st,
	QImage mask,
	Fn<void()> callback)
: _st(st)
, _mask(PixmapFromImage(std::move(mask)))
, _update(callback) {
}


void RippleAnimation::add(QPoint origin, int startRadius) {
	lastStop();
	_ripples.push_back(
		std::make_unique<Ripple>(_st, origin, startRadius, _mask, _update));
}

void RippleAnimation::addFading() {
	lastStop();
	_ripples.push_back(std::make_unique<Ripple>(_st, _mask, _update));
}

void RippleAnimation::lastStop() {
	if (!_ripples.empty()) {
		_ripples.back()->stop();
	}
}

void RippleAnimation::lastUnstop() {
	if (!_ripples.empty()) {
		_ripples.back()->unstop();
	}
}

void RippleAnimation::lastFinish() {
	if (!_ripples.empty()) {
		_ripples.back()->finish();
	}
}

void RippleAnimation::forceRepaint() {
	for (const auto &ripple : _ripples) {
		ripple->clearCache();
	}
	if (_update) {
		_update();
	}
}

void RippleAnimation::paint(
		QPainter &p,
		int x,
		int y,
		int outerWidth,
		const QColor *colorOverride) {
	if (_ripples.empty()) {
		return;
	}

	if (style::RightToLeft()) {
		x = outerWidth - x - (_mask.width() / style::DevicePixelRatio());
	}
	p.translate(x, y);
	for (const auto &ripple : _ripples) {
		ripple->paint(p, _mask, colorOverride);
	}
	p.translate(-x, -y);
	clearFinished();
}

QImage RippleAnimation::MaskByDrawer(
		QSize size,
		bool filled,
		Fn<void(QPainter &p)> drawer) {
	auto result = QImage(
		size * style::DevicePixelRatio(),
		QImage::Format_ARGB32_Premultiplied);
	result.setDevicePixelRatio(style::DevicePixelRatio());
	result.fill(filled ? QColor(255, 255, 255) : Qt::transparent);
	if (drawer) {
		Painter p(&result);
		PainterHighQualityEnabler hq(p);

		p.setPen(Qt::NoPen);
		p.setBrush(QColor(255, 255, 255));
		drawer(p);
	}
	return result;
}

QImage RippleAnimation::RectMask(QSize size) {
	return MaskByDrawer(size, true, nullptr);
}

QImage RippleAnimation::RoundRectMask(QSize size, int radius) {
	return MaskByDrawer(size, false, [&](QPainter &p) {
		p.drawRoundedRect(0, 0, size.width(), size.height(), radius, radius);
	});
}

QImage RippleAnimation::RoundRectMask(
		QSize size,
		Images::CornersMaskRef corners) {
	return MaskByDrawer(size, true, [&](QPainter &p) {
		p.setCompositionMode(QPainter::CompositionMode_Source);
		const auto ratio = style::DevicePixelRatio();
		const auto corner = [&](int index, bool right, bool bottom) {
			if (const auto image = corners.p[index]) {
				if (!image->isNull()) {
					const auto width = image->width() / ratio;
					const auto height = image->height() / ratio;
					p.drawImage(
						QRect(
							right ? (size.width() - width) : 0,
							bottom ? (size.height() - height) : 0,
							width,
							height),
						*image);
				}
			}
		};
		corner(0, false, false);
		corner(1, true, false);
		corner(2, false, true);
		corner(3, true, true);
	});
}

QImage RippleAnimation::EllipseMask(QSize size) {
	return MaskByDrawer(size, false, [&](QPainter &p) {
		p.drawEllipse(0, 0, size.width(), size.height());
	});
}

void RippleAnimation::clearFinished() {
	while (!_ripples.empty() && _ripples.front()->finished()) {
		_ripples.pop_front();
	}
}

void RippleAnimation::clear() {
	_ripples.clear();
}

RippleAnimation::~RippleAnimation() = default;

} // namespace Ui
