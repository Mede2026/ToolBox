#pragma once

#include <windows.h>

#include <filesystem>
#include <string>

namespace util {

std::string ToUtf8(const std::wstring& w);
std::wstring FromUtf8(const std::string& s);

// Chemin complet de ToolBox.exe.
std::filesystem::path ExePath();

// %APPDATA%\ToolBox (créé au besoin).
std::filesystem::path DataDir();

// Nom de l'exécutable de la fenêtre au premier plan, en minuscules (ex. "discord.exe").
std::wstring ForegroundProcessName();

std::wstring ToLower(std::wstring s);

// Prénom de l'utilisateur Windows (ex. « Médéric »), pour l'accueil.
std::wstring UserFirstName();

}  // namespace util
