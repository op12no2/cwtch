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

const OUTPUT_DIR: &str = "/mnt/d/bulletnets/ben";
const DATA_FILES: &[&str] = &["/mnt/d/datagen/gen7.bullet"];
const SB: usize = 100; 
const WDL: f32 = 0.4;
const HIDDEN_SIZE: usize = 384;
const SCALE: i32 = 400;
const QA: i16 = 255;
const QB: i16 = 64;

fn main() {
    
    let mut trainer = ValueTrainerBuilder::default()
        // makes `ntm_inputs` available below
        .dual_perspective()
        // standard optimiser used in NNUE
        // the default AdamW params include clipping to range [-1.98, 1.98]
        .optimiser(optimiser::AdamW)
        // basic piece-square chessboard inputs
        .inputs(inputs::Chess768)
        // chosen such that inference may be efficiently implemented in-engine
        .save_format(&[
            SavedFormat::id("l0w").round().quantise::<i16>(QA),
            SavedFormat::id("l0b").round().quantise::<i16>(QA),
            SavedFormat::id("l1w").round().quantise::<i16>(QB),
            SavedFormat::id("l1b").round().quantise::<i16>(QA * QB),
        ])
        // map output into ranges [0, 1] to fit against our labels which
        // are in the same range
        // `target` == wdl * game_result + (1 - wdl) * sigmoid(search score in centipawns / SCALE)
        // where `wdl` is determined by `wdl_scheduler`
        .loss_fn(|output, target| output.sigmoid().squared_error(target))
        // the basic `(768 -> N)x2 -> 1` inference
        .build(|builder, stm_inputs, ntm_inputs| {
            // weights
            let l0 = builder.new_affine("l0", 768, HIDDEN_SIZE);
            let l1 = builder.new_affine("l1", 2 * HIDDEN_SIZE, 1);

            // inference
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
        //lr_scheduler: lr::StepLR { start: 0.001, gamma: 0.1, step: 18 },
        lr_scheduler: lr::Warmup {
            inner: lr::CosineDecayLR {
                initial_lr: 0.001,
                final_lr: 0.001 * f32::powi(0.3, 5),
                final_superbatch: SB,
            },
            warmup_batches: 200,
        },
        save_rate: 50,
    };

    let settings = LocalSettings { threads: 4, test_set: None, output_directory: OUTPUT_DIR, batch_queue_size: 64 };
    let data_loader = loader::DirectSequentialDataLoader::new(DATA_FILES);

    trainer.run(&schedule, &settings, &data_loader);

}
