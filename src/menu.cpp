#include "config.hpp"

#include <SKSEMenuFramework.h>
#include <mutex>

namespace to
{
namespace
{
std::mutex captureLock;
KeyCapture capture;
bool cancelHovered{};
bool menuInputAvailable{};
bool meterHudAvailable{};
constexpr std::array names{"Pick Up","Drop","Throw"};
constexpr std::array meterCorners{"Top left","Top right","Bottom left","Bottom right"};
static_assert(meterCorners.size()==toleranceMeterCornerCount);

bool __stdcall captureEvent(RE::InputEvent* event)
{
    auto* button = event ? event->AsButtonEvent() : nullptr;
    if (!button) return false;
    const auto key = buttonCode(*button);
    const std::scoped_lock guard(captureLock);
    if (!menuBlocksInput()) capture.cancel();
    return capture.input(key,button->IsDown(),button->IsUp(),key==mouseBase && cancelHovered);
}

void __stdcall menuEvent(SKSEMenuFramework::Model::EventType event)
{
    if (event != SKSEMenuFramework::Model::EventType::kCloseMenu) return;
    const std::scoped_lock guard(captureLock);
    capture.cancel();
    cancelHovered = false;
}

void showBindings()
{
    static std::string notice;
    std::optional<KeyCapture::Result> result;
    bool listening;
    {
        const std::scoped_lock guard(captureLock);
        result = capture.take();
        listening = capture.isListening();
        if (!listening) cancelHovered = false;
    }
    auto keys = getSettings().keys;
    auto apply = [&](const std::array<int,3>& next) {
        if (saveBindings(next)) {
            keys = next;
            notice = "Controls saved.";
            SKSE::log::info("Controls saved: Pick Up {}, Drop {}, Throw {}",
                            bindingName(keys[0]),bindingName(keys[1]),bindingName(keys[2]));
        } else {
            notice = "Could not save controls. See ThrowObject.log.";
        }
    };
    if (result) {
        auto next = keys;
        next[static_cast<std::size_t>(result->action)] = result->key;
        if (auto existing = matchingAction(keys,result->key); existing && *existing!=result->action) {
            notice = bindingName(result->key)+" is already assigned to "+names[static_cast<std::size_t>(*existing)]+".";
        } else apply(next);
    }

    ImGuiMCP::SeparatorText("Controls");
    if (!menuInputAvailable) {
        ImGuiMCP::Text("This Menu Framework build lacks the input capture API.");
        return;
    }
    if (listening) {
        ImGuiMCP::Text("Press any key or mouse button...");
        const bool cancel = ImGuiMCP::Button("Cancel");
        const bool hovered = ImGuiMCP::IsItemHovered();
        const std::scoped_lock guard(captureLock);
        cancelHovered = hovered;
        if (cancel) capture.cancel();
    } else {
        for (std::size_t i=0;i<names.size();++i) {
            ImGuiMCP::PushID(static_cast<int>(i));
            ImGuiMCP::Text("%s: %s",names[i],bindingName(keys[i]).c_str());
            ImGuiMCP::SameLine();
            if (ImGuiMCP::Button("Rebind")) {
                const std::scoped_lock guard(captureLock);
                capture.listen(static_cast<Action>(i));
                notice.clear();
            }
            ImGuiMCP::SameLine();
            ImGuiMCP::BeginDisabled(keys[i]==unbound);
            if (ImGuiMCP::Button("Clear Binding")) {
                auto next = keys;
                next[i] = unbound;
                apply(next);
            }
            ImGuiMCP::EndDisabled();
            ImGuiMCP::PopID();
        }
        if (ImGuiMCP::Button("Reset to Defaults")) apply(defaultBindings);
    }
    if (!notice.empty()) ImGuiMCP::Text("%s",notice.c_str());
}

void __stdcall renderToleranceMeter()
{
    static bool loggedCallback{};
    if (!loggedCallback) {
        loggedCallback = true;
        SKSE::log::info("Tolerance meter HUD callback is running");
    }
    const auto meter = readToleranceMeter();
    if (!meter || menuBlocksInput()) return;
    static const auto getViewport = GetMenuFrameworkFunction<ImGuiMCP::ImGuiViewport*(*)()>("igGetMainViewport");
    std::optional<ToleranceMeterPlacement> position;
    if (const auto* viewport = getViewport ? getViewport() : nullptr)
        position = placeToleranceMeter(meter->corner,viewport->Pos.x,viewport->Pos.y,
                                       viewport->Size.x,viewport->Size.y);
    if (!position) {
        const auto screen = RE::BSGraphics::Renderer::GetScreenSize();
        position = placeToleranceMeter(meter->corner,0.f,0.f,float(screen.width),float(screen.height));
    }
    if (!position) return;

    using namespace ImGuiMCP;
    SetNextWindowPos({position->x,position->y},ImGuiCond_Always,{position->pivotX,position->pivotY});
    static const auto setBackgroundAlpha = GetMenuFrameworkFunction<void(*)(float)>("igSetNextWindowBgAlpha");
    if (setBackgroundAlpha) setBackgroundAlpha(.62f);
    constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoDocking;
    if (Begin("##ThrowObjectToleranceMeter",nullptr,flags)) {
        TextUnformatted(meter->text.c_str());
        static bool loggedDraw{};
        if (!loggedDraw) {
            loggedDraw = true;
            SKSE::log::info("Tolerance meter drew its first target");
        }
    }
    End();
}

void __stdcall render()
{
    static Settings draft = getSettings();
    static const char* saved = "";
    ImGuiMCP::Text("Throw Object 0.3.9 RC3");
    showBindings();
    ImGuiMCP::SeparatorText("Throwing");
    ImGuiMCP::SliderFloat("Base throw speed (m/s)",&draft.metresPerSecond,4.0f,22.0f);
    ImGuiMCP::SliderFloat("Universal force multiplier",&draft.force,0.5f,1.5f);
    ImGuiMCP::SliderFloat("Weight and shape drag",&draft.dragScale,0.0f,3.0f);
    ImGuiMCP::SliderFloat("Aim ray distance (units)",&draft.aimDistance,350.0f,9000.0f);
    ImGuiMCP::Checkbox("Log trajectory",&draft.trajectoryDebug);
    ImGuiMCP::SeparatorText("Object handling");
    ImGuiMCP::SliderFloat("Pickup reach (units)",&draft.pickupDistance,45.0f,240.0f);
    ImGuiMCP::InputFloat3("Hand offset XYZ",draft.xyz);
    ImGuiMCP::InputFloat3("Hand rotation (degrees)",draft.angles);
    ImGuiMCP::SliderFloat("Maximum visual scale",&draft.handSize,0.6f,1.0f);
    if (ImGuiMCP::Button("Reset hand offsets")) {
        auto defaults = Settings{};
        std::copy_n(defaults.xyz,3,draft.xyz);
        std::copy_n(defaults.angles,3,draft.angles);
        draft.handSize = defaults.handSize;
    }
    ImGuiMCP::SeparatorText("Damage and crime");
    ImGuiMCP::Checkbox("Impact damage",&draft.damageEnabled);
    ImGuiMCP::Checkbox("Melee damage for thrown one-handed weapons (50% of normal swing)",&draft.weaponMeleeDamage);
    ImGuiMCP::SliderFloat("Damage multiplier",&draft.damageScale,0.0f,4.0f);
    ImGuiMCP::SliderFloat("Damage cap",&draft.maxDamage,0.0f,100.0f);
    ImGuiMCP::Checkbox("Custom assault bounty",&draft.crime);
    ImGuiMCP::SliderInt("Bounty per assault",&draft.bounty,0,1000);
    ImGuiMCP::SeparatorText("NPC tolerance");
    ImGuiMCP::SliderInt("Minimum daily tolerance",&draft.toleranceMin,1,toleranceMinLimit);
    draft.toleranceMax = toleranceRange(draft.toleranceMin,draft.toleranceMax).maximum;
    ImGuiMCP::SliderInt("Maximum daily tolerance",&draft.toleranceMax,draft.toleranceMin,toleranceMaxLimit);
    ImGuiMCP::Text("New rolls use these limits. Existing rolls reset at 6 AM in game.");
    if (meterHudAvailable) {
        const bool toggled = ImGuiMCP::Checkbox("Show tolerance meter",&draft.toleranceMeter);
        bool moved = false;
        static const bool hasCombo = GetMenuFrameworkFunction<void*>("igCombo_Str_arr") != nullptr;
        if (hasCombo) {
            moved = ImGuiMCP::Combo("Tolerance meter corner",&draft.toleranceMeterCorner,meterCorners.data(),
                                   toleranceMeterCornerCount);
        } else {
            ImGuiMCP::Text("Tolerance meter corner: %s",meterCorners[draft.toleranceMeterCorner]);
            ImGuiMCP::SameLine();
            if ((moved = ImGuiMCP::Button("Change corner")))
                draft.toleranceMeterCorner = (draft.toleranceMeterCorner+1)%toleranceMeterCornerCount;
        }
        if (toggled || moved)
            saved = saveToleranceMeterSettings(draft.toleranceMeter,draft.toleranceMeterCorner) ?
                "Tolerance meter saved." : "Tolerance meter changed for this session. Settings could not be saved.";
    } else {
        ImGuiMCP::Text("Tolerance meter unavailable. See ThrowObject.log.");
    }
    ImGuiMCP::SeparatorText("NPC reactions");
    ImGuiMCP::Checkbox("Verbal reactions",&draft.reactions);
    ImGuiMCP::SliderFloat("Reaction distance",&draft.reactDistance,20.0f,230.0f);
    ImGuiMCP::SliderFloat("Reaction cooldown (s)",&draft.reactCooldown,2.0f,60.0f);
    if (ImGuiMCP::SliderFloat("Minimum investigation kneel (s)",&draft.investigationKneel.minimum,1.f,60.f,"%.1f") &&
        draft.investigationKneel.maximum<draft.investigationKneel.minimum)
        draft.investigationKneel.maximum = draft.investigationKneel.minimum;
    ImGuiMCP::SliderFloat("Maximum investigation kneel (s)",&draft.investigationKneel.maximum,
                         draft.investigationKneel.minimum,60.f,"%.1f");
    ImGuiMCP::SeparatorText("General");
    ImGuiMCP::Checkbox("Enabled",&draft.enabled);
    ImGuiMCP::Checkbox("Debug logging",&draft.log);
    if (ImGuiMCP::Button("Reset gameplay settings")) draft = Settings{};
    if (ImGuiMCP::Button("Apply and save gameplay settings")) {
        draft.keys = getSettings().keys;
        saved = saveSettings(draft) ? "Saved." : "Could not write ThrowObject.ini.";
    }
    ImGuiMCP::Text("%s",saved);
}
}
void registerMenu()
{
    if (!SKSEMenuFramework::IsInstalled() ||
        !GetMenuFrameworkFunction<SKSEMenuFramework::Model::AddSectionItemFunction>("AddSectionItem") ||
        !GetMenuFrameworkFunction<void*>("igSeparatorText")) {
        SKSE::log::warn("Menu Framework 3 UI unavailable; using ThrowObject.ini");
        return;
    }
    // The v3 public export intentionally spells "Input" as "Inpout".
    if (auto registerInput = GetMenuFrameworkFunction<SKSEMenuFramework::Model::RegisterInputEventFuction>(
            "RegisterInpoutEvent")) {
        menuInputAvailable = registerInput(captureEvent) >= 0;
        if (auto registerEvent = GetMenuFrameworkFunction<SKSEMenuFramework::Model::RegisterEventFuction>(
                "RegisterEventPriority")) registerEvent(menuEvent,0.0f);
    }
    if (auto registerHud = GetMenuFrameworkFunction<SKSEMenuFramework::Model::RegisterHudElementFuction>(
            "RegisterHudElement");
        registerHud && GetMenuFrameworkFunction<void*>("igSetNextWindowPos") &&
        GetMenuFrameworkFunction<void*>("igBegin") && GetMenuFrameworkFunction<void*>("igEnd") &&
        GetMenuFrameworkFunction<void*>("igTextUnformatted")) {
        meterHudAvailable = registerHud(renderToleranceMeter)>=0;
    }
    SKSE::log::info("Menu Framework registered, button capture {}, tolerance HUD {}",
                    menuInputAvailable ? "available" : "unavailable",
                    meterHudAvailable ? "available" : "unavailable");
    SKSEMenuFramework::SetSection("Throw Object");
    SKSEMenuFramework::AddSectionItem("Settings",render);
}

bool toleranceMeterAvailable() { return meterHudAvailable; }

bool menuBlocksInput()
{
    if (auto* window = SKSEMenuFramework::GetMainWindow(); window && window->IsOpen.load())
        return true;
    return SKSEMenuFramework::IsAnyBlockingWindowOpened();
}
}
