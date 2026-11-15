#!/usr/bin/env python3
"""Check the installed overlay against the distribution's GTK typelibs."""
import sys
import time

import gi

gi.require_version("Gtk", "4.0")
gi.require_version("Gdk", "4.0")
from gi.repository import Gdk, GLib, Gtk

expected = tuple(map(int, sys.argv[1].split(".")))
actual = (Gtk.get_major_version(), Gtk.get_minor_version(), Gtk.get_micro_version())
assert actual == expected, (actual, expected)
Gtk.init()
display = Gdk.Display.get_default()
assert display is not None, "No Broadway display"
assert display.__gtype__.name == "GdkBroadwayDisplay", display.__gtype__.name
window = Gtk.Window(title="Installed package smoke test")
window.set_child(Gtk.Button(label="GTK package loaded"))
window.present()
context = GLib.MainContext.default()
for _ in range(20):
    while context.pending():
        context.iteration(False)
    time.sleep(0.01)
assert window.get_realized(), "Window did not realize"
window.destroy()
print(f"PASS installed GTK {sys.argv[1]} with distribution typelibs and Broadway daemon")
