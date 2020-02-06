/*
 *  SSchedule
 *  Represents a time range and "scheduled" blocks of time within it.  Include
 *  functions to search for unused blocks of time within the schedule range.
 *
 */

#include "SSchedule.h"

//

typedef struct SScheduleBlock {
    STimeRangeRef           period;
    struct SScheduleBlock   *link;
} SScheduleBlock;

//

static SScheduleBlock   *__SScheduleBlockFreeList = NULL;

SScheduleBlock*
SScheduleBlockAlloc(void)
{
    SScheduleBlock  *newBlock = NULL;

    if ( __SScheduleBlockFreeList ) {
        newBlock = __SScheduleBlockFreeList;
        __SScheduleBlockFreeList = newBlock->link;
    } else {
        newBlock = malloc(sizeof(SScheduleBlock));
    }
    if ( newBlock ) {
        newBlock->period = NULL;
        newBlock->link = NULL;
    }
    return newBlock;
}

//

void
SScheduleBlockDealloc(
    SScheduleBlock  *theBlock
)
{
    if ( theBlock->period ) {
        STimeRangeRelease(theBlock->period);
        theBlock->period = NULL;
    }
    theBlock->link = __SScheduleBlockFreeList;
    __SScheduleBlockFreeList = theBlock;
}

//

typedef struct SSchedule {
    uint32_t        refcount;
    STimeRangeRef   period;
    unsigned int    blockCount;
    SScheduleBlock  *blocks;
    const char      *lastErrorMessage;
    char            staticErrorMessageBuffer[64];
} SSchedule;

//

SSchedule*
__SScheduleAlloc(void)
{
    SSchedule       *newSchedule = malloc(sizeof(SSchedule));

    if ( newSchedule ) {
        newSchedule->refcount = 1;
        newSchedule->period = NULL;
        newSchedule->blockCount = 0;
        newSchedule->blocks = NULL;
        newSchedule->lastErrorMessage = NULL;
    }
    return newSchedule;
}

//

void
__SScheduleDealloc(
    SSchedule   *aSchedule
)
{
    SScheduleBlock  *block = aSchedule->blocks;

    while ( block ) {
        SScheduleBlock  *next = block->link;

        SScheduleBlockDealloc(block);
        block = next;
    }
    if ( aSchedule->period ) STimeRangeRelease(aSchedule->period);
    if ( aSchedule->lastErrorMessage && (aSchedule->lastErrorMessage != aSchedule->staticErrorMessageBuffer) ) free((void*)aSchedule->lastErrorMessage);
    free((void*)aSchedule);
}

//

void
__SScheduleDebug(
    SSchedule   *aSchedule
)
{
    if ( aSchedule ) {
        SScheduleBlock  *block = aSchedule->blocks;
        unsigned int    i = 0;

        printf(
                "SSchedule@%p(%u) {\n"
                "  period: %s\n"
                "  blockCount: %u\n",
                aSchedule, aSchedule->refcount,
                STimeRangeGetCString(aSchedule->period),
                aSchedule->blockCount
            );
        while ( block ) {
            printf("    %d : %s\n", i++, STimeRangeGetCString(block->period));
            block = block->link;
        }
        printf(
                "  lastErrorMessage: %s\n"
                "}\n",
                ( aSchedule->lastErrorMessage ? aSchedule->lastErrorMessage : "<none>" )
            );
    }
}

//

#include <stdarg.h>

void
__SScheduleSetLastErrorMessage(
    SSchedule   *aSchedule,
    const char  *format,
    ...
)
{
    if ( aSchedule->lastErrorMessage ) {
        if ( aSchedule->lastErrorMessage != aSchedule->staticErrorMessageBuffer ) free((void*)aSchedule->lastErrorMessage);
        aSchedule->lastErrorMessage = NULL;
    }
    if ( format ) {
        va_list     argv;
        int         strLen;
        
        va_start(argv, format);
        strLen = vsnprintf(NULL, 0, format, argv);
        va_end(argv);
        
        if ( strLen + 1 < sizeof(aSchedule->staticErrorMessageBuffer) ) {
            aSchedule->lastErrorMessage = aSchedule->staticErrorMessageBuffer;
            va_start(argv, format);
            vsnprintf((char*)aSchedule->staticErrorMessageBuffer, sizeof(aSchedule->staticErrorMessageBuffer), format, argv);
            va_end(argv);
        } else if ( (aSchedule->lastErrorMessage = (const char*)malloc(strLen + 1)) ) {
            va_start(argv, format);
            vsnprintf((char*)aSchedule->lastErrorMessage, strLen + 1, format, argv);
            va_end(argv);
        } else {
            fprintf(stderr, "FATAL:  unable to allocate space to stash last error message\n");
            exit(ENOMEM);
        }
    }
}

