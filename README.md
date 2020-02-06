# dtrmgr

The `dtrmgr` acronym is short for Date-Time Range Manager.  It is a program to manage stateful allocation of blocks of time.  Inception traces to a need to do periodic accounting dumps from Slurm for ingest into XDMod.  Enormous Python calendaring frameworks with lots of dependencies were extreme overkill:  I merely wanted to

1. Represent an overall time range (bounded or unbounded at each end) during which blocks of time could be *allocated* — a **schedule**
2. Locate un-allocated blocks of time in the schedule
3. Allocate non-overlapping ranges of time within the schedule

Ideally, the tool would allow for arbitrary duration when making allocations.  The state of the schedule could be written to a file and restored later; adoption of an easy-to-access format (versus a simple binary dump, let's say) was preferable.

The `dtrmgr` program meets those requirements.  It uses SQLite3 for its serialized on-disk format, with a very simple (thus easily modified) table structure.  The program uses a sequence of CLI options to affect the in-memory state of a *working schedule* either read from disk or initialized fresh.

## Date-Time Ranges

Date-time values in `dtrmgr` use the fixed format:
```
<YYYY><MM><DD>T<HH><MM><SS><±HHMM>
```
A date-time range is two such (optional) values glued together by a colon:
```
{<YYYY><MM><DD>T<HH><MM><SS><±HHMM>}:{<YYYY><MM><DD>T<HH><MM><SS><±HHMM>}
```
If the leading date-time is omitted, the range has no lower bound; likewise, if the trailing date-time is omitted it has no upper bound.  The infinite range is thus just a colon (`:`).

Date-time ranges are serialized to a SQLite3 database in this format.

## SQLite3 Schema

The on-disk storage of a schedule uses an SQLite3 database file.  The file contains two tables:
```
CREATE TABLE schedule (
    period         TEXT NOT NULL
);
CREATE TABLE blocks (
    block_id       INTEGER PRIMARY KEY,
    period         TEXT UNIQUE NOT NULL
);
```
A single row should be present in the `schedule` table.  Allocated blocks of time are stored in ascending order in the `blocks` table (`block_id` can be used to sort the ranges).

## Using the Program

The built-in help summarizes usage of the program:

```
$ ./dtrmgr --help
usage:

    ./dtrmgr {options}

  options:

    -h/--help                              show built-in help for the program

   working schedule i/o options:

    --init=<period>, -i <period>           initialize a new working schedule with the specified
                                           scheduling period
    --load=<file>, -l <file>               load the working schedule from the specified file
    --save{=<file>}, -s{<file>}            save the working schedule; if a <file> is not
                                           specified, the origin file is used
    -p/--print                             summarize the working schedule to stdout

   working schedule modification options:

    --before=<date-time>, -b <date-time>   do not generate time blocks after this date and time
                                           (default: now)
    --duration=<dur>, -d <dur>             generate time blocks of this length
                                           (default: 43200 seconds)
    --next=<N>, -n <N>                     generate up to N unscheduled time blocks
    --add-range=<range>, -a <range>        add a scheduled time range to the working schedule
    --add-file=<file>, -f <file>           add time range(s) read from the given file to the
                                           working schedule

  <date-time> :: a date and time in a variety of formats (as recognized by getdate)
  <dur> :: <integer>{<unit>} | <day>-<hr>{:<min>{:<sec>}} | {<hr>:{<min>:}}<sec>
  <unit> :: d{ay{s}} | h{our{s}} | hr{s} | m{in{ute}{s}} | s{ec{ond}{s}}
  <range> :: {<YYYY><MM><DD>T<HH><MM><SS><±HHMM>}:{<YYYY><MM><DD>T<HH><MM><SS><±HHMM>}
```

The options are handled from left to right in sequence.  Thus, to create a new schedule and write it to disk:

```
$ ./dtrmgr --init=20200101T000000-0500: --save=demo.schedule
$ file demo.schedule 
demo.schedule: SQLite 3.x database
```

To find the next hour of unallocated time in that schedule without actually allocating it:

```
$ ./dtrmgr --load=demo.schedule --duration=1h --next=1
20200101T000000-0500:20200101T005959-0500
```

Assuming your program later decides to allocate that period and then check for another:

```
$ ./dtrmgr --load=demo.schedule --add-range=20200101T000000-0500:20200101T005959-0500 --save
$ ./dtrmgr --load=demo.schedule --duration=1h --next=1
20200101T010000-0500:20200101T015959-0500
```

The `--next` flag actually adds the displayed periods to the in-memory working schedule, but above they were discarded because the working schedule was not saved.  By adding a `--save` flag after the `--next` flag, the displayed periods will remain allocated:

```
$ ./dtrmgr --load=demo.schedule --duration=4h --next=4 --save
20200101T010000-0500:20200101T045959-0500
20200101T050000-0500:20200101T085959-0500
20200101T090000-0500:20200101T125959-0500
20200101T130000-0500:20200101T165959-0500
$ ./dtrmgr --load=demo.schedule --duration=1h --next=1
20200101T170000-0500:20200101T175959-0500
```

The allocated periods do not necessarily have to overlap:

```
$ ./dtrmgr --load=demo.schedule --add-file - --save
20200102T000000-0500:20200105T235959-0500
20200110T083000-0500:20200110T090000-0500
$ ./dtrmgr --load=demo.schedule --print
SSchedule@0x667690(1) {
  period: 20200101T000000-0500:
  blockCount: 3
    0 : 20200101T000000-0500:20200101T165959-0500
    1 : 20200102T000000-0500:20200105T235959-0500
    2 : 20200110T083000-0500:20200110T090000-0500
  lastErrorMessage: <none>
}
```

Notice that contiguous time ranges are merged into a single time range (e.g. block 0 above resulted from the first five time ranges added).
