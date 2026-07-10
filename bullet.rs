// based on simple.rs

use bullet_lib::{
    game::{
        inputs::{ChessBucketsMirrored, get_num_buckets},
        outputs::MaterialCount,
    },
    nn::{
        InitSettings, Shape,
        optimiser::{AdamW, AdamWParams},
    },
    trainer::{
        save::SavedFormat,
        schedule::{TrainingSchedule, TrainingSteps, lr, wdl},
        settings::LocalSettings,
    },
    value::{ValueTrainerBuilder, loader},
};

use viriformat::dataformat::Filter;

const DATA_FILES: &[&str] = &[
    "../datagen/gen7.vf",
    "../datagen/gen6.vf",
    "../datagen/gen5.vf",
];
const OUTPUT_DIR: &str = "/mnt/d/bulletnets/gen7d";
const HIDDEN_SIZE: usize = 512;
const SB: usize = 600;
const WDL_START: f32 = 0.4;
//const WDL_END: f32 = 0.4;
const THREADS: usize = 4;
const SCALE: i32 = 400;
const QA: i16 = 255;
const QB: i16 = 64;
const BUFFER_MB: usize = 4096;

// king bucket layout over files a-d, rank 1 first; files e-h mirror onto a-d
#[rustfmt::skip]
const BUCKET_LAYOUT: [usize; 32] = [
    0, 0, 1, 1,
    2, 2, 2, 2,
    3, 3, 3, 3,
    3, 3, 3, 3,
    3, 3, 3, 3,
    3, 3, 3, 3,
    3, 3, 3, 3,
    3, 3, 3, 3,
];

const NUM_INPUT_BUCKETS: usize = get_num_buckets(&BUCKET_LAYOUT);

// output buckets by piece count: (popcount(occupied) - 2) / 4
const NUM_OUTPUT_BUCKETS: usize = 8;

fn main() {

    let mut trainer = ValueTrainerBuilder::default()
        .dual_perspective()
        .optimiser(AdamW)
        .inputs(ChessBucketsMirrored::new(BUCKET_LAYOUT))
        .output_buckets(MaterialCount::<NUM_OUTPUT_BUCKETS>)
        .save_format(&[
            // merge the factoriser into each bucket's weights
            SavedFormat::id("l0w")
                .transform(|store, weights| {
                    let factoriser = store.get("l0f").values.repeat(NUM_INPUT_BUCKETS);
                    weights.into_iter().zip(factoriser).map(|(a, b)| a + b).collect()
                })
                .round()
                .quantise::<i16>(QA),
            SavedFormat::id("l0b").round().quantise::<i16>(QA),
            SavedFormat::id("l1w").round().quantise::<i16>(QB).transpose(),  // bucket rows contiguous
            SavedFormat::id("l1b").round().quantise::<i16>(QA * QB),
        ])
        .loss_fn(|output, target| output.sigmoid().squared_error(target))
        .build(|builder, stm_inputs, ntm_inputs, output_buckets| {
            // zero-init factoriser shared by all buckets, merged out at save time
            let l0f = builder.new_weights("l0f", Shape::new(HIDDEN_SIZE, 768), InitSettings::Zeroed);
            let expanded_factoriser = l0f.repeat(NUM_INPUT_BUCKETS);

            let mut l0 = builder.new_affine("l0", 768 * NUM_INPUT_BUCKETS, HIDDEN_SIZE);
            l0.weights = l0.weights + expanded_factoriser;

            let l1 = builder.new_affine("l1", 2 * HIDDEN_SIZE, NUM_OUTPUT_BUCKETS);

            let stm_hidden = l0.forward(stm_inputs).sqrrelu();
            let ntm_hidden = l0.forward(ntm_inputs).sqrrelu();
            let hidden_layer = stm_hidden.concat(ntm_hidden);
            l1.forward(hidden_layer).select(output_buckets)
        });

    // l0w and l0f sum at save time so clip each tighter
    let stricter_clipping = AdamWParams { max_weight: 0.99, min_weight: -0.99, ..Default::default() };
    trainer.optimiser.set_params_for_weight("l0w", stricter_clipping);
    trainer.optimiser.set_params_for_weight("l0f", stricter_clipping);

    let schedule = TrainingSchedule {
        net_id: "cwtch".to_string(),
        eval_scale: SCALE as f32,
        steps: TrainingSteps {
            batch_size: 16_384,
            batches_per_superbatch: 6104,
            start_superbatch: 1,
            end_superbatch: SB,
        },
        wdl_scheduler: wdl::ConstantWDL { value: WDL_START },
        //wdl_scheduler: wdl::LinearWDL { start: WDL_START, end: WDL_END },
        lr_scheduler: lr::Warmup {
            inner: lr::CosineDecayLR {
                initial_lr: 0.001,
                final_lr: 0.001 * f32::powi(0.3, 5),
                final_superbatch: SB,
            },
            warmup_batches: 200,
        },
        save_rate: SB,
    };

    let filter = Filter {
        filter_tactical: true,
        filter_check: true,
        max_eval: 10000,
        ..Filter::UNRESTRICTED
    };

    let settings = LocalSettings { threads: THREADS, test_set: None, output_directory: OUTPUT_DIR, batch_queue_size: 64 };
    let data_loader = loader::ViriBinpackLoader::new_concat_multiple(DATA_FILES, BUFFER_MB, THREADS, filter);

    trainer.run(&schedule, &settings, &data_loader);

    // cross-check against ./cwtch et output after embedding the quantised net
    for fen in [
        "r3k2r/2pb1ppp/2pp1q2/p7/1nP1B3/1P2P3/P2N1PPP/R2QK2R w KQkq a6 0 1",
        "4rrk1/2p1b1p1/p1p3q1/4p3/2P2n1p/1P1NR2P/PB3PP1/3R1QK1 b - - 0 1",
        "r3qbrk/6p1/2b2pPp/p3pP1Q/PpPpP2P/3P1B2/2PB3K/R5R1 w - - 0 1",
        "8/8/1p2k1p1/3p3p/1p1P1P1P/1P2PK2/8/8 w - - 0 1",
    ] {
        println!("eval {} cp: {fen}", (SCALE as f32 * trainer.eval(fen)).round());
    }

}
