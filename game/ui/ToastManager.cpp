#include "ToastManager.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace Game::UI {
namespace {
float clamp01(float v) { return std::min(1.0f, std::max(0.0f, v)); }

float easeOutCubic(float t) {
    t = clamp01(t);
    float inv = 1.0f - t;
    return 1.0f - inv * inv * inv;
}

float easeInCubic(float t) {
    t = clamp01(t);
    return t * t * t;
}

Engine::Color withAlpha(Engine::Color c, float alphaMul) {
    float a = std::clamp(alphaMul, 0.0f, 1.0f);
    c.a = static_cast<uint8_t>(std::clamp(static_cast<int>(std::round(c.a * a)), 0, 255));
    return c;
}

Engine::Color withAlphaOverride(Engine::Color c, uint8_t alpha, float alphaMul) {
    c.a = alpha;
    return withAlpha(c, alphaMul);
}

void setBlend(SDL_Renderer* r, SDL_BlendMode mode) {
    if (!r) return;
    SDL_BlendMode prev{};
    SDL_GetRenderDrawBlendMode(r, &prev);
    if (prev != mode) SDL_SetRenderDrawBlendMode(r, mode);
}

void drawFilledCircle(SDL_Renderer* r, int cx, int cy, int radius, const Engine::Color& color) {
    if (!r || radius <= 0) return;
    setBlend(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, color.r, color.g, color.b, color.a);
    for (int dy = -radius; dy <= radius; ++dy) {
        int dx = static_cast<int>(std::floor(std::sqrt(std::max(0, radius * radius - dy * dy))));
        SDL_RenderDrawLine(r, cx - dx, cy + dy, cx + dx, cy + dy);
    }
}

void drawFilledRoundedRect(SDL_Renderer* r, float x, float y, float w, float h, float radius, const Engine::Color& color) {
    if (!r) return;
    if (w <= 0.0f || h <= 0.0f) return;
    float rad = std::min(radius, std::min(w, h) * 0.5f);
    int ri = static_cast<int>(std::round(rad));
    int ix = static_cast<int>(std::round(x));
    int iy = static_cast<int>(std::round(y));
    int iw = static_cast<int>(std::round(w));
    int ih = static_cast<int>(std::round(h));

    setBlend(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, color.r, color.g, color.b, color.a);

    SDL_Rect mid{ix + ri, iy, iw - 2 * ri, ih};
    if (mid.w > 0) SDL_RenderFillRect(r, &mid);

    SDL_Rect left{ix, iy + ri, ri, ih - 2 * ri};
    if (left.w > 0 && left.h > 0) SDL_RenderFillRect(r, &left);

    SDL_Rect right{ix + iw - ri, iy + ri, ri, ih - 2 * ri};
    if (right.w > 0 && right.h > 0) SDL_RenderFillRect(r, &right);

    drawFilledCircle(r, ix + ri, iy + ri, ri, color);
    drawFilledCircle(r, ix + iw - ri - 1, iy + ri, ri, color);
    drawFilledCircle(r, ix + ri, iy + ih - ri - 1, ri, color);
    drawFilledCircle(r, ix + iw - ri - 1, iy + ih - ri - 1, ri, color);
}

std::string ellipsize(const std::string& text, float maxWidth,
                      const std::function<Engine::Vec2(const std::string&, float)>& measure, float scale) {
    if (text.empty()) return text;
    if (maxWidth <= 0.0f) return std::string{};
    if (!measure) return text;
    if (measure(text, scale).x <= maxWidth) return text;
    static constexpr const char* kEllipsis = "...";
    if (measure(kEllipsis, scale).x > maxWidth) return std::string{};

    std::size_t lo = 0;
    std::size_t hi = text.size();
    while (lo + 1 < hi) {
        std::size_t mid = (lo + hi) / 2;
        std::string candidate = text.substr(0, mid) + kEllipsis;
        if (measure(candidate, scale).x <= maxWidth) {
            lo = mid;
        } else {
            hi = mid;
        }
    }
    return text.substr(0, lo) + kEllipsis;
}

std::string formatSeconds(float seconds) {
    float rounded = std::round(seconds * 10.0f) / 10.0f;
    bool whole = std::abs(rounded - std::round(rounded)) < 0.05f;
    std::ostringstream s;
    s << std::fixed << std::setprecision(whole ? 0 : 1) << std::max(0.0f, rounded) << "s";
    return s.str();
}
}  // namespace

