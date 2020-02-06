/*
 * dtrmgr.c
 *
 * Main program.
 *
 */

#include "SSchedule.h"
#include <getopt.h>
#include <stdarg.h>


#ifndef DTRMGR_DEFAULT_DURATION
#define DTRMGR_DEFAULT_DURATION     (12 * 60 * 60)
#endif
const int dtrmgr_default_duration = DTRMGR_DEFAULT_DURATION;

#ifndef DTRMGR_DEFAULT_JUSTIFY
#define DTRMGR_DEFAULT_JUSTIFY      kSTimeRangeJustifyTimeToHours
#endif
const STimeRangeJustifyTimeTo dtrmgr_default_justify = DTRMGR_DEFAULT_JUSTIFY;


time_t
dtrmgrUnitMultiplier(
    const char  *unitStr,
    time_t      matchValue,
    time_t      noMatchValue,
    ...
)
{
    va_list     argv;
    time_t      multiplier = noMatchValue;
    const char  *s;
    
    va_start(argv, noMatchValue);
    while ( (s = va_arg(argv, const char*)) ) {
        if ( strcasecmp(unitStr, s) == 0 ) {
            multiplier = matchValue;
            break;
        }
    }
    va_end(argv);
    return multiplier;
}

const struct option cliOptions[] = {
            { "help",           no_argument,        NULL,       'h' },
            { "init",           required_argument,  NULL,       'i' },
            { "load",           required_argument,  NULL,       'l' },
            { "save",           optional_argument,  NULL,       's' },
            { "print",          no_argument,        NULL,       'p' },
            { "before",         required_argument,  NULL,       'b' },
            { "duration",       required_argument,  NULL,       'd' },
            { "next",           required_argument,  NULL,       'n' },
            { "add-range",      required_argument,  NULL,       'a' },
            { "add-file",       required_argument,  NULL,       'f' },
            { NULL,             0,                  NULL,       0   }
        };
const char *cliOptionsStr = "hi:l:s::pb:d:n:a:f:";

//

void
usage(
    const char  *exe
)
{
    printf(
            "usage:\n\n"
            "    %s {options}\n\n"
            "  options:\n\n"
            "    -h/--help                              show built-in help for the program\n"
            "\n"
            "   working schedule i/o options:\n"
            "\n"
            "    --init=<period>, -i <period>           initialize a new working schedule with the specified\n"
            "                                           scheduling period\n"
            "    --load=<file>, -l <file>               load the working schedule from the specified file\n"
            "    --save{=<file>}, -s{<file>}            save the working schedule; if a <file> is not\n"
            "                                           specified, the origin file is used\n"
            "    -p/--print                             summarize the working schedule to stdout\n"
            "\n"
            "   working schedule modification options:\n"
            "\n"
            "    --before=<date-time>, -b <date-time>   do not generate time blocks after this date and time\n"
            "                                           (default: now)\n"
            "    --duration=<dur>, -d <dur>             generate time blocks of this length\n"
            "                                           (default: %d seconds)\n"
            "    --next=<N>, -n <N>                     generate up to N unscheduled time blocks\n"
            "    --add-range=<range>, -a <range>        add a scheduled time range to the working schedule\n"
            "    --add-file=<file>, -f <file>           add time range(s) read from the given file to the\n"
            "                                           working schedule\n"
            "\n"
            "  <date-time> :: a date and time in a variety of formats (as recognized by getdate)\n"
            "  <dur> :: <integer>{<unit>} | <day>-<hr>{:<min>{:<sec>}} | {<hr>:{<min>:}}<sec>\n"
            "  <unit> :: d{ay{s}} | h{our{s}} | hr{s} | m{in{ute}{s}} | s{ec{ond}{s}}\n"
            "  <range> :: {<YYYY><MM><DD>T<HH><MM><SS><±HHMM>}:{<YYYY><MM><DD>T<HH><MM><SS><±HHMM>}\n"
            "\n",
            exe,
            dtrmgr_default_duration
        );
}

