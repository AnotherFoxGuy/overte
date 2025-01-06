
set(qt_static_lib_dependices
  "qtpcre2"
  "qtlibpng"
  "qtfreetype"
  "Qt6AccessibilitySupport"
  "Qt6FbSupport"
  "Qt6OpenGLExtensions"
  "Qt6QuickTemplates2"
  "Qt6FontDatabaseSupport"
  "Qt6ThemeSupport"
  "Qt6EventDispatcherSupport")

if (WIN32)
elseif(APPLE)
  set(qt_static_lib_dependices
    ${qt_static_lib_dependices}
    "Qt6GraphicsSupport"
    "Qt6CglSupport"
    "Qt6ClipboardSupport")
endif()

set(LIBS_PREFIX "${_Qt6Core_install_prefix}/lib/")
foreach (_qt_static_dep ${qt_static_lib_dependices})
  if (WIN32)
    set(lib_path "${LIBS_PREFIX}${_qt_static_dep}.lib")
  else()
    set(lib_path "${LIBS_PREFIX}lib${_qt_static_dep}.a")
  endif()
  set(QT_STATIC_LIBS ${QT_STATIC_LIBS} ${lib_path})
endforeach()

unset(qt_static_lib_dependices)
