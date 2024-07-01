// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "ui/text/custom_emoji_instance.h"

#include "ui/effects/animation_value.h"
#include "ui/effects/frame_generator.h"
#include "ui/dynamic_image.h"
#include "ui/ui_utility.h"
#include "ui/painter.h"

#include <crl/crl_async.h>
#include <lz4.h>

class QPainter;

namespace Ui::CustomEmoji {
namespace {

constexpr auto kMaxFrames = 180;
constexpr auto kCacheVersion = 1;
constexpr auto kPreloadFrames = 3;

struct CacheHeader {
	int version = 0;
	int size = 0;
	int frames = 0;
	int length = 0;
};

void PaintScaledImage(
		QPainter &p,
		const QRect &target,
		const Cache::Frame &frame,
		const Context &context) {
	static QImage PaintCache;
	const auto cache = context.internal.colorized ? &PaintCache : nullptr;
	auto q = std::optional<QPainter>();
	if (cache) {
		const auto ratio = style::DevicePixelRatio();
		if (cache->width() < target.width() * ratio
			|| cache->height() < target.height() * ratio) {
			*cache = QImage(
				std::max(cache->width(), target.width() * ratio),
				std::max(cache->height(), target.height() * ratio),
				QImage::Format_ARGB32_Premultiplied);
			cache->setDevicePixelRatio(ratio);
		}
		q.emplace(cache);
		q->setCompositionMode(QPainter::CompositionMode_Source);
		if (context.scaled) {
			q->fillRect(QRect(QPoint(), target.size()), Qt::transparent);
		}
		q->translate(-target.topLeft());
	}
	const auto to = q ? &*q : &p;
	if (context.scaled) {
		const auto sx = anim::interpolate(
			target.width() / 2,
			0,
			context.scale);
		const auto sy = (target.height() == target.width())
			? sx
			: anim::interpolate(target.height() / 2, 0, context.scale);
		const auto scaled = target.marginsRemoved({ sx, sy, sx, sy });
		if (frame.source.isNull()) {
			to->drawImage(scaled, *frame.image);
		} else {
			to->drawImage(scaled, *frame.image, frame.source);
		}
	} else if (frame.source.isNull()) {
		to->drawImage(target, *frame.image);
	} else {
		to->drawImage(target, *frame.image, frame.source);
	}
	if (q) {
		q.reset();
		const auto ratio = style::DevicePixelRatio();
		const auto source = QRect(QPoint(), target.size() * ratio);
		const auto &color = context.textColor;
		style::colorizeImage(*cache, color, cache, source, {}, true);
		p.drawImage(target, *cache, source);
	}
}

} // namespace

QColor PreviewColorFromTextColor(QColor color) {
	color.setAlpha((color.alpha() + 1) / 8);
	return color;
}

Preview::Preview(QPainterPath path, float64 scale)
: _data(ScaledPath{ std::move(path), scale }) {
}

Preview::Preview(QImage image, bool exact)
: _data(Image{ .data = std::move(image), .exact = exact }) {
}

void Preview::paint(QPainter &p, const Context &context) {
	if (const auto path = std::get_if<ScaledPath>(&_data)) {
		paintPath(p, context, *path);
	} else if (const auto image = std::get_if<Image>(&_data)) {
		const auto &data = image->data;
		const auto factor = style::DevicePixelRatio();
		const auto rect = QRect(context.position, data.size() / factor);
		PaintScaledImage(p, rect, { .image = &data }, context);
	}
}

bool Preview::isImage() const {
	return v::is<Image>(_data);
}

bool Preview::isExactImage() const {
	if (const auto image = std::get_if<Image>(&_data)) {
		return image->exact;
	}
	return false;
}

QImage Preview::image() const {
	if (const auto image = std::get_if<Image>(&_data)) {
		return image->data;
	}
	return QImage();
}

void Preview::paintPath(
		QPainter &p,
		const Context &context,
		const ScaledPath &path) {
	auto hq = PainterHighQualityEnabler(p);
	p.setBrush(PreviewColorFromTextColor(context.textColor));
	p.setPen(Qt::NoPen);
	const auto scale = path.scale;
	const auto required = (scale != 1.) || context.scaled;
	if (required) {
		p.save();
	}
	p.translate(context.position);
	if (required) {
		p.scale(scale, scale);
		const auto center = QPoint(
			context.size.width() / 2,
			context.size.height() / 2);
		if (context.scaled) {
			p.translate(center);
			p.scale(context.scale, context.scale);
			p.translate(-center);
		}
	}
	p.drawPath(path.path);
	if (required) {
		p.restore();
	} else {
		p.translate(-context.position);
	}
}

Cache::Cache(int size) : _size(size) {
}

std::optional<Cache> Cache::FromSerialized(
		const QByteArray &serialized,
		int requestedSize) {
	if (serialized.size() <= sizeof(CacheHeader)) {
		return {};
	}
	auto header = CacheHeader();
	memcpy(&header, serialized.data(), sizeof(header));
	const auto size = header.size;
	if (size != requestedSize
		|| header.frames <= 0
		|| header.frames >= kMaxFrames
		|| header.length <= 0
		|| header.length > (size * size * header.frames * sizeof(int32))
		|| (serialized.size() != sizeof(CacheHeader)
			+ header.length
			+ (header.frames * sizeof(Cache(0)._durations[0])))) {
		return {};
	}
	const auto rows = (header.frames + kPerRow - 1) / kPerRow;
	const auto columns = std::min(header.frames, kPerRow);
	auto durations = std::vector<uint16>(header.frames, 0);
	auto full = QImage(
		columns * size,
		rows * size,
		QImage::Format_ARGB32_Premultiplied);
	Assert(full.bytesPerLine() == full.width() * sizeof(int32));

	const auto decompressed = LZ4_decompress_safe(
		serialized.data() + sizeof(CacheHeader),
		reinterpret_cast<char*>(full.bits()),
		header.length,
		full.bytesPerLine() * full.height());
	if (decompressed <= 0) {
		return {};
	}
	memcpy(
		durations.data(),
		serialized.data() + sizeof(CacheHeader) + header.length,
		header.frames * sizeof(durations[0]));

	auto result = Cache(size);
	result._finished = true;
	result._full = std::move(full);
	result._frames = header.frames;
	result._durations = std::move(durations);
	return result;
}

QByteArray Cache::serialize() {
	Expects(_finished);
	Expects(_durations.size() == _frames);
	Expects(_full.bytesPerLine() == sizeof(int32) * _full.width());

	auto header = CacheHeader{
		.version = kCacheVersion,
		.size = _size,
		.frames = _frames,
	};
	const auto input = _full.width() * _full.height() * sizeof(int32);
	const auto max = sizeof(CacheHeader)
		+ LZ4_compressBound(input)
		+ (_frames * sizeof(_durations[0]));
	auto result = QByteArray(max, Qt::Uninitialized);
	header.length = LZ4_compress_default(
		reinterpret_cast<const char*>(_full.constBits()),
		result.data() + sizeof(CacheHeader),
		input,
		result.size() - sizeof(CacheHeader));
	Assert(header.length > 0);
	memcpy(result.data(), &header, sizeof(CacheHeader));
	memcpy(
		result.data() + sizeof(CacheHeader) + header.length,
		_durations.data(),
		_frames * sizeof(_durations[0]));
	result.resize(sizeof(CacheHeader)
		+ header.length
		+ _frames * sizeof(_durations[0]));

	return result;
}

int Cache::frames() const {
	return _frames;
}

bool Cache::readyInDefaultState() const {
	return (_frames > 0) && !_frame;
}

Cache::Frame Cache::frame(int index) const {
	Expects(index < _frames);

	const auto row = index / kPerRow;
	const auto inrow = index % kPerRow;
	if (_finished) {
		return { &_full, { inrow * _size, row * _size, _size, _size } };
	}
	return { &_images[row], { 0, inrow * _size, _size, _size } };
}

int Cache::size() const {
	return _size;
}

Preview Cache::makePreview() const {
	Expects(_frames > 0);

	const auto first = frame(0);
	return { first.image->copy(first.source), true };
}

void Cache::reserve(int frames) {
	Expects(!_finished);

	const auto rows = (frames + kPerRow - 1) / kPerRow;
	if (const auto add = rows - int(_images.size()); add > 0) {
		_images.resize(rows);
		for (auto e = end(_images), i = e - add; i != e; ++i) {
			(*i) = QImage(
				_size,
				_size * kPerRow,
				QImage::Format_ARGB32_Premultiplied);
		}
	}
	_durations.reserve(frames);
}

int Cache::frameRowByteSize() const {
	return _size * 4;
}

int Cache::frameByteSize() const {
	return _size * frameRowByteSize();
}

void Cache::add(crl::time duration, const QImage &frame) {
	Expects(!_finished);
	Expects(frame.size() == QSize(_size, _size));
	Expects(frame.format() == QImage::Format_ARGB32_Premultiplied);

	const auto row = (_frames / kPerRow);
	const auto inrow = (_frames % kPerRow);
	const auto rows = row + 1;
	while (_images.size() < rows) {
		_images.emplace_back();
		_images.back() = QImage(
			_size,
			_size * kPerRow,
			QImage::Format_ARGB32_Premultiplied);
	}
	const auto srcPerLine = frame.bytesPerLine();
	const auto dstPerLine = _images[row].bytesPerLine();
	const auto perLine = std::min(srcPerLine, dstPerLine);
	auto dst = _images[row].bits() + inrow * _size * dstPerLine;
	auto src = frame.constBits();
	for (auto y = 0; y != _size; ++y) {
		memcpy(dst, src, perLine);
		dst += dstPerLine;
		src += srcPerLine;
	}
	++_frames;
	_durations.push_back(std::clamp(
		duration,
		crl::time(0),
		crl::time(std::numeric_limits<uint16>::max())));
}

void Cache::finish() {
	_finished = true;
	if (_frame == _frames) {
		_frame = 0;
	}
	const auto rows = (_frames + kPerRow - 1) / kPerRow;
	const auto columns = std::min(_frames, kPerRow);
	const auto zero = (rows * columns) - _frames;
	_full = QImage(
		columns * _size,
		rows * _size,
		QImage::Format_ARGB32_Premultiplied);
	auto dstData = _full.bits();
	const auto perLine = _size * 4;
	const auto dstPerLine = _full.bytesPerLine();
	for (auto y = 0; y != rows; ++y) {
		auto &row = _images[y];
		auto src = row.bits();
		const auto srcPerLine = row.bytesPerLine();
		const auto till = columns - ((y + 1 == rows) ? zero : 0);
		for (auto x = 0; x != till; ++x) {
			auto dst = dstData + y * dstPerLine * _size + x * perLine;
			for (auto line = 0; line != _size; ++line) {
				memcpy(dst, src, perLine);
				src += srcPerLine;
				dst += dstPerLine;
			}
		}
	}
	if (const auto perLine = zero * _size) {
		auto dst = dstData
			+ (rows - 1) * dstPerLine * _size
			+ (columns - zero) * _size * 4;
		for (auto left = 0; left != _size; ++left) {
			memset(dst, 0, perLine);
			dst += dstPerLine;
		}
	}
}

PaintFrameResult Cache::paintCurrentFrame(
		QPainter &p,
		const Context &context) {
	if (!_frames) {
		return {};
	}
	const auto first = context.internal.forceFirstFrame;
	auto last = context.internal.forceLastFrame;
	if (!first && !last) {
		const auto now = context.paused ? 0 : context.now;
		const auto finishes = now ? currentFrameFinishes() : 0;
		if (finishes && now >= finishes) {
			++_frame;
			if (_finished && _frame == _frames) {
				_frame = 0;
				if (context.internal.overrideFirstWithLastFrame) {
					last = true;
				}
			}
			_shown = now;
		} else if (!_shown) {
			_shown = now;
		}
	}
	const auto index = first
		? 0
		: last
		? (_frames - 1)
		: std::min(_frame, _frames - 1);
	const auto info = frame(index);
	const auto size = _size / style::DevicePixelRatio();
	const auto rect = QRect(context.position, QSize(size, size));
	PaintScaledImage(p, rect, info, context);
	const auto next = first ? 0 : currentFrameFinishes();
	return {
		.painted = true,
		.next = next,
		.duration = next ? (next - _shown) : 0,
	};
}

int Cache::currentFrame() const {
	return _frame;
}

crl::time Cache::currentFrameFinishes() const {
	if (!_shown || _frame >= _durations.size()) {
		return 0;
	} else if (const auto duration = _durations[_frame]) {
		return _shown + duration;
	}
	return 0;
}

Cached::Cached(
	const QString &entityData,
	Fn<std::unique_ptr<Loader>()> unloader,
	Cache cache)
: _unloader(std::move(unloader))
, _cache(std::move(cache))
, _entityData(entityData) {
}

QString Cached::entityData() const {
	return _entityData;
}

PaintFrameResult Cached::paint(QPainter &p, const Context &context) {
	return _cache.paintCurrentFrame(p, context);
}

bool Cached::inDefaultState() const {
	return _cache.readyInDefaultState();
}

Preview Cached::makePreview() const {
	return _cache.makePreview();
}

Loading Cached::unload() {
	return Loading(_unloader(), makePreview());
}

Renderer::Renderer(RendererDescriptor &&descriptor)
: _cache(descriptor.size)
, _put(std::move(descriptor.put))
, _loader(std::move(descriptor.loader)) {
	Expects(_loader != nullptr);

	const auto size = _cache.size();
	const auto guard = base::make_weak(this);
	crl::async([=, factory = std::move(descriptor.generator)]() mutable {
		auto generator = factory();
		auto rendered = generator->renderNext(
			QImage(),
			QSize(size, size),
			Qt::KeepAspectRatio);
		if (rendered.image.isNull()) {
			return;
		}
		crl::on_main(guard, [
			=,
			frame = std::move(rendered),
			generator = std::move(generator)
		]() mutable {
			frameReady(
				std::move(generator),
				frame.duration,
				std::move(frame.image));
		});
	});
}

Renderer::~Renderer() = default;

void Renderer::frameReady(
		std::unique_ptr<Ui::FrameGenerator> generator,
		crl::time duration,
		QImage frame) {
	if (frame.isNull()) {
		finish();
		return;
	}
	if (const auto count = generator->count()) {
		if (!_cache.frames()) {
			_cache.reserve(std::max(count, kMaxFrames));
		}
	}
	const auto current = _cache.currentFrame();
	const auto total = _cache.frames();
	const auto explicitRepaint = (current == total);
	_cache.add(duration, frame);
	if (explicitRepaint && _repaint) {
		_repaint();
	}
	if (!duration || total + 1 >= kMaxFrames) {
		finish();
	} else if (current + kPreloadFrames > total) {
		renderNext(std::move(generator), std::move(frame));
	} else {
		_generator = std::move(generator);
		_storage = std::move(frame);
	}
}

void Renderer::renderNext(
		std::unique_ptr<Ui::FrameGenerator> generator,
		QImage storage) {
	const auto size = _cache.size();
	const auto guard = base::make_weak(this);
	crl::async([
		=,
		storage = std::move(storage),
		generator = std::move(generator)
	]() mutable {
		auto rendered = generator->renderNext(
			std::move(storage),
			QSize(size, size),
			Qt::KeepAspectRatio);
		crl::on_main(guard, [
			=,
			frame = std::move(rendered),
			generator = std::move(generator)
		]() mutable {
			frameReady(
				std::move(generator),
				frame.duration,
				std::move(frame.image));
		});
	});
}

void Renderer::finish() {
	_finished = true;
	_cache.finish();
	if (_put) {
		_put(_cache.serialize());
	}
}

PaintFrameResult Renderer::paint(QPainter &p, const Context &context) {
	const auto result = _cache.paintCurrentFrame(p, context);
	if (_generator
		&& (!result.painted
			|| _cache.currentFrame() + kPreloadFrames >= _cache.frames())) {
		renderNext(std::move(_generator), std::move(_storage));
	}
	return result;
}

std::optional<Cached> Renderer::ready(const QString &entityData) {
	return _finished
		? Cached{ entityData, std::move(_loader), std::move(_cache) }
		: std::optional<Cached>();
}

std::unique_ptr<Loader> Renderer::cancel() {
	return _loader();
}

bool Renderer::canMakePreview() const {
	return _cache.frames() > 0;
}

Preview Renderer::makePreview() const {
	return _cache.makePreview();
}

bool Renderer::readyInDefaultState() const {
	return _cache.readyInDefaultState();
}

void Renderer::setRepaintCallback(Fn<void()> repaint) {
	_repaint = std::move(repaint);
}

Cache Renderer::takeCache() {
	return std::move(_cache);
}

Loading::Loading(std::unique_ptr<Loader> loader, Preview preview)
: _loader(std::move(loader))
, _preview(std::move(preview)) {
}

QString Loading::entityData() const {
	return _loader->entityData();
}

void Loading::load(Fn<void(Loader::LoadResult)> done) {
	_loader->load(crl::guard(this, [this, done = std::move(done)](
			Loader::LoadResult result) mutable {
		if (const auto caching = std::get_if<Caching>(&result)) {
			caching->preview = _preview
				? std::move(_preview)
				: _loader->preview();
		}
		done(std::move(result));
	}));
}

bool Loading::loading() const {
	return _loader->loading();
}

void Loading::paint(QPainter &p, const Context &context) {
	if (!_preview) {
		if (auto preview = _loader->preview()) {
			_preview = std::move(preview);
		}
	}
	_preview.paint(p, context);
}

bool Loading::hasImagePreview() const {
	return _preview.isImage();
}

Preview Loading::imagePreview() const {
	return _preview.isImage() ? _preview : Preview();
}

void Loading::updatePreview(Preview preview) {
	if (!_preview.isImage() && preview.isImage()) {
		_preview = std::move(preview);
	} else if (!_preview) {
		if (auto loaderPreview = _loader->preview()) {
			_preview = std::move(loaderPreview);
		} else if (preview) {
			_preview = std::move(preview);
		}
	}
}

void Loading::cancel() {
	_loader->cancel();
	invalidate_weak_ptrs(this);
}

Instance::Instance(
	Loading loading,
	Fn<void(not_null<Instance*>, RepaintRequest)> repaintLater)
: _state(std::move(loading))
, _repaintLater(std::move(repaintLater)) {
}

QString Instance::entityData() const {
	return v::match(_state, [](const Loading &state) {
		return state.entityData();
	}, [](const Caching &state) {
		return state.entityData;
	}, [](const Cached &state) {
		return state.entityData();
	});
}

void Instance::paint(QPainter &p, const Context &context) {
	context.internal.colorized = _colored;

	v::match(_state, [&](Loading &state) {
		state.paint(p, context);
		load(state);
	}, [&](Caching &state) {
		auto result = state.renderer->paint(p, context);
		if (!result.painted) {
			state.preview.paint(p, context);
		} else {
			if (!state.preview.isExactImage()) {
				state.preview = state.renderer->makePreview();
			}
			if (result.next > context.now) {
				_repaintLater(this, { result.next, result.duration });
			}
		}
		if (auto cached = state.renderer->ready(state.entityData)) {
			_state = std::move(*cached);
		}
	}, [&](Cached &state) {
		const auto result = state.paint(p, context);
		if (result.next > context.now) {
			_repaintLater(this, { result.next, result.duration });
		}
	});
}

bool Instance::ready() {
	return v::match(_state, [&](Loading &state) {
		if (state.hasImagePreview()) {
			return true;
		}
		if (!_usage.empty()) {
			load(state);
		}
		return false;
	}, [](Caching &state) {
		return state.renderer->canMakePreview();
	}, [](Cached &state) {
		return true;
	});
}

bool Instance::readyInDefaultState() {
	return v::match(_state, [&](Loading &state) {
		if (state.hasImagePreview()) {
			return true;
		}
		load(state);
		return false;
	}, [](Caching &state) {
		return state.renderer->readyInDefaultState();
	}, [](Cached &state) {
		return state.inDefaultState();
	});
}

void Instance::load(Loading &state) {
	state.load([=](Loader::LoadResult result) {
		if (auto caching = std::get_if<Caching>(&result)) {
			caching->renderer->setRepaintCallback([=] { repaint(); });
			_state = std::move(*caching);
		} else if (auto cached = std::get_if<Cached>(&result)) {
			_state = std::move(*cached);
			repaint();
		} else {
			Unexpected("Value in Loader::LoadResult.");
		}
	});
}

bool Instance::hasImagePreview() const {
	return v::match(_state, [](const Loading &state) {
		return state.hasImagePreview();
	}, [](const Caching &state) {
		return state.preview.isImage();
	}, [](const Cached &state) {
		return true;
	});
}

Preview Instance::imagePreview() const {
	return v::match(_state, [](const Loading &state) {
		return state.imagePreview();
	}, [](const Caching &state) {
		return state.preview.isImage() ? state.preview : Preview();
	}, [](const Cached &state) {
		return state.makePreview();
	});
}

void Instance::updatePreview(Preview preview) {
	v::match(_state, [&](Loading &state) {
		state.updatePreview(std::move(preview));
	}, [&](Caching &state) {
		if ((!state.preview.isImage() && preview.isImage())
			|| (!state.preview && preview)) {
			state.preview = std::move(preview);
		}
	}, [](const Cached &) {});
}

void Instance::setColored() {
	if (!_colored) {
		_colored = true;
		if (ready()) {
			_repaintLater(this, { .when = crl::now() + 1 });
		}
	}
}

void Instance::repaint() {
	for (const auto &object : _usage) {
		object->repaint();
	}
}

void Instance::incrementUsage(not_null<Object*> object) {
	_usage.emplace(object);
}

void Instance::decrementUsage(not_null<Object*> object) {
	_usage.remove(object);
	if (!_usage.empty()) {
		return;
	}
	v::match(_state, [](Loading &state) {
		state.cancel();
	}, [&](Caching &state) {
		_state = Loading{
			state.renderer->cancel(),
			std::move(state.preview),
		};
	}, [&](Cached &state) {
		_state = state.unload();
	});
	_repaintLater(this, RepaintRequest());
}

Object::Object(not_null<Instance*> instance, Fn<void()> repaint)
: _instance(instance)
, _repaint(std::move(repaint)) {
}

Object::~Object() {
	unload();
}

int Object::width() {
	return st::emojiSize + 2 * st::emojiPadding;
}

QString Object::entityData() {
	return _instance->entityData();
}

void Object::paint(QPainter &p, const Context &context) {
	if (!_using) {
		_using = true;
		_instance->incrementUsage(this);
	}
	_instance->paint(p, context);
}

void Object::unload() {
	if (_using) {
		_using = false;
		_instance->decrementUsage(this);
	}
}

bool Object::ready() {
	if (!_using) {
		_using = true;
		_instance->incrementUsage(this);
	}
	return _instance->ready();
}

bool Object::readyInDefaultState() {
	if (!_using) {
		_using = true;
		_instance->incrementUsage(this);
	}
	return _instance->readyInDefaultState();
}

void Object::repaint() {
	_repaint();
}

Internal::Internal(
	QString entityData,
	QImage image,
	QMargins padding,
	bool colored)
: _entityData(std::move(entityData))
, _image(std::move(image))
, _padding(padding)
, _colored(colored) {
}

int Internal::width() {
	return _padding.left()
		+ (_image.width() / _image.devicePixelRatio())
		+ _padding.right();
}

QString Internal::entityData() {
	return _entityData;
}

void Internal::paint(QPainter &p, const Context &context) {
	context.internal.colorized = _colored;

	const auto size = _image.size() / style::DevicePixelRatio();
	const auto rect = QRect(
		context.position + QPoint(_padding.left(), _padding.top()),
		size);
	PaintScaledImage(p, rect, { &_image }, context);
}

void Internal::unload() {
}

bool Internal::ready() {
	return true;
}

bool Internal::readyInDefaultState() {
	return true;
}

DynamicImageEmoji::DynamicImageEmoji(
	QString entityData,
	std::shared_ptr<DynamicImage> image,
	Fn<void()> repaint,
	QMargins padding,
	int size)
: _entityData(entityData)
, _image(std::move(image))
, _repaint(std::move(repaint))
, _padding(padding)
, _size(size) {
}

int DynamicImageEmoji::width() {
	return _padding.left() + _size + _padding.right();
}

QString DynamicImageEmoji::entityData() {
	return _entityData;
}

void DynamicImageEmoji::paint(QPainter &p, const Context &context) {
	if (!_subscribed) {
		_subscribed = true;
		_image->subscribeToUpdates(_repaint);
	}

	const auto rect = QRect(
		context.position + QPoint(_padding.left(), _padding.top()),
		QSize(_size, _size));
	auto image = _image->image(_size);
	context.internal.colorized = false;
	PaintScaledImage(p, rect, { &image }, context);
}

void DynamicImageEmoji::unload() {
	if (_subscribed) {
		_subscribed = false;
		_image->subscribeToUpdates(nullptr);
	}
}

bool DynamicImageEmoji::ready() {
	return true;
}

bool DynamicImageEmoji::readyInDefaultState() {
	return true;
}

} // namespace Ui::CustomEmoji
