## I Have Issues -- GNOME application
##
## Build with:
##     meson setup build
##     ninja -C build
##
## This directory is the GNOME front end only. The .issues format, the domain
## model and the GitHub sync flow live in the shared library at
## libs/issueskit and are compiled in below; what stays here is the GTK4 /
## libadwaita UI plus the two platform adapters (libsoup HTTP, libsecret token
## storage).

project('ihaveissues-gnome', ['c', 'cpp'],
  version: '1.0.0',
  license: 'GPL-3.0-or-later',
  meson_version: '>= 0.64.0',
  default_options: [
    'cpp_std=c++17',
    'warning_level=2',
  ],
)

# Used verbatim as the D-Bus name, the .desktop basename, the AppStream
# component id, the icon name and the GResource prefix (with dots as slashes).
app_id = 'com.druware.IHaveIssues'

gnome = import('gnome')

# Minimum versions, and why:
#   gtk4 4.14        -- the version libadwaita 1.5 itself requires. GtkFileDialog
#                       (4.10) and gtk_list_box_remove_all-era API are well
#                       inside it.
#   libadwaita 1.5   -- AdwDialog / AdwAlertDialog / AdwAboutDialog. The
#                       adaptive pieces (AdwNavigationSplitView, AdwToolbarView,
#                       AdwBreakpoint, AdwSwitchRow) arrived in 1.4, so 1.5 is
#                       set by the dialogs alone.
#   libsoup 3.0      -- a different library from libsoup 2.4, not a newer one.
#   libsecret 0.20   -- the simple secret_password_* API is far older, but 0.20
#                       is what current distributions ship and pins nothing new.
gtk_dep    = dependency('gtk4',         version: '>= 4.14')
adw_dep    = dependency('libadwaita-1', version: '>= 1.5')
soup_dep   = dependency('libsoup-3.0',  version: '>= 3.0')
secret_dep = dependency('libsecret-1',  version: '>= 0.20')

# The shared core -------------------------------------------------------------
#
# libs/issueskit lives OUTSIDE this Meson project. Meson refuses files() and
# include_directories() that climb out of the project root ("sandbox
# violation"), so the sources are handed over as absolute path strings and the
# include path as a plain -I compile argument. Both sidestep that check.
#
# VERIFY: this is the single most likely thing in the build to need adjusting,
# and it cannot be tested here -- no Meson was available. If Meson rejects the
# absolute source paths on your version, the documented fallbacks are in
# README.md ("Build risks").

issueskit_dir = get_option('issueskit_dir')
if issueskit_dir == ''
  issueskit_dir = meson.current_source_dir() / '..' / '..' / 'libs' / 'issueskit'
endif

# Globbed rather than listed, so adding a file to the shared library cannot
# silently leave this port behind -- libs/issueskit/README.md asks for exactly
# this. `check: true` turns a wrong path into a hard configure failure.
issueskit_ls = run_command('sh', '-c',
  'ls "' + issueskit_dir + '"/src/*.cpp', check: true)
issueskit_sources = issueskit_ls.stdout().strip().split('\n')

if issueskit_sources.length() == 0 or issueskit_sources[0] == ''
  error('No shared library sources found under ' + issueskit_dir +
        '/src -- is libs/issueskit present? Override with -Dissueskit_dir=...')
endif

issueskit_include_arg = '-I' + issueskit_dir / 'include'

issueskit_lib = static_library('issueskit',
  issueskit_sources,
  cpp_args: [issueskit_include_arg],
  install: false,
)

issueskit_dep = declare_dependency(
  link_with: issueskit_lib,
  compile_args: [issueskit_include_arg],
)

subdir('src')
subdir('data')

# Refreshes the desktop and MIME caches so a freshly installed build actually
# owns .issues files. Harmless when run into a staging prefix.
gnome.post_install(
  gtk_update_icon_cache: true,
  update_desktop_database: true,
  update_mime_database: true,
)

summary({
    'issueskit': issueskit_dir,
    'sources': issueskit_sources.length(),
  },
  section: 'Shared core',
)
