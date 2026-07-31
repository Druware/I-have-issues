## Desktop integration: launcher, AppStream metadata and the .issues MIME type.
##
## None of these are generated from .in templates, because the application is
## not translated: there is nothing to substitute. If translations are ever
## added, they become .desktop.in / .metainfo.xml.in and go through i18n.merge_file().

desktop_file = app_id + '.desktop'
install_data(desktop_file,
  install_dir: get_option('datadir') / 'applications',
)

metainfo_file = app_id + '.metainfo.xml'
install_data(metainfo_file,
  install_dir: get_option('datadir') / 'metainfo',
)

# Registers *.issues as a sub-class of application/json.
mime_file = app_id + '.mime.xml'
install_data(mime_file,
  install_dir: get_option('datadir') / 'mime' / 'packages',
)

# Validation, when the tools happen to be installed. `required: false` keeps a
# machine without them building normally.
desktop_file_validate = find_program('desktop-file-validate', required: false)
if desktop_file_validate.found()
  test('validate-desktop-file',
    desktop_file_validate,
    args: [meson.current_source_dir() / desktop_file],
    suite: 'data',
  )
endif

appstreamcli = find_program('appstreamcli', required: false)
if appstreamcli.found()
  test('validate-metainfo',
    appstreamcli,
    args: ['validate', '--no-net', '--explain',
           meson.current_source_dir() / metainfo_file],
    suite: 'data',
  )
endif

# TODO: no application icon is installed. See README.md -- an icon is a design
# asset, and shipping a placeholder that looks broken is worse than shipping
# none. Icon= in the .desktop file and <icon> in the MIME package already name
# `com.druware.IHaveIssues`, so dropping
# data/icons/hicolor/scalable/apps/com.druware.IHaveIssues.svg into place and
# installing it here is the whole change.
