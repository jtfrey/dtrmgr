/*
 *  STimeRange
 *
 *  Represent a range of seconds relative to the Unix epoch
 *
 */

#include "STimeRange.h"

//

const char *__STimeRangeDateTimeFormat = "%Y%m%dT%H%M%S%z";
const size_t __STimeRangeDateTimeFormatMaxOutLen = 4 + 2 + 2 + 1 + 2 + 2 + 2 + 5 + 1;

//

time_t
STimeRangeJustifyTime(
    time_t                  theTime,
    STimeRangeJustifyTimeTo justifyTo,
    bool                    roundUp
)
{
    struct tm               dateTimeComponents;

    if ( localtime_r(&theTime, &dateTimeComponents) ) {
        dateTimeComponents.tm_isdst = -1;
        if ( roundUp ) {
            if ( dateTimeComponents.tm_sec > 0 ) dateTimeComponents.tm_min++;
            dateTimeComponents.tm_sec = 0;
            if ( justifyTo > kSTimeRangeJustifyTimeToMinutes ) {
                if ( dateTimeComponents.tm_min > 0 ) dateTimeComponents.tm_hour++;
                dateTimeComponents.tm_min = 0;
                if ( justifyTo > kSTimeRangeJustifyTimeToHours ) {
                    if ( dateTimeComponents.tm_hour > 0 ) dateTimeComponents.tm_mday++;
                    dateTimeComponents.tm_hour = 0;
                }
            }
        } else {
            dateTimeComponents.tm_sec = 0;
            if ( justifyTo > kSTimeRangeJustifyTimeToMinutes ) {
                dateTimeComponents.tm_min = 0;
                if ( justifyTo > kSTimeRangeJustifyTimeToHours ) {
                    dateTimeComponents.tm_hour = 0;
                }
            }
        }
        return mktime(&dateTimeComponents);
    }
    return 0;
}

//

const char* const STimeRangeParseFormats[] = {
        "%Y%m%dT%H%M%S%z",
        "%Y%m%dT%H%M%S",
        "%Y%m%dT%H%M",
        "%Y%m%d",
        NULL
    };

bool
STimeRangeParseDateAndTime(
    const char  *dateTimeStr,
    time_t      *outTimestamp
)
{
    char        *endptr;
    struct tm   dateTimeComponents;
    bool        okay = false;

    if ( strcasecmp(dateTimeStr, "now") == 0 ) {
        if ( outTimestamp ) *outTimestamp = time(NULL);
        return true;
    }
    if ( strcasecmp(dateTimeStr, "today") == 0 ) {
        if ( outTimestamp ) *outTimestamp = STimeRangeJustifyTime(time(NULL), kSTimeRangeJustifyTimeToDays, false);
        return true;
    }
    if ( strcasecmp(dateTimeStr, "yesterday") == 0 ) {
        if ( outTimestamp ) *outTimestamp = STimeRangeJustifyTime(time(NULL) - 86400, kSTimeRangeJustifyTimeToDays, false);
        return true;
    }
    if ( strcasecmp(dateTimeStr, "tomorrow") == 0 ) {
        if ( outTimestamp ) *outTimestamp = STimeRangeJustifyTime(time(NULL) + 86400, kSTimeRangeJustifyTimeToDays, false);
        return true;
    }

    const char* const   *formats = STimeRangeParseFormats;

    while ( *formats ) {
        endptr = strptime(dateTimeStr, __STimeRangeDateTimeFormat, &dateTimeComponents);
        if ( endptr <= dateTimeStr ) {
            if ( outTimestamp ) *outTimestamp = mktime(&dateTimeComponents);
            return true;
        }
        formats++;
    }
    return false;
}

//

enum {
    kSTimeRangeIsStatic         = 1 << 0,
    kSTimeRangeIsConst          = 1 << 1,
    kSTimeRangeIsValid          = 1 << 2,
    kSTimeRangeHasLowerBound    = 1 << 3,
    kSTimeRangeHasUpperBound    = 1 << 4,
    kSTimeRangeOptionMax,
    kSTimeRangeOptionMin        = kSTimeRangeIsStatic,
    //
    kSTimeRangeBoundsMask       = kSTimeRangeHasLowerBound | kSTimeRangeHasUpperBound
};

const char* __STimeRangeOptionNames[] = {
        "static",
        "const",
        "valid",
        "lower-bound",
        "upper-bound"
    };

typedef struct STimeRange {
    uint32_t    refcount;
    time_t      start, end;
    uint32_t    options;
    const char  *cstr;
} STimeRange;

//

const STimeRange __STimeRangeInvalid = {
                            .refcount = 1,
                            .start = 0,
                            .end = 0,
                            .options = kSTimeRangeIsStatic | kSTimeRangeIsConst,
                            .cstr = "<invalid>"
                        };
const STimeRangeRef STimeRangeInvalid = (STimeRangeRef)&__STimeRangeInvalid;

const STimeRange __STimeRangeInfinite = {
                            .refcount = 1,
                            .start = 0,
                            .end = 0,
                            .options = kSTimeRangeIsStatic | kSTimeRangeIsConst | kSTimeRangeIsValid,
                            .cstr = "-"
                        };
const STimeRangeRef STimeRangeInfinite = (STimeRangeRef)&__STimeRangeInfinite;

//

#ifndef STIMERANGE_QUICKSTORE_CAPACITY
#define STIMERANGE_QUICKSTORE_CAPACITY 32
#endif

static unsigned int __STimeRangeQuickStoreCapacity = STIMERANGE_QUICKSTORE_CAPACITY;
static STimeRange __STimeRangeQuickStore[STIMERANGE_QUICKSTORE_CAPACITY];
static bool __STimeRangeQuickStoreMark[STIMERANGE_QUICKSTORE_CAPACITY];
static unsigned int __STimeRangeQuickStoreAllocateCount = 0;
static bool __STimeRangeQuickStoreIsInited = false;

