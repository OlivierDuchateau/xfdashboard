/*
 * hot-corner: Activates application when pointer is move to a corner
 * 
 * Copyright 2012-2016 Stephan Haller <nomad@froevel.de>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>
#include <math.h>

#include "hot-corner.h"

#include <window-tracker.h>
#include <application.h>

/* Definitions */
typedef enum
{
	XFDASHBOARD_HOT_CORNER_AREA_TOP_LEFT=0,
	XFDASHBOARD_HOT_CORNER_AREA_TOP_RIGHT,
	XFDASHBOARD_HOT_CORNER_AREA_BOTTOM_LEFT,
	XFDASHBOARD_HOT_CORNER_AREA_BOTTOM_RIGHT,
} XfdashboardHotCornerArea;


/* Define this class in GObject system */
G_DEFINE_DYNAMIC_TYPE(XfdashboardHotCorner,
						xfdashboard_hot_corner,
						G_TYPE_OBJECT)

/* Define this class in this plugin */
XFDASHBOARD_DEFINE_PLUGIN_TYPE(xfdashboard_hot_corner);

/* Private structure - access only by public API if needed */
#define XFDASHBOARD_HOT_CORNER_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE((obj), XFDASHBOARD_TYPE_HOT_CORNER, XfdashboardHotCornerPrivate))

struct _XfdashboardHotCornerPrivate
{
	/* Properties related */
	XfdashboardHotCornerArea	hotCornerArea;
	gint						activationRadius;
	GTimeSpan					activationMicroseconds;

	/* Instance related */
	XfdashboardApplication		*application;
	XfdashboardWindowTracker	*windowTracker;
	GdkWindow					*rootWindow;
	GdkDeviceManager			*deviceManager;

	guint						timeoutID;
	GDateTime					*enteredTime;
	gboolean					wasHandledRecently;
};


/* IMPLEMENTATION: Private variables and methods */
#define POLL_POINTER_POSITION_INTERVAL			100
#define POLL_POINTER_ACTIVATION_RADIUS			4
#define POLL_POINTER_ACTIVATION_MILLISECONDS	300

typedef struct _XfdashboardHotCornerBox		XfdashboardHotCornerBox;
struct _XfdashboardHotCornerBox
{
	gint		x1, y1;
	gint		x2, y2;
};

