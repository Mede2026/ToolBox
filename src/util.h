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

// Nom lisible d'un programme (description du fichier .exe, ex. « Google Chrome »), sinon le nom du fichier.
std::string FileDescription(const std::wstring& exe_path);

// Chemin complet de l'exécutable d'un processus (vide si inaccessible).
std::wstring ProcessPath(DWORD pid);

// Ajoute une ligne à %APPDATA%\ToolBox\crash.log (erreur non fatale).
void LogError(const std::string& what);

// Met du texte dans le presse-papiers.
bool SetClipboardText(HWND owner, const std::wstring& text);

// Met une fenêtre au premier plan même si ToolBox n'y est pas (ouverte par raccourci clavier).
void ForceForeground(HWND hwnd);

// Prénom de l'utilisateur Windows (ex. « Médéric »), pour l'accueil.
std::wstring UserFirstName();

}  // namespace util
