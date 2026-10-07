cmake_minimum_required(VERSION 3.25)

project(PFV LANGUAGES CXX)

# Configure through the presets that `python build.py presets` generates
# (CMakeUserPresets.json); they supply Qt's location and the PEEL_* cache vars.
include(${CMAKE_CURRENT_SOURCE_DIR}/core/cmake/PeelApp.cmake)

find_package(Qt6 REQUIRED COMPONENTS Widgets)
qt_standard_project_setup()

# The GUI is C++; the PFV library (pfv-public/) runs in the embedded Python
# and is called through pfv-public/pfv_app.py (src/pfvBackend.*).
# Dev-only files in pfv-public stay out of the staged app.
peel_add_app(PFV PYTHON PYTHON_SCRIPTS "${CMAKE_CURRENT_SOURCE_DIR}/pfv-public"
             PYTHON_SCRIPTS_EXCLUDE docs demo_setup.py localtest.py
             SENTRY LICENSE SCRIPT_EDITOR SOURCES
    src/main.cpp
    src/appPython.h
    src/appPython.cpp
    src/pfvBackend.h
    src/pfvBackend.cpp
    src/fileTreeModel.h
    src/fileTreeModel.cpp
    src/dialogs.h
    src/dialogs.cpp
    src/mainwindow.h
    src/mainwindow.cpp
    src/bindings.h
)
target_link_libraries(PFV PRIVATE Qt6::Widgets)

# The built-in "pfvgui" Python module (skipped when the Qt kit has no Python).
peel_add_python_bindings(PFV
    MODULE pfvgui
    TYPESYSTEM src/bindings.xml
    HEADER src/bindings.h
    CLASSES MainWindow
    DEPENDS src/mainwindow.h
)
