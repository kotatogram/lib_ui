// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include "ui/text/text_custom_emoji.h"
#include "base/weak_ptr.h"
#include "base/bytes.h"
#include "base/timer.h"

#include <QtGui/QPainterPath>

class QColor;
class QPainter;

namespace Ui {
class DynamicImage;
class FrameGenerator;
} // namespace Ui

namespace Ui::CustomEmoji {

using Context = Ui::Text::CustomEmoji::Context;

[[nodiscard]] QColor PreviewColorFromTextColor(QColor color);

class Preview final {
public:
	Preview() = default;
	Preview(QImage image, bool exact);
	Preview(QPainterPath path, float64 scale);

	void paint(QPainter &p, const Context &context);
	[[nodiscard]] bool isImage() const;
	[[nodiscard]] bool isExactImage() const;
	[[nodiscard]] QImage image() const;

	[[nodiscard]] explicit operator bool() const {
		return !v::is_null(_data);
	}

private:
	struct ScaledPath {
		QPainterPath path;
		float64 scale = 1.;
	};
	struct Image {
		QImage data;
		bool exact = false;
	};

	void paintPath(
		QPainter &p,
		const Context &context,
		const ScaledPath &path);

	std::variant<v::null_t, ScaledPath, Image> _data;

};

struct PaintFrameResult {
	bool painted = false;
	crl::time next = 0;
	crl::time duration = 0;
};

class Cache final {
public:
	Cache(int size);

	struct Frame {
		not_null<const QImage*> image;
		QRect source;
	};

	[[nodiscard]] static std::optional<Cache> FromSerialized(
		const QByteArray &serialized,
		int requestedSize);
	[[nodiscard]] QByteArray serialize();

	[[nodiscard]] int size() const;
	[[nodiscard]] int frames() const;
	[[nodiscard]] bool readyInDefaultState() const;
	[[nodiscard]] Frame frame(int index) const;
	void reserve(int frames);
	void add(crl::time duration, const QImage &frame);
	void finish();

	[[nodiscard]] Preview makePreview() const;

	PaintFrameResult paintCurrentFrame(QPainter &p, const Context &context);
	[[nodiscard]] int currentFrame() const;

private:
	static constexpr auto kPerRow = 16;

	[[nodiscard]] int frameRowByteSize() const;
	[[nodiscard]] int frameByteSize() const;
	[[nodiscard]] crl::time currentFrameFinishes() const;

	std::vector<QImage> _images;
	std::vector<uint16> _durations;
	QImage _full;
	crl::time _shown = 0;
	int _frame = 0;
	int _size = 0;
	int _frames = 0;
	bool _finished = false;

};

class Loader;
class Loading;

class Cached final {
public:
	Cached(
		const QString &entityData,
		Fn<std::unique_ptr<Loader>()> unloader,
		Cache cache);

	[[nodiscard]] QString entityData() const;
	[[nodiscard]] Preview makePreview() const;
	PaintFrameResult paint(QPainter &p, const Context &context);
	[[nodiscard]] bool inDefaultState() const;
	[[nodiscard]] Loading unload();

private:
	Fn<std::unique_ptr<Loader>()> _unloader;
	Cache _cache;
	QString _entityData;

};

struct RendererDescriptor {
	Fn<std::unique_ptr<Ui::FrameGenerator>()> generator;
	Fn<void(QByteArray)> put;
	Fn<std::unique_ptr<Loader>()> loader;
	int size = 0;
};

class Renderer final : public base::has_weak_ptr {
public:
	explicit Renderer(RendererDescriptor &&descriptor);
	virtual ~Renderer();

	PaintFrameResult paint(QPainter &p, const Context &context);
	[[nodiscard]] std::optional<Cached> ready(const QString &entityData);
	[[nodiscard]] std::unique_ptr<Loader> cancel();

	[[nodiscard]] bool canMakePreview() const;
	[[nodiscard]] Preview makePreview() const;
	[[nodiscard]] bool readyInDefaultState() const;

	void setRepaintCallback(Fn<void()> repaint);
	[[nodiscard]] Cache takeCache();

private:
	void frameReady(
		std::unique_ptr<Ui::FrameGenerator> generator,
		crl::time duration,
		QImage frame);
	void renderNext(
		std::unique_ptr<Ui::FrameGenerator> generator,
		QImage storage);
	void finish();

