cmake_minimum_required(VERSION 3.21)

project(vmosedge VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin/${CMAKE_BUILD_TYPE})

if(APPLE)
    set(APPLICATION_DIR_PATH ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${PROJECT_NAME}.app/Contents/MacOS)
else()
    set(APPLICATION_DIR_PATH ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
endif()

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Core Quick Network Concurrent QuickControls2 WebEngineQuick Gui ShaderTools)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Core Quick Network Concurrent QuickControls2 WebEngineQuick Gui ShaderTools)
find_package(libyuv CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(LibArchive REQUIRED)
find_package(zstd CONFIG REQUIRED)
find_path(STB_INCLUDE_DIRS "stb.h")
find_package(PkgConfig REQUIRED)
# pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET
#     libavcodec
#     libavdevice
#     libavfilter
#     libavformat
#     libavutil
#     libswresample
#     libswscale
# )

find_program(QT_LUPDATE NAMES lupdate)
find_program(QT_LRELEASE NAMES lrelease)
file(GLOB TS_FILE_PATHS ${CMAKE_CURRENT_LIST_DIR}/ *.ts)
add_custom_target(Script-UpdateTranslations
    COMMAND ${QT_LUPDATE} ${CMAKE_CURRENT_LIST_DIR} -ts language_zh.ts WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
    COMMAND ${QT_LUPDATE} ${CMAKE_CURRENT_LIST_DIR} -ts language_zh-hant.ts WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
    COMMAND ${QT_LUPDATE} ${CMAKE_CURRENT_LIST_DIR} -ts language_en.ts WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
    COMMAND ${QT_LUPDATE} ${CMAKE_CURRENT_LIST_DIR} -ts language_ru.ts WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
    COMMAND ${QT_LUPDATE} ${CMAKE_CURRENT_LIST_DIR} -ts language_ko.ts WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
    COMMAND ${QT_LUPDATE} ${CMAKE_CURRENT_LIST_DIR} -ts language_ja.ts WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
    COMMAND ${QT_LUPDATE} ${CMAKE_CURRENT_LIST_DIR} -ts language_vi.ts WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
    COMMAND ${QT_LUPDATE} ${CMAKE_CURRENT_LIST_DIR} -ts language_th.ts WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
    COMMAND ${QT_LUPDATE} ${CMAKE_CURRENT_LIST_DIR} -ts language_ms.ts WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
    COMMAND ${QT_LUPDATE} ${CMAKE_CURRENT_LIST_DIR} -ts language_ar.ts WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
    COMMAND ${QT_LUPDATE} ${CMAKE_CURRENT_LIST_DIR} -ts language_es.ts WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
    COMMAND ${QT_LUPDATE} ${CMAKE_CURRENT_LIST_DIR} -ts language_id.ts WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
    COMMAND ${QT_LUPDATE} ${CMAKE_CURRENT_LIST_DIR} -ts language_pt.ts WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
    COMMAND ${QT_LUPDATE} ${CMAKE_CURRENT_LIST_DIR} -ts language_ur.ts WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
    COMMAND ${QT_LUPDATE} ${CMAKE_CURRENT_LIST_DIR} -ts language_fil.ts WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
    COMMAND ${QT_LUPDATE} ${CMAKE_CURRENT_LIST_DIR} -ts language_km.ts WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
    COMMAND ${QT_LUPDATE} ${CMAKE_CURRENT_LIST_DIR} -ts language_fr.ts WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
    COMMAND ${QT_LUPDATE} ${CMAKE_CURRENT_LIST_DIR} -ts language_uk.ts WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
    COMMAND ${QT_LRELEASE} language_zh.ts WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
    COMMAND ${QT_LRELEASE} language_zh-hant.ts WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
    COMMAND ${QT_LRELEASE} language_en.ts WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
    COMMAND ${QT_LRELEASE} language_ru.ts WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
    COMMAND ${QT_LRELEASE} language_ko.ts WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
    COMMAND ${QT_LRELEASE} language_ja.ts WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
    COMMAND ${QT_LRELEASE} language_vi.ts WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
    COMMAND ${QT_LRELEASE} language_th.ts WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
    COMMAND ${QT_LRELEASE} language_ms.ts WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
    COMMAND ${QT_LRELEASE} language_ar.ts WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
    COMMAND ${QT_LRELEASE} language_es.ts WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
    COMMAND ${QT_LRELEASE} language_id.ts WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
    COMMAND ${QT_LRELEASE} language_pt.ts WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
    COMMAND ${QT_LRELEASE} language_ur.ts WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
    COMMAND ${QT_LRELEASE} language_fil.ts WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
    COMMAND ${QT_LRELEASE} language_km.ts WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
    COMMAND ${QT_LRELEASE} language_fr.ts WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
    COMMAND ${QT_LRELEASE} language_uk.ts WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${APPLICATION_DIR_PATH}/i18n
    COMMAND ${CMAKE_COMMAND} -E rename language_zh.qm ${APPLICATION_DIR_PATH}/i18n/language_zh.qm
    COMMAND ${CMAKE_COMMAND} -E rename language_zh-hant.qm ${APPLICATION_DIR_PATH}/i18n/language_zh-hant.qm
    COMMAND ${CMAKE_COMMAND} -E rename language_en.qm ${APPLICATION_DIR_PATH}/i18n/language_en.qm
    COMMAND ${CMAKE_COMMAND} -E rename language_ru.qm ${APPLICATION_DIR_PATH}/i18n/language_ru.qm
    COMMAND ${CMAKE_COMMAND} -E rename language_ko.qm ${APPLICATION_DIR_PATH}/i18n/language_ko.qm
    COMMAND ${CMAKE_COMMAND} -E rename language_ja.qm ${APPLICATION_DIR_PATH}/i18n/language_ja.qm
    COMMAND ${CMAKE_COMMAND} -E rename language_vi.qm ${APPLICATION_DIR_PATH}/i18n/language_vi.qm
    COMMAND ${CMAKE_COMMAND} -E rename language_th.qm ${APPLICATION_DIR_PATH}/i18n/language_th.qm
    COMMAND ${CMAKE_COMMAND} -E rename language_ms.qm ${APPLICATION_DIR_PATH}/i18n/language_ms.qm
    COMMAND ${CMAKE_COMMAND} -E rename language_ar.qm ${APPLICATION_DIR_PATH}/i18n/language_ar.qm
    COMMAND ${CMAKE_COMMAND} -E rename language_es.qm ${APPLICATION_DIR_PATH}/i18n/language_es.qm
    COMMAND ${CMAKE_COMMAND} -E rename language_id.qm ${APPLICATION_DIR_PATH}/i18n/language_id.qm
    COMMAND ${CMAKE_COMMAND} -E rename language_pt.qm ${APPLICATION_DIR_PATH}/i18n/language_pt.qm
    COMMAND ${CMAKE_COMMAND} -E rename language_ur.qm ${APPLICATION_DIR_PATH}/i18n/language_ur.qm
    COMMAND ${CMAKE_COMMAND} -E rename language_fil.qm ${APPLICATION_DIR_PATH}/i18n/language_fil.qm
    COMMAND ${CMAKE_COMMAND} -E rename language_km.qm ${APPLICATION_DIR_PATH}/i18n/language_km.qm
    COMMAND ${CMAKE_COMMAND} -E rename language_fr.qm ${APPLICATION_DIR_PATH}/i18n/language_fr.qm
    COMMAND ${CMAKE_COMMAND} -E rename language_uk.qm ${APPLICATION_DIR_PATH}/i18n/language_uk.qm
    SOURCES ${TS_FILE_PATHS}
)

include_directories(
    ${CMAKE_SOURCE_DIR}/armcloud_pc_sdk/include
    ${CMAKE_SOURCE_DIR}/aliyun-oss-cpp-sdk/include
)

#遍历所有Cpp文件
file(GLOB_RECURSE CPP_FILES *.cpp *.h *.cc)
foreach (filepath ${CPP_FILES})
    string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}/" "" filename ${filepath})
    list(APPEND sources_files ${filename})
