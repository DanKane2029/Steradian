# renders

Where to put renders worth keeping.

Images here are **not committed**. The repository's `.gitignore` already excludes `*.png`
everywhere except `docs/`, `res/textures/` and `tests/golden/`, so anything written here
stays local, and this file is the only thing in the directory that git tracks.

That is deliberate rather than incidental. A full-size render is easily ten megabytes: the
3840x3072 glass dragon is 11.7 MB, eight times the version on the front page, and the
object store is already 1.1 GB. Committing output at that size would make cloning the
project noticeably worse in exchange for pictures nobody views at full size in a browser.

```sh
./build-gpu/src/steradian --config res/configs/test_config.json \
                          --scene  res/scenes/glass_dragon.json \
                          --out    renders/glass_dragon.png \
                          --samples 4096 --device gpu
```

## Promoting one to the front page

The images the README shows live in `docs/` and are committed, so they are kept small:
420x420 for the Stanford models, 640x512 for the glass dragon, all under 350 KB. To put a
new one there, render it at the size the page actually displays rather than rendering it
huge and scaling down — the renderer is the better resampler, and at these speeds it costs
minutes.

## Worth knowing before spending an hour on one

More samples stop helping sooner than expected. Measured on the glass dragon against an
independent reference at a different seed, the mean channel difference out of 255 falls
2.156 at 1,000 samples, 1.323 at 4,000, 0.774 at 16,000, 0.504 at 64,000 — a little worse
than the ideal halving per quadrupling, because the reference carries its own noise. By
sixteen thousand the error is around 0.3% of range and further sampling is invisible.

Resolution is where the time goes instead. The sparkle in that scene is not noise at all:
it is the checkered floor refracted through a lumpy surface, and it resolves into fine
detail as pixels are added rather than smoothing away as samples are added. Path depth is
not involved either, 8, 24 and 64 bounces being indistinguishable.

For scale, 3840x3072 at 32,000 samples is 377 G samples: 37 minutes on an RTX 3060 Ti, and
about nine days on eight CPU threads.