void
__STimeRangeQuickStoreInit(void)
{
    if ( ! __STimeRangeQuickStoreIsInited ) {
        unsigned int    i = 0;

        while ( i < STIMERANGE_QUICKSTORE_CAPACITY ) __STimeRangeQuickStoreMark[i++] = false;
        __STimeRangeQuickStoreIsInited = true;
    }
}

STimeRange*
__STimeRangeQuickStoreAlloc(void)
{
    if ( __STimeRangeQuickStoreAllocateCount < STIMERANGE_QUICKSTORE_CAPACITY ) {
        unsigned int    i = 0;

        __STimeRangeQuickStoreInit();
        while ( i < STIMERANGE_QUICKSTORE_CAPACITY ) {
            if ( ! __STimeRangeQuickStoreMark[i] ) {
                __STimeRangeQuickStoreMark[i] = true;
                __STimeRangeQuickStoreAllocateCount++;
                __STimeRangeQuickStore[i].options = kSTimeRangeIsStatic;
                return &__STimeRangeQuickStore[i];
            }
            i++;
        }
    }
    return NULL;
}

void
__STimeRangeQuickStoreDealloc(
    STimeRange  *aTimeRange
)
{
    unsigned int    i = aTimeRange - &__STimeRangeQuickStore[0];

    if ( i < STIMERANGE_QUICKSTORE_CAPACITY ) {
        __STimeRangeQuickStoreMark[i] = false;
        __STimeRangeQuickStoreAllocateCount--;
    }
}

//

STimeRange*
__STimeRangeAlloc(void)
{
    STimeRange  *newRange = __STimeRangeQuickStoreAlloc();

    if ( ! newRange ) {
        newRange = malloc(sizeof(STimeRange));
        if ( newRange ) newRange->options = 0;
    }
    if ( newRange ) {
        newRange->refcount = 1;
        newRange->cstr = NULL;
    }
    return newRange;
}

//

void
__STimeRangeDealloc(
    STimeRange  *aTimeRange
)
{
    if ( aTimeRange->cstr ) {
        free((void*)aTimeRange->cstr);
    }
    if ( (aTimeRange->options & kSTimeRangeIsStatic) ) {
        __STimeRangeQuickStoreDealloc(aTimeRange);
    } else {
        free((void*)aTimeRange);
    }
}

//

const char*
__STimeRangeGetCString(
    STimeRange  *aTimeRange
)
{
    if ( ! aTimeRange->cstr ) {
        struct tm       unparsed_time;

        if ( (aTimeRange->options & kSTimeRangeHasLowerBound) ) {

            if ( (aTimeRange->options & kSTimeRangeHasUpperBound) ) {
                char    buffer[2 * __STimeRangeDateTimeFormatMaxOutLen];

                strftime(buffer, __STimeRangeDateTimeFormatMaxOutLen, __STimeRangeDateTimeFormat, localtime_r(&aTimeRange->start, &unparsed_time));
                buffer[__STimeRangeDateTimeFormatMaxOutLen - 1] = ':';
                strftime(buffer + __STimeRangeDateTimeFormatMaxOutLen, __STimeRangeDateTimeFormatMaxOutLen, __STimeRangeDateTimeFormat, localtime_r(&aTimeRange->end, &unparsed_time));
                aTimeRange->cstr = strdup(buffer);
            } else {
                char    buffer[1 + __STimeRangeDateTimeFormatMaxOutLen];

                strftime(buffer, __STimeRangeDateTimeFormatMaxOutLen, __STimeRangeDateTimeFormat, localtime_r(&aTimeRange->start, &unparsed_time));
                buffer[__STimeRangeDateTimeFormatMaxOutLen - 1] = ':';
                buffer[__STimeRangeDateTimeFormatMaxOutLen] = '\0';
                aTimeRange->cstr = strdup(buffer);
            }
        } else if ( (aTimeRange->options & kSTimeRangeHasUpperBound) ) {
            char    buffer[1 + __STimeRangeDateTimeFormatMaxOutLen];
            buffer[0] = ':';
            strftime(buffer + 1, __STimeRangeDateTimeFormatMaxOutLen, __STimeRangeDateTimeFormat, localtime_r(&aTimeRange->end, &unparsed_time));
            aTimeRange->cstr = strdup(buffer);
        } else {
            /* infinite time range */
            aTimeRange->cstr = strdup("-");
        }
    }
    return ( aTimeRange->cstr ? aTimeRange->cstr : "" );
}

//

void
__STimeRangeDebug(
    STimeRange  *aTimeRange
)
{
    if ( aTimeRange ) {
        uint32_t    opt = kSTimeRangeOptionMin, optIdx = 0;
        bool        hasDisp = false;

        printf(
            "STimeRange@%p(%u) {\n"
            "  start: %lld\n"
            "  end: %lld\n"
            "  options: ",
            aTimeRange, aTimeRange->refcount,
            (long long int)aTimeRange->start,
            (long long int)aTimeRange->end
        );

        while ( opt < kSTimeRangeOptionMax ) {
            if ( (aTimeRange->options & opt) ) {
                printf("%s%s", (hasDisp ? "|" : ""), __STimeRangeOptionNames[optIdx]);
                hasDisp = true;
            }
            opt <<= 1;
            optIdx++;
        }

        printf(
            "\n"
            "  cstr: \"%s\"\n"
            "}\n",
            __STimeRangeGetCString(aTimeRange)
        );
    }
}

//

