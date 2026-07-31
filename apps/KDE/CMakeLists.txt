# CMakeLists.txt -- I Have Issues, KDE (Qt 6 + KDE Frameworks 6) port.
#
# Minimum versions and why:
#   Qt 6.5.0  -- the first Qt 6 LTS. Every Qt call in this tree predates it
#                (QNetworkAccessManager::sendCustomRequest, setTransferTimeout(int),
#                rawHeaderPairs, QStyledItemDelegate, QSplitter, QStackedWidget).
#                Nothing here uses the Qt 6.7+ additions (QHttpHeaders, the
#                std::chrono setTransferTimeout overload, QDateTime's QTimeZone-only
#                overloads), deliberately, so the code stays buildable on 6.5.
#   KF 6.0.0  -- the first KDE Frameworks 6 release, which itself requires Qt 6.5.
#                The KF classes used (KXmlGuiWindow, KActionCollection,
#                KStandardAction, KMessageBox, KLocalizedString, KAboutData,
#                KColorScheme, KWallet) all exist in 6.0.
#
# The shared core in libs/issueskit is compiled straight into a static library
# here, exactly as libs/issueskit/README.md prescribes. It contains no Qt and no
# Q_OBJECT, so AUTOMOC is switched OFF for that target.
cmake_minimum_required(VERSION 3.16)

project(ihaveissues VERSION 1.0.0 LANGUAGES CXX)

set(QT_MIN_VERSION 6.5.0)
set(KF_MIN_VERSION 6.0.0)

# VERIFY: extra-cmake-modules for KF6 is versioned in lockstep with the
# frameworks, so ECM 6.0.0 is the right floor. On a distribution that ships ECM
# under a KF5-style 5.x version this find_package will fail loudly rather than
# silently misconfigure -- which is the intent.
find_package(ECM ${KF_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(FeatureSummary)

find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS
    Core
    Widgets
    Network
)

# CoreAddons  -- KAboutData
# I18n        -- KLocalizedString / i18n()
# XmlGui      -- KXmlGuiWindow, KActionCollection
# Config      -- KConfig, pulled in by the window-state saving KMainWindow does
# ConfigWidgets  -- KStandardAction, KColorScheme
# WidgetsAddons  -- KMessageBox, KStandardGuiItem
# Wallet      -- KWallet::Wallet
#
# ConfigWidgets and WidgetsAddons are listed explicitly even though XmlGui
# depends on both: this build includes <KStandardAction>, <KColorScheme>,
# <KMessageBox> and <KStandardGuiItem> directly, and a target should link what it
# includes rather than inherit it by accident.
find_package(KF6 ${KF_MIN_VERSION} REQUIRED COMPONENTS
    CoreAddons
    I18n
    XmlGui
    Config
    ConfigWidgets
    WidgetsAddons
    Wallet
)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC OFF)          # no .ui files: every widget is built in code
set(CMAKE_AUTORCC OFF)          # no .qrc files
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
# The shared core.
# ---------------------------------------------------------------------------

set(ISSUESKIT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../libs/issueskit)

# Globbed on purpose: libs/issueskit/README.md asks every consumer to glob, so
# that adding a file to the core cannot silently leave one port behind.
# CONFIGURE_DEPENDS makes CMake re-glob when the directory changes.
file(GLOB ISSUESKIT_SRCS CONFIGURE_DEPENDS ${ISSUESKIT_DIR}/src/*.cpp)
if(NOT ISSUESKIT_SRCS)
    message(FATAL_ERROR "No sources found in ${ISSUESKIT_DIR}/src -- is the "
                        "shared library checked out?")
endif()

add_library(issueskit STATIC ${ISSUESKIT_SRCS})
target_include_directories(issueskit PUBLIC ${ISSUESKIT_DIR}/include)
target_compile_features(issueskit PUBLIC cxx_std_17)
# The core is pure C++17 with no Q_OBJECT anywhere. Scanning it with moc would
# be wasted work at best and a portability violation at worst.
set_target_properties(issueskit PROPERTIES
    AUTOMOC OFF
    POSITION_INDEPENDENT_CODE ON
)

# ---------------------------------------------------------------------------
# The application.
# ---------------------------------------------------------------------------

set(ihaveissues_SRCS
    src/main.cpp

    src/github/QtHttpClient.cpp
    src/github/KWalletTokenStore.cpp
    src/github/SyncWorker.cpp

    src/ui/IssuePresentation.cpp
    src/ui/IssueListModel.cpp
    src/ui/IssueItemDelegate.cpp
    src/ui/IssueDetailWidget.cpp
    src/ui/IssueEditDialog.cpp
    src/ui/ProjectSettingsDialog.cpp
    src/ui/GitHubSyncDialog.cpp
    src/ui/MainWindow.cpp
)

add_executable(ihaveissues ${ihaveissues_SRCS})

target_include_directories(ihaveissues PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(ihaveissues
    PRIVATE
        issueskit
        Qt6::Core
        Qt6::Widgets
        Qt6::Network
        KF6::CoreAddons
        KF6::I18n
        KF6::XmlGui
        KF6::ConfigCore
        KF6::ConfigWidgets
        KF6::WidgetsAddons
        KF6::Wallet
)

# ---------------------------------------------------------------------------
# Installation -- the standard KDE plumbing.
# ---------------------------------------------------------------------------

install(TARGETS ihaveissues ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})

# KXmlGui looks the file up as <kxmlguidir>/<componentName>/<file>, and the
# component name is the first argument of KAboutData in src/main.cpp.
install(FILES ihaveissuesui.rc
        DESTINATION ${KDE_INSTALL_KXMLGUIDIR}/ihaveissues)

install(PROGRAMS com.druware.IHaveIssues.desktop
        DESTINATION ${KDE_INSTALL_APPDIR})

install(FILES com.druware.IHaveIssues.metainfo.xml
        DESTINATION ${KDE_INSTALL_METAINFODIR})

# shared-mime-info: registers *.issues as a subclass of application/json.
install(FILES com.druware.IHaveIssues.xml
        DESTINATION ${KDE_INSTALL_MIMEDIR})

# ---------------------------------------------------------------------------
# Translations.
# ---------------------------------------------------------------------------
#
# Every user-visible string goes through i18n(), so the catalogue is ready to be
# extracted. ki18n_install() is only called when a po/ directory actually exists,
# because the macro errors out on a missing directory.
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/po)
    ki18n_install(po)
endif()

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)
