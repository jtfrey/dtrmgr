/*
 *  STimeRange
 *
 *  Represent a range of seconds relative to the Unix epoch
 *
 */

#ifndef __STIMERANGE_H__
#define __STIMERANGE_H__

#include "config.h"

/*!
 * @function STimeRangeParseDateAndTime
 *
 * Parse a date-time string with various possible formats:
 *
 *   - now, today, yesterday, tomorrow
 *   - YYYYMMDDTHHMMSS±HHMM
 *   - YYYYMMDDTHHMMSS±HH
 *   - YYYYMMDDTHHMMSS
 *   - YYYYMMDDTHHMM
 *   - YYYYMMDD
 *
 * If successful and outTimestamp is not NULL, outTimestamp is set to
 * the parsed value.
 *
 * @return Boolean true if successful, false otherwise.
 */
bool STimeRangeParseDateAndTime(const char *dateTimeStr, time_t *outTimestamp);

/*!
 * @enum STimeRangeJustifyTimeTo
 *
 * Enumerated type that indicates to what precision a time should be rounded.
 * Rounding direction is not implied.
 *
 * @constant kSTimeRangeJustifyTimeToMinutes
 *      Round seconds to minutes
 * @constant kSTimeRangeJustifyTimeToMinutes
 *      Round seconds to minutes, minutes to hours
 * @constant kSTimeRangeJustifyTimeToMinutes
 *      Round seconds to minutes, minutes to hours, hours to days
 */
enum {
    kSTimeRangeJustifyTimeToMinutes = 0,
    kSTimeRangeJustifyTimeToHours = 1,
    kSTimeRangeJustifyTimeToDays = 2
};
typedef unsigned int STimeRangeJustifyTimeTo;

/*!
 * @function STimeRangeJustifyTime
 *
 * Round the given Unix timestamp to the given justification (minutes, hours, days).  If
 * roundUp is true, any non-zero field is set to zero and the next level field incremented
 * (e.g. seconds=3 -> seconds=0, minutes++).
 *
 * @return The altered time value.
 */
time_t STimeRangeJustifyTime(time_t theTime, STimeRangeJustifyTimeTo justifyTo, bool roundUp);

/*!
 * @typedef STimeRangeRef
 *
 * Type of a reference to a STimeRange object.
 */
typedef struct STimeRange const * STimeRangeRef;

/*!
 * @constant STimeRangeInvalid
 *
 * Shared STimeRange instance representing an invalid time range.
 */
extern const STimeRangeRef STimeRangeInvalid;

/*!
 * @constant STimeRangeInfinite
 *
 * Shared STimeRange instance representing a time range with no lower or upper bound.
 */
extern const STimeRangeRef STimeRangeInfinite;

/*!
 * @function STimeRangeCreate
 *
 * Returns a reference to a STimeRange with the given start and end timestamp.
 *
 * @return A reference to an STimeRange object or NULL on a memory error.
 */
STimeRangeRef STimeRangeCreate(time_t start, time_t end);
/*!
 * @function STimeRangeCreateWithEnd
 *
 * Returns a reference to a STimeRange with the given end timestamp and no lower bound.
 *
 * @return A reference to an STimeRange object or NULL on a memory error.
 */
STimeRangeRef STimeRangeCreateWithEnd(time_t end);
/*!
 * @function STimeRangeCreateWithStart
 *
 * Returns a reference to a STimeRange with the given start timestamp and no upper bound.
 *
 * @return A reference to an STimeRange object or NULL on a memory error.
 */
STimeRangeRef STimeRangeCreateWithStart(time_t start);
/*!
 * @function STimeRangeCreateWithStartAndDuration
 *
 * Returns a reference to a STimeRange with the given start timestamp and a duration (in
 * seconds).
 *
 * @return A reference to an STimeRange object or NULL on a memory error.
 */