STimeRangeRef
STimeRangeCreate(
    time_t start,
    time_t end
)
{
    if ( start <= end ) {
        STimeRange  *newRange = __STimeRangeAlloc();

        if ( newRange ) {
            uint32_t    bits = kSTimeRangeHasLowerBound | kSTimeRangeHasUpperBound;

            newRange->start = start;
            newRange->end = end;
            if ( start <= end ) bits |= kSTimeRangeIsValid;
            newRange->options |= bits;
        }
        return (STimeRangeRef)newRange;
    }
    return NULL;
}

STimeRangeRef
STimeRangeCreateWithEnd(
    time_t  end
)
{
    STimeRange  *newRange = __STimeRangeAlloc();

    if ( newRange ) {
        newRange->start = 0;
        newRange->end = end;
        newRange->options |= kSTimeRangeHasUpperBound | kSTimeRangeIsValid;
    }
    return (STimeRangeRef)newRange;
}

STimeRangeRef
STimeRangeCreateWithStart(
    time_t  start
)
{
    STimeRange  *newRange = __STimeRangeAlloc();

    if ( newRange ) {
        newRange->start = start;
        newRange->end = 0;
        newRange->options |= kSTimeRangeHasLowerBound | kSTimeRangeIsValid;
    }
    return (STimeRangeRef)newRange;
}

STimeRangeRef
STimeRangeCreateWithStartAndDuration(
    time_t          start,
    time_t          duration
)
{
    return STimeRangeCreate(start, start + (duration - 1));
}

STimeRangeRef
STimeRangeCreateWithString(
    const char  *timeRangeStr,
    const char* *outEndPtr
)
{
    STimeRangeRef   newRange = NULL;
    time_t          start, end;
    bool            isStartSet = false, isEndSet = false;
    struct tm       parsed_time;

    while ( isspace(*timeRangeStr) ) timeRangeStr++;

    if ( *timeRangeStr != ':' ) {
        /*
         * the string doesn't lead with a colon, so it has a start date-time
         */
        const char  *endptr = strptime(timeRangeStr, __STimeRangeDateTimeFormat, &parsed_time);

        if ( (endptr == NULL) || (endptr == timeRangeStr) ) return STimeRangeInvalid;
        timeRangeStr = endptr;
        // Dunno about DST...
        parsed_time.tm_isdst = -1;
        start = mktime(&parsed_time);
        isStartSet = true;
    }
    if ( *timeRangeStr == ':' ) {
        /*
         * there's a colon, so check for a trailing end date-time
         */
        if ( *(++timeRangeStr) ) {
            const char  *endptr = strptime(timeRangeStr, __STimeRangeDateTimeFormat, &parsed_time);

            if ( (endptr == NULL) || (endptr == timeRangeStr) ) return STimeRangeInvalid;
            timeRangeStr = endptr;
            // Dunno about DST...
            parsed_time.tm_isdst = -1;
            end = mktime(&parsed_time);
            isEndSet = true;
        }
    }
    if ( outEndPtr) *outEndPtr = timeRangeStr;
    if ( isStartSet ) {
        if ( isEndSet ) {
            newRange = STimeRangeCreate(start, end);
        } else {
            newRange = STimeRangeCreateWithStart(start);
        }
    } else if ( isEndSet ) {
        newRange = STimeRangeCreateWithEnd(end);
    } else {
        newRange = STimeRangeInfinite;
    }
    return newRange;
}

//

STimeRangeRef
STimeRangeCopy(
    STimeRangeRef   aTimeRange
)
{
    STimeRange      *newRange = __STimeRangeAlloc();

    if ( newRange ) {
        newRange->refcount = 1;
        newRange->start = aTimeRange->start;
        newRange->end = aTimeRange->end;
        newRange->options |= (aTimeRange->options & ~(kSTimeRangeIsStatic | kSTimeRangeIsConst));
    }
    return (STimeRangeRef)newRange;
}

//

void
STimeRangeRelease(
    STimeRangeRef   aTimeRange
)
{
    if ( ! (aTimeRange->options & kSTimeRangeIsConst) ) {
        STimeRange  *mutableTimeRange = (STimeRange*)aTimeRange;

        if ( --mutableTimeRange->refcount == 0 ) __STimeRangeDealloc(mutableTimeRange);
    }
}

STimeRangeRef
STimeRangeRetain(
    STimeRangeRef   aTimeRange
)
{
    if ( ! (aTimeRange->options & kSTimeRangeIsConst) ) {
        STimeRange  *mutableTimeRange = (STimeRange*)aTimeRange;

        mutableTimeRange->refcount++;
    }
    return aTimeRange;
}

//

bool
STimeRangeIsValid(
    STimeRangeRef   aTimeRange
)
{
    return ( (aTimeRange->options & kSTimeRangeIsValid) != 0 );
}

bool
STimeRangeIsStartTimeSet(
    STimeRangeRef   aTimeRange
)
{
    return ( (aTimeRange->options & kSTimeRangeHasLowerBound) != 0 );
}
bool
STimeRangeGetStartTime(
    STimeRangeRef   aTimeRange,
    time_t          *startTime
)
{
    if ( (aTimeRange->options & kSTimeRangeHasLowerBound) ) {
        *startTime = aTimeRange->start;
        return true;
    }
    return false;
}

bool
STimeRangeIsEndTimeSet(
    STimeRangeRef   aTimeRange
)
{
    return ( (aTimeRange->options & kSTimeRangeHasUpperBound) != 0 );
}
bool
STimeRangeGetEndTime(
    STimeRangeRef   aTimeRange,
    time_t          *endTime
)
{
    if ( (aTimeRange->options & kSTimeRangeHasUpperBound) ) {
        *endTime = aTimeRange->end;
        return true;
    }
    return false;
}

const char*
STimeRangeGetCString(
    STimeRangeRef   aTimeRange
)
{
    return __STimeRangeGetCString((STimeRange*)aTimeRange);
}

//