endforeach (filepath)

if(WIN32)
    set(APP_ICON_RESOURCE_WINDOWS ${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_NAME}.rc)
    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_NAME}.rc.in
        ${APP_ICON_RESOURCE_WINDOWS}
    )
endif()

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(${PROJECT_NAME}
        MANUAL_FINALIZATION
        ${sources_files}
        resources.qrc
        ${APP_ICON_RESOURCE_WINDOWS}
    )
else()
    add_executable(${PROJECT_NAME}
        ${sources_files}
        resources.qrc
        ${APP_ICON_RESOURCE_WINDOWS}
    )
endif()

add_dependencies(${PROJECT_NAME} Script-UpdateTranslations)

#复制程序运行所需要的动态库
if (WIN32)
    if (MSVC)
        if (CMAKE_SIZEOF_VOID_P EQUAL 8)
            file(GLOB_RECURSE 3RDPARTY_DLL_DIR ${CMAKE_SOURCE_DIR}/3rdparty/msvc/*.dll
                                               ${CMAKE_SOURCE_DIR}/3rdparty/msvc/*.exe)
        endif ()
    endif ()
    file(COPY ${3RDPARTY_DLL_DIR} DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
endif ()
target_include_directories(${PROJECT_NAME} PRIVATE
    ${STB_INCLUDE_DIRS}
)
target_include_directories(${PROJECT_NAME} PRIVATE ${LibArchive_INCLUDE_DIRS})

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Quick
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::QuickControls2
    Qt${QT_VERSION_MAJOR}::WebEngineQuick
    Qt${QT_VERSION_MAJOR}::Concurrent
    Qt${QT_VERSION_MAJOR}::GuiPrivate
    Qt${QT_VERSION_MAJOR}::ShaderToolsPrivate
    fluentuiplugin
    yuv
    # PkgConfig::FFMPEG
    ${LibArchive_LIBRARIES}
    zstd::libzstd_shared
    OpenSSL::SSL
    QtScrcpyCore
)

if(${QT_VERSION} VERSION_LESS 6.1.0)
    set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.vmos.${PROJECT_NAME})
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE_ICON_FILE ${PROJECT_NAME}.icns
    MACOSX_BUNDLE_NAME ${APP_NAME}
    MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_NAME}.plist.in
)

if(APPLE)
    # 设置 macOS 最低版本要求
    set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0") # 指定 macOS 最低版本号

    # 指定图标文件路径
    set(ICON_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_NAME}.icns")

    # 验证图标文件是否存在
    if(NOT EXISTS ${ICON_SOURCE})
        message(FATAL_ERROR "图标文件 ${ICON_SOURCE} 不存在！请确保已生成 ${PROJECT_NAME}.icns。")
    endif()

    # 复制图标到 .app/Contents/Resources/
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${ICON_SOURCE}
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/../Resources/${PROJECT_NAME}.icns
        COMMENT "Copying icon file to Resources directory"
    )
endif()

include(GNUInstallDirs)
install(TARGETS ${PROJECT_NAME}
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})


if (CMAKE_BUILD_TYPE MATCHES "Release")
    if (APPLE)
        find_program(QT_DEPLOY_QT NAMES macdeployqt)
        add_custom_target(Script-DeployRelease
            COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_SOURCE_DIR}/dist
            COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_RUNTIME_OUTPUT_DIRECTORY} ${CMAKE_SOURCE_DIR}/dist
            COMMAND ${QT_DEPLOY_QT} ${CMAKE_SOURCE_DIR}/dist/${PROJECT_NAME}.app -qmldir=${CMAKE_CURRENT_LIST_DIR}
            COMMENT "MacOs Deploying Qt Dependencies After Build........."
            SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        )
    endif()
    if(WIN32)
        find_program(QT_DEPLOY_QT NAMES windeployqt)
        add_custom_target(Script-DeployRelease
            COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_SOURCE_DIR}/dist
            COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_RUNTIME_OUTPUT_DIRECTORY} ${CMAKE_SOURCE_DIR}/dist
            COMMAND ${QT_DEPLOY_QT} --qmldir=${CMAKE_CURRENT_LIST_DIR} --plugindir ${CMAKE_SOURCE_DIR}/dist/plugins --no-translations --compiler-runtime ${CMAKE_SOURCE_DIR}/dist/${PROJECT_NAME}.exe
            COMMENT "Windows Deploying Qt Dependencies After Build........."
            SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        )
    endif()
endif()
