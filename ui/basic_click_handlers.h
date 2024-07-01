// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include "base/algorithm.h"
#include "ui/click_handler.h"

class TextClickHandler : public ClickHandler {
public:
	TextClickHandler(bool fullDisplayed = true)
	: _fullDisplayed(fullDisplayed) {
	}

	QString copyToClipboardText() const override {
		return url();
	}

	QString tooltip() const override {
		return _fullDisplayed ? QString() : readable();
	}

	void setFullDisplayed(bool full) {
		_fullDisplayed = full;
	}

protected:
	virtual QString readable() const;

	bool _fullDisplayed;

};

class UrlClickHandler : public TextClickHandler {
public:
	UrlClickHandler(const QString &url, bool fullDisplayed = true);

	[[nodiscard]] QString originalUrl() const {
		return _originalUrl;
	}

	QString copyToClipboardContextItemText() const override;

	QString dragText() const override {
		return url();
	}

	TextEntity getTextEntity() const override;

	static void Open(QString url, QVariant context = {});
	void onClick(ClickContext context) const override {
		const auto button = context.button;
		if (button == Qt::LeftButton || button == Qt::MiddleButton) {
			Open(url(), context.other);
		}
	}

	[[nodiscard]] static bool IsEmail(const QString &url) {
		const auto at = url.indexOf('@'), slash = url.indexOf('/');
		return ((at > 0) && (slash < 0 || slash > at));
	}
	[[nodiscard]] static QString EncodeForOpening(const QString &originalUrl);
	[[nodiscard]] static bool IsSuspicious(const QString &url);
	[[nodiscard]] static QString ShowEncoded(const QString &url);

protected:
	QString url() const override {
		return EncodeForOpening(_originalUrl);
	}
	QString readable() const override {
		return _readable;
	}

private:
	[[nodiscard]] bool isEmail() const {
		return IsEmail(_originalUrl);
	}

	QString _originalUrl, _readable;

};
