/*
 *  SSchedule
 *  Represents a time range and "scheduled" blocks of time within it.  Include
 *  functions to search for unused blocks of time within the schedule range.
 *
 */

#ifndef __SSCHEDULE_H__
#define __SSCHEDULE_H__

#include "STimeRange.h"

typedef struct SSchedule const * SScheduleRef;

SScheduleRef SScheduleCreate(STimeRangeRef period);
SScheduleRef SScheduleCreateWithFile(const char *filepath);
SScheduleRef SScheduleCreateWithFileQuick(const char *filepath);

void SScheduleRelease(SScheduleRef aSchedule);
SScheduleRef SScheduleRetain(SScheduleRef aSchedule);

STimeRangeRef SScheduleGetPeriod(SScheduleRef aSchedule);
unsigned int SScheduleGetBlockCount(SScheduleRef aSchedule);
STimeRangeRef SScheduleGetBlock(SScheduleRef aSchedule, unsigned int index);

const char* SScheduleGetLastErrorMessage(SScheduleRef aSchedule);

bool SScheduleIsFull(SScheduleRef aSchedule);
STimeRangeRef SScheduleGetNextOpenBlock(SScheduleRef aSchedule);
STimeRangeRef SScheduleGetNextOpenBlockBefore(SScheduleRef aSchedule, time_t beforeTime);

bool SScheduleAddScheduledBlock(SScheduleRef aSchedule, STimeRangeRef scheduledBlock);
bool SScheduleRemoveScheduledBlock(SScheduleRef aSchedule, STimeRangeRef scheduledBlock);

bool SScheduleWriteToFile(SScheduleRef aSchedule, const char *filepath);

void SScheduleSummarize(SScheduleRef aSchedule, FILE *outStream);

#endif /* __SSCHEDULE_H__ */