/* Timeout callback to check for activation or suspend via hot corner */
static gboolean _xfdashboard_hot_corner_check_hot_corner(gpointer inUserData)
{
	XfdashboardHotCorner				*self;
	XfdashboardHotCornerPrivate			*priv;
	XfdashboardWindowTrackerWindow		*activeWindow;
	GdkDevice							*pointerDevice;
	gint								pointerX, pointerY;
	XfdashboardWindowTrackerMonitor		*primaryMonitor;
	XfdashboardHotCornerBox				monitorRect;
	XfdashboardHotCornerBox				hotCornerRect;
	GDateTime							*currentTime;
	GTimeSpan							timeDiff;

	g_return_val_if_fail(XFDASHBOARD_IS_HOT_CORNER(inUserData), G_SOURCE_CONTINUE);

	self=XFDASHBOARD_HOT_CORNER(inUserData);
	priv=self->priv;

	/* Do nothing if current window is fullscreen but not this application */
	activeWindow=xfdashboard_window_tracker_get_active_window(priv->windowTracker);
	if(activeWindow &&
		xfdashboard_window_tracker_window_is_fullscreen(activeWindow) &&
		!xfdashboard_window_tracker_window_is_stage(activeWindow))
	{
		return(G_SOURCE_CONTINUE);
	}

	/* Get current position of pointer */
	pointerDevice=gdk_device_manager_get_client_pointer(priv->deviceManager);
	if(!pointerDevice)
	{
		g_critical(_("Could not get pointer to determine pointer position"));
		return(G_SOURCE_CONTINUE);
	}

	gdk_window_get_device_position(priv->rootWindow, pointerDevice, &pointerX, &pointerY, NULL);

	/* Get position and size of primary monitor */
	primaryMonitor=xfdashboard_window_tracker_get_primary_monitor(priv->windowTracker);
	if(primaryMonitor)
	{
		gint							w, h;

		xfdashboard_window_tracker_monitor_get_geometry(primaryMonitor,
														&monitorRect.x1,
														&monitorRect.y1,
														&w,
														&h);
		monitorRect.x2=monitorRect.x1+w;
		monitorRect.y2=monitorRect.y1+h;
	}
		else
		{
			/* Set position to 0,0 and size to screen size */
			monitorRect.x1=monitorRect.y1=0;
			monitorRect.x2=xfdashboard_window_tracker_get_screen_width(priv->windowTracker);
			monitorRect.y2=xfdashboard_window_tracker_get_screen_height(priv->windowTracker);
		}

	/* Get rectangle where pointer must be inside to activate hot corner */
	switch(priv->hotCornerArea)
	{
		case XFDASHBOARD_HOT_CORNER_AREA_TOP_RIGHT:
			hotCornerRect.x2=monitorRect.x2;
			hotCornerRect.x1=MAX(monitorRect.x2-priv->activationRadius, monitorRect.x1);
			hotCornerRect.y1=monitorRect.y1;
			hotCornerRect.y2=MIN(monitorRect.y1+priv->activationRadius, monitorRect.y2);
			break;

		case XFDASHBOARD_HOT_CORNER_AREA_BOTTOM_LEFT:
			hotCornerRect.x1=monitorRect.x1;
			hotCornerRect.x2=MIN(monitorRect.x1+priv->activationRadius, monitorRect.x2);
			hotCornerRect.y2=monitorRect.y2;
			hotCornerRect.y1=MAX(monitorRect.y2-priv->activationRadius, monitorRect.y1);
			break;

		case XFDASHBOARD_HOT_CORNER_AREA_BOTTOM_RIGHT:
			hotCornerRect.x2=monitorRect.x2;
			hotCornerRect.x1=MAX(monitorRect.x2-priv->activationRadius, monitorRect.x1);
			hotCornerRect.y2=monitorRect.y2;
			hotCornerRect.y1=MAX(monitorRect.y2-priv->activationRadius, monitorRect.y1);
			break;

		case XFDASHBOARD_HOT_CORNER_AREA_TOP_LEFT:
		default:
			hotCornerRect.x1=monitorRect.x1;
			hotCornerRect.x2=MIN(monitorRect.x1+priv->activationRadius, monitorRect.x2);
			hotCornerRect.y1=monitorRect.y1;
			hotCornerRect.y2=MIN(monitorRect.y1+priv->activationRadius, monitorRect.y2);
			break;
	}

	/* Check if pointer is in configured hot corner for a configured interval.
	 * If it is not reset entered time and return immediately without doing anything.
	 */
	if(pointerX<hotCornerRect.x1 || pointerX>=hotCornerRect.x2 ||
		pointerY<hotCornerRect.y1 || pointerY>=hotCornerRect.y2)
	{
		/* Reset entered time */
		if(priv->enteredTime)
		{
			g_date_time_unref(priv->enteredTime);
			priv->enteredTime=NULL;
		}

		/* Return without doing anything */
		return(G_SOURCE_CONTINUE);
	}

	/* If no entered time was registered yet we assume the pointer is in hot corner
	 * for the first time. So remember entered time for next polling interval.
	 */
	if(!priv->enteredTime)
	{
		/* Remember entered time */
		priv->enteredTime=g_date_time_new_now_local();

		/* Reset handled flag to get duration checked next time */
		priv->wasHandledRecently=FALSE;

		/* Return without doing anything */
		return(G_SOURCE_CONTINUE);
	}

	/* If handled flag is set then do nothing to avoid flapping between activation
	 * and suspending application once the activation duration was reached.
	 */
	if(priv->wasHandledRecently) return(G_SOURCE_CONTINUE);

	/* We know the time the pointer entered hot corner. Check if pointer have stayed
	 * in hot corner for the duration to activate/suspend application. If duration
	 * was not reached yet, just return immediately.
	 */
	currentTime=g_date_time_new_now_local();
	timeDiff=g_date_time_difference(currentTime, priv->enteredTime);
	g_date_time_unref(currentTime);

	if(timeDiff<priv->activationMicroseconds) return(G_SOURCE_CONTINUE);

	/* Activation duration reached so activate application if suspended or suspend it
	 * if active currently.
	 */
	if(!xfdashboard_application_is_suspended(priv->application))
	{
		xfdashboard_application_suspend_or_quit(priv->application);
	}
		else
		{
			g_application_activate(G_APPLICATION(priv->application));
		}

	/* Set flag that activation was handled recently */
	priv->wasHandledRecently=TRUE;

	return(G_SOURCE_CONTINUE);
}