bool
STimeRangeContainsTime(
    STimeRangeRef   aTimeRange,
    time_t          theTime
)
{
    if ( ! (aTimeRange->options & kSTimeRangeIsValid) ) return false;
    if ( (aTimeRange->options & kSTimeRangeHasLowerBound) && theTime < aTimeRange->start ) return false;
    if ( (aTimeRange->options & kSTimeRangeHasUpperBound) && theTime > aTimeRange->end ) return false;
    return true;
}

//

bool
STimeRangeIsFullyBounded(
    STimeRangeRef   aTimeRange
)
{
    if ( ! (aTimeRange->options & kSTimeRangeIsValid) ) return false;
    return ( (aTimeRange->options & kSTimeRangeHasLowerBound) && (aTimeRange->options & kSTimeRangeHasUpperBound) ) ? true : false;
}

//

bool
STimeRangeGetDuration(
    STimeRangeRef   aTimeRange,
    time_t          *duration
)
{
    if ( ! (aTimeRange->options & kSTimeRangeIsValid) ) return false;
    if ( (aTimeRange->options & kSTimeRangeHasLowerBound) && (aTimeRange->options & kSTimeRangeHasUpperBound) ) {
        *duration = (aTimeRange->end - aTimeRange->start + 1);
        return true;
    }
    return false;
}

//

bool
STimeRangeIsEqual(
    STimeRangeRef   aTimeRange,
    STimeRangeRef   anotherTimeRange
)
{
    return  ( (aTimeRange->options & kSTimeRangeIsValid) && (anotherTimeRange->options & kSTimeRangeIsValid) &&
             ((aTimeRange->options & kSTimeRangeBoundsMask) == (anotherTimeRange->options & kSTimeRangeBoundsMask)) &&
             (aTimeRange->start == anotherTimeRange->start) &&  (aTimeRange->end == anotherTimeRange->end)
            );
}

bool
STimeRangeDoesIntersect(
    STimeRangeRef   aTimeRange,
    STimeRangeRef   anotherTimeRange
)
{
    if ( (aTimeRange->options & kSTimeRangeIsValid) && (anotherTimeRange->options & kSTimeRangeIsValid) ) {
        /* Find the earliest date-time: */
        if ( (aTimeRange->options & kSTimeRangeHasLowerBound) ) {
            /* aTimeRange has a finite start date-time, so we need to check anotherTimeRange
             * for the same
             */
            if ( (anotherTimeRange->options & kSTimeRangeHasLowerBound) ) {
                /* both have a start time, so which one is first? */
                if ( aTimeRange->start < anotherTimeRange->start ) {
                    /* check for anotherTimeRange.start before aTimeRange->end */
                    if ( (aTimeRange->options & kSTimeRangeHasUpperBound) ) {
                        if ( anotherTimeRange->start <= aTimeRange->end ) return true;
                    } else {
                        /* aTimeRange is inifinite, so anotherTimeRange definitely intersects */
                        return true;
                    }
                } else {
                    /* check for aTimeRange.start before anotherTimeRange->end */
                    if ( (anotherTimeRange->options & kSTimeRangeHasUpperBound) ) {
                        if ( aTimeRange->start <= anotherTimeRange->end ) return true;
                    } else {
                        /* anotherTimeRange is inifinite, so aTimeRange definitely intersects */
                        return true;
                    }
                }
            } else {
                /* anotherTimeRange has not finite start time, so we need to check
                 * that aTimeRange->start is before anotherTimeRange->end
                 */
                if ( (anotherTimeRange->options & kSTimeRangeHasUpperBound) ) {
                    if ( aTimeRange->start <= anotherTimeRange->end ) return true;
                } else {
                    /* anotherTimeRange is inifinite, so aTimeRange definitely intersects */
                    return true;
                }
            }
        } else if ( (anotherTimeRange->options & kSTimeRangeHasLowerBound) ) {
            /* anotherTimeRange has a finite start date-time, aTimeRange does not
             * so we need to check that anotherTimeRange->start is before aTimeRange->end
             */
            if ( (aTimeRange->options & kSTimeRangeHasUpperBound) ) {
                if ( anotherTimeRange->start <= aTimeRange->end ) return true;
            } else {
                /* aTimeRange is inifinite, so anotherTimeRange definitely intersects */
                return true;
            }
        } else {
            /* neither have a finite start time so there's intersection no matter what */
            return true;
        }
    }
    return false;
}

bool
STimeRangeIsContained(
    STimeRangeRef   aTimeRange,
    STimeRangeRef   inAnotherTimeRange
)
{
    if ( (aTimeRange->options & kSTimeRangeIsValid) && (inAnotherTimeRange->options & kSTimeRangeIsValid) ) {
        if ( (inAnotherTimeRange->options & kSTimeRangeHasLowerBound) ) {
            /* aTimeRange must have a start date-time >= inAnotherTimeRange->start */
            if ( ! (aTimeRange->options & kSTimeRangeHasLowerBound) || (aTimeRange->start < inAnotherTimeRange->start) ) return false;
        }
        /* inAnotherTimeRange has no finite start date-time, so aTimeRange->start
         * MUST be >= that, no else clause necessary
        */
        if ( (inAnotherTimeRange->options & kSTimeRangeHasUpperBound) ) {
            /* aTimeRange must have an end date-time <= inAnotherTimeRange->end */
            if ( ! (aTimeRange->options & kSTimeRangeHasUpperBound) || (aTimeRange->end > inAnotherTimeRange->end) ) return false;
        }
        return true;
    }
    return false;
}

