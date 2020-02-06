/*
 *  SSchedule
 *
 *  Represents a time range and "scheduled" blocks of time within it.  Include
 *  functions to search for unused blocks of time within the schedule range.
 *
 */

#ifndef __SSCHEDULE_H__
#define __SSCHEDULE_H__

#include "STimeRange.h"

/*!
 * @typedef SScheduleRef
 *
 * Type of a reference to an SSchedule object.
 */
typedef struct SSchedule const * SScheduleRef;

/*!
 * @function SScheduleCreate
 *
 * Returns a reference to a new SSchedule with the given scheduling period.
 *
 * @return A reference to an SSchedule object or NULL on a memory error.
 */
SScheduleRef SScheduleCreate(STimeRangeRef period);
/*!
 * @function SScheduleCreateWithFile
 *
 * Returns a reference to a new SSchedule initialized with the data present
 * in filepath (where filepath should point to an SSchedule serialized using
 * SScheduleWriteToFile()).
 *
 * Each scheduled block of time in the file is passed to the
 * SScheduleAddScheduledBlock() so it can be validated and possibly coallesced
 * with other scheduled ranges.  If the veracity of filepath is guaranteed, the
 * SScheduleCreateWithFileQuick() function can be used to forego these expensive
 * sanity checks.
 *
 * @return A reference to an SSchedule object or NULL if any error occurred.
 */
SScheduleRef SScheduleCreateWithFile(const char *filepath);
/*!
 * @function SScheduleCreateWithFileQuick
 *
 * Returns a reference to a new SSchedule initialized with the data present
 * in filepath (where filepath should point to an SSchedule serialized using
 * SScheduleWriteToFile()).
 *
 * Scheduled blocks of time in the file are used as-is and in the order
 * dictated by the table.  If the veracity of filepath is not guaranteed, the
 * SScheduleCreateWithFile() function should be used to check all incoming
 * date-time ranges.
 *
 * @return A reference to an SSchedule object or NULL if any error occurred.
 */
SScheduleRef SScheduleCreateWithFileQuick(const char *filepath);

/*!
 * @function SScheduleRelease
 *
 * Decrease the reference count of aSchedule and deallocate once it reaches
 * zero.
 */
void SScheduleRelease(SScheduleRef aSchedule);
/*!
 * @function SScheduleRetain
 *
 * Increase the reference count of aSchedule.
 *
 * @return Returns a reference to aSchedule.
 */
SScheduleRef SScheduleRetain(SScheduleRef aSchedule);

/*!
 * @function SScheduleGetPeriod
 *
 * Retrieve the scheduling period for aSchedule.
 *
 * @return The object's reference to its scheduling period STimeRange object (caller
 *    must NOT release it)
 */
STimeRangeRef SScheduleGetPeriod(SScheduleRef aSchedule);
/*!
 * @function SScheduleGetBlockCount
 *
 * Determine how many scheduled blocks of time exist in aSchedule.
 *
 * @return The number of STimeRange blocks scheduled, or zero.
 */
unsigned int SScheduleGetBlockCount(SScheduleRef aSchedule);
/*!
 * @function SScheduleGetBlockAtIndex
 *
 * Retrieve the index-th STimeRange object representing a scheduled block of time.
 *
 * @return The object's reference to a scheduled time period (caller must NOT release
 *    it), NULL if index is not in range.
 */
STimeRangeRef SScheduleGetBlockAtIndex(SScheduleRef aSchedule, unsigned int index);

/*!
 * @function SScheduleGetLastErrorMessage
 *
 * Get a description of the last error that occurred in association with aSchedule.
 * Mostly useful when SScheduleWriteToFile() fails to get SQLite error descriptions,
 * for example.
 *
 * @return NULL if no errors are present or a pointer to a C string (owned by aSchedule)
 *    containing an error description.
 */
const char* SScheduleGetLastErrorMessage(SScheduleRef aSchedule);

/*!
 * @function SScheduleIsFull
 *
 * Test whether aSchedule has any time not yet scheduled.
 *
 * @return Boolean true if the scheduled block(s) of time in aSchedule do not completely
 *    cover its scheduling period, false otherwise.
 */
bool SScheduleIsFull(SScheduleRef aSchedule);

/*!
 * @function SScheduleGetNextOpenBlock
 *
 * Locate the next block of time in the scheduling period of aSchedule for which there are
 * no scheduled blocks.
 *
 * @return A reference to a STimeRange representing an unscheduled block of time, NULL
 *    otherwise.
 */
STimeRangeRef SScheduleGetNextOpenBlock(SScheduleRef aSchedule);
/*!
 * @function SScheduleGetNextOpenBlockBeforeTime
 *
 * Locate the next block of time in the scheduling period of aSchedule for which there are
 * no scheduled blocks.  The resulting block of time must occur BEFORE beforeTime.
 *
 * Most useful given aSchedule whose scheduling period has no upper bound but we don't
 * wish to allocate time in the future (which describes the application that spawned this
 * code).
 *
 * @return A reference to a STimeRange representing an unscheduled block of time, NULL
 *    otherwise.
 */
STimeRangeRef SScheduleGetNextOpenBlockBeforeTime(SScheduleRef aSchedule, time_t beforeTime);

/*!
 * @function SScheduleAddScheduledBlock
 *
 * Mark as "scheduled" any time in scheduledBlock that intersects the scheduling
 * period of aSchedule.
 *
 * @return Boolean true if scheduledBlock was successfully absorbed, false otherwise.
 */
bool SScheduleAddScheduledBlock(SScheduleRef aSchedule, STimeRangeRef scheduledBlock);

/*!
 * @function SScheduleWriteToFile
 *
 * Serialize aSchedule to an SQLite3 database at filepath.
 *
 * Most failures will set the lastErrorMessage of aSchedule with descriptive (maybe even
 * informative) information about the failure.
 *
 * @return Boolean true if successful, false otherwise.
 */
bool SScheduleWriteToFile(SScheduleRef aSchedule, const char *filepath);

/*!
 * @function SScheduleSummarize
 *
 * Write a summary of aSchedule to the given i/o stream.
 */
void SScheduleSummarize(SScheduleRef aSchedule, FILE *outStream);

#endif /* __SSCHEDULE_H__ */