STimeRangeRef STimeRangeCreateWithStartAndDuration(time_t start, time_t duration);
/*!
 * @function STimeRangeCreateWithString
 *
 * Attempt to parse the given date-time range string and return a reference to a STimeRange
 * representing that range.  If outEndPtr is not NULL, it is set to the address of the
 * character following the parsed portion of timeRangeStr.
 *
 * @return A reference to an STimeRange object (possibly STimeRangeInvalid or
 *     STimeRangeInfinite) or NULL on a memory error.
 */
STimeRangeRef STimeRangeCreateWithString(const char *timeRangeStr, const char* *outEndPtr);
/*!
 * @function STimeRangeCopy
 *
 * Returns a reference to a STimeRange that is a duplicate of aTimeRange.
 *
 * @return A reference to an STimeRange object or NULL on a memory error.
 */
STimeRangeRef STimeRangeCopy(STimeRangeRef aTimeRange);
/*!
 * @function STimeRangeRelease
 *
 * Decrease the reference count of aTimeRange and deallocate if no more references
 * exist.
 */
void STimeRangeRelease(STimeRangeRef aTimeRange);
/*!
 * @function STimeRangeRetain
 *
 * Increase the reference count of aTimeRange.
 */
STimeRangeRef STimeRangeRetain(STimeRangeRef aTimeRange);

/*!
 * @function STimeRangeIsValid
 *
 * Test whether aTimeRange represents a valid time range.
 *
 * @return Boolean true if valid, false otherwise.
 */
bool STimeRangeIsValid(STimeRangeRef aTimeRange);

/*!
 * @function STimeRangeIsStartTimeSet
 *
 * Test whether aTimeRange has a lower bound.
 *
 * @return Boolean true if aTimeRange has a start time, false otherwise.
 */
bool STimeRangeIsStartTimeSet(STimeRangeRef aTimeRange);
/*!
 * @function STimeRangeGetStartTime
 *
 * Copy the start time of aTimeRange to startTime if aTimeRange has a
 * lower bound.
 *
 * @return Boolean true if aTimeRange has a start time, false otherwise.
 */
bool STimeRangeGetStartTime(STimeRangeRef aTimeRange, time_t *startTime);
/*!
 * @function STimeRangeIsEndTimeSet
 *
 * Test whether aTimeRange has an upper bound.
 *
 * @return Boolean true if aTimeRange has an end time, false otherwise.
 */
bool STimeRangeIsEndTimeSet(STimeRangeRef aTimeRange);
/*!
 * @function STimeRangeGetEndTime
 *
 * Copy the end time of aTimeRange to endTime if aTimeRange has an
 * upper bound.
 *
 * @return Boolean true if aTimeRange has an end time, false otherwise.
 */
bool STimeRangeGetEndTime(STimeRangeRef aTimeRange, time_t *endTime);
/*!
 * @function STimeRangeIsFullyBounded
 *
 * Test whether aTimeRange has both upper and lower bounds.
 *
 * @return Boolean true if both bounds are set, false otherwise.
 */
bool STimeRangeIsFullyBounded(STimeRangeRef aTimeRange);

/*!
 * @function STimeRangeGetDuration
 *
 * For a fully-bounded aTimeRange, sets duration to  (end - start + 1).
 *
 * @return Boolean true if duration was set (implies aTimeRange is fully
 *    bounded), false otherwise.
 */
bool STimeRangeGetDuration(STimeRangeRef aTimeRange, time_t *duration);

/*!
 * @function STimeRangeGetCString
 *
 * Returns a pointer to a C string containing the textual representation
 * of aTimeRange.
 *
 * @return Pointer to a C string in a buffer owned by aTimeRange.
 */
const char* STimeRangeGetCString(STimeRangeRef aTimeRange);

/*!
 * @function STimeRangeContainsTime
 *
 * Test whether theTime is in the range represented by aTimeRange.
 *
 * @return Boolean true if theTime is in the range, false otherwise.
 */
