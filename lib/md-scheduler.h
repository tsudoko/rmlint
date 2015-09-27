/*
 *  This file is part of rmlint.
 *
 *  rmlint is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  rmlint is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with rmlint.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *
 *  - Christopher <sahib> Pahl 2010-2015 (https://github.com/sahib)
 *  - Daniel <SeeSpotRun> T.   2014-2015 (https://github.com/SeeSpotRun)
 *
 * Hosted on http://github.com/sahib/rmlint
 *
 */

#ifndef RM_MD_SCHEDULER_H
#define RM_MD_SCHEDULER_H

#include <glib.h>
#include "config.h"
#include "utilities.h"
#include "session.h"

/**
 * @file md-scheduler.h
 * @brief Multi-disk scheduler/optimiser API for IO related tasks.
 *
 * A multi-disk scheduler for improving speed of IO-related tasks.
 * Tasks pushed to the scheduler are sorted according to associated
 * physical disk/device.  A worker thread is created for each physical
 * disk.
 *
 * Tasks sent to each worker thread are queued and processed in order
 * according to a prioritisation function (eg an elevator algorithm
 * based on disk offsets).
 *
 * Device workers are reference-counted, which may be useful eg in cases
 * where there are known future tasks on a device, eg tasks which can't
 * be started until other tasks have completed.
 *
 * Typical workflow:
 *
 *     RmMDS *mds = rm_mds_new(task_callback, max_threads, sorter, user_data);
 *     while (task) {
 *         rm_mds_push_path(mds, path, task_user_data);
 *     }
 *     rm_mds_free(mds);
 *
 *
 * If the tasks inode, dev and disk are known,
 * then rm_mds_push_dev(mds, disk, dev, node, user_data) may be
 * used instead of rm_mds_push_path().
 *
 **/

/**
 * @struct RmSheduler
 *
 * RmSheduler is an opaque data structure to represent a scheduling engine.
 * It should be created and destroyed via rm_mds_new() and
 * rm_mds_free().
 *
 **/
typedef struct _RmMDS RmMDS;

/**
 * @struct RmTask
 *
 **/
typedef struct RmMDSTask {
    dev_t dev;
    guint64 offset;
    gpointer task_data;
} RmMDSTask;

/**
 * @brief RmMDSFunc function prototype, called for each task
 *
 * @param task_user_data User data passed via rm_mds_push_...()
 * @param session_user_data User data passed to rm_mds_new()
 * @retval amount to decrement device worker's per-pass quota
 *
 * The number of tasks processed on each pass through a device can be
 * limited by a quota.  This may be useful in cases where the same file
 * is accessed several times in quick succession, since the file
 * metadata and data may still be cached.
 *
 **/
typedef gint (*RmMDSFunc)(RmMDSTask *task, gpointer session_user_data);

/**
 * @brief RmMDSTask task prioritisation function prototype
 *
 * @param dev_a  Device number for task a
 * @param dev_b  Device number for task b
 * @param off_a  Device offset bytes for task a
 * @param dev_a  Device offset bytes for task b
 * @param data_a User data for task a
 * @param data_b User data for task b
 * @retval negative value or zero if a should be processed before b, positive value if b
 *should be processed before a
 **/
typedef gint (*RmMDSSortFunc)(const RmMDSTask *task_a, const RmMDSTask *task_b);

/**
 * @brief Allocate and initialise a new MDS scheduler
 *
 * @param max_threads  Maximum number of concurrent device threads
 * @param mount_table RmMountTable to use; if NULL then will create new one
 *
 * Scheduler is initially paused.
 * session_user_data will be passed to task callback.
 *
 **/
RmMDS *rm_mds_new(const gint max_threads, RmMountTable *mount_table);

/**
 * @brief Configure or reconfigure a MDS scheduler
 *
 * @param func The callback function called for each task
 * @param user_data Pointer to user data associated with the scheduler
 * @param pass_quota  Quota tasks per pass (refer RmMDSTask)
 * @param prioritiser  Compare function for prioritising
 *
 **/
void rm_mds_configure(RmMDS *self,
                      const RmMDSFunc func,
                      const gpointer user_data,
                      const gint pass_quota,
                      RmMDSSortFunc prioritiser);

/**
 * @brief start a paused MDS scheduler
 **/
void rm_mds_start(RmMDS *mds);

/**
 * @brief abort a MDS session
 **/
void rm_mds_abort(RmMDS *mds);

/**
 * @brief Wait for all RmMDS scheduler tasks to finish
 *
 * @param mds Pointer to the MDS scheduler
 **/
void rm_mds_finish(RmMDS *mds);

/**
 * @brief Wait for all tasks to finish then free the RmMDS scheduler
 *
 * @param mds Pointer to the MDS scheduler
 * @param free_mount_table Whether to free the scheduler's mount table
 **/
void rm_mds_free(RmMDS *mds, const gboolean free_mount_table);

/**
 * @brief Return pointer to the RmMountTable of a MDS scheduler
 *
 * @param mds Pointer to the MDS scheduler
 **/
RmMountTable *rm_mds_get_mount_table(const RmMDS *mds);

/**
 * @brief increase or decrease MDS reference count for the disk associated with a path
 *
 * @param mds Pointer to the MDS scheduler
 * @param path The file of folder path
 * @param ref_count The amount to increase/decrease reference count
 **/
void rm_mds_ref_path(RmMDS *mds, const char *path, const gint ref_count);

/**
 * @brief increase or decrease MDS reference count for a disk or dev
 *
 * @param mds Pointer to the scheduler object
 * @param dev The dev or disk ID
 * @param ref_count The amount to increase/decrease reference count
 **/
void rm_mds_ref_dev(RmMDS *mds, const dev_t dev, const gint ref_count);

/**
 * @brief Push a new task to a RmMDS scheduler, based on file/folder path.
 *
 * The scheduler will look up the path's dev and disk.  If these are already known, it
 * is more efficient to use rm_mds_push_dev().
 *
 * @param mds Pointer to the MDS scheduler
 * @param path  The file of folder path
 * @param offset Physical offset of file on dev (-1 triggers lookup)
 * @param task_user_data Pointer to user data associated with the task.
 **/
void rm_mds_push_task_by_path(RmMDS *mds,
                              const char *path,
                              gint64 offset,
                              const gpointer task_data);

/**
 * @brief Push a new task to a RmMDS scheduler, based on disk or dev ID.
 *
 * @param mds Pointer to the RmMDS scheduler object
 * @param dev The dev or disk ID
 * @param offset Physical offset of file on dev (-1 triggers lookup)
 * @param path Optional file path (required for lookup)
 * @param task_user_data Pointer to user data associated with the task.
 **/
void rm_mds_push_task_by_dev(RmMDS *mds,
                             const dev_t dev,
                             gint64 offset,
                             const char *path,
                             const gpointer task_data);

/**
 * @brief prioritiser function for basic elevator algorithm
 **/
gint rm_mds_elevator_cmp(const RmMDSTask *task_a, const RmMDSTask *task_b);

// TODO: sort function based on most recently accessed dev/inode??

#endif /* end of include guard */