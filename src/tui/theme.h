#pragma once
#include "ftxui/screen/color.hpp"
#include "ftxui/dom/elements.hpp"

namespace Theme {
    using namespace ftxui;

    // Colors
    inline const Color BgDark = Color::RGB(13, 17, 23);
    inline const Color BorderMuted = Color::RGB(48, 54, 61);
    inline const Color BorderActive = Color::RGB(88, 166, 255);
    inline const Color TextWhite = Color::RGB(201, 209, 217);
    inline const Color TextDim = Color::RGB(139, 148, 158);
    
    inline const Color AccentGPU = Color::RGB(63, 185, 80);    // Green
    inline const Color AccentCPU = Color::RGB(88, 166, 255);   // Blue
    inline const Color AccentWarning = Color::RGB(210, 153, 34); // Amber
    inline const Color AccentError = Color::RGB(248, 81, 73);   // Coral Red

    // NerdFont Symbols / Glyphs
    inline const std::string TreeExpanded = "▼ ";
    inline const std::string TreeCollapsed = "▶ ";
    inline const std::string Bullet = "● ";
    inline const std::string CaptureTarget = "► ";
    inline const std::string AnomalyWarn = "⚠ ";
    inline const std::string AnomalyErr = "✖ ";
    inline const std::string AnomalyInfo = "ℹ ";

    // Decorators
    inline Decorator StyledBorder(bool focused) {
        return borderStyled(focused ? LIGHT : ROUNDED) | color(focused ? BorderActive : BorderMuted);
    }
}