void ToastManager::configure(const ToastConfig& config, const ToastTheme& theme) {
    config_ = config;
    theme_ = theme;
}

void ToastManager::clear() {
    toasts_.clear();
}

void ToastManager::pushPickupToast(const std::string& text, std::optional<Engine::Color> accentOverride) {
    if (!config_.enabled) return;
    ToastData toast{};
    toast.kind = ToastKind::Pickup;
    toast.category = ToastCategory::Pickup;
    toast.title = text;
    toast.lifetime = config_.lifetime;
    toast.remaining = config_.lifetime;
    toast.state = ToastState::Entering;
    toast.stateTimer = 0.0f;
    toast.alpha = 0.0f;
    toast.slideOffset = config_.enterOffset;
    toast.accentOverride = accentOverride;
    toasts_.push_back(std::move(toast));
    pruneToMaxVisible();
}

void ToastManager::pushTalentPointToast(const std::string& text) {
    if (!config_.enabled) return;
    ToastData toast{};
    toast.kind = ToastKind::Talent;
    toast.category = ToastCategory::Talent;
    toast.title = text;
    toast.lifetime = config_.lifetime;
    toast.remaining = config_.lifetime;
    toast.state = ToastState::Entering;
    toast.stateTimer = 0.0f;
    toast.alpha = 0.0f;
    toast.slideOffset = config_.enterOffset;
    toasts_.push_back(std::move(toast));
    pruneToMaxVisible();
}

void ToastManager::addOrRefreshEffectToast(const std::string& key, const std::string& name, float remaining,
                                           float duration, ToastCategory category) {
    if (!config_.enabled) return;
    ToastData* existing = findToastByKey(key);
    if (existing) {
        existing->title = name;
        existing->duration = duration;
        existing->remaining = remaining;
        existing->category = category;
        if (existing->state == ToastState::Exiting) {
            existing->state = ToastState::Active;
            existing->stateTimer = 0.0f;
            existing->alpha = 1.0f;
            existing->slideOffset = 0.0f;
        }
        return;
    }

    ToastData toast{};
    toast.key = key;
    toast.kind = ToastKind::Effect;
    toast.category = category;
    toast.title = name;
    toast.duration = duration;
    toast.remaining = remaining;
    toast.lifetime = duration;
    toast.state = ToastState::Entering;
    toast.stateTimer = 0.0f;
    toast.alpha = 0.0f;
    toast.slideOffset = config_.enterOffset;
    toasts_.push_back(std::move(toast));
    pruneToMaxVisible();
}

void ToastManager::endMissingEffectToasts(const std::unordered_set<std::string>& activeKeys) {
    if (!config_.enabled) return;
    for (auto& toast : toasts_) {
        if (toast.kind != ToastKind::Effect) continue;
        if (activeKeys.find(toast.key) == activeKeys.end()) {
            if (toast.state != ToastState::Exiting) {
                toast.state = ToastState::Exiting;
                toast.stateTimer = 0.0f;
            }
        }
    }
}