//

SScheduleRef
SScheduleCreate(
    STimeRangeRef   period
)
{
    SSchedule       *newSchedule = __SScheduleAlloc();

    if ( newSchedule ) {
        newSchedule->period = STimeRangeRetain(period);
    }
    return (SScheduleRef)newSchedule;
}

//

SScheduleRef
SScheduleCreateWithFileQuick(
    const char  *filepath
)
{
    SSchedule   *newSchedule = NULL;
    sqlite3     *dbHandle;
    int         rc;
    
    rc = sqlite3_open_v2(filepath, &dbHandle, SQLITE_OPEN_READONLY, NULL);
    if ( rc == SQLITE_OK ) {
        sqlite3_stmt    *sqlQuery;
        
        rc = sqlite3_prepare_v2(dbHandle, "SELECT period FROM schedule LIMIT 1", -1, &sqlQuery, NULL);
        if ( rc == SQLITE_OK ) {
            const unsigned char *colVal;
            STimeRangeRef       period = NULL;
            unsigned int        blockCount = 0;
            SScheduleBlock      *head = NULL, *last = NULL;
            
            //
            // Get the SSchedule instance vars:
            //
            rc = sqlite3_step(sqlQuery);
            if ( rc == SQLITE_ROW ) {
                colVal = sqlite3_column_text(sqlQuery, 0);
                if ( colVal && *colVal ) {
                    period = STimeRangeCreateWithString(colVal, NULL);
                    if ( ! period || ! STimeRangeIsValid(period) ) {
                        if ( period ) STimeRangeRelease(period);
                        period = NULL;
                        rc = SQLITE_CORRUPT;
                    }
                } else {
                    // Fake an error code:
                    rc = SQLITE_CORRUPT;
                }
            }
            sqlite3_finalize(sqlQuery);
            
            if ( period ) {
                //
                // Get the list of scheduled blocks:
                //
                rc = sqlite3_prepare_v2(dbHandle, "SELECT period FROM blocks ORDER BY block_id", -1, &sqlQuery, NULL);
                if ( rc == SQLITE_OK ) {
                    while ( (rc = sqlite3_step(sqlQuery)) == SQLITE_ROW ) {
                        SScheduleBlock  *newBlock = NULL;
                        
                        colVal = sqlite3_column_text(sqlQuery, 0);
                        if ( colVal && *colVal ) {
                            STimeRangeRef   blockPeriod = STimeRangeCreateWithString(colVal, NULL);
                            if ( blockPeriod && STimeRangeIsValid(blockPeriod) ) {
                                newBlock = SScheduleBlockAlloc();
                                if ( newBlock ) {
                                    newBlock->period = blockPeriod;
                                    if ( ! head ) {
                                        head = last = newBlock;
                                    } else {
                                        last->link = newBlock;
                                        last = newBlock;
                                    }
                                    blockCount++;
                                    rc = SQLITE_OK;
                                } else {
                                    STimeRangeRelease(blockPeriod);
                                    blockPeriod = NULL;
                                    rc = SQLITE_NOMEM;
                                }
                            }  else {
                                if ( blockPeriod ) STimeRangeRelease(blockPeriod);
                                blockPeriod = NULL;
                                rc = SQLITE_CORRUPT;
                            }
                        } else {
                            rc = SQLITE_CORRUPT;
                        }
                        if ( rc !=  SQLITE_OK) break;
                    }
                    if ( rc == SQLITE_DONE ) {
                        //
                        // We've got everything, create the new object:
                        //
                        newSchedule = __SScheduleAlloc();
                        if ( newSchedule ) {
                            newSchedule->period = STimeRangeRetain(period);
                            newSchedule->blockCount = blockCount;
                            newSchedule->blocks = head;
                            rc = SQLITE_OK;
                        } else {
                            //
                            // Ugh...get rid of the block list:
                            //
                            while ( head ) {
                                SScheduleBlock  *next = head->link;
                                
                                SScheduleBlockDealloc(head);
                                head = next;
                            }
                            rc = SQLITE_NOMEM;
                        }
                    }
                    sqlite3_finalize(sqlQuery);
                }
                STimeRangeRelease(period);
            }
        }
        sqlite3_close_v2(dbHandle);
        if ( ! newSchedule && (rc != SQLITE_OK) ) {
            fprintf(stderr, "ERROR:  unable to open `%s` (sqlite err = %d)\n", filepath, rc);
        }
    } else {
        if ( dbHandle ) {
            fprintf(stderr, "ERROR:  unable to open `%s` (sqlite err = %d, %s)\n", filepath, rc, sqlite3_errmsg(dbHandle));
            sqlite3_close_v2(dbHandle);
        } else {
            fprintf(stderr, "ERROR:  unable to open `%s` (sqlite err = %d)\n", filepath, rc);
        }
    }
    return newSchedule;
}

