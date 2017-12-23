/**
 * Quick random-number generation via the XorShift prng; uniform generators for floats and int32s;
 * and sampling routines for Monte Carlo raytracing.
 *
 * Some portions of this code are borrowed from the Rust rand library (see doc comments).
 * The copyright for that code is:
 * Copyright 2013-2017 The Rust Project Developers. See the COPYRIGHT
 * file at the top-level directory of this distribution and at
 * http://rust-lang.org/COPYRIGHT.
 *
 * Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
 * http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
 * <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
 * option. This file may not be copied, modified, or distributed
 * except according to those terms.
 */

/** Note: this isn't thread-safe, including the next_* functions, which modify this. */
type rng = {
    mutable x: int32,
    mutable y: int32,
    mutable z: int32,
    mutable w: int32,
};

/** The number of steradians in a sphere (4 * Pi). */
let _steradians_per_sphere = Math.pi *. 4.0;

/** Creates a new XorShift rng seeded from the global rng. */
let create_rng = () => {
    Random.self_init();
    let rand_int32 = () => {
        /* Random.bits gives 30 bits, so take 30 from a and 2 from b for an int32. */
        let a = Int32.of_int(Random.bits());
        let b = Int32.of_int(Random.bits());
        Int32.logor(a, Int32.shift_left(b, 30))
    };
    {x: rand_int32(), y: rand_int32(), z: rand_int32(), w: rand_int32()}
};

/**
 * Returns the next random int32 (with all 32 bits randomized).
 * This implementation is borrowed from Rust's rand library's XorShiftRng. See module doc.
 */
let next_int32 = (r: rng) => {
    /* See https://en.wikipedia.org/wiki/Xorshift */
    let x = r.x;
    let t = Int32.logxor(x, Int32.shift_left(x, 11));
    r.x = r.y;
    r.y = r.z;
    r.z = r.w;
    let w_ = r.w;
    r.w = Int32.logxor(w_,
            Int32.logxor(Int32.shift_right_logical(w_, 19),
            Int32.logxor(t, Int32.shift_right_logical(t, 8))));
    r.w
};

/**
 * Returns the next random float selected from the half-open interval [0, 1).
 * This implementation is borrowed from Rust's rand library's Rng. See module doc.
 */
let next_float = (r: rng) => {
    let i = next_int32(r);
    let upper_mask = 0x3F800000l;
    let lower_mask = 0x7FFFFFl;
    let tmp = Int32.logor(upper_mask, Int32.logand(i, lower_mask));
    let result = Int32.float_of_bits(tmp);
    result -. 1.0
};

/** Returns the next random float in the half-open interval [a, b). */
let next_float_range = (r: rng, a: float, b: float) => {
    a +. next_float(r) *. (b -. a)
};

/**
 * The standard normal (Gaussian) distribution. This uses the Ziggurat method for sampling random
 * Gaussians. See George Marsaglia and Wai Wan Tsang. "The Ziggurat Method for Generating Random
 * Variables" (2000). <http://www.jstatsoft.org/v05/i08/paper>.
 */
module GaussianDistribution = {
    let _m = 2147483648.0;
    let _v = 9.91256303526217e-3;
    let _d = 3.442619855899;
    let (_kn, _wn, _fn) = {
        /** Recursive helper for filling Ziggurat tables. */
        let rec zigfill = (kn: array(int32), wn: array(float), fn: array(float),
            dn: float, i: int)  =>
        {
            let dn_ = sqrt(~-.2.0 *. log(_v /. dn +. exp(~-.0.5 *. dn *. dn)));
            kn[i + 1] = Int32.of_float((dn_ /. dn) *. _m);
            fn[i] = exp(~-.0.5 *. dn_ *. dn_);
            wn[i] = dn_ /. _m;

            if (i > 1) {
                zigfill(kn, wn, fn, dn_, i - 1);
            };
        };

        let q = _v /. exp(~-.0.5 *. _d *. _d);

        let kn = Array.make(128, 0l);
        let wn = Array.make(128, 0.0);
        let fn = Array.make(128, 0.0);

        kn[0] = Int32.of_float((_d /. q) *. _m);
        kn[1] = 0l;
        wn[0] = q /. _m;
        wn[127] = _d /. _m;
        fn[0] = 1.0;
        fn[127] = exp(~-.0.5 *. _d *. _d);
        zigfill(kn, wn, fn, _d, 126);

        (kn, wn, fn)
    };

