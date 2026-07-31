## The application binary.

# The GResource bundle. glib-compile-resources emits C, which is why the
# project declares both 'c' and 'cpp'. The generated source registers itself
# from a constructor, so nothing has to call a register function by hand.
#
# VERIFY: gnome.compile_resources() generates with automatic registration
# unless --manual-register is passed. If the style sheet turns out to be
# missing at runtime, that assumption is where to look.
ihaveissues_resources = gnome.compile_resources(
  'ihaveissues-resources',
  'ihaveissues.gresource.xml',
  source_dir: '.',
  c_name: 'ihaveissues',
)

ihaveissues_sources = files(
  'Application.cpp',
  'GitHubSyncDialog.cpp',
  'IssueDetailView.cpp',
  'IssueEditDialog.cpp',
  'IssueObject.cpp',
  'IssuePresentation.cpp',
  'MainWindow.cpp',
  'ProjectSettingsDialog.cpp',
  'SecretTokenStore.cpp',
  'SoupHttpClient.cpp',
  'main.cpp',
)

executable('ihaveissues',
  ihaveissues_sources,
  ihaveissues_resources,
  dependencies: [
    gtk_dep,
    adw_dep,
    soup_dep,
    secret_dep,
    issueskit_dep,
  ],
  install: true,
)
