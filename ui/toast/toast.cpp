// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "ui/toast/toast.h"

#include "ui/toast/toast_manager.h"
#include "ui/toast/toast_widget.h"
#include "styles/style_widgets.h"

namespace Ui {
namespace Toast {
namespace {

QPointer<QWidget> DefaultParent;

} // namespace

Instance::Instance(
	const Config &config,
	not_null<QWidget*> widgetParent,
	const Private &)
: _st(config.st)
, _hideAt(crl::now()
	+ (config.duration ? config.duration : kDefaultDuration))
, _sliding(config.slideSide != RectPart::None)
, _widget(std::make_unique<internal::Widget>(widgetParent, config)) {
	_shownAnimation.start(
		[=] { shownAnimationCallback(); },
		0.,
		1.,
		_sliding ? _st->durationSlide : _st->durationFadeIn);
}

void SetDefaultParent(not_null<QWidget*> parent) {
	DefaultParent = parent;
}

base::weak_ptr<Instance> Show(
		not_null<QWidget*> parent,
		const Config &config) {
	const auto manager = internal::Manager::instance(parent);
	return manager->addToast(std::make_unique<Instance>(
		config,
		parent,
		Instance::Private()));
}

base::weak_ptr<Instance> Show(const Config &config) {
	if (const auto parent = DefaultParent.data()) {
		return Show(parent, config);
	}
	return nullptr;
}

base::weak_ptr<Instance> Show(
		not_null<QWidget*> parent,
		const QString &text) {
	return Show(parent, Config{ .text = { text }, .st = &st::defaultToast });
}

base::weak_ptr<Instance> Show(const QString &text) {
	return Show(Config{ .text = { text }, .st = &st::defaultToast });
}

void Instance::shownAnimationCallback() {
	_widget->setShownLevel(_shownAnimation.value(_hiding ? 0. : 1.));
	if (!_shownAnimation.animating()) {
		if (_hiding) {
			hide();
		}
	}
}

void Instance::hideAnimated() {
	_hiding = true;
	_shownAnimation.start(
		[=] { shownAnimationCallback(); },
		1.,
		0.,
		_sliding ? _st->durationSlide : _st->durationFadeOut);
}

void Instance::hide() {
	_widget->hide();
	_widget->deleteLater();
}

void Instance::setInputUsed(bool used) {
	_widget->setInputUsed(used);
}

not_null<RpWidget*> Instance::widget() const {
	return _widget.get();
}

} // namespace Toast
} // namespace Ui