/* IMPLEMENTATION: GObject */

/* Dispose this object */
static void _xfdashboard_hot_corner_dispose(GObject *inObject)
{
	XfdashboardHotCorner			*self=XFDASHBOARD_HOT_CORNER(inObject);
	XfdashboardHotCornerPrivate		*priv=self->priv;

	/* Release allocated resources */
	if(priv->enteredTime)
	{
		g_date_time_unref(priv->enteredTime);
		priv->enteredTime=NULL;
	}

	if(priv->windowTracker)
	{
		g_object_unref(priv->windowTracker);
		priv->windowTracker=NULL;
	}

	if(priv->timeoutID)
	{
		g_source_remove(priv->timeoutID);
		priv->timeoutID=0;
	}

	/* Call parent's class dispose method */
	G_OBJECT_CLASS(xfdashboard_hot_corner_parent_class)->dispose(inObject);
}

/* Class initialization
 * Override functions in parent classes and define properties
 * and signals
 */
void xfdashboard_hot_corner_class_init(XfdashboardHotCornerClass *klass)
{
	GObjectClass			*gobjectClass=G_OBJECT_CLASS(klass);

	/* Override functions */
	gobjectClass->dispose=_xfdashboard_hot_corner_dispose;

	/* Set up private structure */
	g_type_class_add_private(klass, sizeof(XfdashboardHotCornerPrivate));
}

/* Class finalization */
void xfdashboard_hot_corner_class_finalize(XfdashboardHotCornerClass *klass)
{
}

/* Object initialization
 * Create private structure and set up default values
 */
void xfdashboard_hot_corner_init(XfdashboardHotCorner *self)
{
	XfdashboardHotCornerPrivate		*priv;
	GdkScreen						*screen;
	GdkDisplay						*display;

	self->priv=priv=XFDASHBOARD_HOT_CORNER_GET_PRIVATE(self);

	/* Set up default values */
	priv->hotCornerArea=XFDASHBOARD_HOT_CORNER_AREA_TOP_LEFT;
	priv->activationRadius=POLL_POINTER_ACTIVATION_RADIUS;

	priv->windowTracker=xfdashboard_window_tracker_get_default();
	priv->rootWindow=NULL;
	priv->deviceManager=NULL;

	priv->timeoutID=0;
	priv->enteredTime=NULL;
	priv->activationMicroseconds=POLL_POINTER_ACTIVATION_MILLISECONDS*G_TIME_SPAN_MILLISECOND;
	priv->wasHandledRecently=FALSE;
	priv->application=xfdashboard_application_get_default();

	/* Get device manager for polling pointer position */
	if(xfdashboard_application_is_daemonized(priv->application))
	{
		screen=gdk_screen_get_default();
		priv->rootWindow=gdk_screen_get_root_window(screen);
		if(priv->rootWindow)
		{
			display=gdk_window_get_display(priv->rootWindow);
			priv->deviceManager=gdk_display_get_device_manager(display);
		}
			else
			{
				g_critical(_("Disabling hot-corner plugin because the root window to determine pointer position could not be found."));
			}

		if(priv->deviceManager)
		{
			/* Start polling pointer position */
			priv->timeoutID=g_timeout_add(POLL_POINTER_POSITION_INTERVAL,
											(GSourceFunc)_xfdashboard_hot_corner_check_hot_corner,
											self);
		}
			else
			{
				g_critical(_("Disabling hot-corner plugin because the device manager to determine pointer position could not be found."));
			}
	}
		else
		{
			g_warning(_("Disabling hot-corner plugin because application is not running as daemon."));
		}
}

XfdashboardHotCorner* xfdashboard_hot_corner_new(void)
{
	GObject		*hotCorner;

	hotCorner=g_object_new(XFDASHBOARD_TYPE_HOT_CORNER, NULL);
	if(!hotCorner) return(NULL);

	return(XFDASHBOARD_HOT_CORNER(hotCorner));
}