bool
STimeRangeIsContiguous(
    STimeRangeRef   aTimeRange,
    STimeRangeRef   anotherTimeRange
)
{
    if ( (aTimeRange->options & kSTimeRangeIsValid) && (anotherTimeRange->options & kSTimeRangeIsValid) ) {
        /* the start of one must be the end of the other + 1 */
        if ( (aTimeRange->options & kSTimeRangeHasUpperBound) && (anotherTimeRange->options & kSTimeRangeHasLowerBound) ) {
            if ( aTimeRange->end + 1 == anotherTimeRange->start ) return true;
        }
        if ( (aTimeRange->options & kSTimeRangeHasLowerBound) && (anotherTimeRange->options & kSTimeRangeHasUpperBound) ) {
            if ( anotherTimeRange->end + 1 == aTimeRange->start ) return true;
        }
    }
    return false;
}

//

bool
STimeRangeDoesTimePrecedeRange(
    STimeRangeRef   aTimeRange,
    time_t          aTime
)
{
    if ( (aTimeRange->options & kSTimeRangeIsValid) ) {
        if ( (aTimeRange->options & kSTimeRangeHasLowerBound) && (aTime < aTimeRange->start) ) return true;
    }
    return false;
}

bool
STimeRangeDoesTimeFollowRange(
    STimeRangeRef   aTimeRange,
    time_t          aTime
)
{
    if ( (aTimeRange->options & kSTimeRangeIsValid) ) {
        if ( (aTimeRange->options & kSTimeRangeHasUpperBound) && (aTime > aTimeRange->end) ) return true;
    }
    return false;
}

//

STimeRangeRef
STimeRangeIntersection(
    STimeRangeRef   aTimeRange,
    STimeRangeRef   anotherTimeRange
)
{
    STimeRangeRef   outRange = STimeRangeInvalid;

    if ( STimeRangeDoesIntersect(aTimeRange, anotherTimeRange) ) {
        time_t      start, end;
        bool        isStartSet = false, isEndSet = false;

        /* find maximum start time */
        if ( (aTimeRange->options & kSTimeRangeHasLowerBound) ) {
            if ( (anotherTimeRange->options & kSTimeRangeHasLowerBound) ) {
                if ( aTimeRange->start <= anotherTimeRange->start ) {
                    start = anotherTimeRange->start;
                } else {
                    start = aTimeRange->start;
                }
                isStartSet = true;
            } else {
                start = aTimeRange->start;
                isStartSet = true;
            }
        } else if ( (anotherTimeRange->options & kSTimeRangeHasLowerBound) ) {
            start = anotherTimeRange->start;
            isStartSet = true;
        }

        /* find minimum end time */
        if ( (aTimeRange->options & kSTimeRangeHasUpperBound) ) {
            if ( (anotherTimeRange->options & kSTimeRangeHasUpperBound) ) {
                if ( aTimeRange->end <= anotherTimeRange->end ) {
                    end = aTimeRange->end;
                } else {
                    end = anotherTimeRange->end;
                }
                isEndSet = true;
            } else {
                end = aTimeRange->end;
                isEndSet = true;
            }
        } else if ( (anotherTimeRange->options & kSTimeRangeHasUpperBound) ) {
            end = anotherTimeRange->end;
            isEndSet = true;
        }

        /* create accordingly */
        if ( isStartSet ) {
            if ( isEndSet ) {
                outRange = STimeRangeCreate(start, end);
            } else {
                outRange = STimeRangeCreateWithStart(start);
            }
        } else if ( isEndSet ) {
            outRange = STimeRangeCreateWithEnd(end);
        } else {
            outRange = STimeRangeInfinite;
        }
    }
    return outRange;
}

//

STimeRangeRef
STimeRangeUnion(
    STimeRangeRef   aTimeRange,
    STimeRangeRef   anotherTimeRange
)
{
    STimeRangeRef   outRange = STimeRangeInvalid;

    if ( STimeRangeDoesIntersect(aTimeRange, anotherTimeRange) ) {
        time_t      start, end;
        bool        isStartSet = false, isEndSet = false;

        /* find minimum start time */
        if ( (aTimeRange->options & kSTimeRangeHasLowerBound) && (anotherTimeRange->options & kSTimeRangeHasLowerBound) ) {
            if ( aTimeRange->start <= anotherTimeRange->start ) {
                start = aTimeRange->start;
            } else {
                start = anotherTimeRange->start;
            }
            isStartSet = true;
        }

        /* find maximum end time */
        if ( (aTimeRange->options & kSTimeRangeHasUpperBound) && (anotherTimeRange->options & kSTimeRangeHasUpperBound) ) {
            if ( aTimeRange->end <= anotherTimeRange->end ) {
                end = anotherTimeRange->end;
            } else {
                end = aTimeRange->end;
            }
            isEndSet = true;
        }

        /* create accordingly */
        if ( isStartSet ) {
            if ( isEndSet ) {
                outRange = STimeRangeCreate(start, end);
            } else {
                outRange = STimeRangeCreateWithStart(start);
            }
        } else if ( isEndSet ) {
            outRange = STimeRangeCreateWithEnd(end);
        } else {
            outRange = STimeRangeInfinite;
        }
    }
    return outRange;
}

//

STimeRangeRef
STimeRangeJoin(
    STimeRangeRef   aTimeRange,
    STimeRangeRef   anotherTimeRange
)
{
    STimeRangeRef   outRange = STimeRangeInvalid;

    if ( STimeRangeIsContiguous(aTimeRange, anotherTimeRange) ) {
        time_t      start, end;
        bool        isStartSet = false, isEndSet = false;

        /* find minimum start time */
        if ( (aTimeRange->options & kSTimeRangeHasLowerBound) && (anotherTimeRange->options & kSTimeRangeHasLowerBound) ) {
            if ( aTimeRange->start <= anotherTimeRange->start ) {
                start = aTimeRange->start;
            } else {
                start = anotherTimeRange->start;
            }
            isStartSet = true;
        }

        /* find maximum end time */
        if ( (aTimeRange->options & kSTimeRangeHasUpperBound) && (anotherTimeRange->options & kSTimeRangeHasUpperBound) ) {
            if ( aTimeRange->end <= anotherTimeRange->end ) {
                end = anotherTimeRange->end;
            } else {
                end = aTimeRange->end;
            }
            isEndSet = true;
        }

        /* create accordingly */
        if ( isStartSet ) {
            if ( isEndSet ) {
                outRange = STimeRangeCreate(start, end);
            } else {
                outRange = STimeRangeCreateWithStart(start);
            }
        } else if ( isEndSet ) {
            outRange = STimeRangeCreateWithEnd(end);
        } else {
            outRange = STimeRangeInfinite;
        }
    }
    return outRange;
}

