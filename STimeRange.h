/*
 *  STimeRange
 *  Represent a range of seconds relative to the Unix epoch
 *
 */

#ifndef __STIMERANGE_H__
#define __STIMERANGE_H__

#include "config.h"

bool STimeRangeParseDateAndTime(const char *dateTimeStr, time_t *outTimestamp);

enum {
    kSTimeRangeJustifyTimeToMinutes = 0,
    kSTimeRangeJustifyTimeToHours = 1,
    kSTimeRangeJustifyTimeToDays = 2
};
typedef unsigned int STimeRangeJustifyTimeTo;

time_t STimeRangeJustifyTime(time_t theTime, STimeRangeJustifyTimeTo justifyTo, bool roundUp);

typedef struct STimeRange const * STimeRangeRef;

extern const STimeRangeRef STimeRangeInvalid;
extern const STimeRangeRef STimeRangeInfinite;

STimeRangeRef STimeRangeCreate(time_t start, time_t end);
STimeRangeRef STimeRangeCreateWithEnd(time_t end);
STimeRangeRef STimeRangeCreateWithStart(time_t start);
STimeRangeRef STimeRangeCreateWithStartAndDuration(time_t start, time_t duration);
STimeRangeRef STimeRangeCreateWithString(const char *timeRangeStr, const char* *outEndPtr);

STimeRangeRef STimeRangeCopy(STimeRangeRef aTimeRange);

void STimeRangeRelease(STimeRangeRef aTimeRange);
STimeRangeRef STimeRangeRetain(STimeRangeRef aTimeRange);

bool STimeRangeIsValid(STimeRangeRef aTimeRange);

bool STimeRangeIsStartTimeSet(STimeRangeRef aTimeRange);
bool STimeRangeGetStartTime(STimeRangeRef aTimeRange, time_t *startTime);

bool STimeRangeIsEndTimeSet(STimeRangeRef aTimeRange);
bool STimeRangeGetEndTime(STimeRangeRef aTimeRange, time_t *endTime);

bool STimeRangeIsFullyBounded(STimeRangeRef aTimeRange);
bool STimeRangeGetDuration(STimeRangeRef aTimeRange, time_t *duration);

const char* STimeRangeGetCString(STimeRangeRef aTimeRange);

bool STimeRangeContainsTime(STimeRangeRef aTimeRange, time_t theTime);

bool STimeRangeIsEqual(STimeRangeRef aTimeRange, STimeRangeRef anotherTimeRange);
bool STimeRangeDoesIntersect(STimeRangeRef aTimeRange, STimeRangeRef anotherTimeRange);
bool STimeRangeIsContained(STimeRangeRef aTimeRange, STimeRangeRef inAnotherTimeRange);
bool STimeRangeIsContiguous(STimeRangeRef aTimeRange, STimeRangeRef anotherTimeRange);
bool STimeRangeDoesTimePrecedeRange(STimeRangeRef aTimeRange, time_t aTime);
bool STimeRangeDoesTimeFollowRange(STimeRangeRef aTimeRange, time_t aTime);

STimeRangeRef STimeRangeIntersection(STimeRangeRef aTimeRange, STimeRangeRef anotherTimeRange);
STimeRangeRef STimeRangeUnion(STimeRangeRef aTimeRange, STimeRangeRef anotherTimeRange);
STimeRangeRef STimeRangeJoin(STimeRangeRef aTimeRange, STimeRangeRef anotherTimeRange);
bool STimeRangeClipToTimeRange(STimeRangeRef clipThis, STimeRangeRef toThis);
STimeRangeRef STimeRangeLeading(STimeRangeRef haystack, STimeRangeRef needle);
STimeRangeRef STimeRangeLeadingBeforeTime(STimeRangeRef haystack, time_t beforeTime);
STimeRangeRef STimeRangeTrailing(STimeRangeRef haystack, STimeRangeRef needle);
STimeRangeRef STimeRangeTrailingAfterTime(STimeRangeRef haystack, time_t afterTime);

int STimeRangeCmp(STimeRangeRef lhs, STimeRangeRef rhs);
int STimeRangeRightCmpToTime(STimeRangeRef lhs, time_t rhs);
int STimeRangeLeftCmpToTime(time_t lhs, STimeRangeRef rhs);

unsigned int STimeRangeGetCountOfPeriodsOfLength(STimeRangeRef aTimeRange, time_t duration);
STimeRangeRef STimeRangeGetPeriodOfLengthAtIndex(STimeRangeRef aTimeRange, time_t duration, unsigned int index);

#endif /* __STIMERANGE_H__ */
