// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include <QtCore/QString>
#include <QtCore/QPoint>
#include <QtCore/QRect>
#include <QtGui/QColor>
#include <QtGui/QCursor>
#include <QtGui/QFont>

#include "ui/style/style_core_font.h"
#include "ui/style/style_core_color.h"
#include "ui/style/style_core_icon.h"

namespace style {

using string = QString;
using rect = QRect;
using point = QPoint;
using size = QSize;
using cursor = Qt::CursorShape;
using align = Qt::Alignment;
using margins = QMargins;
using font = internal::Font;
using owned_font = internal::OwnedFont;
using color = internal::Color;
using owned_color = internal::OwnedColor;
using complex_color = internal::ComplexColor;
using icon = internal::Icon;

static constexpr cursor cur_default = Qt::ArrowCursor;
static constexpr cursor cur_pointer = Qt::PointingHandCursor;
static constexpr cursor cur_text = Qt::IBeamCursor;
static constexpr cursor cur_cross = Qt::CrossCursor;
static constexpr cursor cur_sizever = Qt::SizeVerCursor;
static constexpr cursor cur_sizehor = Qt::SizeHorCursor;
static constexpr cursor cur_sizebdiag = Qt::SizeBDiagCursor;
static constexpr cursor cur_sizefdiag = Qt::SizeFDiagCursor;
static constexpr cursor cur_sizeall = Qt::SizeAllCursor;

static constexpr align al_topleft = (Qt::AlignTop | Qt::AlignLeft);
static constexpr align al_top = (Qt::AlignTop | Qt::AlignHCenter);
static constexpr align al_topright = (Qt::AlignTop | Qt::AlignRight);
static constexpr align al_right = (Qt::AlignVCenter | Qt::AlignRight);
static constexpr align al_bottomright = (Qt::AlignBottom | Qt::AlignRight);
static constexpr align al_bottom = (Qt::AlignBottom | Qt::AlignHCenter);
static constexpr align al_bottomleft = (Qt::AlignBottom | Qt::AlignLeft);
static constexpr align al_left = (Qt::AlignVCenter | Qt::AlignLeft);
static constexpr align al_center = (Qt::AlignVCenter | Qt::AlignHCenter);

} // namespace style