bool STimeRangeContainsTime(STimeRangeRef aTimeRange, time_t theTime);
/*!
 * @function STimeRangeDoesTimePrecedeRange
 *
 * Test whether aTime occurs prior to the lower-bounded time range in aTimeRange.
 *
 * @return Boolean true if aTime occurs before aTimeRange, false otherwise.
 */
bool STimeRangeDoesTimePrecedeRange(STimeRangeRef aTimeRange, time_t aTime);
/*!
 * @function STimeRangeDoesTimeFollowRange
 *
 * Test whether aTime occurs after the upper-bounded time range in aTimeRange.
 *
 * @return Boolean true if aTime occurs after aTimeRange, false otherwise.
 */
bool STimeRangeDoesTimeFollowRange(STimeRangeRef aTimeRange, time_t aTime);

/*!
 * @function STimeRangeIsEqual
 *
 * Test whether two time ranges are equivalent.
 *
 * @return Boolean true if the two ranges are the same, false otherwise.
 */
bool STimeRangeIsEqual(STimeRangeRef aTimeRange, STimeRangeRef anotherTimeRange);
/*!
 * @function STimeRangeDoesIntersect
 *
 * Test whether two time ranges overlap one another.
 *
 * @return Boolean true if the two ranges overlap, false otherwise.
 */
bool STimeRangeDoesIntersect(STimeRangeRef aTimeRange, STimeRangeRef anotherTimeRange);
/*!
 * @function STimeRangeIsContained
 *
 * Test whether aTimeRange is completely contained within inAnotherTimeRange.
 *
 * @return Boolean true if aTimeRange is contained within inAnotherTimeRange, false
 *    otherwise.
 */
bool STimeRangeIsContained(STimeRangeRef aTimeRange, STimeRangeRef inAnotherTimeRange);
/*!
 * @function STimeRangeIsContiguous
 *
 * Test whether the two time ranges do not intersect but occur one after the other
 * with no seconds between them.
 *
 * @return Boolean true if the ranges are contiguous, false otherwise.
 */
bool STimeRangeIsContiguous(STimeRangeRef aTimeRange, STimeRangeRef anotherTimeRange);

/*!
 * @function STimeRangeIntersection
 *
 * Create a new STimeRange representing the intersection of aTimeRange and anotherTimeRange.
 *
 * @return A reference to the overlap range, STimeRangeInvalid if the two do not intersect,
 *    or NULL on memory error.
 */
STimeRangeRef STimeRangeIntersection(STimeRangeRef aTimeRange, STimeRangeRef anotherTimeRange);
/*!
 * @function STimeRangeUnion
 *
 * Create a new STimeRange representing the union of aTimeRange and anotherTimeRange if
 * they intersect.
 *
 * @return A reference to the union range, STimeRangeInvalid if the two do not intersect,
 *    or NULL on memory error.
 */
STimeRangeRef STimeRangeUnion(STimeRangeRef aTimeRange, STimeRangeRef anotherTimeRange);
/*!
 * @function STimeRangeJoin
 *
 * Create a new STimeRange if aTimeRange and anotherTimeRange are contiguous ranges
 * representing the range covered by the two together.
 *
 * @return A reference to the combined range, STimeRangeInvalid if the two are not
 *    contiguous, or NULL on memory error.
 */
STimeRangeRef STimeRangeJoin(STimeRangeRef aTimeRange, STimeRangeRef anotherTimeRange);
/*!
 * @function STimeRangeCreateByClippingToTimeRange
 *
 * Create a new STimeRange containing the portion of clipThis occuring inside the range
 * represented by toThis.
 *
 * @return A reference to the new range, STimeRangeInvalid if the two do not intersect,
 *    or NULL on memory error.
 */
