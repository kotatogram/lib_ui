// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "ui/style/style_core_custom_font.h"

#include "ui/style/style_core_font.h"
#include "ui/integration.h"

#include <QFontDatabase>

namespace style {
namespace {

using namespace internal;

auto RegularFont = CustomFont();
auto BoldFont = CustomFont();

} // namespace

void SetCustomFonts(const CustomFont &regular, const CustomFont &bold) {
	RegularFont = regular;
	BoldFont = bold;
}

QFont ResolveFont(uint32 flags, int size) {
	static auto Database = QFontDatabase();

	const auto fontSettings = Ui::Integration::Instance().fontSettings();
	const auto overrideIsEmpty = GetPossibleEmptyOverride(flags).isEmpty();

	const auto bold = ((flags & FontBold) || (flags & FontSemibold));
	const auto italic = (flags & FontItalic);
	const auto &custom = bold ? BoldFont : RegularFont;
	const auto useCustom = !custom.family.isEmpty();

	auto result = QFont();
	if (flags & FontMonospace) {
		result.setFamily(MonospaceFont());
	} else if (useCustom) {
		const auto sizes = Database.smoothSizes(custom.family, custom.style);
		const auto good = sizes.isEmpty()
			? Database.pointSizes(custom.family, custom.style)
			: sizes;
		const auto point = good.isEmpty() ? size : good.front();
		result = Database.font(custom.family, custom.style, point);
	} else {
		if (!fontSettings.useSystemFont || !overrideIsEmpty) {
			result.setFamily(GetFontOverride(flags));
		}
		if (bold) {
			if (fontSettings.semiboldIsBold) {
				result.setBold(true);
#ifdef DESKTOP_APP_USE_PACKAGED_FONTS
			} else {
				result.setWeight(QFont::DemiBold);
#else // DESKTOP_APP_USE_PACKAGED_FONTS
			} else if (fontSettings.useSystemFont) {
				result.setWeight(QFont::DemiBold);
			} else {
				result.setBold(true);
#endif // !DESKTOP_APP_USE_PACKAGED_FONTS
			}

			if (!fontSettings.semiboldIsBold) {
				if (flags & FontItalic) {
					result.setStyleName("Semibold Italic");
				} else {
					result.setStyleName("Semibold");
				}
			}
		}
	}
	if (italic) {
		result.setItalic(true);
	}

	result.setUnderline(flags & FontUnderline);
	result.setStrikeOut(flags & FontStrikeOut);
	result.setPixelSize(size + fontSettings.fontSize);

	return result;
}

} // namespace style