void ToastManager::update(float dt) {
    if (!config_.enabled) return;
    lastDt_ = dt;
    for (auto& toast : toasts_) {
        if (toast.state == ToastState::Entering) {
            toast.stateTimer += dt;
            float t = (config_.enterDuration > 0.0f) ? toast.stateTimer / config_.enterDuration : 1.0f;
            float eased = easeOutCubic(t);
            toast.alpha = eased;
            toast.slideOffset = (1.0f - eased) * config_.enterOffset;
            if (t >= 1.0f) {
                toast.state = ToastState::Active;
                toast.stateTimer = 0.0f;
                toast.alpha = 1.0f;
                toast.slideOffset = 0.0f;
            }
        } else if (toast.state == ToastState::Active) {
            if (toast.kind != ToastKind::Effect) {
                toast.age += dt;
                toast.remaining = std::max(0.0f, toast.lifetime - toast.age);
                if (toast.age >= toast.lifetime && toast.lifetime > 0.0f) {
                    toast.state = ToastState::Exiting;
                    toast.stateTimer = 0.0f;
                }
            } else {
                if (toast.remaining <= 0.0f && toast.duration > 0.0f) {
                    toast.state = ToastState::Exiting;
                    toast.stateTimer = 0.0f;
                }
            }
        } else if (toast.state == ToastState::Exiting) {
            toast.stateTimer += dt;
            float t = (config_.exitDuration > 0.0f) ? toast.stateTimer / config_.exitDuration : 1.0f;
            float eased = easeInCubic(t);
            toast.alpha = 1.0f - eased;
            toast.slideOffset = eased * config_.exitOffset;
        }
    }

    toasts_.erase(std::remove_if(toasts_.begin(), toasts_.end(), [&](const ToastData& t) {
                      if (t.state != ToastState::Exiting) return false;
                      return (config_.exitDuration <= 0.0f) || (t.stateTimer >= config_.exitDuration);
                  }),
                  toasts_.end());
}