//

SScheduleRef
SScheduleCreateWithFile(
    const char  *filepath
)
{
    SScheduleRef    newSchedule = NULL;
    sqlite3         *dbHandle;
    int             rc;
    
    rc = sqlite3_open_v2(filepath, &dbHandle, SQLITE_OPEN_READONLY, NULL);
    if ( rc == SQLITE_OK ) {
        sqlite3_stmt    *sqlQuery;
        
        rc = sqlite3_prepare_v2(dbHandle, "SELECT period FROM schedule LIMIT 1", -1, &sqlQuery, NULL);
        if ( rc == SQLITE_OK ) {
            const unsigned char *colVal;
            STimeRangeRef       period = NULL;
            
            //
            // Get the SSchedule instance vars:
            //
            rc = sqlite3_step(sqlQuery);
            if ( rc == SQLITE_ROW ) {
                colVal = sqlite3_column_text(sqlQuery, 0);
                if ( colVal && *colVal ) {
                    period = STimeRangeCreateWithString(colVal, NULL);
                    if ( ! period || ! STimeRangeIsValid(period) ) {
                        if ( period ) STimeRangeRelease(period);
                        period = NULL;
                        rc = SQLITE_CORRUPT;
                    }
                } else {
                    // Fake an error code:
                    rc = SQLITE_CORRUPT;
                }
            }
            sqlite3_finalize(sqlQuery);
            
            if ( period ) {
                newSchedule = SScheduleCreate(period);
                STimeRangeRelease(period);
                
                if ( newSchedule ) {
                    //
                    // Get the list of scheduled blocks:
                    //
                    rc = sqlite3_prepare_v2(dbHandle, "SELECT period FROM blocks ORDER BY block_id", -1, &sqlQuery, NULL);
                    if ( rc == SQLITE_OK ) {
                        while ( (rc = sqlite3_step(sqlQuery)) == SQLITE_ROW ) {
                            colVal = sqlite3_column_text(sqlQuery, 0);
                            if ( colVal && *colVal ) {
                                STimeRangeRef   blockPeriod = STimeRangeCreateWithString(colVal, NULL);
                                if ( blockPeriod && STimeRangeIsValid(blockPeriod) ) {
                                    rc = SScheduleAddScheduledBlock(newSchedule, blockPeriod) ? SQLITE_OK : SQLITE_NOMEM;
                                    STimeRangeRelease(blockPeriod);
                                }  else {
                                    if ( blockPeriod ) STimeRangeRelease(blockPeriod);
                                    blockPeriod = NULL;
                                    rc = SQLITE_CORRUPT;
                                }
                            } else {
                                rc = SQLITE_CORRUPT;
                            }
                            if ( rc !=  SQLITE_OK) break;
                        }
                        if ( rc != SQLITE_DONE ) {
                            SScheduleRelease(newSchedule);
                            newSchedule = NULL;
                        }
                        sqlite3_finalize(sqlQuery);
                    }
                }
            }
        }
        sqlite3_close_v2(dbHandle);
        if ( ! newSchedule && (rc != SQLITE_OK) ) {
            fprintf(stderr, "ERROR:  unable to open `%s` (sqlite err = %d)\n", filepath, rc);
        }
    } else {
        if ( dbHandle ) {
            fprintf(stderr, "ERROR:  unable to open `%s` (sqlite err = %d, %s)\n", filepath, rc, sqlite3_errmsg(dbHandle));
            sqlite3_close_v2(dbHandle);
        } else {
            fprintf(stderr, "ERROR:  unable to open `%s` (sqlite err = %d)\n", filepath, rc);
        }
    }
    return newSchedule;
}

//

void
SScheduleRelease(
    SScheduleRef    aSchedule
)
{
    SSchedule       *SCHEDULE = (SSchedule*)aSchedule;

    if ( --(SCHEDULE->refcount) == 0 ) __SScheduleDealloc(SCHEDULE);
}

//

SScheduleRef
SScheduleRetain(
    SScheduleRef    aSchedule
)
{
    SSchedule       *SCHEDULE = (SSchedule*)aSchedule;

    SCHEDULE->refcount++;
    return aSchedule;
}

//

STimeRangeRef
SScheduleGetPeriod(
    SScheduleRef    aSchedule
)
{
    return aSchedule->period;
}

//

unsigned int
SScheduleGetBlockCount(
    SScheduleRef    aSchedule
)
{
    return aSchedule->blockCount;
}

