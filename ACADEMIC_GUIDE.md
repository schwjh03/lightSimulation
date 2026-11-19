# Academic use and validation

This program is an educational, two-dimensional geometric-optics simulator. The
checks below establish agreement with selected analytical cases; they do not
certify the program as a calibrated laboratory instrument or establish accuracy
for every arrangement. Validate the particular exercise before assigning it.

## Units and conventions

| Quantity | Convention |
| --- | --- |
| Physical lengths and positions | Centimeters; 10 stored world units = 1 cm |
| Bench | 100 × 70.4 cm |
| Lens length | Full clear aperture, perpendicular to its optical axis |
| Lens thickness | Vertex-to-vertex thickness on the optical axis |
| Prism radius | Center-to-vertex distance, not prism side length |
| Mirror, wall, splitter and detector length | Endpoint-to-endpoint distance |
| Curvature radii R1 and R2 | Centimeters; positive when the center of curvature is to the right, for light traveling left to right along the lens's local axis |
| Flat face | Infinite radius, zero curvature |
| Rotation/direction | Degrees in controls; converted to radians for trigonometry |
| Screen coordinates | x increases rightward, y downward; positive rotation appears clockwise |
| Wavelength | nm; glass dispersion formulas convert nm to micrometers |
| Refractive index | Dimensionless; exterior medium has n = 1 |
| Detector power | Percent of the **combined** emitted power of all lasers, not watts |

Display decimal places are formatting, not a statement of experimental uncertainty.
The visible hardware outlines and ray glow have decorative screen widths; these
are not material thicknesses or calibrated beam diameters. Mirrors, walls,
splitters and detectors are zero-thickness line elements in the ray solver.

## Physics model

The solver applies Snell's law at actual planar or spherical interfaces and uses
local surface normals for reflection and refraction. Spherical lens surfaces use
analytic intersections; rendered curves are sampled for drawing. There is no
thin-lens shortcut in the traced ray paths. Lens intersections use double-precision
quadratic intermediates and a numerically stable spherical sag calculation.
The underlying scene vectors and most other calculations use single precision.

