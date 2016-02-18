/*
 * utils: Common functions, helpers and definitions
 * 
 * Copyrigt 2012-2016 Stephan Haller <nomad@froevel.de>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 * 
 * 
 */

#ifndef __XFDASHBOARD_UTILS__
#define __XFDASHBOARD_UTILS__

#include <clutter/clutter.h>
#include <gio/gio.h>

#include <libxfdashboard/window-tracker-workspace.h>
#include <libxfdashboard/stage-interface.h>
#include <libxfdashboard/stage.h>

G_BEGIN_DECLS

/* Debug macros */
#define _DEBUG_OBJECT_NAME(x) \
	((x)!=NULL ? G_OBJECT_TYPE_NAME((x)) : "<nil>")

#define _DEBUG_BOX(msg, box) \
	g_message("%s: %s: x1=%.2f, y1=%.2f, x2=%.2f, y2=%.2f [%.2fx%.2f]", \
				__func__, \
				msg, \
				(box).x1, (box).y1, (box).x2, (box).y2, \
				(box).x2-(box).x1, (box).y2-(box).y1)

#define _DEBUG_NOTIFY(self, property, format, ...) \
	g_message("%s: Property '%s' of %p (%s) changed to "format, \
				__func__, \
				property, \
				(self), (self) ? G_OBJECT_TYPE_NAME((self)) : "<nil>", \
				## __VA_ARGS__)

/* Gobject type of pointer array (GPtrArray) */
#define XFDASHBOARD_TYPE_POINTER_ARRAY		(xfdashboard_pointer_array_get_type())

/* Public API */

/**
 * GTYPE_TO_POINTER:
 * @gtype: A #GType to stuff into the pointer
 *
 * Stuffs the #GType specified at @gtype into a pointer type.
 */
#define GTYPE_TO_POINTER(gtype) \
	(GSIZE_TO_POINTER(gtype))

/**
 * GPOINTER_TO_GTYPE:
 * @pointer: A pointer to extract a #GType from
 *
 * Extracts a #GType from a pointer. The #GType must have been stored in the pointer with GTYPE_TO_POINTER().
 */
#define GPOINTER_TO_GTYPE(pointer) \
	((GType)GPOINTER_TO_SIZE(pointer))

GType xfdashboard_pointer_array_get_type(void);

void xfdashboard_notify(ClutterActor *inSender,
							const gchar *inIconName,
							const gchar *inFormat, ...)
							G_GNUC_PRINTF(3, 4);

GAppLaunchContext* xfdashboard_create_app_context(XfdashboardWindowTrackerWorkspace *inWorkspace);

void xfdashboard_register_gvalue_transformation_funcs(void);

ClutterActor* xfdashboard_find_actor_by_name(ClutterActor *inActor, const gchar *inName);

XfdashboardStageInterface* xfdashboard_get_stage_of_actor(ClutterActor *inActor);
XfdashboardStage* xfdashboard_get_global_stage_of_actor(ClutterActor *inActor);

gchar** xfdashboard_split_string(const gchar *inString, const gchar *inDelimiters);

gboolean xfdashboard_is_valid_id(const gchar *inString);

gchar* xfdashboard_get_enum_value_name(GType inEnumClass, gint inValue);

void xfdashboard_dump_actor(ClutterActor *inActor);

G_END_DECLS

#endif	/* __XFDASHBOARD_UTILS__ */