//

int
STimeRangeCmp(
    STimeRangeRef   lhs,
    STimeRangeRef   rhs
)
{
    if ( (lhs->options & kSTimeRangeIsValid) ) {
        if ( (rhs->options & kSTimeRangeIsValid) ) {
            /* both are valid ranges; see which has the earlier start
             * time
             */
            if ( (lhs->options & kSTimeRangeHasLowerBound) ) {
                if ( (rhs->options & kSTimeRangeHasLowerBound) ) {
                    if ( lhs->start > rhs->start ) return -1;
                    if ( lhs->start < rhs->start ) return +1;
                } else {
                    /* rhs has no finite start date-time, so lhs is > */
                    return -1;
                }
                /* if we get here, then the start is considered == */
            } else if ( (rhs->options & kSTimeRangeHasLowerBound) ) {
                /* lhs has no finite start date-time, so rhs is > */
                return +1;
            }
            /* same start time, now order base on who ends first */
            if ( (lhs->options & kSTimeRangeHasUpperBound) ) {
                if ( (rhs->options & kSTimeRangeHasUpperBound) ) {
                    if ( lhs->end > rhs->end ) return -1;
                    if ( lhs->end < rhs->end ) return +1;
                } else {
                    /* rhs has no finite end date-time, so lhs is < */
                    return +1;
                }
            } else if ( (rhs->options & kSTimeRangeHasUpperBound) ) {
                /* lhs has no finite end date-time, so rhs is < */
                return -1;
            }
            /* same range */
            return 0;
        } else {
            /* rhs is invalid, sort first */
            return -1;
        }
    } else if ( (rhs->options & kSTimeRangeIsValid) ) {
        /* lhs is invalid, sort first */
        return +1;
    }
    /* both are invalid */
    return 0;
}

//

int
STimeRangeRightCmpToTime(
    STimeRangeRef   lhs,
    time_t          rhs
)
{
    if ( (lhs->options & kSTimeRangeHasLowerBound) ) {
        if ( lhs->start < rhs ) return +1;
        if ( lhs->start == rhs ) return 0;
        return -1;
    }
    return +1;
}

int
STimeRangeLeftCmpToTime(
    time_t          lhs,
    STimeRangeRef   rhs
)
{
    if ( (rhs->options & kSTimeRangeHasLowerBound) ) {
        if ( lhs < rhs->start ) return +1;
        if ( lhs == rhs->start ) return 0;
        return -1;
    }
    return -1;
}

//

STimeRangeRef
STimeRangeCreateByClippingToTimeRange(
    STimeRangeRef   clipThis,
    STimeRangeRef   toThis
)
{
    // Default to the invalid range, e.g. if the two don't overlap:
    STimeRangeRef   outRange = STimeRangeInvalid;

    if ( STimeRangeDoesIntersect(clipThis, toThis) ) {
        time_t      start, end;
        bool        isStartSet = false, isEndSet = false;
        
        outRange = STimeRangeCopy(clipThis);
        if ( ! outRange ) return outRange;
        
        /* find maximum start time */
        if ( (outRange->options & kSTimeRangeHasLowerBound) ) {
            if ( (toThis->options & kSTimeRangeHasLowerBound) ) {
                if ( outRange->start <= toThis->start ) {
                    start = toThis->start;
                } else {
                    start = outRange->start;
                }
                isStartSet = true;
            } else {
                start = outRange->start;
                isStartSet = true;
            }
        } else if ( (toThis->options & kSTimeRangeHasLowerBound) ) {
            start = toThis->start;
            isStartSet = true;
        }

        /* find minimum end time */
        if ( (outRange->options & kSTimeRangeHasUpperBound) ) {
            if ( (toThis->options & kSTimeRangeHasUpperBound) ) {
                if ( outRange->end <= toThis->end ) {
                    end = outRange->end;
                } else {
                    end = toThis->end;
                }
                isEndSet = true;
            } else {
                end = outRange->end;
                isEndSet = true;
            }
        } else if ( (toThis->options & kSTimeRangeHasUpperBound) ) {
            end = toThis->end;
            isEndSet = true;
        }

        /* update accordingly */
        STimeRange  *CLIPTHIS = (STimeRange*)outRange;
        
        if ( isStartSet ) {
            CLIPTHIS->options |= kSTimeRangeHasLowerBound;
            CLIPTHIS->start = start;
        } else {
            CLIPTHIS->options &= ~kSTimeRangeHasLowerBound;
            CLIPTHIS->start = 0;
        }
        if ( isEndSet ) {
            CLIPTHIS->options |= kSTimeRangeHasUpperBound;
            CLIPTHIS->end = end;
        } else {
            CLIPTHIS->options &= ~kSTimeRangeHasUpperBound;
            CLIPTHIS->end = 0;
        }
    }
    
    return outRange;
}

//