//

STimeRangeRef
SScheduleGetBlockAtIndex(
    SScheduleRef    aSchedule,
    unsigned int    index
)
{
    if ( index < aSchedule->blockCount ) {
        SScheduleBlock  *block = aSchedule->blocks;

        while ( index-- ) block = block->link;
        return block->period;
    }
    return NULL;
}

//

const char*
SScheduleGetLastErrorMessage(
    SScheduleRef    aSchedule
)
{
    return aSchedule->lastErrorMessage;
}

//

bool
SScheduleIsFull(
    SScheduleRef    aSchedule
)
{
    // A full schedule means that all dates in the scheduling period are
    // marked, implying the list of blocks is one element long and its
    // period is equal to the scheduling period:
    return ((aSchedule->blockCount == 1) && STimeRangeIsEqual(aSchedule->period, aSchedule->blocks->period));
}

//

STimeRangeRef
SScheduleGetNextOpenBlock(
    SScheduleRef    aSchedule
)
{
    STimeRangeRef   outBlock = NULL;

    //
    // Trivial case, nothing scheduled:
    //
    if ( aSchedule->blockCount == 0 ) return STimeRangeRetain(aSchedule->period);

    //
    // First, check if there's unaccounted time at the start of the scheduling
    // period:
    //
    outBlock = STimeRangeLeading(aSchedule->period, aSchedule->blocks->period);
    if ( ! outBlock ) {
        //
        // For each pair of blocks, check if they are contiguous; if not, then
        // there's open time between them:
        //
        SScheduleBlock      *block = aSchedule->blocks;

        while ( ! outBlock && block->link ) {
            if ( ! STimeRangeIsContiguous(block->period, block->link->period) ) {
                //
                // Run from end+1 of one through start of the other:
                //
                time_t      start, end;
                STimeRangeGetEndTime(block->period, &start);
                STimeRangeGetStartTime(block->link->period, &end);
                start++; end--;
                outBlock = STimeRangeCreate(start, end);
                break;
            }
            block = block->link;
        }
        if ( ! outBlock && block ) {
            //
            // We're at the last block of time, check for trailing time:
            //
            outBlock = STimeRangeTrailing(aSchedule->period, block->period);
        }
    }
    return outBlock;
}

//

STimeRangeRef
SScheduleGetNextOpenBlockBeforeTime(
    SScheduleRef    aSchedule,
    time_t          beforeTime
)
{
    STimeRangeRef   outBlock = NULL;
    
    //
    // Full?
    //
    if ( SScheduleIsFull(aSchedule) ) return NULL;
    
    //
    // Is beforeTime inside the scheduling period?
    //
    if ( ! STimeRangeContainsTime(aSchedule->period, beforeTime) ) {
        //
        // Does beforeTime occur AFTER the scheduling period?
        //
        time_t      endOfPeriod;
        
        if ( ! STimeRangeGetEndTime(aSchedule->period, &endOfPeriod) ) return NULL;
        if ( beforeTime < endOfPeriod ) return NULL;
        beforeTime = endOfPeriod + 1;
    }

    //
    // Trivial case, nothing scheduled:
    //
    if ( aSchedule->blockCount == 0 ) {
        if ( STimeRangeContainsTime(aSchedule->period, beforeTime) ) {
            if ( STimeRangeIsStartTimeSet(aSchedule->period) ) {
                time_t  startTime;
        
                STimeRangeGetStartTime(aSchedule->period, &startTime);
                return STimeRangeCreate(startTime, beforeTime - 1);
            }
            return STimeRangeCreateWithEnd(beforeTime - 1);
        } else if ( STimeRangeDoesTimeFollowRange(aSchedule->period, beforeTime) ) {
            //
            // The whole period, please:
            //
            return STimeRangeCopy(aSchedule->period);
        }
        return NULL;
    }

    //
    // First, check if there's unaccounted time at the start of the scheduling
    // period:
    //
    outBlock = STimeRangeLeading(aSchedule->period, aSchedule->blocks->period);
    if ( outBlock ) {
        if ( STimeRangeContainsTime(outBlock, beforeTime) ) {
            if ( STimeRangeIsStartTimeSet(outBlock) ) {
                time_t  startTime;
            
                STimeRangeGetStartTime(outBlock, &startTime);
                return STimeRangeCreate(startTime, beforeTime - 1);
            }
            return STimeRangeCreateWithEnd(beforeTime - 1);
        }
        STimeRangeRelease(outBlock); outBlock = NULL;
    }
    
    //
    // For each pair of blocks, check if they are contiguous; if not, then
    // there's open time between them:
    //
    SScheduleBlock      *block = aSchedule->blocks;

    while ( ! outBlock && block->link ) {
        if ( ! STimeRangeIsContiguous(block->period, block->link->period) ) {
            //
            // Run from end+1 of one through start of the other:
            //
            time_t      start, end;
            STimeRangeGetEndTime(block->period, &start);
            STimeRangeGetStartTime(block->link->period, &end);
            start++; end--;
            if ( (start <= beforeTime) && (end >= beforeTime) ) {
                outBlock = STimeRangeCreate(start, beforeTime);
            }
            break;
        }
        block = block->link;
    }
    if ( ! outBlock && block ) {
        //
        // We're at the last block of time, check for trailing time:
        //
        outBlock = STimeRangeTrailing(aSchedule->period, block->period);
        if ( outBlock ) {
            if ( STimeRangeContainsTime(outBlock, beforeTime) ) {
                if ( STimeRangeIsStartTimeSet(outBlock) ) {
                    time_t  startTime;
            
                    STimeRangeGetStartTime(outBlock, &startTime);
                    return STimeRangeCreate(startTime, beforeTime - 1);
                }
                return STimeRangeCreateWithEnd(beforeTime - 1);
            }
            STimeRangeRelease(outBlock); outBlock = NULL;
        }
    }
    return outBlock;
}