    let sample = (r: rng) => {
        let hz = next_int32(r);
        let iz = Int32.to_int(hz) land 127;
        if (Int32.abs(hz) < _kn[iz]) {
            Int32.to_float(hz) *. _wn[iz]
        }
        else {
            let rec nfix_zero = (r: rng, hz: int32) => {
                let x = ~-.log(next_float(r)) /. _d;
                let y = ~-.log(next_float(r));
                if ((y +. y) >= (x +. x)) {
                    if (hz > 0l) {
                        _d +. x
                    }
                    else {
                        ~-._d -. ~-.x
                    }
                }
                else {
                    nfix_zero(r, hz)
                }
            };
            let rec nfix = (r: rng, hz: int32, iz: int) => {
                let x = Int32.to_float(hz) *. _wn[iz];
                if (iz == 0) {
                    nfix_zero(r, hz)
                }
                else if ((_fn[iz] +. next_float(r) *. (_fn[iz - 1] -. _fn[iz]))
                        < exp(~-.0.5 *. x *. x)) {
                    x
                }
                else {
                    let hz = next_int32(r);
                    let iz = Int32.to_int(hz) land 127;
                    if (Int32.abs(hz) < _kn[iz]) {
                        Int32.to_float(hz) *. _wn[iz]
                    }
                    else {
                        nfix(r, hz, iz)
                    }
                }
            };

            nfix(r, hz, iz)
        }
    };
};

/** Uniform distribution over unit disk sphere area. */
module AreaSampleDisk = {
    /**
     * Samples a unit disk, ensuring that the samples are uniformally distributed
     * throughout the area of the disk.
     *
     * Taken from Pharr & Humphreys' p. 667.
     */
    let sample = (r: rng) => {
        let sx = next_float_range(r, -1.0, 1.0);
        let sy = next_float_range(r, -1.0, 1.0);

        /* Handle degeneracy at the origin. */
        if (sx == 0.0 && sy == 0.0) {
            (0.0, 0.0)
        }
        else {
            let (r, theta) = if (abs_float(sx) > abs_float(sy)) {
                (sx, (Math.pi /. 4.0) *. (sy /. sx))
            }
            else {
                (sy, (Math.pi /. 2.0) -. ((Math.pi /. 4.0) *. (sx /. sy)))
            };

            (r *. cos(theta), r *. sin(theta))
        }
    };
};

/** Cosine-weighted distribution over unit hemisphere. */
module CosineSampleHemisphere = {
    /**
     * Samples a unit hemisphere with a cosine-weighted distribution.
     * Directions with a higher cosine value (more parallel to the normal) are
     * more likely to be chosen than those with a lower cosine value (more
     * perpendicular to the normal).
     *
     * Taken from Pharr & Humphreys p. 669.
     *
     * @param flipped whether to sample from the hemisphere on the negative
     *                Z-axis instead; false will sample from the positive
     *                hemisphere and true will sample from the negative hemisphere
     */
    let sample = (r: rng, flipped: bool) => {
        let (x, y) = AreaSampleDisk.sample(r);
        let z = sqrt(max(0.0, ~-.1.0 -. (x *. x) -. (y *. y)));

        if (flipped) {
            Vec.xyz(x, y, ~-.1.0 *. z)
        }
        else {
            Vec.xyz(x, y, z)
        }
    };

    /**
     * Returns the probability that the given direction was sampled from a unit
     * hemisphere using a cosine-weighted distribution. (It does not matter
     * whether the hemisphere is on the positive or negative Z-axis.)
     */
    let pdf = (direction: Vec.t) => {
        Vec.abs_cos_theta(direction) *. (1.0 /. Math.pi)
    };
};

/* Uniform distribution over unit sphere surface area. */
module UniformSampleSphere = {
    /**
     * Returns the probability that any solid angle was sampled uniformly
     * from a unit sphere.
     */
    let pdf = () => {
        1.0 /. _steradians_per_sphere
    };
};

module Tests = {
    let test_gaussian = () => {
        let rng = create_rng();
        let rec gen_gaussian = (l: list(float), i: int) => {
            if (i < 0) {
                l
            }
            else {
                gen_gaussian([GaussianDistribution.sample(rng), ...l], i - 1)
            }
        };

        let t0 = Sys.time();
        let num_samples = 10_000_000;
        let gaussians = gen_gaussian([], num_samples);
        let t1 = Sys.time();

        Printf.printf("Gaussian generation time: %f s\n", t1 -. t0);

        let avg = List.fold_left(
                (a, b) => a +. (b /. float_of_int(num_samples)),
                0.0, gaussians);
        Printf.printf("    avg=%f\n", avg);
        assert(abs_float(avg) < 0.001);

        let variance = List.fold_left(
                (a, b) => a +. (((b -. avg) *. (b -. avg)) /. float_of_int(num_samples)),
                0.0, gaussians);
        Printf.printf("    variance=%f\n", variance);
        assert(variance > 0.99 && variance < 1.01);
    };
}