// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "ui/layers/box_content.h"

#include "ui/widgets/buttons.h"
#include "ui/widgets/scroll_area.h"
#include "ui/widgets/labels.h"
#include "ui/widgets/shadow.h"
#include "ui/wrap/fade_wrap.h"
#include "ui/text/text_utilities.h"
#include "ui/rect_part.h"
#include "ui/painter.h"
#include "base/timer.h"
#include "styles/style_layers.h"
#include "styles/palette.h"

namespace Ui {

void BoxContent::setTitle(rpl::producer<QString> title) {
	getDelegate()->setTitle(std::move(title) | Text::ToWithEntities());
}

QPointer<AbstractButton> BoxContent::addButton(
		object_ptr<AbstractButton> button) {
	auto result = QPointer<AbstractButton>(button.data());
	getDelegate()->addButton(std::move(button));
	return result;
}

QPointer<RoundButton> BoxContent::addButton(
		rpl::producer<QString> text,
		Fn<void()> clickCallback) {
	return addButton(
		std::move(text),
		std::move(clickCallback),
		getDelegate()->style().button);
}

QPointer<RoundButton> BoxContent::addButton(
		rpl::producer<QString> text,
		const style::RoundButton &st) {
	return addButton(std::move(text), nullptr, st);
}

QPointer<RoundButton> BoxContent::addButton(
		rpl::producer<QString> text,
		Fn<void()> clickCallback,
		const style::RoundButton &st) {
	auto button = object_ptr<RoundButton>(this, std::move(text), st);
	auto result = QPointer<RoundButton>(button.data());
	result->setTextTransform(RoundButton::TextTransform::NoTransform);
	result->setClickedCallback(std::move(clickCallback));
	getDelegate()->addButton(std::move(button));
	return result;
}

QPointer<AbstractButton> BoxContent::addLeftButton(
		object_ptr<AbstractButton> button) {
	auto result = QPointer<AbstractButton>(button.data());
	getDelegate()->addLeftButton(std::move(button));
	return result;
}

QPointer<RoundButton> BoxContent::addLeftButton(
		rpl::producer<QString> text,
		Fn<void()> clickCallback) {
	return addLeftButton(
		std::move(text),
		std::move(clickCallback),
		getDelegate()->style().button);
}

QPointer<RoundButton> BoxContent::addLeftButton(
		rpl::producer<QString> text,
		Fn<void()> clickCallback,
		const style::RoundButton &st) {
	auto button = object_ptr<RoundButton>(this, std::move(text), st);
	const auto result = QPointer<RoundButton>(button.data());
	result->setTextTransform(RoundButton::TextTransform::NoTransform);
	result->setClickedCallback(std::move(clickCallback));
	getDelegate()->addLeftButton(std::move(button));
	return result;
}

QPointer<AbstractButton> BoxContent::addTopButton(
		object_ptr<AbstractButton> button) {
	auto result = QPointer<AbstractButton>(button.data());
	getDelegate()->addTopButton(std::move(button));
	return result;
}

QPointer<IconButton> BoxContent::addTopButton(
		const style::IconButton &st,
		Fn<void()> clickCallback) {
	auto button = object_ptr<IconButton>(this, st);
	const auto result = QPointer<IconButton>(button.data());
	result->setClickedCallback(std::move(clickCallback));
	getDelegate()->addTopButton(std::move(button));
	return result;
}

void BoxContent::setInner(object_ptr<TWidget> inner) {
	setInner(std::move(inner), st::boxScroll);
}

void BoxContent::setInner(
		object_ptr<TWidget> inner,
		const style::ScrollArea &st) {
	if (inner) {
		getDelegate()->setLayerType(true);
		_scroll.create(this, st);
		_scroll->setGeometryToLeft(0, _innerTopSkip, width(), 0);
		_scroll->setOwnedWidget(std::move(inner));
		if (_topShadow) {
			_topShadow->raise();
			_bottomShadow->raise();
		} else {
			_topShadow.create(this);
			_bottomShadow.create(this);
		}
		if (!_preparing) {
			// We didn't set dimensions yet, this will be called from finishPrepare();
			finishScrollCreate();
		}
	} else {
		getDelegate()->setLayerType(false);
		_scroll.destroyDelayed();
		_topShadow.destroyDelayed();
		_bottomShadow.destroyDelayed();
	}
}

void BoxContent::finishPrepare() {
	_preparing = false;
	if (_scroll) {
		finishScrollCreate();
	}
	setInnerFocus();
}

void BoxContent::finishScrollCreate() {
	Expects(_scroll != nullptr);

	if (!_scroll->isHidden()) {
		_scroll->show();
	}
	updateScrollAreaGeometry();
	_scroll->scrolls(
	) | rpl::start_with_next([=] {
		updateInnerVisibleTopBottom();
		updateShadowsVisibility();
	}, lifetime());
	_scroll->innerResizes(
	) | rpl::start_with_next([=] {
		updateInnerVisibleTopBottom();
		updateShadowsVisibility();
	}, lifetime());
	_draggingScroll.scrolls(
	) | rpl::start_with_next([=](int delta) {
		if (_scroll) {
			_scroll->scrollToY(_scroll->scrollTop() + delta);
		}
	}, lifetime());
}

void BoxContent::scrollToWidget(not_null<QWidget*> widget) {
	if (_scroll) {
		_scroll->scrollToWidget(widget);
	}
}

RectParts BoxContent::customCornersFilling() {
	return {};
}

void BoxContent::scrollToY(int top, int bottom) {
	if (_scroll) {
		_scroll->scrollToY(top, bottom);
	}
}

void BoxContent::scrollByDraggingDelta(int delta) {
	_draggingScroll.checkDeltaScroll(_scroll ? delta : 0);
}

void BoxContent::updateInnerVisibleTopBottom() {
	const auto widget = static_cast<TWidget*>(_scroll
		? _scroll->widget()
		: nullptr);
	if (widget) {
		const auto top = _scroll->scrollTop();
		widget->setVisibleTopBottom(top, top + _scroll->height());
	}
}

void BoxContent::updateShadowsVisibility() {
	if (!_scroll) {
		return;
	}

	const auto top = _scroll->scrollTop();
	_topShadow->toggle(
		(top > 0
			|| (_innerTopSkip > 0 && !_topShadowWithSkip)),
		anim::type::normal);
	_bottomShadow->toggle(
		(top < _scroll->scrollTopMax()
			|| (_innerBottomSkip > 0 && !_bottomShadowWithSkip)),
		anim::type::normal);
}

void BoxContent::setDimensionsToContent(
		int newWidth,
		not_null<RpWidget*> content) {
	content->resizeToWidth(newWidth);
	content->heightValue(
	) | rpl::start_with_next([=](int height) {
		setDimensions(newWidth, height);
	}, content->lifetime());
}

void BoxContent::setInnerTopSkip(int innerTopSkip, bool scrollBottomFixed) {
	if (_innerTopSkip != innerTopSkip) {
		const auto delta = innerTopSkip - _innerTopSkip;
		_innerTopSkip = innerTopSkip;
		if (_scroll && width() > 0) {
			auto scrollTopWas = _scroll->scrollTop();
			updateScrollAreaGeometry();
			if (scrollBottomFixed) {
				_scroll->scrollToY(scrollTopWas + delta);
			}
		}
	}
}

void BoxContent::setInnerBottomSkip(int innerBottomSkip) {
	if (_innerBottomSkip != innerBottomSkip) {
		_innerBottomSkip = innerBottomSkip;
		if (_scroll && width() > 0) {
			updateScrollAreaGeometry();
		}
	}
}

void BoxContent::setInnerVisible(bool scrollAreaVisible) {
	if (_scroll) {
		_scroll->setVisible(scrollAreaVisible);
	}
}

QPixmap BoxContent::grabInnerCache() {
	const auto isTopShadowVisible = !_topShadow->isHidden();
	const auto isBottomShadowVisible = !_bottomShadow->isHidden();
	if (isTopShadowVisible) {
		_topShadow->setVisible(false);
	}
	if (isBottomShadowVisible) {
		_bottomShadow->setVisible(false);
	}
	const auto result = GrabWidget(this, _scroll->geometry());
	if (isTopShadowVisible) {
		_topShadow->setVisible(true);
	}
	if (isBottomShadowVisible) {
		_bottomShadow->setVisible(true);
	}
	return result;
}

void BoxContent::resizeEvent(QResizeEvent *e) {
	if (_scroll) {
		updateScrollAreaGeometry();
	}
}

void BoxContent::keyPressEvent(QKeyEvent *e) {
	if (e->key() == Qt::Key_Escape && !_closeByEscape) {
		e->accept();
	} else {
		RpWidget::keyPressEvent(e);
	}
}

void BoxContent::updateScrollAreaGeometry() {
	const auto newScrollHeight = height() - _innerTopSkip - _innerBottomSkip;
	const auto changed = (_scroll->height() != newScrollHeight);
	_scroll->setGeometryToLeft(0, _innerTopSkip, width(), newScrollHeight);
	_topShadow->entity()->resize(width(), st::lineWidth);
	_topShadow->moveToLeft(0, _innerTopSkip);
	_bottomShadow->entity()->resize(width(), st::lineWidth);
	_bottomShadow->moveToLeft(
		0,
		height() - _innerBottomSkip - st::lineWidth);
	if (changed) {
		updateInnerVisibleTopBottom();

		const auto top = _scroll->scrollTop();
		_topShadow->toggle(
			(top > 0
				|| (_innerTopSkip > 0 && !_topShadowWithSkip)),
			anim::type::instant);
		_bottomShadow->toggle(
			(top < _scroll->scrollTopMax()
				|| (_innerBottomSkip > 0 && !_bottomShadowWithSkip)),
			anim::type::instant);
	}
}

object_ptr<TWidget> BoxContent::doTakeInnerWidget() {
	return _scroll->takeWidget<TWidget>();
}

void BoxContent::paintEvent(QPaintEvent *e) {
	Painter p(this);

	if (testAttribute(Qt::WA_OpaquePaintEvent)) {
		const auto &color = getDelegate()->style().bg;
		for (const auto &rect : e->region()) {
			p.fillRect(rect, color);
		}
	}
}

BoxShow::BoxShow(not_null<Ui::BoxContent*> box)
: Show()
, _weak(Ui::MakeWeak(box.get())) {
}

BoxShow::~BoxShow() = default;

void BoxShow::showBox(
		object_ptr<BoxContent> content,
		LayerOptions options) const {
	if (_weak && _weak->isBoxShown()) {
		_weak->getDelegate()->show(std::move(content), options);
	}
}

void BoxShow::hideLayer() const {
	if (_weak && _weak->isBoxShown()) {
		_weak->getDelegate()->hideLayer();
	}
}

not_null<QWidget*> BoxShow::toastParent() const {
	if (!_toastParent) {
		Assert(_weak != nullptr);
		_toastParent = Ui::MakeWeak(_weak->window()); // =(
	}
	return _toastParent.data();
}

bool BoxShow::valid() const {
	return _weak;
}

BoxShow::operator bool() const {
	return valid();
}

} // namespace Ui