//

bool
SScheduleAddScheduledBlock(
    SScheduleRef    aSchedule,
    STimeRangeRef   scheduledBlock
)
{
    SSchedule       *SCHEDULE = (SSchedule*)aSchedule;
    SScheduleBlock  *block, *lastBlock;
    STimeRangeRef   addThisRange = NULL;
    bool            rc = false;

    //
    // First, let's see if the scheduled block is fully contained in the
    // scheduling period:
    //
    if ( STimeRangeIsContained(scheduledBlock, aSchedule->period) ) {
        addThisRange = STimeRangeRetain(scheduledBlock);
    }
    //
    // Nope, not fully contained...does it at least intersect the scheduling
    // period?
    //
    else if ( STimeRangeDoesIntersect(scheduledBlock, aSchedule->period) ) {
        addThisRange = STimeRangeIntersection(scheduledBlock, aSchedule->period);
    }
    //
    // The block of time isn't something we can schedule, get outta here now.
    //
    else {
        return false;
    }

    //
    // Trivial case, we don't have any blocks yet:
    //
    if ( aSchedule->blockCount == 0 ) {
        block = SScheduleBlockAlloc();
        if ( block ) {
            block->period = STimeRangeRetain(addThisRange);
            SCHEDULE->blocks = block;
            SCHEDULE->blockCount++;
            rc = true;
        }
        STimeRangeRelease(addThisRange);
        return rc;
    }

    //
    // Now, we want to figure out where in our range list we should insert
    // addThisRange:
    //
    block = aSchedule->blocks;
    lastBlock = NULL;
    while ( block ) {
        if ( STimeRangeCmp(addThisRange, block->period) > 0 ) break;
        lastBlock = block;
        block = block->link;
    }

    //
    // Wherever we ended up, do we intersect or are we contiguous with the
    // previous scheduled block?
    //
    rc = false;
    if ( lastBlock ) {
        if ( STimeRangeIsContiguous(addThisRange, lastBlock->period) ) {
            STimeRangeRef   mergedRange = STimeRangeJoin(addThisRange, lastBlock->period);

            STimeRangeRelease(addThisRange);
            addThisRange = mergedRange;
            rc = true;
        }
        else if ( STimeRangeDoesIntersect(addThisRange, lastBlock->period) ) {
            if ( ! STimeRangeIsContained(addThisRange, lastBlock->period) ) {
                STimeRangeRef   mergedRange = STimeRangeUnion(addThisRange, lastBlock->period);

                STimeRangeRelease(addThisRange);
                addThisRange = mergedRange;
                rc = true;
            } else {
                // addThisRange doesn't need to be added:
                STimeRangeRelease(addThisRange);
                return true;
            }
        }
        if ( rc ) {
            if ( ! addThisRange ) return false;
            STimeRangeRelease(lastBlock->period);
            lastBlock->period = addThisRange;
        }
    }
    //
    // If we didn't merge with the previous scheduled block, then how about
    // merging with a trailing scheduled block?
    //
    if ( ! rc && block ) {
        if ( STimeRangeIsContiguous(addThisRange, block->period) ) {
            STimeRangeRef   mergedRange = STimeRangeJoin(addThisRange, block->period);

            STimeRangeRelease(addThisRange);
            addThisRange = mergedRange;
            rc = true;
        }
        else if ( STimeRangeDoesIntersect(addThisRange, block->period) ) {
            if ( ! STimeRangeIsContained(addThisRange, block->period) ) {
                STimeRangeRef   mergedRange = STimeRangeUnion(addThisRange, block->period);

                STimeRangeRelease(addThisRange);
                addThisRange = mergedRange;
                rc = true;
            } else {
                // addThisRange doesn't need to be added:
                STimeRangeRelease(addThisRange);
                return true;
            }
        }
        if ( rc ) {
            if ( ! addThisRange ) return false;
            STimeRangeRelease(block->period);
            block->period = addThisRange;
        }
    }
    //
    // If we didn't merge, then we need to insert a new SScheduleBlock
    // between lastBlock and block:
    //
    if ( ! rc ) {
        SScheduleBlock  *newBlock = SScheduleBlockAlloc();

        if ( ! newBlock ) return false;
        newBlock->period = addThisRange;

        if ( lastBlock ) {
            // Insert the item in the list:
            newBlock->link = lastBlock->link;
            lastBlock->link = newBlock;
        }
        else if ( block ) {
            // Head of the linked list:
            newBlock->link = aSchedule->blocks;
            SCHEDULE->blocks = newBlock;
        }
        SCHEDULE->blockCount++;
        return true;
    }

    //
    // We get here after we merged the new block with an existing one;
    // now we need to make another pass at the list, merging any
    // subsequent blocks that intersect or are contiguous.
    //
    block = aSchedule->blocks;
    while ( block->link ) {
        //
        // Contiguous?
        //
        if ( STimeRangeIsContiguous(block->period, block->link->period) ) {
            SScheduleBlock  *nextBlock = block->link;
            STimeRangeRef   mergedRange = STimeRangeJoin(block->period, nextBlock->period);

            if ( ! mergedRange ) {
                fprintf(stderr, "FATAL:  unable to allocated merged STimeRange while adding a scheduled block!\n");
                exit(ENOMEM);
            }
            STimeRangeRelease(block->period);
            block->period = mergedRange;
            block->link = nextBlock->link;
            SScheduleBlockDealloc(nextBlock);
            SCHEDULE->blockCount--;
        }
        //
        // Intersects?
        //
        else if ( STimeRangeDoesIntersect(block->period, block->link->period) ) {
            SScheduleBlock  *nextBlock = block->link;
            STimeRangeRef   mergedRange = STimeRangeUnion(block->period, nextBlock->period);

            if ( ! mergedRange ) {
                fprintf(stderr, "FATAL:  unable to allocated merged STimeRange while adding a scheduled block!\n");
                exit(ENOMEM);
            }
            STimeRangeRelease(block->period);
            block->period = mergedRange;
            block->link = nextBlock->link;
            SScheduleBlockDealloc(nextBlock);
            SCHEDULE->blockCount--;
        }
        //
        // Move to next block:
        //
        else {
            block = block->link;
        }
    }

    return rc;
}