For the conventions used by the solver, the laws of reflection, refraction and
the critical-angle condition agree with the equations summarized by
[OpenStax University Physics, volume 3](https://openstax.org/books/university-physics-volume-3/pages/1-key-equations).

Fresnel reflection uses the average of s and p intensity reflectances for
unpolarized light. Transmission receives the remaining power at a lossless
interface. The Fresnel control applies to all glass objects in the scene. Turning Fresnel
reflections off is an idealized demonstration mode;
total internal reflection still applies. Polarization is not tracked between
interfaces, so this is not a polarization experiment simulator.

The glass models use SCHOTT Sellmeier coefficients for N-BK7 and N-SF11. The
reference test includes the N-SF11 blue/red catalog indices from the
[SCHOTT N-SF11 datasheet](https://media.schott.com/api/public/content/78e83df5ca2c4da4ad4490a52c80a146?v=1a468147).
Water is an approximate Cauchy model, not a temperature-controlled reference
material. All material-button indices refer to **589 nm**, which is distinct from
the commonly tabulated d line at 587.6 nm.

A custom n shifts the selected material's dispersion curve to the chosen index
at 589 nm, with n clamped to at least 1. It is a synthetic material model, not a
measured glass specification. The glass models evaluate wavelengths in the
400–750 nm range; source spectra are within that range. Temperature, pressure,
material tolerances, coatings and bulk absorption are not modeled.

## Lens geometry and focal lengths

Let h be the semi-aperture in cm. Unflipped lenses use:

| Lens | R1 | R2 |
| --- | --- | --- |
| Biconvex | +1.65 h | −1.65 h |
| Plano-convex | infinity | −1.65 h |
| Biconcave | −1.65 h | +1.65 h |
| Plano-concave | infinity | +1.65 h |
| Meniscus (+) | +1.65 h | +2.8 h |
| Meniscus (−) | +2.8 h | +1.65 h |

Changing length also changes curvature radii through h. Changing thickness
separates the vertices while preserving the radii for a fixed length. A minimum
thickness prevents the faces from crossing. If increasing length would make the
rim invalid, thickness is increased to the valid minimum. Flipping swaps the
faces and reverses their signed radii; it does not change the material.

Meniscus +/- names describe the default configurations. Large changes to
thickness or index can change the optical power; use the current focal readout
rather than assuming the name guarantees its sign.

The inspector reports signed **effective paraxial focal length** f at 589 nm in
air, calculated independently of the ray intersections:

```
1/f = (n − 1) [1/R1 − 1/R2 + (n − 1)d/(n R1 R2)]
```

Use d and both radii in cm to obtain f in cm. For a flat face, use zero reciprocal
radius. Positive f means converging paraxial power; negative f means diverging
paraxial power; infinity means zero paraxial power. To obtain power in diopters,
use 100/f when f is in cm. This is the effective focal length, measured from a
principal plane, not generally from the center or rear surface of a thick lens.

The corresponding back focal distance for parallel light entering from the left
is:

```
BFL = f [1 − (n − 1)d/(n R1)]
```

The distinction between vertex distances and principal-plane focal distances is
explained in [University of Tennessee's refracting-systems notes](https://labs.phys.utk.edu/mbreinig/phys421/modules/m3/Refracting%20systems%20details.html).

Flipping can change BFL even when effective focal length is unchanged. Rays away
from the axis can show spherical aberration and need not meet at the paraxial
focus. Tests compare small-height rays with this analytical prediction, not
large-aperture rays with the thin-lens equation.

### Setting up a focal-length exercise

1. Use one isolated lens, Crown preset, no overlapping objects, and Fresnel
   reflections off when isolating primary ray directions.
2. Use monochromatic yellow sources (589 nm) to match the focal readout.
3. Add a **Laser wall**, aim it along the optical axis and choose a narrow width
   in cm. Its beams originate along a line perpendicular to its direction, all
   with the same direction. The width changes the source aperture; beam count
   changes sampling without changing total source power. You can alternatively
   use several lasers with equal direction and spread = 0. A regular laser with
   nonzero spread makes a divergent fan, not a parallel beam.
4. Record length, center thickness, R1, R2, material/index, wavelength and rotation.
   Measure the axis crossing of near-axis rays from the **rear vertex** and
   compare that distance with BFL. Use backward extensions for virtual foci.
5. Increase ray height to investigate spherical aberration separately. Change
   wavelength to investigate dispersion. Save the scene with the exercise.

The Lense testing starter is an overview illuminated by a fan, not a calibrated
parallel-beam focal-length experiment. A regular zero-spread laser traces one
coincident ray regardless of its ray-count setting. Laser walls use their full
beam count and ignore the regular laser's stored angular spread. A laser wall
counts as one equal-power source in the combined detector denominator.
`make ARGS=--laser-wall` launches a monochromatic parallel-beam focusing example.

## Power and measurement limits

All lasers have equal normalized emitted power. Adding another laser changes
the denominator of every detector percentage. Each source's spectrum and sampled
ray directions divide that source's power, so adding ray samples does not create
power. The sampled white source has 15 equal-weight bands from 420 to 700 nm;
cyan and purple are two-band mixtures, not monochromatic lasers.

Mirrors use fixed 82% intensity reflectance; the remaining 18% is absorbed.
Splitters use the chosen ideal, wavelength-independent reflectance and complementary
transmittance. Detectors absorb every intercepted ray with ideal efficiency.
No absolute source wattage, detector responsivity, noise or saturation is modeled.
Ray glow and RGB colors have display gain and are not photometric measurements.

Weak branches below 0.0005 of a traced wavelength/direction's initial energy are
pruned. Reflection budgets are limited to 12, with 64 interaction steps and 256
segments per trace; scene output is capped near 120,000 segments. Detector totals
can undercount truncated paths. Repeat power experiments at higher ray counts
and reflection budgets to check convergence; displayed digits do not override
sampling or truncation error.

Keep refracting bodies separated. Overlapping bodies use last-placed-medium
priority, not mechanical intersection or material mixing. Touching boundaries,
prism corners and grazing rays are subject to finite intersection tolerances.
The ray origins advance 0.002 cm after an interface, and medium sampling uses a
0.003 cm forward probe; resolve experimental separations above those scales.

This model omits diffraction, interference, coherence, evolving polarization,
three-dimensional apertures, temperature effects and manufacturing errors.
Use another model or a physical experiment when those effects matter.

## Validation performed

`make test-core` checks analytical optics, units, spectral normalization,
energy bookkeeping, saved scenes and scene-library behavior. The academic
lens matrix covers **288 configurations**: six shapes, two aperture sizes, two
catalog glasses, three wavelengths, two orientations and two thickness settings.
For those paraxial cases, back focal distance must agree with the independent
thick-lens prediction within **0.05 cm or 0.5%**, whichever is larger. Reverse
ray direction must agree within 0.0005 in unit-vector distance. These are test
acceptance bounds over these cases, not guaranteed measurement uncertainty over
the full control range.

Additional checks sweep Snell/Fresnel angles, bracket critical angles, compare
catalog reference indices and verify power normalization and Fresnel branch
energy accounting. `make test` also exercises scene buttons, inspectors, metric
sliders, presets, Flip, deletion/Undo, popup redraws and resizing.

This audit's automated runtime verification was on **Linux/X11**. The Windows
inspector source was updated for matching labels and units, but its runtime
behavior was not verified on Windows in this environment.