STimeRangeRef
STimeRangeLeading(
    STimeRangeRef   haystack,
    STimeRangeRef   needle
)
{
    //
    // Returns time from haystack leading up to needle
    //
    if ( (haystack->options & kSTimeRangeIsValid) && (needle->options & kSTimeRangeIsValid) ) {
        //
        // The two time ranges have to at least intersect:
        //
        if ( ! STimeRangeDoesIntersect(haystack, needle) ) return NULL;

        //
        // Is the haystack's start time bounded or unbounded?
        //
        if ( (haystack->options & kSTimeRangeHasLowerBound) ) {
            //
            // is the haystack start < that of needle?
            //
            if ( (needle->options & kSTimeRangeHasLowerBound) && (haystack->start < needle->start) ) {
                //
                // from the start of haystack up to the needle:
                //
                return STimeRangeCreate(haystack->start, needle->start - 1);
            }
        } else {
            //
            // haystack start is unbounded, check if needle is bounded:
            //
            if ( (needle->options & kSTimeRangeHasLowerBound) ) {
                //
                // unbounded start time up to the needle:
                //
                return STimeRangeCreateWithEnd(needle->start - 1);
            }
        }
    }
    return NULL;
}

//

STimeRangeRef
STimeRangeLeadingBeforeTime(
    STimeRangeRef   haystack,
    time_t          beforeTime
)
{
    //
    // Returns time from start of haystack leading up to beforeTime
    //
    if ( (haystack->options & kSTimeRangeIsValid) ) {
        if ( STimeRangeContainsTime(haystack, beforeTime) ) {
            //
            // From start time of haystack up-to beforeTime:
            //
            return (haystack->options & kSTimeRangeHasLowerBound) ? STimeRangeCreate(haystack->start, beforeTime - 1) : STimeRangeCreateWithEnd(beforeTime - 1);
        }
    }
    return NULL;
}

//

STimeRangeRef
STimeRangeTrailing(
    STimeRangeRef   haystack,
    STimeRangeRef   needle
)
{
    //
    // Returns time from end of needle through end of haystack
    //
    if ( (haystack->options & kSTimeRangeIsValid) && (needle->options & kSTimeRangeIsValid) ) {
        //
        // The two time ranges have to at least intersect:
        //
        if ( ! STimeRangeDoesIntersect(haystack, needle) ) return NULL;

        //
        // Is the haystack's end time bounded or unbounded?
        //
        if ( (haystack->options & kSTimeRangeHasUpperBound) ) {
            //
            // is the haystack end > that of needle?
            //
            if ( (needle->options & kSTimeRangeHasUpperBound) && (haystack->end > needle->end) ) {
                //
                // from just after needle through end of haystack:
                //
                return STimeRangeCreate(needle->end + 1, haystack->end);
            }
        } else {
            //
            // haystack end is unbounded, check if needle is bounded:
            //
            if ( (needle->options & kSTimeRangeHasUpperBound) ) {
                //
                // unbounded start time up to the needle:
                //
                return STimeRangeCreateWithStart(needle->end + 1);
            }
        }
    }
    return NULL;
}

//

STimeRangeRef
STimeRangeTrailingAfterTime(
    STimeRangeRef   haystack,
    time_t          afterTime
)
{
    //
    // Returns time from afterTime through end of haystack
    //
    if ( (haystack->options & kSTimeRangeIsValid) ) {
        if ( STimeRangeContainsTime(haystack, afterTime) ) {
            //
            // From start time of haystack up-to beforeTime:
            //
            return (haystack->options & kSTimeRangeHasUpperBound) ? STimeRangeCreate(haystack->end, afterTime) : STimeRangeCreateWithStart(afterTime);
        }
    }
    return NULL;
}

//

unsigned int
STimeRangeGetCountOfPeriodsOfLength(
    STimeRangeRef   aTimeRange,
    time_t          duration
)
{
    if ( ! (aTimeRange->options & kSTimeRangeIsValid) ) return 0;
    if ( (aTimeRange->options & kSTimeRangeHasLowerBound) ) {
        //
        // Do we have an upper bound?
        //
        if ( (aTimeRange->options & kSTimeRangeHasUpperBound) ) {
            uint64_t    dt = (aTimeRange->end - aTimeRange->start + 1);

            return (dt / duration) + ((dt % duration) ? 1 : 0);
        }
        //
        // Basically an infinite number of indices:
        //
        return UINT_MAX;
    } else {
        //
        // Basically an infinite number of indices:
        //
        return UINT_MAX;
    }
    return 0;
}

//

STimeRangeRef
STimeRangeGetPeriodOfLengthAtIndex(
    STimeRangeRef   aTimeRange,
    time_t          duration,
    unsigned int    index
)
{
    time_t          dt = duration * index;

    if ( ! (aTimeRange->options & kSTimeRangeIsValid) ) return NULL;

    //
    // If the range is unbounded at the start, then we allocate periods from the end:
    //
    if ( (aTimeRange->options & kSTimeRangeHasLowerBound) ) {
        //
        // Start from lower end:
        //
        time_t      start = aTimeRange->start + dt;

        //
        // We have a lower bound, do we have an upper bound?
        //
        if ( (aTimeRange->options & kSTimeRangeHasUpperBound) ) {
            //
            // Make sure start is below the end time:
            //
            if ( start < aTimeRange->end ) {
                time_t  end = start + duration - 1;

                if ( end >= aTimeRange->end ) end = aTimeRange->end;
                return STimeRangeCreate(start, end);
            }
        } else {
            //
            // No upper bound:
            //
            return STimeRangeCreate(start, start + duration - 1);
        }
    } else {
        //
        // No lower bound, do we have an upper bound?
        //
        if ( (aTimeRange->options & kSTimeRangeHasUpperBound) ) {
            //
            // Start from upper end:
            //
            return STimeRangeCreate(aTimeRange->end - dt - duration + 1, aTimeRange->end - dt);
        }
    }
    return NULL;
}

//

#ifdef STIMERANGE_UNIT_TEST