//

const char*
fgetline(
    FILE    *fptr,
    size_t  *lineLen
)
{
    static char     *fgetline_buffer = NULL;
    static size_t   fgetline_buffer_size = 0;
    
    size_t          i = 0;
    int             c;
    
    while ( (c = fgetc(fptr)) != EOF ) {
        if ( c == '\n' ) break;
        if ( i + 1 > fgetline_buffer_size ) {
            size_t  new_buffer_len = 4096 * ((fgetline_buffer_size / 4096) + 1);
            char    *new_buffer = realloc(fgetline_buffer, new_buffer_len);
            
            if ( ! new_buffer ) {
                fprintf(stderr, "FATAL:  unable to resize fgetline() input buffer\n");
                exit(ENOMEM);
            }
            fgetline_buffer = new_buffer;
            fgetline_buffer_size = new_buffer_len;
        }
        fgetline_buffer[i++] = c;
    }
    if ( lineLen ) *lineLen = i;
    fgetline_buffer[i] = '\0';
    return (const char*)fgetline_buffer;
}

//

int
main(
    int                         argc,
    char*                       argv[]
)
{
    SScheduleRef                theSchedule = NULL;
    const char                  *theScheduleDBFile = NULL;
    bool                        shouldQuickLoad = true;
    time_t                      duration = (time_t)dtrmgr_default_duration;
    time_t                      beforeTime = time(NULL);
    STimeRangeJustifyTimeTo     justify = dtrmgr_default_justify;
    int                         optc;
    
    while ( (optc = getopt_long(argc, argv, cliOptionsStr, cliOptions, NULL)) != -1 ) {
        switch ( optc ) {
            case 'h': {
                usage(argv[0]);
                exit(0);
            }
            
            case 'i': {
                if ( optarg ) {
                    STimeRangeRef   period = STimeRangeCreateWithString(optarg, NULL);
                
                    if ( ! period || ! STimeRangeIsValid(period) ) {
                        if ( period ) STimeRangeRelease(period);
                        fprintf(stderr, "ERROR:  invalid scheduling time period: %s\n", optarg);
                        exit(EINVAL);
                    }
                    if ( theSchedule ) SScheduleRelease(theSchedule);
                    theSchedule = SScheduleCreate(period);
                } else {
                    fprintf(stderr, "ERROR:  invalid scheduling time period: %s\n", optarg);
                    exit(EINVAL);
                }
                break;
            }
            
            case 'l': {
                if ( theSchedule ) SScheduleRelease(theSchedule);
                theSchedule = shouldQuickLoad? SScheduleCreateWithFileQuick(optarg) : SScheduleCreateWithFile(optarg);
                if ( theSchedule ) theScheduleDBFile = optarg;
                break;
            }
            
            case 's': {
                if ( theSchedule ) {
                    if ( optarg && *optarg ) {
                        if ( ! SScheduleWriteToFile(theSchedule, optarg) ) {
                            fprintf(stderr, "ERROR:  unable to save working schedule: %s\n", SScheduleGetLastErrorMessage(theSchedule));
                        } else {
                               theScheduleDBFile = optarg;
                        }
                    } else if ( (optind < argc) && (argv[optind][0] != '-') ) {
                        if ( ! SScheduleWriteToFile(theSchedule, argv[optind]) ) {
                            fprintf(stderr, "ERROR:  unable to save working schedule: %s\n", SScheduleGetLastErrorMessage(theSchedule));
                        } else {
                               theScheduleDBFile = argv[optind];
                        }
                        optind++;
                    } else if ( theScheduleDBFile ) {
                        if ( ! SScheduleWriteToFile(theSchedule, theScheduleDBFile) ) {
                            fprintf(stderr, "ERROR:  unable to save working schedule: %s\n", SScheduleGetLastErrorMessage(theSchedule));
                        }
                    } else {
                        fprintf(stderr, "ERROR:  no filename to which to save working schedule\n");
                        exit(EINVAL);
                    }
                }
                break;
            }
            
            case 'p': {
                if ( theSchedule ) SScheduleSummarize(theSchedule, stdout);
                break;
            }
            
            case 'b': {
                if ( ! STimeRangeParseDateAndTime(optarg, &beforeTime) ) {
                    fprintf(stderr, "ERROR:  invalid date/time provided with --before/-b: %s\n", optarg);
                    exit(EINVAL);
                }
                break;   
            }
            
            case 'd': {
                char        *endptr;
                long        value = strtol(optarg, &endptr, 0);
                
                if ( (endptr > optarg) && (value > 0) ) {
                    if ( *endptr ) {
                        if ( (*endptr == ':') || (*endptr == '-') ) {
                            long    components[4] = {value, 0, 0, 0};
                            int     nComponents = 1, nComponentsMax = (*endptr == '-') ? 4 : 3;
                            char    *endendptr;
                            
                            while ( nComponents < nComponentsMax ) {
                                components[nComponents] = strtol(++endptr, &endendptr, 0);
                                if ( ! (endendptr > endptr) ) break;
                                endptr = endendptr;
                                if ( (++nComponents < nComponentsMax) && (*endptr != ':') ) break;
                            }
                            if ( *endptr ) {
                                fprintf(stderr, "ERROR:  invalid duration component at: %s\n", endptr);
                                exit(EINVAL);
                            }
                            if ( nComponentsMax == 4 ) {
                                switch ( nComponents ) {
                                    case 4:
                                        duration = components[3] + 60 * components[2] + 3600 * components[1] + 86400 * components[0];
                                        break;
                                    case 3:
                                        duration = 60 * components[2] + 3060 * components[1] + 86400 * components[0];
                                        break;
                                    case 2:
                                        duration = 3600 * components[1] + 86400 * components[0];
                                        break;
                                }
                            } else {
                                switch ( nComponents ) {
                                    case 3:
                                        duration = components[2] + 60 * components[1] + 3600 * components[0];
                                        break;
                                    case 2:
                                        duration = 60 * components[1] + 3600 * components[0];
                                        break;
                                }
                            }
                        } else {
                            //
                            // Check for a unit:
                            //
                            time_t  multiplier = dtrmgrUnitMultiplier(endptr, 1, 0, "seconds", "second", "secs", "sec", "s", NULL);
                            if ( multiplier == 0 ) multiplier = dtrmgrUnitMultiplier(endptr, 60, 0, "minutes", "minute", "mins", "min", "m", NULL);
                            if ( multiplier == 0 ) multiplier = dtrmgrUnitMultiplier(endptr, 3600, 0, "hours", "hour", "hrs", "hr", "h", NULL);
                            if ( multiplier == 0 ) multiplier = dtrmgrUnitMultiplier(endptr, 86400, 0, "days", "day", "d", NULL);
                            if ( multiplier == 0 ) {
                                fprintf(stderr, "ERROR:  invalid duration unit provided with --duration/-d: %s\n", endptr);
                                exit(EINVAL);
                            }
                            duration = value * multiplier;
                        }
                    } else {
                        duration = value;
                    }
                    
                    //
                    // Figure justification interval:
                    //
                    if ( duration >= 86400 ) {
                        justify = kSTimeRangeJustifyTimeToDays;
                    }
                    else if ( duration >= 3600 ) {
                        justify = kSTimeRangeJustifyTimeToHours;
                    }
                    else {
                        justify = kSTimeRangeJustifyTimeToMinutes;
                    }
                } else {
                    fprintf(stderr, "ERROR:  invalid duration provided with --duration/-d: %s\n", optarg);
                    exit(EINVAL);
                }
                break;
            }
            
            case 'n': {
                char        *endptr;
                long        N = strtol(optarg, &endptr, 0);
                
                if ( (endptr > optarg) && (N > 0) ) {
                    if ( ! theSchedule ) {
                        fprintf(stderr, "ERROR:  no working schedule\n");
                        exit(EINVAL);
                    }
                    if ( ! SScheduleIsFull(theSchedule) ) {
                        time_t              beforeTimeAdj = STimeRangeJustifyTime(beforeTime, justify, false);
                        
                        while ( N > 0 ) {
                            STimeRangeRef   nextBlock = SScheduleGetNextOpenBlockBefore(theSchedule, beforeTimeAdj);
                            
                            if ( ! nextBlock ) break;
                            //
                            // How many blocks of size duration is that?
                            //
                            unsigned int        nPeriods = STimeRangeGetCountOfPeriodsOfLength(nextBlock, duration);
                            
                            if ( nPeriods > 0 ) {
                                unsigned int    iPeriod = 0;
                                
                                while ( (N > 0) && (iPeriod < nPeriods) ) {
                                    STimeRangeRef   subRange = STimeRangeGetPeriodOfLengthAtIndex(nextBlock, duration, iPeriod++);
                                    
                                    if ( ! subRange ) {
                                        fprintf(stderr, "ERROR:  unable to allocate scheduling sub-range\n");
                                        exit(ENOMEM);
                                    }
                                    printf("%s\n", STimeRangeGetCString(subRange));
                                    SScheduleAddScheduledBlock(theSchedule, subRange);
                                    STimeRangeRelease(subRange);
                                    N--;
                                }
                            } else {
                                printf("%s\n", STimeRangeGetCString(nextBlock));
                                SScheduleAddScheduledBlock(theSchedule, nextBlock);
                                N--;
                            }
                            STimeRangeRelease(nextBlock);
                        }
                    }
                } else {
                    fprintf(stderr, "ERROR:  invalid block count provided with --next/-n: %s\n", optarg);
                    exit(EINVAL);
                }
                break;
            }
            
            case 'a': {
                if ( ! theSchedule ) {
                    fprintf(stderr, "ERROR:  no working schedule\n");
                    exit(EINVAL);
                }
                
                STimeRangeRef   nextRange = STimeRangeCreateWithString(optarg, NULL);
                
                if ( nextRange && STimeRangeIsValid(nextRange) ) {
                    SScheduleAddScheduledBlock(theSchedule, nextRange);
                    STimeRangeRelease(nextRange);
                } else {
                    fprintf(stderr, "ERROR:  invalid time range string for addition: %s\n", optarg);
                    exit(EINVAL);
                }
                break;
            }
            
            case 'f': {
                if ( ! theSchedule ) {
                    fprintf(stderr, "ERROR:  no working schedule\n");
                    exit(EINVAL);
                }
                
                FILE        *inputFPtr;
                bool        closeWhenDone = true;
            
                if ( strcmp(optarg, "-") == 0 ) {
                    inputFPtr = stdin;
                    closeWhenDone = false;
                } else {
                    inputFPtr = fopen(optarg, "r");
                    if ( ! inputFPtr ) {
                        fprintf(stderr, "ERROR:  unable open file for reading time ranges (errno = %d): %s\n", errno, optarg);
                        exit(errno);
                    }
                }
                //
                // Process time ranges in a loop:
                //
                const char  *nextRangeStr;
            
                while ( ! feof(inputFPtr) && (nextRangeStr = fgetline(inputFPtr, NULL)) ) {
                    while ( isspace(*nextRangeStr) ) nextRangeStr++;
                    if ( *nextRangeStr ) {
                        STimeRangeRef   nextRange = STimeRangeCreateWithString(nextRangeStr, NULL);
                
                        if ( nextRange && STimeRangeIsValid(nextRange) ) {
                            SScheduleAddScheduledBlock(theSchedule, nextRange);
                            STimeRangeRelease(nextRange);
                        } else {
                            fprintf(stderr, "ERROR:  invalid time range string for addition: %s\n", nextRangeStr);
                            exit(EINVAL);
                        }
                    }
                }
                if ( closeWhenDone ) fclose(inputFPtr);
                break;
            }            
        }
    }
    
    return 0;
}