//

bool
__SScheduleCreateTables(
    SSchedule       *aSchedule,
    sqlite3         *dbHandle
)
{
    int             rc;
    char*           sqlErrMsg;
    
    //
    // Create the schedule table:
    //
    rc = sqlite3_exec(
                dbHandle,
                "CREATE TABLE schedule ("
                "  period           TEXT NOT NULL"
                ")",
                NULL,
                NULL,
                &sqlErrMsg
            );
    if ( rc == SQLITE_OK ) {
        //
        // Create the blocks table:
        //
        rc = sqlite3_exec(
                    dbHandle,
                    "CREATE TABLE blocks ("
                    "  block_id         INTEGER PRIMARY KEY,"
                    "  period           TEXT UNIQUE NOT NULL"
                    ")",
                    NULL,
                    NULL,
                    &sqlErrMsg
                );
        if ( rc == SQLITE_OK ) {
            //
            // Add the default scheduling period record:
            //
            rc = sqlite3_exec(
                        dbHandle,
                        "INSERT INTO schedule (period) VALUES (':')",
                        NULL,
                        NULL,
                        &sqlErrMsg
                    );            
        }
    }
    if ( rc != SQLITE_OK ) {
        if ( sqlErrMsg ) {
            __SScheduleSetLastErrorMessage(aSchedule, "Failure while creating tables in export file (sqlite err = %d): %s", rc, sqlErrMsg);
            free((void*)sqlErrMsg);
        } else {
            __SScheduleSetLastErrorMessage(aSchedule, "Failure while creating tables in export file (sqlite err = %d)", rc);
        }
        return false;
    }
    return true;
}

