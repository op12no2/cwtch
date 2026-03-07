
const OUTPUT_DIR: &str = "/home/xyzzy/cwtch/nets/louis";
const SB: usize = 300;
const L1: usize = 384;
const WDL: f32 = 0.4;

const DATA_FILES: [&str; 1] = [
    "/home/xyzzy/cwtch/data/gen4567.bullet",
];

use bullet_lib::{
    nn::{optimiser, Activation},
    trainer::{
        default::{
            inputs, loader, outputs, Loss, TrainerBuilder,
        },
        schedule::{lr, wdl, TrainingSchedule, TrainingSteps},
        settings::LocalSettings,
    },
};

fn main() {

    let mut trainer = TrainerBuilder::default()
        .quantisations(&[255, 64])
        .optimiser(optimiser::RAdam)
        .loss_fn(Loss::SigmoidMSE)
        .input(inputs::Chess768)
        .output_buckets(outputs::Single)
        .feature_transformer(L1)
        .activate(Activation::SqrReLU)
        .add_layer(1)
        .build();

    let schedule = TrainingSchedule {
        net_id: "cwtch".to_string(),
        eval_scale: 400.0,
        steps: TrainingSteps {
            batch_size: 16_384,
            batches_per_superbatch: 6104,
            start_superbatch: 1,
            end_superbatch: SB,
        },
        wdl_scheduler: wdl::ConstantWDL { value: WDL },
        //lr_scheduler: lr::StepLR {
        //    start: 0.001,
        //    gamma: 0.3,
        //    step: 300,
        //},
        lr_scheduler: lr::Warmup {
            inner: lr::CosineDecayLR {
                initial_lr: 0.001,
                final_lr: 0.001 * f32::powi(0.3, 5),
                final_superbatch: SB,
            },
            warmup_batches: 200,
        },
        save_rate: 100,
    };

    //trainer.set_optimiser_params(optimiser::AdamWParams::default());

    let settings = LocalSettings {
        threads: 4,
        test_set: None,
        output_directory: &OUTPUT_DIR,
        batch_queue_size: 64,
    };

    let data_loader = loader::DirectSequentialDataLoader::new(&DATA_FILES);

    trainer.run(&schedule, &settings, &data_loader);
}