void ToastManager::render(const ToastRenderContext& ctx) {
    if (!config_.enabled) return;
    if (!ctx.render || !ctx.measureText || !ctx.drawText) return;
    if (toasts_.empty()) return;

    const float s = ctx.uiScale;
    const float paddingX = config_.paddingX * s;
    const float paddingY = config_.paddingY * s;
    const float spacing = config_.spacing * s;
    const float maxWidth = config_.maxWidth * s;
    const float titleScale = config_.titleScale * s;
    const float subtitleScale = config_.subtitleScale * s;
    const float timerScale = config_.timerScale * s;
    const float subtitleGap = config_.subtitleGap * s;
    const float radius = config_.cornerRadius * s;
    const float accentW = config_.accentWidth * s;
    const float shadowOffset = config_.shadowOffset * s;
    const float topBorderH = std::max(1.0f, config_.topBorderHeight * s);
    const float bottomBorderH = std::max(1.0f, config_.bottomBorderHeight * s);
    const float timerGap = config_.timerGap * s;

    const float baseX = ctx.anchorX;
    float cursorY = static_cast<float>(ctx.viewportH) - ctx.anchorY;

    for (auto it = toasts_.rbegin(); it != toasts_.rend(); ++it) {
        ToastData& toast = *it;
        if (toast.alpha <= 0.001f) continue;

        std::string timerText;
        if (toast.kind == ToastKind::Effect && toast.duration > 0.0f) {
            timerText = formatSeconds(toast.remaining);
        }

        float contentMax = std::max(0.0f, maxWidth - paddingX * 2.0f - accentW);
        Engine::Vec2 timerSz = timerText.empty() ? Engine::Vec2{} : ctx.measureText(timerText, timerScale);
        float titleAvail = contentMax;
        if (!timerText.empty()) {
            titleAvail = std::max(0.0f, contentMax - timerSz.x - timerGap);
        }

        std::string title = ellipsize(toast.title, titleAvail, ctx.measureText, titleScale);
        Engine::Vec2 titleSz = ctx.measureText(title, titleScale);
        Engine::Vec2 subtitleSz{};
        std::string subtitle;
        if (!toast.subtitle.empty()) {
            subtitle = ellipsize(toast.subtitle, contentMax, ctx.measureText, subtitleScale);
            subtitleSz = ctx.measureText(subtitle, subtitleScale);
        }

        float lineH = std::max(titleSz.y, timerSz.y);
        float contentW = std::max(titleSz.x + (timerText.empty() ? 0.0f : (timerGap + timerSz.x)), subtitleSz.x);
        float panelW = std::min(maxWidth, contentW + paddingX * 2.0f + accentW);
        float panelH = paddingY * 2.0f + lineH + (subtitle.empty() ? 0.0f : (subtitleGap + subtitleSz.y));

        cursorY -= panelH;
        float targetY = cursorY;
        cursorY -= spacing;

        if (!toast.yInitialized) {
            toast.currentY = targetY;
            toast.yInitialized = true;
        } else if (lastDt_ > 0.0f) {
            float blend = 1.0f - std::exp(-config_.settleSpeed * lastDt_);
            toast.currentY += (targetY - toast.currentY) * blend;
        }

        float drawY = toast.currentY + toast.slideOffset * s;
        float drawX = baseX;

        Engine::Color shadow{0, 0, 0, static_cast<uint8_t>(config_.shadowAlpha)};
        Engine::Color panel = withAlphaOverride(theme_.bg, static_cast<uint8_t>(config_.panelAlpha), toast.alpha);
        Engine::Color outline = withAlphaOverride(theme_.outline, static_cast<uint8_t>(config_.outlineAlpha), toast.alpha);

        if (ctx.sdl) {
            drawFilledRoundedRect(ctx.sdl, drawX, drawY + shadowOffset, panelW, panelH, radius,
                                  withAlpha(shadow, toast.alpha));
            drawFilledRoundedRect(ctx.sdl, drawX, drawY, panelW, panelH, radius, panel);
        } else {
            ctx.render->drawFilledRect(Engine::Vec2{drawX, drawY + shadowOffset}, Engine::Vec2{panelW, panelH},
                                       withAlpha(shadow, toast.alpha));
            ctx.render->drawFilledRect(Engine::Vec2{drawX, drawY}, Engine::Vec2{panelW, panelH}, panel);
        }

        if (panelW > 4.0f) {
            ctx.render->drawFilledRect(Engine::Vec2{drawX, drawY}, Engine::Vec2{panelW, topBorderH}, outline);
            ctx.render->drawFilledRect(Engine::Vec2{drawX, drawY + panelH - bottomBorderH},
                                       Engine::Vec2{panelW, bottomBorderH}, outline);
        }

        Engine::Color accent = theme_.neutral;
        if (toast.category == ToastCategory::Buff) accent = theme_.buff;
        else if (toast.category == ToastCategory::Debuff) accent = theme_.debuff;
        else if (toast.category == ToastCategory::Talent) accent = theme_.talent;
        else if (toast.category == ToastCategory::Pickup) accent = theme_.neutral;
        if (toast.accentOverride.has_value()) accent = *toast.accentOverride;

        ctx.render->drawFilledRect(Engine::Vec2{drawX, drawY}, Engine::Vec2{accentW, panelH}, withAlpha(accent, toast.alpha));

        Engine::Color titleCol = withAlpha(theme_.text, toast.alpha);
        Engine::Color subCol = withAlpha(theme_.subtleText, toast.alpha);

        const float textX = drawX + paddingX + accentW;
        float textY = drawY + paddingY;
        ctx.drawText(title, Engine::Vec2{textX, textY}, titleScale, titleCol);
        if (!timerText.empty()) {
            float timerX = drawX + panelW - paddingX - timerSz.x;
            ctx.drawText(timerText, Engine::Vec2{timerX, textY}, timerScale, subCol);
        }
        if (!subtitle.empty()) {
            float subY = textY + lineH + subtitleGap;
            ctx.drawText(subtitle, Engine::Vec2{textX, subY}, subtitleScale, subCol);
        }
    }
}

const ToastManager::ToastData* ToastManager::findToastByKey(const std::string& key) const {
    for (const auto& toast : toasts_) {
        if (toast.key == key) return &toast;
    }
    return nullptr;
}

ToastManager::ToastData* ToastManager::findToastByKey(const std::string& key) {
    for (auto& toast : toasts_) {
        if (toast.key == key) return &toast;
    }
    return nullptr;
}

void ToastManager::pruneToMaxVisible() {
    if (config_.maxVisible <= 0) return;
    auto countVisible = [&]() {
        int count = 0;
        for (const auto& t : toasts_) {
            if (t.state != ToastState::Exiting) ++count;
        }
        return count;
    };

    int visible = countVisible();
    while (visible > config_.maxVisible) {
        auto it = std::find_if(toasts_.begin(), toasts_.end(), [](const ToastData& t) {
            return t.kind != ToastKind::Effect && t.state != ToastState::Exiting;
        });
        if (it == toasts_.end()) break;
        it->state = ToastState::Exiting;
        it->stateTimer = 0.0f;
        visible = countVisible();
    }
}

}  // namespace Game::UI