bool
SScheduleWriteToFile(
    SScheduleRef    aSchedule,
    const char      *filepath
)
{
    SSchedule       *SCHEDULE = (SSchedule*)aSchedule;
    sqlite3         *dbHandle;
    struct stat     finfo;
    int             rc = stat(filepath, &finfo);
    bool            shouldCreateTables = false;
    
    if ( rc == 0 ) {
        if ( (finfo.st_mode & S_IFREG) ) {
            rc = sqlite3_open_v2(filepath, &dbHandle, SQLITE_OPEN_READWRITE, NULL);
        } else {
            __SScheduleSetLastErrorMessage(SCHEDULE, "Attempt to write schedule to non-file object (st_mode = %x) `%s`", finfo.st_mode, filepath);
            return false;
        }
    } else {
        shouldCreateTables = true;
        rc = sqlite3_open_v2(filepath, &dbHandle, SQLITE_OPEN_CREATE | SQLITE_OPEN_READWRITE, NULL);
    }
    if ( rc == SQLITE_OK ) {
        sqlite3_stmt    *sqlQuery;
        SScheduleBlock  *blocks = aSchedule->blocks;
        const char      *timeRangeStr, *errorSource;
        
        //
        // Create tables?
        //
        if ( shouldCreateTables && ! __SScheduleCreateTables(SCHEDULE, dbHandle) ) goto cleanup;
        
        //
        // Start transaction:
        //
        rc = sqlite3_exec(dbHandle, "BEGIN", NULL, NULL, NULL);
        if ( rc != SQLITE_OK ) { errorSource = "start transaction"; goto cleanup; }
        
        //
        // Update the period:
        //
        rc = sqlite3_prepare_v2(
                    dbHandle,
                    "UPDATE schedule SET period = ?",
                    -1,
                    &sqlQuery,
                    NULL
                );
        if ( rc != SQLITE_OK ) { errorSource = "prepare schedule table update"; goto cleanup; }
        timeRangeStr = STimeRangeGetCString(aSchedule->period);
        if ( ! timeRangeStr ) {
            rc = SQLITE_NOMEM;
            errorSource = "invalid scheduling period string";
            goto cleanup;
        }
        rc = sqlite3_bind_text(sqlQuery, 1, timeRangeStr, -1, SQLITE_STATIC);
        if ( rc != SQLITE_OK ) { errorSource = "bind scheduling period string to query"; goto cleanup; }
        rc = sqlite3_step(sqlQuery);
        if ( rc != SQLITE_DONE ) { errorSource = "update scheduling period"; goto cleanup; }
        sqlite3_finalize(sqlQuery);
        sqlQuery = NULL;
        
        //
        // Delete all rows from the blocks table:
        //
        rc = sqlite3_exec(dbHandle, "DELETE FROM blocks", NULL, NULL, NULL);
        if ( rc != SQLITE_OK ) { errorSource = "scrub scheduled blocks table"; goto cleanup; }
        
        //
        // Prep the blocks insert query:
        //
        rc = sqlite3_prepare_v2(
                    dbHandle,
                    "INSERT INTO blocks (period) VALUES (?)",
                    -1,
                    &sqlQuery,
                    NULL
                );
        if ( rc != SQLITE_OK ) { errorSource = "prepare scheduled blocks table insert"; goto cleanup; }
        
        SScheduleBlock  *block = aSchedule->blocks;
        
        while ( block ) {
            timeRangeStr = STimeRangeGetCString(block->period);
            if ( ! timeRangeStr ) {
                rc = SQLITE_NOMEM;
                errorSource = "invalid scheduling period string";
                goto cleanup;
            }
            rc = sqlite3_bind_text(sqlQuery, 1, timeRangeStr, -1, SQLITE_STATIC);
            if ( rc != SQLITE_OK ) { errorSource = "bind block period string to query"; goto cleanup; }
            rc = sqlite3_step(sqlQuery);
            if ( rc != SQLITE_DONE ) { errorSource = "insert into scheduled blocks"; goto cleanup; }
            rc = sqlite3_reset(sqlQuery);
            if ( rc != SQLITE_OK ) { errorSource = "reset scheduled blocks query"; goto cleanup; }
            
            block = block->link;
        }
        sqlite3_finalize(sqlQuery);
        sqlQuery = NULL;
        
        //
        // Commit the changes:
        //
        rc = sqlite3_exec(dbHandle, "COMMIT", NULL, NULL, NULL);
        if ( rc != SQLITE_OK ) { errorSource = "commit transaction"; goto cleanup; }
        
        //
        // Hooray, we did it!
        //
        sqlite3_close_v2(dbHandle);
        return true;
        
cleanup:
        sqlite3_exec(dbHandle, "ROLLBACK", NULL, NULL, NULL);
        __SScheduleSetLastErrorMessage(SCHEDULE, "Error at %s for `%s` (sqlite err = %d, %s)\n", errorSource, filepath, rc, sqlite3_errmsg(dbHandle));
        if ( sqlQuery ) sqlite3_finalize(sqlQuery);
        sqlite3_close_v2(dbHandle);
    } else {
        if ( dbHandle ) {
            __SScheduleSetLastErrorMessage(SCHEDULE, "Unable to open `%s` (sqlite err = %d, %s)\n", filepath, rc, sqlite3_errmsg(dbHandle));
            sqlite3_close_v2(dbHandle);
        } else {
            __SScheduleSetLastErrorMessage(SCHEDULE, "Unable to open `%s` (sqlite err = %d)\n", filepath, rc);
        }
    }
    return false;
}