	Cache _cache;
	std::unique_ptr<Ui::FrameGenerator> _generator;
	QImage _storage;
	Fn<void(QByteArray)> _put;
	Fn<void()> _repaint;
	Fn<std::unique_ptr<Loader>()> _loader;
	bool _finished = false;

};

struct Caching {
	std::unique_ptr<Renderer> renderer;
	QString entityData;
	Preview preview;
};

class Loader {
public:
	using LoadResult = std::variant<Caching, Cached>;
	[[nodiscard]] virtual QString entityData() = 0;
	virtual void load(Fn<void(LoadResult)> loaded) = 0;
	[[nodiscard]] virtual bool loading() = 0;
	virtual void cancel() = 0;
	[[nodiscard]] virtual Preview preview() = 0;
	virtual ~Loader() = default;
};

class Loading final : public base::has_weak_ptr {
public:
	Loading(std::unique_ptr<Loader> loader, Preview preview);

	[[nodiscard]] QString entityData() const;

	void load(Fn<void(Loader::LoadResult)> done);
	[[nodiscard]] bool loading() const;
	void paint(QPainter &p, const Context &context);
	[[nodiscard]] bool hasImagePreview() const;
	[[nodiscard]] Preview imagePreview() const;
	void updatePreview(Preview preview);
	void cancel();

private:
	std::unique_ptr<Loader> _loader;
	Preview _preview;

};

struct RepaintRequest {
	crl::time when = 0;
	crl::time duration = 0;
};

class Object;
class Instance final : public base::has_weak_ptr {
public:
	Instance(
		Loading loading,
		Fn<void(not_null<Instance*>, RepaintRequest)> repaintLater);
	Instance(const Instance&) = delete;
	Instance &operator=(const Instance&) = delete;

	[[nodiscard]] QString entityData() const;
	void paint(QPainter &p, const Context &context);
	[[nodiscard]] bool ready();
	[[nodiscard]] bool readyInDefaultState();
	[[nodiscard]] bool hasImagePreview() const;
	[[nodiscard]] Preview imagePreview() const;
	void updatePreview(Preview preview);
	void setColored();

	void incrementUsage(not_null<Object*> object);
	void decrementUsage(not_null<Object*> object);

	void repaint();

private:
	void load(Loading &state);

	std::variant<Loading, Caching, Cached> _state;
	base::flat_set<not_null<Object*>> _usage;
	Fn<void(not_null<Instance*> that, RepaintRequest)> _repaintLater;
	bool _colored = false;

};

class Object final : public Ui::Text::CustomEmoji {
public:
	Object(not_null<Instance*> instance, Fn<void()> repaint);
	~Object();

	int width() override;
	QString entityData() override;
	void paint(QPainter &p, const Context &context) override;
	void unload() override;
	bool ready() override;
	bool readyInDefaultState() override;

	void repaint();

private:
	const not_null<Instance*> _instance;
	Fn<void()> _repaint;
	bool _using = false;

};

class Internal final : public Text::CustomEmoji {
public:
	Internal(
		QString entityData,
		QImage image,
		QMargins padding,
		bool colored);

	int width() override;
	QString entityData() override;
	void paint(QPainter &p, const Context &context) override;
	void unload() override;
	bool ready() override;
	bool readyInDefaultState() override;

private:
	const QString _entityData;
	const QImage _image;
	const QMargins _padding;
	const bool _colored = false;

};

class DynamicImageEmoji final : public Ui::Text::CustomEmoji {
public:
	DynamicImageEmoji(
		QString entityData,
		std::shared_ptr<DynamicImage> image,
		Fn<void()> repaint,
		QMargins padding,
		int size);

	int width() override;
	QString entityData() override;
	void paint(QPainter &p, const Context &context) override;
	void unload() override;
	bool ready() override;
	bool readyInDefaultState() override;

private:
	const QString _entityData;
	const std::shared_ptr<DynamicImage> _image;
	const Fn<void()> _repaint;
	const QMargins _padding;
	const int _size = 0;
	bool _subscribed = false;

};

} // namespace Ui::CustomEmoji
