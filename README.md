# Shannon
Shannon is the digitisation package for the SHiP experiment. It turns the
simulated hits written by [aegir](../simulation/index.md) into per-detector
digitised hit collections, which [Trout](../trout/index.md) then reconstructs.

Repository: [ShipSoft/Shannon](https://github.com/ShipSoft/Shannon)

API reference: [Doxygen](https://shipsoft.github.io/Shannon/)

## How it works

Shannon is a set of [Phlex](https://github.com/Framework-R-D/phlex) plugins
configured with Jsonnet workflows (stage `digitisation`):

| Plugin | Role |
|--------|------|
| `read_sim_file` | Reads the `events` RNTuple from an aegir output file |
| `digitise_hits` | Digitises the simulated hits per detector (upstream tagger, surround tagger, timing detector, straw tubes, calorimeter) |
| `digitised_output_module` | Writes one RNTuple per detector hit collection into a single ROOT file, plus validation histograms |

Hit smearing uses a counter-based Philox generator from
[ship-random](../ship-random/index.md), seeded per event, so results are
reproducible regardless of the number of threads.

`workflows/digitise_hits.jsonnet` reads `../aegir/fixed_target_mt_output.root`
and writes `digitised_hits.root`. Edit the paths for your own files.

## Building

```bash
git clone https://github.com/ShipSoft/Shannon.git
cd Shannon
pixi run build
pixi run test
pixi run smoke     # generate a small input, run workflows/smoke.jsonnet, check the outputs
```

Other tasks: `configure`, `install`, `clean`, and `clang-tidy` /
`clang-tidy-diff` for static analysis. Shannon is also packaged as `shannon`
on the [prefix.dev/ship-experiment/stable](https://prefix.dev/channels/@ship-experiment/stable) channel.