int
main()
{
    STimeRangeRef   t1, t2, t3, t4, t5, t6, t7, r1, r2, r3, r4;
    time_t          t = time(NULL), tprime;

    printf("Origin %s\n", ctime(&t));

    tprime = STimeRangeJustifyTime(t, kSTimeRangeJustifyTimeToMinutes, false);
    printf("%lld %lld = %s\n", t, tprime, ctime(&tprime));

    tprime = STimeRangeJustifyTime(t, kSTimeRangeJustifyTimeToHours, false);
    printf("%lld %lld = %s\n", t, tprime, ctime(&tprime));

    tprime = STimeRangeJustifyTime(t, kSTimeRangeJustifyTimeToDays, false);
    printf("%lld %lld = %s\n", t, tprime, ctime(&tprime));

    tprime = STimeRangeJustifyTime(t, kSTimeRangeJustifyTimeToMinutes, true);
    printf("%lld %lld = %s\n", t, tprime, ctime(&tprime));

    tprime = STimeRangeJustifyTime(t, kSTimeRangeJustifyTimeToHours, true);
    printf("%lld %lld = %s\n", t, tprime, ctime(&tprime));

    tprime = STimeRangeJustifyTime(t, kSTimeRangeJustifyTimeToDays, true);
    printf("%lld %lld = %s\n", t, tprime, ctime(&tprime));

    return 0;

    t1 = STimeRangeCreate(0, 172799);
    if ( ! t1 ) exit(ENOMEM);
    __STimeRangeDebug((STimeRange*)t1);

    t2 = STimeRangeCreate(86399, 86400);
    if ( ! t2 ) exit(ENOMEM);
    __STimeRangeDebug((STimeRange*)t2);

    r1 = STimeRangeIntersection(t1, t2);
    __STimeRangeDebug((STimeRange*)r1);

    r2 = STimeRangeUnion(t1, t2);
    __STimeRangeDebug((STimeRange*)r2);

    printf("t1 == t2 ? %d\n", STimeRangeIsEqual(t1, t2));
    printf("t1 ^ t2 ? %d\n", STimeRangeDoesIntersect(t1, t2));
    printf("t1 [] t2 ? %d\n", STimeRangeIsContained(t1, t2));
    printf("t2 [] t1 ? %d\n", STimeRangeIsContained(t2, t1));
    printf("cmp(t1,t2) = %d\n", STimeRangeCmp(t1, t2));
    printf("cmp(t2,t1) = %d\n", STimeRangeCmp(t2, t1));

    t3 = STimeRangeCreateWithString("20190801T000000-0000:20190831T235959-0000", NULL);
    if ( ! t3 ) exit(ENOMEM);
    __STimeRangeDebug((STimeRange*)t3);
    printf("t1 ^ t3 ? %d\n", STimeRangeDoesIntersect(t1, t3));

    t4 = STimeRangeCreateWithString("20190901T000000-0000:20190930T235959-0000", NULL);
    if ( ! t4 ) exit(ENOMEM);
    __STimeRangeDebug((STimeRange*)t4);
    printf("t3 ^ t4 ? %d\n", STimeRangeDoesIntersect(t3, t4));
    printf("t3 … t4 ? %d\n", STimeRangeIsContiguous(t3, t4));
    printf("t4 … t3 ? %d\n", STimeRangeIsContiguous(t4, t3));
    printf("t3 [] t4 ? %d\n", STimeRangeIsContained(t3, t4));
    printf("t4 [] t3 ? %d\n", STimeRangeIsContained(t4, t3));
    printf("cmp(t3,t4) = %d\n", STimeRangeCmp(t3, t4));
    printf("cmp(t4,t3) = %d\n", STimeRangeCmp(t4, t3));
    printf("cmp(t4,t4) = %d\n", STimeRangeCmp(t4, t4));

    r4 = STimeRangeJoin(t3, t4);
    __STimeRangeDebug((STimeRange*)r4);

    t5 = STimeRangeCreateWithString("20190815T000000-0000:20190904T000000-0000", NULL);
    if ( ! t5 ) exit(ENOMEM);
    __STimeRangeDebug((STimeRange*)t5);

    r3 = STimeRangeUnion(t3, t5);
    __STimeRangeDebug((STimeRange*)r3);

    t6 = STimeRangeCreateWithString("20190815T000000-0000:", NULL);
    if ( ! t6 ) exit(ENOMEM);
    __STimeRangeDebug((STimeRange*)t6);

    t7 = STimeRangeCreateWithString(":20190904T000000-0000", NULL);
    if ( ! t7 ) exit(ENOMEM);
    __STimeRangeDebug((STimeRange*)t7);

    printf("t1 = %s\n", STimeRangeGetCString(t1));
    printf("t2 = %s\n", STimeRangeGetCString(t2));
    printf("t3 = %s\n", STimeRangeGetCString(t3));
    printf("t4 = %s\n", STimeRangeGetCString(t4));
    printf("t5 = %s\n", STimeRangeGetCString(t5));
    printf("t6 = %s\n", STimeRangeGetCString(t6));
    printf("t7 = %s\n", STimeRangeGetCString(t7));
    printf("r1 = %s\n", STimeRangeGetCString(r1));
    printf("r2 = %s\n", STimeRangeGetCString(r2));
    printf("r3 = %s\n", STimeRangeGetCString(r3));
    printf("r4 = %s\n", STimeRangeGetCString(r4));

    printf("STimeRangeInvalid = "); __STimeRangeDebug((STimeRange*)STimeRangeInvalid);
    printf("STimeRangeInfinite = "); __STimeRangeDebug((STimeRange*)STimeRangeInfinite);

    STimeRangeRelease(t1);
    STimeRangeRelease(t2);
    STimeRangeRelease(t3);
    STimeRangeRelease(t4);
    STimeRangeRelease(t5);
    STimeRangeRelease(t6);
    STimeRangeRelease(t7);
    STimeRangeRelease(r1);
    STimeRangeRelease(r2);
    STimeRangeRelease(r3);

    return 0;
}

#endif
