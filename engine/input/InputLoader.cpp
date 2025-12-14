#include "InputLoader.h"

#include <fstream>
#include <nlohmann/json.hpp>

namespace Engine {

namespace {
std::vector<std::string> readStrings(const nlohmann::json& arr) {
    std::vector<std::string> out;
    if (!arr.is_array()) return out;
    for (const auto& v : arr) {
        if (v.is_string()) out.push_back(v.get<std::string>());
    }
    return out;
}

void readAxis(const nlohmann::json& src, AxisBinding& dst) {
    if (src.contains("positiveX")) dst.positiveX = readStrings(src["positiveX"]);
    if (src.contains("negativeX")) dst.negativeX = readStrings(src["negativeX"]);
    if (src.contains("positiveY")) dst.positiveY = readStrings(src["positiveY"]);
    if (src.contains("negativeY")) dst.negativeY = readStrings(src["negativeY"]);
}
}  // namespace

std::optional<InputBindings> InputLoader::loadFromFile(const std::string& path) {
    std::ifstream in(path);
    if (!in.is_open()) {
        return std::nullopt;
    }

    nlohmann::json j;
    try {
        in >> j;
    } catch (...) {
        return std::nullopt;
    }

    InputBindings bindings;
    if (j.contains("move")) {
        readAxis(j["move"], bindings.move);
    }
    if (j.contains("camera")) {
        readAxis(j["camera"], bindings.camera);
    }
    if (j.contains("toggleFollow")) {
        bindings.toggleFollow = readStrings(j["toggleFollow"]);
    }
    if (j.contains("restart")) {
        bindings.restart = readStrings(j["restart"]);
    }
    if (j.contains("toggleShop")) {
        bindings.toggleShop = readStrings(j["toggleShop"]);
    }
    if (j.contains("characterScreen")) {
        bindings.characterScreen = readStrings(j["characterScreen"]);
    }
    if (j.contains("pause")) {
        bindings.pause = readStrings(j["pause"]);
    }
    if (j.contains("dash")) {
        bindings.dash = readStrings(j["dash"]);
    }
    if (j.contains("interact")) {
        bindings.interact = readStrings(j["interact"]);
    }
    if (j.contains("useItem")) {
        bindings.useItem = readStrings(j["useItem"]);
    }
    if (j.contains("ability1")) {
        bindings.ability1 = readStrings(j["ability1"]);
    }
    if (j.contains("ability2")) {
        bindings.ability2 = readStrings(j["ability2"]);
    }
    if (j.contains("ability3")) {
        bindings.ability3 = readStrings(j["ability3"]);
    }
    if (j.contains("ultimate")) {
        bindings.ultimate = readStrings(j["ultimate"]);
    }
    if (j.contains("reload")) {
        bindings.reload = readStrings(j["reload"]);
    }
    if (j.contains("menuBack")) {
        bindings.menuBack = readStrings(j["menuBack"]);
    }
    if (j.contains("buildMenu")) {
        bindings.buildMenu = readStrings(j["buildMenu"]);
    }

    return bindings;
}

}  // namespace Engine
