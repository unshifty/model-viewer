TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle
CONFIG -= qt

# Some useful debug/release variables
win32:CONFIG(debug, debug|release) {
    BUILD_TYPE = debug
}
else:win32:CONFIG(release, debug|release) {
    BUILD_TYPE = release
}
3RD_PARTY_PATH = $$PWD/../../3rdParty
DESTDIR = $$OUT_PWD/../$$BUILD_TYPE

INCLUDEPATH = \
$$3RD_PARTY_PATH/sdl_2.0.5/source/include \
$$3RD_PARTY_PATH/sdl2_ttf_2.0.14/include \
$$3RD_PARTY_PATH/glew_2.0.0/include \
$$3RD_PARTY_PATH/glm_0.9.8.3 \
$$3RD_PARTY_PATH/imgui \
$$3RD_PARTY_PATH/assimp_3.3.1/include

LIBS = \
opengl32.lib \
$$3RD_PARTY_PATH/sdl_2.0.5/sdl2.lib \
$$3RD_PARTY_PATH/sdl2_ttf_2.0.14/lib/x86/sdl2_ttf.lib \
$$3RD_PARTY_PATH/sdl_2.0.5/sdl2Main.lib \
$$3RD_PARTY_PATH/glew_2.0.0/lib/release/win32/glew32.lib \
$$3RD_PARTY_PATH/assimp_3.3.1/bin/x86/assimp-vc140-mt.lib

win32: {
    QMAKE_POST_LINK += echo "glew copy" && xcopy /Y /D /i \"$$3RD_PARTY_PATH\\glew_2.0.0\\bin\\release\\win32\\glew32.dll\" \"$$DESTDIR\"
    QMAKE_POST_LINK += && echo "sdl copy" && xcopy /Y /D /i \"$$3RD_PARTY_PATH\\sdl_2.0.5\\sdl2.dll\" \"$$DESTDIR\"
    QMAKE_POST_LINK += && echo "assimp copy" && xcopy /Y /D /i \"$$3RD_PARTY_PATH\\assimp_3.3.1\\bin\\x86\\assimp-vc140-mt.dll\" \"$$DESTDIR\"
    QMAKE_POST_LINK += && echo "vert shader copy" && xcopy /Y /D /i \"$$PWD\\*.vert\" \"$$DESTDIR\"
    QMAKE_POST_LINK += && echo "frag shader copy" && xcopy /Y /D /i \"$$PWD\\*.frag\" \"$$DESTDIR\"
}

SOURCES += \
main.cpp \
$$3RD_PARTY_PATH/imgui/imgui.cpp \
$$3RD_PARTY_PATH/imgui/imgui_draw.cpp \
$$3RD_PARTY_PATH/imgui/ \
imgui_impl_sdl_gl3.cpp \
tiny_obj_loader.cpp

HEADERS += \
main.h \
imgui_impl_sdl_gl3.h \
tiny_obj_loader.h

DISTFILES += \
defaultfragshader.frag \
defaultvertshader.vert \
phongvertshader.vert \
phongfragshader.frag
