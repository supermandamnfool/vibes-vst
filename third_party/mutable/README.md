# Vendored Mutable Instruments sources

* `beads/` - DSP and lookup tables from https://github.com/pichenettes/eurorack (Beads firmware)
* `stmlib/` - https://github.com/pichenettes/stmlib

Both are Copyright Emilie Gillet, MIT licensed (see stmlib/LICENSE).

Local modifications (all marked as such in the history of this file):
1. beads/dsp/wavetable_granular_player.h: removed a debug printf from the audio path.
2. stmlib/utils/random.{h,cc}: RNG state is thread_local (plugin instances run on different threads).
3. beads/dsp/processor.h: Processor::Trim() now has the missing `break` statements.
