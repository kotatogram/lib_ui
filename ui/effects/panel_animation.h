// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include "styles/style_widgets.h"

namespace Ui {

class RoundShadowAnimation {
public:
	void setCornerMasks(const std::array<QImage, 4> &corners);

protected:
	void start(int frameWidth, int frameHeight, float64 devicePixelRatio);
	void setShadow(const style::Shadow &st);

	bool started() const {
		return !_frame.isNull();
	}

	struct Corner {
		QImage image;
		int width = 0;
		int height = 0;
		const uchar *bytes = nullptr;
		int bytesPerPixel = 0;
		int bytesPerLineAdded = 0;

		bool valid() const {
			return !image.isNull();
		}
	};
	void setCornerMask(Corner &corner, const QImage &image);
	void paintCorner(Corner &corner, int left, int top);

	struct Shadow {
		style::margins extend;
		QImage left, topLeft, top, topRight, right, bottomRight, bottom, bottomLeft;

		bool valid() const {
			return !left.isNull();
		}
	};
	QImage cloneImage(const style::icon &source);
	void paintShadow(int left, int top, int right, int bottom);
	void paintShadowCorner(int left, int top, const QImage &image);
	void paintShadowVertical(int left, int top, int bottom, const QImage &image);
	void paintShadowHorizontal(int left, int right, int top, const QImage &image);

	Shadow _shadow;

	Corner _topLeft;
	Corner _topRight;
	Corner _bottomLeft;
	Corner _bottomRight;

	QImage _frame;
	uint32 *_frameInts = nullptr;
	int _frameWidth = 0;
	int _frameHeight = 0;
	int _frameAlpha = 0; // recounted each getFrame()
	int _frameIntsPerLine = 0;
	int _frameIntsPerLineAdded = 0;

};

class PanelAnimation : public RoundShadowAnimation {
public:
	enum class Origin {
		TopLeft,
		TopRight,
		BottomLeft,
		BottomRight,
	};
	PanelAnimation(const style::PanelAnimation &st, Origin origin) : _st(st), _origin(origin) {
	}

	struct PaintState {
		float64 opacity = 0.;
		float64 widthProgress = 0.;
		float64 heightProgress = 0.;
		float64 fade = 0.;
		int width = 0;
		int height = 0;
	};

	void setFinalImage(QImage &&finalImage, QRect inner);
	void setSkipShadow(bool skipShadow);

	void start();
	[[nodiscard]] PaintState computeState(float64 dt, float64 opacity) const;
	PaintState paintFrame(
		QPainter &p,
		int x,
		int y,
		int outerWidth,
		float64 dt,
		float64 opacity);

private:
	void setStartWidth();
	void setStartHeight();
	void setStartAlpha();
	void setStartFadeTop();
	void createFadeMask();
	void setWidthDuration();
	void setHeightDuration();
	void setAlphaDuration();

	const style::PanelAnimation &_st;
	Origin _origin = Origin::TopLeft;

	QPixmap _finalImage;
	int _finalWidth = 0;
	int _finalHeight = 0;
	int _finalInnerLeft = 0;
	int _finalInnerTop = 0;
	int _finalInnerRight = 0;
	int _finalInnerBottom = 0;
	int _finalInnerWidth = 0;
	int _finalInnerHeight = 0;

	bool _skipShadow = false;
	int _startWidth = -1;
	int _startHeight = -1;
	int _startAlpha = 0;

	int _startFadeTop = 0;
	QPixmap _fadeMask;
	int _fadeHeight = 0;
	QBrush _fadeFirst, _fadeLast;

	float64 _widthDuration = 1.;
	float64 _heightDuration = 1.;
	float64 _alphaDuration = 1.;

};

} // namespace Ui
