project('manga-reader', 'c',
  version : '0.1.0',
  default_options : ['c_std=c11', 'warning_level=2'])

gtk2 = dependency('gtk+-2.0')
libcurl = dependency('libcurl')
libxml2 = dependency('libxml-2.0')
sqlite3 = dependency('sqlite3')
libdl = meson.get_compiler('c').find_library('dl', required : false)

# Compile the git SHA into the binary for update checking
git_sha = run_command('git', 'rev-parse', '--short', 'HEAD', check : false)
if git_sha.returncode() == 0
  build_commit = git_sha.stdout().strip()
else
  build_commit = 'unknown'
endif
add_project_arguments('-DBUILD_COMMIT="' + build_commit + '"', language : 'c')

sources = files(
  'src/main.c',
  'src/app.c',
  'src/updater.c',
  'src/ui/search_view.c',
  'src/ui/manga_view.c',
  'src/ui/reader_view.c',
  'src/ui/settings_view.c',
  'src/ui/widgets.c',
  'src/ui/keyboard.c',
  'src/device/brightness.c',
  'src/sources/mangakatana.c',
  'src/sources/source_registry.c',
  'src/net/http.c',
  'src/net/image_loader.c',
  'src/util/html_parser.c',
  'src/util/cache.c',
  'src/util/database.c',
)

executable('manga-reader',
  sources,
  dependencies : [gtk2, libcurl, libxml2, sqlite3, libdl],
  install : true)
