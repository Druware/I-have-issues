#!/bin/sh
#
# check-portable.sh -- enforce the invariant that makes this library shareable.
#
# libs/issueskit must depend on nothing but C++17 and the standard library. No
# Be API, no GTK/GLib, no Qt/KDE. The moment one leaks in, the library stops
# being usable by the other two ports and nobody notices until their build
# breaks.
#
# This greps for platform headers rather than trusting review, and exits
# non-zero if any appear. Run it from anywhere.

set -eu

cd "$(dirname "$0")"

status=0

fail() {
	echo "PORTABILITY VIOLATION: $1"
	status=1
}

# Toolkit headers, matched on the #include line only so prose in comments (which
# legitimately names these platforms to explain rationale) does not trip it.
scan() {
	pattern="$1"
	label="$2"
	hits=$(grep -rnE "^[[:space:]]*#[[:space:]]*include[[:space:]]*[<\"]($pattern)" \
		include src tests 2>/dev/null || true)
	if [ -n "$hits" ]; then
		fail "$label"
		echo "$hits" | sed 's/^/    /'
	fi
}

# Haiku / Be API: the kit headers are capitalised and extensionless-ish, so match
# the well-known ones plus anything obviously Be-flavoured.
scan 'Application\.h|Window\.h|View\.h|Looper\.h|Handler\.h|Message\.h|MessageRunner\.h|Messenger\.h|Alert\.h|Box\.h|Button\.h|CheckBox\.h|ListItem\.h|ListView\.h|OutlineListView\.h|StringItem\.h|StringView\.h|MenuBar\.h|MenuField\.h|MenuItem\.h|Menu\.h|PopUpMenu\.h|ScrollView\.h|SplitView\.h|TabView\.h|TextControl\.h|TextView\.h|LayoutBuilder\.h|GroupLayout\.h|GridLayout\.h|FilePanel\.h|Path\.h|Entry\.h|File\.h|Directory\.h|NodeInfo\.h|Mime\.h|KeyStore\.h|Key\.h|OS\.h|SupportDefs\.h|InterfaceDefs\.h|GraphicsDefs\.h|Font\.h|Rect\.h|Point\.h|String\.h|DataIO\.h|Url\.h|UrlRequest\.h|UrlProtocolRoster\.h|UrlProtocolListener\.h|HttpRequest\.h|HttpHeaders\.h|HttpResult\.h|Json\.h' \
	'Haiku / Be API header included in the shared library'

# GNOME: GTK, GLib, GIO, libsecret, libsoup.
scan 'gtk|glib|gio|gobject|gdk|libsecret|secret|libsoup|libsoup-.*|adwaita|gtk/.*|glib/.*' \
	'GTK / GLib / GNOME header included in the shared library'

# KDE: Qt and KDE frameworks.
scan 'Q[A-Z][A-Za-z]*|Qt[A-Za-z]*|Qt[A-Za-z]*/.*|K[A-Z][A-Za-z]*|KF[0-9].*|kwallet.*|KWallet.*' \
	'Qt / KDE header included in the shared library'

if [ $status -eq 0 ]; then
	echo "PORTABLE: libs/issueskit includes no platform toolkit header."
	echo
	echo "Non-standard includes present (should all be project headers):"
	grep -rhE '^[[:space:]]*#[[:space:]]*include' include src tests \
		| sed 's/.*include[[:space:]]*//' \
		| sort -u \
		| sed 's/^/    /'
fi

exit $status