STimeRangeRef STimeRangeCreateByClippingToTimeRange(STimeRangeRef clipThis, STimeRangeRef toThis);
/*!
 * @function STimeRangeLeading
 *
 * Create a new STimeRange containing the portion of haystack occuring before the range
 * represented by needle.
 *
 * @return A reference to the new range, STimeRangeInvalid if the two do not intersect,
 *    or NULL on memory error.
 */
STimeRangeRef STimeRangeLeading(STimeRangeRef haystack, STimeRangeRef needle);
/*!
 * @function STimeRangeLeadingBeforeTime
 *
 * Create a new STimeRange containing the portion of haystack occuring before the given
 * time.
 *
 * @return A reference to the new range, STimeRangeInvalid if the range would be empty,
 *    or NULL on memory error.
 */
STimeRangeRef STimeRangeLeadingBeforeTime(STimeRangeRef haystack, time_t beforeTime);
/*!
 * @function STimeRangeTrailing
 *
 * Create a new STimeRange containing the portion of haystack occuring after the range
 * represented by needle.
 *
 * @return A reference to the new range, STimeRangeInvalid if the two do not intersect,
 *    or NULL on memory error.
 */
STimeRangeRef STimeRangeTrailing(STimeRangeRef haystack, STimeRangeRef needle);
/*!
 * @function STimeRangeTrailingAfterTime
 *
 * Create a new STimeRange containing the portion of haystack occuring after the given
 * time.
 *
 * @return A reference to the new range, STimeRangeInvalid if the range would be empty,
 *    or NULL on memory error.
 */
STimeRangeRef STimeRangeTrailingAfterTime(STimeRangeRef haystack, time_t afterTime);

/*!
 * @function STimeRangeCmp
 *
 * Determine the ordering of two time ranges.
 *
 * @return A negative integer if (lhs > rhs); zero if the two are equal; and a positive
 *    integer if (lhs < rhs).
 */
int STimeRangeCmp(STimeRangeRef lhs, STimeRangeRef rhs);
/*!
 * @function STimeRangeRightCmpToTime
 *
 * Determine the ordering of a time range versus a timestamp.  The lower bound of lhs
 * is used in the determination.
 *
 * @return A negative integer if rhs occurs after the start of lhs; zero if rhs and the
 *    start of lhs are equal; a positive integer otherwise.
 */
int STimeRangeRightCmpToTime(STimeRangeRef lhs, time_t rhs);
/*!
 * @function STimeRangeLeftCmpToTime
 *
 * Determine the ordering of a time range versus a timestamp.  The lower bound of rhs
 * is used in the determination.
 *
 * @return A negative integer if lhs occurs before the start of rhs; zero if lhs and the
 *    start of rhs are equal; a positive integer otherwise.
 */
int STimeRangeLeftCmpToTime(time_t lhs, STimeRangeRef rhs);

/*!
 * @function STimeRangeGetCountOfPeriodsOfLength
 *
 * Determine the number of periods of length duration seconds contained in aTimeRange.  This
 * includes one fractional-length period if duration does not cleanly divide the range.
 *
 * For aTimeRange lacking an upper or lower bound, an infinite number of periods exist.
 *
 * @return Zero for an invalid range, UINT_MAX for a range not fully-bounded, a positive
 *    integer otherwise.
 */
unsigned int STimeRangeGetCountOfPeriodsOfLength(STimeRangeRef aTimeRange, time_t duration);

/*!
 * @function STimeRangeGetPeriodOfLengthAtIndex
 *
 * Returns an STimeRange representing the index-th period of length duration in aTimeRange.
 *
 * - For aTimeRange fully-bounded or with just a lower-bound, the 0th period occurs at the
 *   start
 * - For aTimeRange with just an upper-bound, the 0th period occurs at the end
 *
 * @return A reference to an STimePeriod or NULL if no period exists at the given index.
 */
STimeRangeRef STimeRangeGetPeriodOfLengthAtIndex(STimeRangeRef aTimeRange, time_t duration, unsigned int index);

#endif /* __STIMERANGE_H__ */
