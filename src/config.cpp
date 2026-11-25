#include "config.hpp"

#include <windows.h>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>

namespace to
{
namespace fs = std::filesystem;
namespace
{
const fs::path defaultsPath{L"Data/SKSE/Plugins/ThrowObject.ini"};
const fs::path userPath{L"Data/SKSE/Plugins/ThrowObject.user.ini"};
std::mutex lock;
Settings settings;

void sanitize(Settings& s)
{
    if (!validKeys(s.keys)) s.keys = defaultBindings;
    auto bound = [](float& value,float min,float max,float fallback) {
        value = std::isfinite(value) ? std::clamp(value,min,max) : fallback;
    };
    bound(s.pickupDistance,45,240,135);
    bound(s.metresPerSecond,4,22,13);
    bound(s.force,0.5f,1.5f,1);
    bound(s.dragScale,0,3,1);
    bound(s.aimDistance,350,9000,3500);
    for (int i=0; i<3; ++i) {
        bound(s.xyz[i],-18,18,Settings{}.xyz[i]);
        bound(s.angles[i],-180,180,0);
    }
    bound(s.handSize,0.6f,1,1);
    bound(s.damageScale,0,4,1);
    bound(s.maxDamage,0,100,25);
    bound(s.reactDistance,20,230,95);
    bound(s.reactCooldown,2,60,9);
    s.bounty = std::clamp(s.bounty,0,1000);
    const auto range = toleranceRange(s.toleranceMin,s.toleranceMax);
    s.toleranceMin = range.minimum;
    s.toleranceMax = range.maximum;
    s.toleranceMeterCorner = std::clamp(s.toleranceMeterCorner,0,toleranceMeterCornerCount-1);
}

template<class Stream, class S, class Field>
void fields(Stream& stream,S& s,Field field)
{
    field(stream,"pickup",s.keys[0]); field(stream,"drop",s.keys[1]); field(stream,"throw",s.keys[2]);
    field(stream,"enabled",s.enabled); field(stream,"debug",s.log);
    field(stream,"trajectory_debug",s.trajectoryDebug); field(stream,"damage",s.damageEnabled);
    field(stream,"reactions",s.reactions); field(stream,"crime",s.crime);
    field(stream,"pickup_distance",s.pickupDistance); field(stream,"speed_m_s",s.metresPerSecond);
    field(stream,"force_multiplier",s.force); field(stream,"drag_scale",s.dragScale);
    field(stream,"aim_distance",s.aimDistance);
    field(stream,"hand_x",s.xyz[0]); field(stream,"hand_y",s.xyz[1]); field(stream,"hand_z",s.xyz[2]);
    field(stream,"hand_pitch",s.angles[0]); field(stream,"hand_roll",s.angles[1]); field(stream,"hand_yaw",s.angles[2]);
    field(stream,"hand_visual_scale",s.handSize); field(stream,"damage_multiplier",s.damageScale);
    field(stream,"damage_cap",s.maxDamage); field(stream,"reaction_distance",s.reactDistance);
    field(stream,"reaction_cooldown",s.reactCooldown); field(stream,"bounty",s.bounty);
    field(stream,"reaction_topic_form_id",s.reactionTopic);
    field(stream,"tolerance_min",s.toleranceMin);
    field(stream,"tolerance_max",s.toleranceMax);
    field(stream,"tolerance_meter",s.toleranceMeter);
    field(stream,"tolerance_meter_corner",s.toleranceMeterCorner);
}
}

Settings getSettings()
{
    const std::scoped_lock guard(lock);
    return settings;
}

void loadSettings()
{
    Settings next;
    for (const auto& path : {defaultsPath,userPath}) {
        std::ifstream input(path);
        for (std::string line; std::getline(input,line);) {
            const auto split = line.find('=');
            if (split == std::string::npos) continue;
            const auto key = line.substr(0,split);
            std::istringstream value(line.substr(split+1));
            fields(value,next,[&key](auto& in,const char* name,auto& destination) {
                if (key != name) return;
                auto parsed = destination;
                if (in >> parsed) destination = parsed;
            });
        }
    }
    sanitize(next);
    const std::scoped_lock guard(lock);
    settings = next;
    SKSE::log::info("Settings loaded: Pick Up {}, Drop {}, Throw {}",
                    bindingName(settings.keys[0]),bindingName(settings.keys[1]),bindingName(settings.keys[2]));
    SKSE::log::info("Tolerance meter starts {} (corner {})",
                    settings.toleranceMeter ? "enabled" : "disabled",settings.toleranceMeterCorner);
}

namespace
{
bool saveLocked(Settings next)
{
    if (!validKeys(next.keys)) return false;
    sanitize(next);
    auto temporary = userPath;
    temporary += L".tmp";
    {
        std::ofstream out(temporary,std::ios::trunc);
        fields(out,next,[](auto& file,const char* name,const auto& value) { file<<name<<'='<<value<<'\n'; });
        out.close();
        if (!out) {
            SKSE::log::error("Could not write ThrowObject.user.ini temporary file");
            return false;
        }
    }
    if (!MoveFileExW(temporary.c_str(),userPath.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) {
        SKSE::log::error("Could not replace ThrowObject.user.ini (Windows error {})",GetLastError());
        return false;
    }
    settings = next;
    return true;
}
}

bool saveSettings(Settings next)
{
    const std::scoped_lock guard(lock);
    return saveLocked(next);
}

bool saveBindings(const std::array<int, 3>& keys)
{
    const std::scoped_lock guard(lock);
    auto next = settings;
    next.keys = keys;
    return saveLocked(next);
}

bool saveToleranceMeterSettings(bool enabled,int corner)
{
    const std::scoped_lock guard(lock);
    auto next = settings;
    next.toleranceMeter = enabled;
    next.toleranceMeterCorner = std::clamp(corner,0,toleranceMeterCornerCount-1);
    if (saveLocked(next)) return true;
    settings.toleranceMeter = next.toleranceMeter;
    settings.toleranceMeterCorner = next.toleranceMeterCorner;
    return false;
}

int buttonCode(const RE::ButtonEvent& event)
{
    const auto device = event.GetDevice();
    if (device==RE::INPUT_DEVICE::kKeyboard) return bindingCode(Device::keyboard,event.GetIDCode());
    if (device==RE::INPUT_DEVICE::kMouse) return bindingCode(Device::mouse,event.GetIDCode());
    return unbound;
}

std::string bindingName(int key)
{
    if (!key) return "Unbound";
    if (key>=mouseBase) {
        constexpr const char* mouseNames[] = {"Left Mouse Button","Right Mouse Button","Middle Mouse Button",
                                              "Mouse Button 4","Mouse Button 5","Mouse Button 6",
                                              "Mouse Button 7","Mouse Button 8"};
        if (key<bindingLimit) return mouseNames[key-mouseBase];
    }
    constexpr std::array<int,10> keypad{82,79,80,81,75,76,77,71,72,73};
    for (std::size_t i=0;i<keypad.size();++i)
        if (key==keypad[i]) return "Numpad "+std::to_string(i);
    auto name = SKSE::InputMap::GetKeyName(static_cast<std::uint32_t>(key));
    return name.empty() ? "Unknown keyboard key" : name;
}
}
