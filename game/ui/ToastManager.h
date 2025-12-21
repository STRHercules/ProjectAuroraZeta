#pragma once

#include <SDL.h>

#include <functional>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

#include "../../engine/math/Vec2.h"
#include "../../engine/render/Color.h"
#include "../../engine/render/RenderDevice.h"

namespace Game::UI {

enum class ToastCategory {
    Buff,
    Debuff,
    Pickup,
    Talent,
    Neutral
};

struct ToastConfig {
    bool enabled{true};
    float marginLeft{24.0f};
    float marginBottom{24.0f};
    float maxWidth{380.0f};
    float paddingX{14.0f};
    float paddingY{10.0f};
    float spacing{10.0f};
    float enterDuration{0.22f};
    float exitDuration{0.22f};
    float enterOffset{28.0f};
    float exitOffset{12.0f};
    float settleSpeed{16.0f};
    float lifetime{2.8f};
    int maxVisible{7};
    float titleScale{0.95f};
    float subtitleScale{0.82f};
    float timerScale{0.82f};
    float subtitleGap{4.0f};
    float cornerRadius{12.0f};
    float accentWidth{5.0f};
    float shadowOffset{4.0f};
    float shadowAlpha{90.0f};
    float panelAlpha{215.0f};
    float outlineAlpha{150.0f};
    float topBorderHeight{2.0f};
    float bottomBorderHeight{2.0f};
    float timerGap{8.0f};
};

struct ToastTheme {
    Engine::Color bg{12, 16, 24, 220};
    Engine::Color outline{45, 70, 95, 190};
    Engine::Color text{220, 235, 250, 235};
    Engine::Color subtleText{180, 200, 220, 220};
    Engine::Color buff{120, 220, 140, 240};
    Engine::Color debuff{240, 120, 120, 240};
    Engine::Color talent{120, 235, 255, 240};
    Engine::Color neutral{235, 240, 245, 235};
};

struct ToastRenderContext {
    Engine::RenderDevice* render{nullptr};
    SDL_Renderer* sdl{nullptr};
    std::function<Engine::Vec2(const std::string&, float)> measureText;
    std::function<void(const std::string&, const Engine::Vec2&, float, Engine::Color)> drawText;
    int viewportW{0};
    int viewportH{0};
    float uiScale{1.0f};
    float anchorX{24.0f};
    float anchorY{24.0f};
};

class ToastManager {
public:
    void configure(const ToastConfig& config, const ToastTheme& theme);
    void clear();

    void pushPickupToast(const std::string& text, std::optional<Engine::Color> accentOverride = std::nullopt);
    void pushTalentPointToast(const std::string& text);
    void addOrRefreshEffectToast(const std::string& key, const std::string& name, float remaining, float duration,
                                 ToastCategory category);
    void endMissingEffectToasts(const std::unordered_set<std::string>& activeKeys);

    void update(float dt);
    void render(const ToastRenderContext& ctx);

private:
    enum class ToastKind { Pickup, Effect, Talent, Neutral };
    enum class ToastState { Entering, Active, Exiting };

    struct ToastData {
        std::string key;
        ToastKind kind{ToastKind::Neutral};
        ToastCategory category{ToastCategory::Neutral};
        std::string title;
        std::string subtitle;
        float lifetime{0.0f};
        float remaining{0.0f};
        float duration{0.0f};
        float age{0.0f};
        float stateTimer{0.0f};
        float alpha{1.0f};
        float slideOffset{0.0f};
        float currentY{0.0f};
        bool yInitialized{false};
        ToastState state{ToastState::Entering};
        std::optional<Engine::Color> accentOverride;
    };

    const ToastData* findToastByKey(const std::string& key) const;
    ToastData* findToastByKey(const std::string& key);
    void pruneToMaxVisible();

    ToastConfig config_{};
    ToastTheme theme_{};
    std::vector<ToastData> toasts_{};
    float lastDt_{0.0f};
};

}  // namespace Game::UI
