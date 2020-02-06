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
