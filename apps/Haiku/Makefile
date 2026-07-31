## I Have Issues -- Haiku application
##
## Build with a plain `make` on Haiku. This uses the standard makefile-engine
## that ships in the Haiku development package, so `make`, `make clean` and
## `make bindcatalogs` all behave the way they do for every other Haiku app.
##
## This app is the Haiku front end only. The .issues format, the domain model and
## the GitHub sync flow live in the shared library at libs/issueskit and are
## compiled in below; what stays here is the Be API UI plus the two platform
## adapters (Services Kit HTTP, BKeyStore token storage).
##
## NOTE: makefile-engine derives each object file name from the BASENAME of the
## source, so no two files anywhere in SRCS may share a basename -- including the
## shared library sources pulled in from outside this directory. They currently
## do not; keep it that way when adding files.

NAME = IHaveIssues
TYPE = APP
APP_MIME_SIG = application/x-vnd.Druware-IHaveIssues

## The shared, portable core lives outside this app and is compiled straight into
## it -- no intermediate static library, so there is one build to run and no
## staleness between the two.
##
## VERIFY: makefile-engine handles sources outside the app directory by adding
## every source's directory to VPATH and naming each object after the source's
## BASENAME. Relative paths that climb out with `..` are therefore expected to
## work, but this has not been built on Haiku. If the engine cannot find them,
## the fallbacks are (a) an absolute path via $(shell cd ../../libs/issueskit &&
## pwd), or (b) building libs/issueskit into a static library first and adding it
## to LIBS/LIBPATHS.
ISSUESKIT_DIR = ../../libs/issueskit

## VERIFY: $(wildcard) is a GNU make function and makefile-engine is GNU-make
## only, so this is safe -- but it is evaluated before the engine is included,
## and a wrong path yields an EMPTY list rather than an error. The guard below
## turns that silent failure into a loud one.
ISSUESKIT_SRCS = $(wildcard $(ISSUESKIT_DIR)/src/*.cpp)

ifeq ($(strip $(ISSUESKIT_SRCS)),)
$(error Shared library sources not found at $(ISSUESKIT_DIR)/src -- \
	is libs/issueskit present?)
endif

## VERIFY: makefile-engine derives object names from basenames, so no shared
## source may share a basename with a file under src/. None currently do; the
## engine gives no warning if that ever changes, it just silently builds one of
## them twice.
SRCS = \
	$(ISSUESKIT_SRCS) \
	src/github/ServicesKitHttpClient.cpp \
	src/github/KeyStoreTokenStore.cpp \
	src/ui/IssuePresentation.cpp \
	src/ui/IssueListItem.cpp \
	src/ui/IssueDetailView.cpp \
	src/ui/IssueEditWindow.cpp \
	src/ui/ProjectSettingsWindow.cpp \
	src/ui/GitHubSyncWindow.cpp \
	src/ui/MainWindow.cpp \
	src/ui/IHaveIssuesApp.cpp

RDEFS = IHaveIssues.rdef

RSRCS =

#	be         -- Application, Interface, Storage and Support kits, and BKeyStore
#	bnetapi    -- the classic Services Kit (BUrlRequest / BHttpRequest).
#	             If you port ServicesKitHttpClient.cpp to the new API, this
#	             becomes `netservices2` instead (a static library).
#	network    -- sockets, pulled in by bnetapi.
#
# The shared library adds nothing here: it is pure C++17 and needs no library
# beyond the standard one.
LIBS = be bnetapi network $(STDCPPLIBS)

LIBPATHS =

SYSTEM_INCLUDE_PATHS =

## $(ISSUESKIT_DIR)/include is what makes `#include <issueskit/IssueModel.h>`
## resolve; the app's own directories are for its quote-includes.
##
## VERIFY: makefile-engine passes LOCAL_INCLUDE_PATHS through as -I. A relative
## path here is relative to this makefile's directory, which is where the engine
## invokes the compiler from -- confirm on the box.
LOCAL_INCLUDE_PATHS = \
	$(ISSUESKIT_DIR)/include \
	src/github \
	src/ui

OPTIMIZE := FULL

LOCALES =

DEFINES =

WARNINGS = ALL

SYMBOLS :=

DEBUGGER :=

# std::optional and structured lifetimes need C++17. Haiku's default gcc (13.x
# on x86_64) supports it; the gcc2 hybrid compiler does NOT, so this app cannot
# be built for BeOS binary compatibility.
COMPILER_FLAGS = -std=c++17

LINKER_FLAGS =

APP_VERSION :=

DRIVER_PATH :=

## The engine lives in the develop tree. `findpaths` resolves it whether the app
## is being built against the system or a package-managed development tree.
DEVEL_DIRECTORY := \
	$(shell findpaths -r "makefile_engine" B_FIND_PATH_DEVELOP_DIRECTORY)

include $(DEVEL_DIRECTORY)/makefile-engine
