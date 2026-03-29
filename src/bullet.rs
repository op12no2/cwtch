// based on simple.rs

use bullet_lib::{
    game::inputs,
    nn::optimiser,
    trainer::{
        save::SavedFormat,
        schedule::{TrainingSchedule, TrainingSteps, lr, wdl},
        settings::LocalSettings,
    },
    value::{ValueTrainerBuilder, loader},
};

use viriformat::dataformat::Filter;

const DATA_FILES: &[&str] = &[
    "/mnt/d/datagen/gen1.vf",
    "/mnt/d/datagen/gen2.vf",
    "/mnt/d/datagen/gen3.vf",
    "/mnt/d/datagen/gen4.vf",
    "/mnt/d/datagen/gen5.vf",
    "/mnt/d/datagen/gen6.vf",
    "/mnt/d/datagen/gen7.vf",
];
const OUTPUT_DIR: &str = "/mnt/d/bulletnets/gen7b";
const BUFFER_MB: usize = 512;
const THREADS: usize = 4;
const SB: usize = 100;
const WDL: f32 = 0.5;
const HIDDEN_SIZE: usize = 384;
const SCALE: i32 = 400;
const QA: i16 = 255;
const QB: i16 = 64;

fn main() {

    let mut trainer = ValueTrainerBuilder::default()
        .dual_perspective()
        .optimiser(optimiser::AdamW)
        .inputs(inputs::Chess768)
        .save_format(&[
            SavedFormat::id("l0w").round().quantise::<i16>(QA),
            SavedFormat::id("l0b").round().quantise::<i16>(QA),
            SavedFormat::id("l1w").round().quantise::<i16>(QB),
            SavedFormat::id("l1b").round().quantise::<i16>(QA * QB),
        ])
        .loss_fn(|output, target| output.sigmoid().squared_error(target))
        .build(|builder, stm_inputs, ntm_inputs| {
            let l0 = builder.new_affine("l0", 768, HIDDEN_SIZE);
            let l1 = builder.new_affine("l1", 2 * HIDDEN_SIZE, 1);

            let stm_hidden = l0.forward(stm_inputs).sqrrelu();
            let ntm_hidden = l0.forward(ntm_inputs).sqrrelu();
            let hidden_layer = stm_hidden.concat(ntm_hidden);
            l1.forward(hidden_layer)
        });

    let schedule = TrainingSchedule {
        net_id: "cwtch".to_string(),
        eval_scale: SCALE as f32,
        steps: TrainingSteps {
            batch_size: 16_384,
            batches_per_superbatch: 6104,
            start_superbatch: 1,
            end_superbatch: SB,
        },
        wdl_scheduler: wdl::ConstantWDL { value: WDL },
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

}
