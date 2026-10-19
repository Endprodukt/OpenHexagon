// The levels' words for the controls: see hints.hpp.
#include "hints.hpp"

#include "vpbind.h"

#include <cctype>

#include "SSVOpenHexagon/Utils/LuaWrapper.hpp"

#include <SSVUtils/Core/Log/Log.hpp>

#include <fstream>
#include <sstream>

namespace vp {

namespace {

// What a function's controls are, the first of them, as a PC player knows
// them: "left ctrl".
std::string pcKey(const std::string& controls)
{
    char b[VPB_LENGTH * 2];
    std::string r = vpb_spelled(controls.substr(0, controls.find('+')).c_str(), b, sizeof b);
    for(char& c : r) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return r;
}

// What a control is called: on the cabinet by what it does, on a PC by its
// key.
std::string name(Button b, const Settings& s)
{
    if(!s.cabinet) return pcKey(s.button[b]);
    switch(b)
    {
        case BUTTON_FOCUS: return "the focus button";
        case BUTTON_SWAP: return "the swap button";
        case BUTTON_BACK: return "the back button";
        case BUTTON_LEFT: return "left";
        case BUTTON_RIGHT: return "right";
        default: return "";
    }
}

} // namespace

std::optional<std::string> controlText(const std::string& t, const Settings& s)
{
    // --- tutorial/babysteps
    if(t == "use left/right to rotate around the center")
    {
        if(s.cabinet) return "turn the spinner or use left/right\nto rotate around the center";
        return "move the mouse or use " + name(BUTTON_LEFT, s) + "/" + name(BUTTON_RIGHT, s) +
               "\nto rotate around the center";
    }
    if(t == "by default, use left shift")
    {
        return "hold " + name(BUTTON_FOCUS, s);
    }
    if(t == "focused (left shift)")
    {
        return "focused (" + name(BUTTON_FOCUS, s) + ")";
    }

    // --- hypercube/evotutorial
    if(t == "Try swapping now!\nPress space or middle mouse button to swap")
    {
        return "Try swapping now!\nPress " + name(BUTTON_SWAP, s) + " to swap";
    }

    // --- hypercube/slither
    if(t == "remember, you can focus with lshift!")
    {
        return "remember, you can focus with " + name(BUTTON_FOCUS, s) + "!";
    }

    // --- the hints after a death (HexagonGame::newGame)
    // Arcade: the next life starts by itself, there is nothing to press.
    if(s.arcade && (t == "@restart" || t == "@replay"))
    {
        return "";
    }
    if(t == "@restart")
    {
        return s.cabinet ? "PRESS START TO RESTART" : "PRESS 1 OR " + name(BUTTON_FOCUS, s) + " TO RESTART";
    }
    if(t == "@replay")
    {
        return s.cabinet ? "PRESS SWAP TO REPLAY" : "PRESS " + name(BUTTON_SWAP, s) + " TO REPLAY";
    }

    return std::nullopt;
}

std::string swapHint(const Settings& s)
{
    if(s.cabinet) return "USE THE SWAP BUTTON TO MASTER THIS LEVEL";
    std::string k = name(BUTTON_SWAP, s);
    for(char& c : k) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return "USE " + k + " (SWAP) TO MASTER THIS LEVEL";
}

void runLevelOverride(Lua::LuaContext& lua, const std::string& levelId, const Settings& s)
{
    std::ifstream in("VectorPie/" + levelId + ".lua");
    if(!in)
    {
        return;
    }

    std::stringstream code;
    code << in.rdbuf();

    try
    {
        lua.writeVariable("vp_cabinet", s.cabinet);
        lua.executeCode(code.str());
    }
    catch(const std::exception& e)
    {
        ssvu::lo("vp::runLevelOverride") << levelId << ": " << e.what() << '\n';
    }
}

} // namespace vp
