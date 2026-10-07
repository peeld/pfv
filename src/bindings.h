#pragma once

// What shiboken6 sees when it generates the built-in "pfvgui" Python module
// (src/bindings.xml, peel_add_python_bindings() in CMakeLists.txt).

#include "mainwindow.h"

// The app's main window, or nullptr once it's gone. Python doesn't own it.
MainWindow *mainWindow();
