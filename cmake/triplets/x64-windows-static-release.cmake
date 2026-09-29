# Bibliothèques statiques (tout dans ToolBox.exe), runtime C++ statique (/MT),
# et seulement la version Release : la compilation de Tesseract est deux fois plus rapide.
set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE static)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_BUILD_TYPE release)
