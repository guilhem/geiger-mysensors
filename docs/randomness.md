# Physical randomness on the Uno

Watch bytes derived from Geiger pulse timing alongside the CPM and tick readings.
The physical source has **not been qualified for entropy**, and there are no
online health tests. Do not use these bytes for keys, passwords, tokens, or
other secrets.

## From four pulses to one bit

The D2 falling-edge interrupt records `micros()` timestamps. Following the
interval-comparison approach described by [HotBits](https://www.fourmilab.ch/hotbits/how3.html),
each candidate bit uses four distinct pulses and two disjoint intervals:

```text
pulse:       p1          p2          p3          p4
interval:        T1                     T2
             (p2 - p1)              (p4 - p3)
```

Discard equal measured intervals. Otherwise accept a bit from their ordering:
initially `T1 > T2` gives `1`, and `T1 < T2` gives `0`. Reverse this mapping after
each accepted bit; ties do not change the polarity. The p2-to-p3 gap is ignored,
and no pulse is reused in the next group. Pack eight accepted bits, most
significant bit first, into one byte (`0`–`255`). There is no PRNG fallback or
fabricated startup byte.

A pulse gap of **60 seconds or more** discards the unfinished four-pulse group
and partial byte. Comparison polarity persists across that expiry, as do the
last completed byte and sample count. The next pulse starts a fresh group.
An Uno reset also resets polarity and counters. All captured pulses still count
toward CPM and ticks, including those in rejected or expired random groups.

## Expected rate

Eight accepted bits require **at least 32 pulses**. At 43 CPM, the ideal estimate is:

```text
32 / 43 × 60 ≈ 44.7 seconds per byte
```

This assumes independent, identically distributed intervals, negligible ties,
and no expiry. Waiting times vary; it is neither measured throughput nor a
maximum delay. Timer resolution, detector dead time, electrical noise, interrupt
latency, and correlations affect the result. Interval comparison and alternating
polarity alone establish no entropy bound or protection against a failed source.

## Observe the output

**Random Byte** holds the latest completed byte. **Random Samples** counts each
completion, including identical consecutive bytes. The first sample count is
`1`; a zero byte is valid. Both appear only after the first complete byte.
They provide latest-state observation; for counter resets, skipped samples, and
the separate byte/sequence updates, see
[Home Assistant delivery limits](home-assistant.md#delivery-and-failure-behavior).

A [live observation](hardware-validation.md#what-has-been-observed) confirms changing byte and sample
values with the current application. It does not qualify the timing extractor
or physical source. Software tests cover grouping, ties, packing, expiry, and
clock rollover; controlled pulses and entropy characterization remain separate
checks. The [hardware checklist](hardware-validation.md) describes that work.
