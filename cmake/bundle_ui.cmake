# Fusionne ui/index.html + style.css + app.js en un seul fichier HTML.
# Utilisation : cmake -DUI_DIR=... -DOUT=... -P bundle_ui.cmake
file(READ ${UI_DIR}/index.html html)
file(READ ${UI_DIR}/fonts.css fonts)
file(READ ${UI_DIR}/style.css css)
file(READ ${UI_DIR}/app.js js)

string(REPLACE "<link rel=\"stylesheet\" href=\"fonts.css\">" "<style>\n${fonts}\n</style>" html "${html}")
string(REPLACE "<link rel=\"stylesheet\" href=\"style.css\">" "<style>\n${css}\n</style>" html "${html}")
string(REPLACE "<script src=\"app.js\"></script>" "<script>\n${js}\n</script>" html "${html}")

file(WRITE ${OUT} "${html}")
