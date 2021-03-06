let s2 = Sphere.create(
    Material.create_diffuse_light(Vec.xyz(2.0, 2.0, 2.0)),
    Mat.translation(Vec.xyz(12.0, 3.0, -90.0)),
    5.0);

let s3 = Sphere.create(
    Material.create_disney(
            ~base_color=Vec.xyz(0.5, 0.9, 0.0),
            ~roughness=0.5,
            ~metallic=1.0,
            ()),
    Mat.translation(Vec.xyz(-25.0, 0.0, -50.0)),
    75.0);

let s4 = Sphere.create(
    Material.create_disney(
            ~base_color=Vec.xyz(1.0, 1.0, 1.0),
            ~specular_trans=1.0,
            ~roughness=0.2,
            ~ior=1.8,
            ~metallic=0.0,
            ()),
    Mat.translation(Vec.xyz(6.0, -10.0, -90.0)),
    4.0);

let prims = ref([s2, s3, s4]);
for (i in 0 to 19) {
    let color = Vec.xyz(
            if (i mod 2 == 0) { 0.9 } else { 0.2 },
            if (i mod 3 == 0) { 0.9 } else { 0.2 },
            if (i mod 5 == 0) { 0.9 } else { 0.2 }
    );
    prims := [Sphere.create(
         Material.create_disney(
                ~base_color=color,
                ~roughness=0.5,
                ~metallic=1.0,
                ~anisotropic=0.8,
                ~clearcoat=1.0,
                ~clearcoat_gloss=1.0,
                ()),
        Mat.translation(Vec.xyz(-20.0 +. float_of_int(i * 4), 12.0, -100.0)),
        1.8), ...prims^];
};
for (i in 0 to 19) {
    let color = Vec.xyz(
            if (i mod 2 == 0) { 0.9 } else { 0.2 },
            if (i mod 3 == 0) { 0.9 } else { 0.2 },
            if (i mod 5 == 0) { 0.9 } else { 0.2 }
    );
    prims := [Sphere.create(
         Material.create_disney(
                ~base_color=color,
                ~roughness=0.2,
                ~ior=1.8,
                ~metallic=0.0,
                ~sheen=1.0,
                ()),
        Mat.translation(Vec.xyz(-20.0 +. float_of_int(i * 4), -12.0, -100.0)),
        1.8), ...prims^];
};

let height = 512;
let width = int_of_float(float_of_int(height) *. Camera.aspect_ratio(Camera.default));
Printf.printf("Aspect ratio: %f, Width: %d, Height: %d\n",
        Camera.aspect_ratio(Camera.default), width, height);

let stage = Stage.create(Array.of_list(prims^), Camera.default, Integrator.path_tracer, width, height);
let exr = Exr.create(width, height);
let oc = open_out_bin("output.exr");

Printf.printf("Press ^C to stop\n");

let i = ref(0);
while (true) {
    let t0 = Sys.time();
    Stage.trace(stage);
    let t1 = Sys.time();
    Printf.printf("Frame %d (%f s)\n", i^, t1 -. t0);
    ignore(Exr.update(exr, stage.film));

    seek_out(oc, 0);
    ignore(Exr.output_exr(oc, exr));
    flush(oc);

    i := i^ + 1;
    flush(stdout);
};
