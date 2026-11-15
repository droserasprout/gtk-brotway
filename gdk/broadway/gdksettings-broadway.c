/* GDK - The GIMP Drawing Kit
 *
 * Read the browser's prefers-color-scheme from broadwayd's Settings portal and
 * expose it as GtkSettings:gtk-interface-color-scheme, so plain GTK apps follow
 * the browser theme too (libadwaita reads the same portal on its own). broadwayd
 * owns org.freedesktop.portal.Desktop and serves org.freedesktop.appearance
 * appearance settings; here we are the client for color scheme and reduced motion.
 *
 * This library is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 2.1 of the License, or (at your option)
 * any later version.
 */

#include "config.h"

#include "gdkprivate-broadway.h"
#include "gdkdisplay-broadway.h"

#include "gdk/gdkprivate.h"
#include "gdk/gdkdisplayprivate.h"

#include <gio/gio.h>
#include <string.h>

#define PORTAL_BUS_NAME         "org.freedesktop.portal.Desktop"
#define PORTAL_OBJECT_PATH      "/org/freedesktop/portal/desktop"
#define PORTAL_SETTINGS_IFACE   "org.freedesktop.portal.Settings"
#define APPEARANCE_NS           "org.freedesktop.appearance"
#define COLOR_SCHEME_KEY        "color-scheme"
#define COLOR_SCHEME_SETTING    "gtk-interface-color-scheme"
#define REDUCED_MOTION_KEY      "reduced-motion"
#define REDUCED_MOTION_SETTING  "gtk-interface-reduced-motion"

/* Portal appearance color-scheme (0 no-pref, 1 dark, 2 light) maps onto the
 * GtkInterfaceColorScheme enum by +1 (0 UNSUPPORTED, 1 DEFAULT, 2 DARK, 3 LIGHT),
 * exactly like the wayland backend. */
static const char *
apply_setting (GdkDisplay *display,
               const char *name,
               GVariant   *value)
{
  GdkBroadwayDisplay *self = GDK_BROADWAY_DISPLAY (display);

  if (g_str_equal (name, COLOR_SCHEME_KEY) && !self->color_scheme_forced)
    {
      self->color_scheme = (int) (g_variant_get_uint32 (value) + 1);
      return COLOR_SCHEME_SETTING;
    }
  if (g_str_equal (name, REDUCED_MOTION_KEY))
    {
      self->reduced_motion = g_variant_get_uint32 (value) == 1 ? 1 : 0;
      return REDUCED_MOTION_SETTING;
    }
  return NULL;
}

static void
settings_portal_changed (GDBusProxy *proxy,
                         const char *sender_name,
                         const char *signal_name,
                         GVariant   *parameters,
                         gpointer    user_data)
{
  GdkDisplay *display = user_data;
  const char *namespace;
  const char *name;
  GVariant *value;

  if (strcmp (signal_name, "SettingChanged") != 0)
    return;

  g_variant_get (parameters, "(&s&sv)", &namespace, &name, &value);

  if (g_str_equal (namespace, APPEARANCE_NS))
    {
      const char *setting = apply_setting (display, name, value);
      if (setting != NULL)
        gdk_display_setting_changed (display, setting);
    }

  g_variant_unref (value);
}

void
_gdk_broadway_display_init_settings (GdkDisplay *display)
{
  GdkBroadwayDisplay *self = GDK_BROADWAY_DISPLAY (display);
  GDBusProxy *proxy;
  GVariant *ret;
  GVariant *value = NULL;
  GError *error = NULL;
  const char *forced;
  const char *keys[] = { COLOR_SCHEME_KEY, REDUCED_MOTION_KEY };

  self->color_scheme = 0; /* GTK_INTERFACE_COLOR_SCHEME_UNSUPPORTED */
  self->reduced_motion = 0; /* GTK_REDUCED_MOTION_NO_PREFERENCE */

  /* BROTWAY_COLOR_SCHEME forces the scheme for this process and ignores the
   * browser: dark/light pin it; auto (or unset) follows the portal as usual. */
  forced = g_getenv ("BROTWAY_COLOR_SCHEME");
  if (forced != NULL && *forced != '\0' && g_ascii_strcasecmp (forced, "auto") != 0)
    {
      if (g_ascii_strcasecmp (forced, "dark") == 0)
        {
          self->color_scheme = 2;  /* GTK_INTERFACE_COLOR_SCHEME_DARK */
          self->color_scheme_forced = TRUE;
        }
      else if (g_ascii_strcasecmp (forced, "light") == 0)
        {
          self->color_scheme = 3;  /* GTK_INTERFACE_COLOR_SCHEME_LIGHT */
          self->color_scheme_forced = TRUE;
        }
      else
        g_warning ("BROTWAY_COLOR_SCHEME: unknown value '%s' (want auto/light/dark)",
                   forced);

      /* A pinned color scheme must still follow the motion preference. */
    }

  if (gdk_display_get_debug_flags (display) &
      (GDK_DEBUG_DEFAULT_SETTINGS | GDK_DEBUG_NO_PORTALS))
    return;

  /* No should_use_portal() gate: broadwayd always serves this when present, and
   * the proxy quietly follows if it appears later. If there is no session bus or
   * portal at all, the ReadOne below fails and we fall back to defaults. */
  proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                         G_DBUS_PROXY_FLAGS_NONE,
                                         NULL,
                                         PORTAL_BUS_NAME,
                                         PORTAL_OBJECT_PATH,
                                         PORTAL_SETTINGS_IFACE,
                                         NULL,
                                         &error);
  if (proxy == NULL)
    {
      g_debug ("broadway: no Settings portal (%s); not following appearance settings",
               error->message);
      g_clear_error (&error);
      return;
    }

  /* Missing keys are fine; retain defaults and subscribe to later changes. */
  for (gsize i = 0; i < G_N_ELEMENTS (keys); i++)
    {
      if (self->color_scheme_forced && g_str_equal (keys[i], COLOR_SCHEME_KEY))
        continue;
      ret = g_dbus_proxy_call_sync (proxy, "ReadOne",
                                    g_variant_new ("(ss)", APPEARANCE_NS, keys[i]),
                                    G_DBUS_CALL_FLAGS_NONE, G_MAXINT, NULL, &error);
      if (ret != NULL)
        {
          g_variant_get (ret, "(v)", &value);
          apply_setting (display, keys[i], value);
          g_variant_unref (value);
          g_variant_unref (ret);
        }
      else
        {
          g_debug ("broadway: initial %s read failed (%s); waiting for changes",
                   keys[i], error->message);
          g_clear_error (&error);
        }
    }

  g_signal_connect (proxy, "g-signal",
                    G_CALLBACK (settings_portal_changed), display);

  self->settings_portal = proxy;
}

void
_gdk_broadway_display_finalize_settings (GdkDisplay *display)
{
  GdkBroadwayDisplay *self = GDK_BROADWAY_DISPLAY (display);

  g_clear_object (&self->settings_portal);
}

gboolean
_gdk_broadway_display_get_setting (GdkDisplay *display,
                                   const char *name,
                                   GValue     *value)
{
  GdkBroadwayDisplay *self = GDK_BROADWAY_DISPLAY (display);

  if (g_str_equal (name, COLOR_SCHEME_SETTING) &&
      (self->settings_portal != NULL || self->color_scheme_forced))
    {
      g_value_set_enum (value, self->color_scheme);
      return TRUE;
    }

  if (g_str_equal (name, REDUCED_MOTION_SETTING) && self->settings_portal != NULL)
    {
      g_value_set_enum (value, self->reduced_motion);
      return TRUE;
    }

  return FALSE;
}