//

void
SScheduleSummarize(
    SScheduleRef    aSchedule,
    FILE            *outStream
)
{
    if ( aSchedule ) {
        SScheduleBlock  *block = aSchedule->blocks;
        unsigned int    i = 0;

        fprintf(outStream,
                "SSchedule@%p(%u) {\n"
                "  period: %s\n"
                "  blockCount: %u\n",
                aSchedule, aSchedule->refcount,
                STimeRangeGetCString(aSchedule->period),
                aSchedule->blockCount
            );
        while ( block ) {
            fprintf(outStream,"    %d : %s\n", i++, STimeRangeGetCString(block->period));
            block = block->link;
        }
        fprintf(outStream,
                "  lastErrorMessage: %s\n"
                "}\n",
                ( aSchedule->lastErrorMessage ? aSchedule->lastErrorMessage : "<none>" )
            );
    }
}

//

#ifdef SSCHEDULE_UNIT_TEST

#include <stdio.h>
#include <errno.h>

int
main()
{
    STimeRangeRef       thePeriod = STimeRangeCreateWithString("20191001T000000-0400:", NULL);
    SScheduleRef        theSchedule;
    int                 outerLoop, outerLoopMax = 1 + (random() % 5), retry = 1 + (random() % 5);
    bool                ok;
    
    theSchedule = SScheduleCreateWithFile("dump-schedule.sqlite3db");
    if ( ! theSchedule ) theSchedule = SScheduleCreate(thePeriod);

    STimeRangeRelease(thePeriod);
    if ( ! theSchedule ) return ENOMEM;

start_of_outer_loop:
    outerLoop = 0;
    while ( outerLoop < outerLoopMax ) {
        STimeRangeRef   tmpPeriod = NULL;

        __SScheduleDebug((SSchedule*)theSchedule);

        thePeriod = SScheduleGetNextOpenBlock(theSchedule);
        if ( thePeriod ) {
            unsigned int    i = 0, iMax = STimeRangeGetCountOfPeriodsOfLength(thePeriod, 60 * 60 * 24);

            printf("%s\n", STimeRangeGetCString(thePeriod));
            if ( iMax > 5 ) iMax = 5;
            while ( i < iMax ) {
                STimeRangeRef   subPeriod = STimeRangeGetPeriodOfLengthAtIndex(thePeriod, 60 * 60 * 24, i);

                if ( subPeriod ) {
                    if ( i == outerLoop ) tmpPeriod = STimeRangeRetain(subPeriod);

                    printf("%05u : %s\n", i, STimeRangeGetCString(subPeriod));
                    STimeRangeRelease(subPeriod);
                }
                i++;
            }
            STimeRangeRelease(thePeriod);
        }
        if ( tmpPeriod ) {
            printf("Schedule time: %s = %d\n", STimeRangeGetCString(tmpPeriod), SScheduleAddScheduledBlock(theSchedule, tmpPeriod));
            STimeRangeRelease(tmpPeriod);
        }
        outerLoop++;
    }

    printf("---------\n");
    thePeriod = STimeRangeCreateWithString("20191002T000000-0400:20191002T235959-0400", NULL);
    if ( thePeriod ) {
        printf("Schedule time: %s = %d\n", STimeRangeGetCString(thePeriod), SScheduleAddScheduledBlock(theSchedule, thePeriod));
        STimeRangeRelease(thePeriod);

        __SScheduleDebug((SSchedule*)theSchedule);
    }

    if ( retry-- ) goto start_of_outer_loop;

    ok = SScheduleWriteToFile(theSchedule, "dump-schedule.sqlite3db");
    if ( ! ok ) {
        printf("Write schedule to `%s` failed\n:: %s\n", "dump-schedule.sqlite3db", SScheduleGetLastErrorMessage(theSchedule));
    } else {
        printf("Wrote schedule to `%s`\n", "dump-schedule.sqlite3db");
    }
    
    SScheduleRelease(theSchedule);

    return 0;
}

#endif

