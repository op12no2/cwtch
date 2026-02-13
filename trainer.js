
// messy/hakky code

//{{{  lang fold
/*

*/

//}}}

const id_suffix = '';

const ACTI_RELU   = 1;
const ACTI_CRELU  = 2;
const ACTI_SRELU  = 3;
const ACTI_SCRELU = 4;

const fs = require('fs');
const readline = require('readline');
const { exec } = require('child_process');
const path = require('path');
//const fs = require('fs').promises;

const dataFiles       = ['data/data1.shuf','data/data2.shuf'];

const acti            = ACTI_RELU;
const hiddenSize      = 75;
const interp          = 0.5;

const shuffle         = true;
const batchSize       = 500;
const learningRate    = 0.001;
const K               = 100;
const useL2Reg        = false;
const useAdamW        = false;

const reportRate      = 50;   // mean batch loss freq during epoch
const lossRate        = 50;   // dataset loss freq
const epochs          = 10000;
const inputSize       = 768;
const outputSize      = 1;
const maxActiveInputs = 32;
const beta1           = 0.9;
const beta2           = 0.999;
const epsilon         = 1e-7;
const l2RegFactor     = 0.001;
const weightDecay     = 0.01;

const id = activationName() + '_' + hiddenSize + '_' + Math.trunc(interp * 10) + id_suffix;
console.log(id);

//{{{  line constants
//
// 0                         1    2      3  4    5   6     7    8         9           10             11
// 8/8/8/8/6p1/5nk1/p7/3RrK2 w    -      -  3    169 -1124 d1e1 n         c           -              0.0
// board                     turn rights ep game ply score move noisy n|- incheck c|- givescheck g|- wdl 0.0|0.5|1.0
//

const PART_BOARD      = 0;
const PART_TURN       = 1;
const PART_RIGHTS     = 2;
const PART_EP         = 3;
const PART_GAME       = 4;
const PART_PLY        = 5;
const PART_SCORE      = 6;
const PART_MOVE       = 7;
const PART_NOISY      = 8;
const PART_INCHECK    = 9;
const PART_GIVESCHECK = 10;
const PART_WDL        = 11;

//}}}

let minLoss = 9999;
let numBatches = 0;

//{{{  myround

function myround(x) {
  return Math.sign(x) * Math.round(Math.abs(x));
}

//}}}
//{{{  shuffle

async function shuffleFile(filePath) {

  const tempFile = `${filePath}.tmp`;

  return new Promise((resolve, reject) => {
    // Shuffle the file and write it to a temporary file
    exec(`shuf ${filePath} > ${tempFile}`, (err, stdout, stderr) => {
      if (err) {
        reject(`Error shuffling file: ${stderr}`);
        return;
      }

      // After shuffling, move the temporary file to overwrite the original file
      exec(`mv ${tempFile} ${filePath}`, (err, stdout, stderr) => {
        if (err) {
          reject(`Error moving file: ${stderr}`);
        } else {
          resolve(`Shuffled: ${filePath}`);
        }
      });
    });
  });
}

async function shuffleAllFiles(files) {
  try {
    const shufflePromises = files.map(file => shuffleFile(file));
    const results = await Promise.all(shufflePromises);
    //results.forEach(result => console.log(result));
  } catch (error) {
    console.error(error);
  }
}

//}}}
//{{{  createLineStream

async function* createLineStream(filenames) {
  for (const filename of filenames) {
    const fileStream = fs.createReadStream(filename);
    const rl = readline.createInterface({
      input: fileStream,
      crlfDelay: Infinity
    });
    for await (const line of rl) {
      yield line;
    }
    rl.close();
  }
}

//}}}
//{{{  lerp

function lerp(eval, wdl, t) {
  let sg = sigmoid(eval);
  let l = sg + (wdl - sg) * t;
  return l;
}

//}}}
//{{{  optiName

function optiName() {
  if (useAdamW)
    return "adamw";
  else
    return "adam";
  end
}

//}}}
//{{{  activations

function sigmoid(x) {
  return 1 / (1 + Math.exp(-x / K));
}

function relu(x) {
  return Math.max(0, x);
}

function drelu(x) {
  return x > 0 ? 1 : 0;
}

function crelu(x) {
  return Math.min(Math.max(x, 0), 1);
}

function dcrelu(x) {
  return (x > 0 && x < 1) ? 1 : 0;
}

function srelu(x) {
  return Math.max(0, x) * Math.max(0, x);
}

function dsrelu(x) {
  return x > 0 ? 2*x : 0;
}

function screlu(x) {
  const y = Math.min(Math.max(x, 0), 1);
  return y * y;
}

function dscrelu(x) {
  return (x > 0 && x < 1) ? 2*x : 0;
}

function activationFunction(x) {
  switch (acti) {
    case ACTI_RELU:
      return relu(x);
    case ACTI_CRELU:
      return crelu(x);
    case ACTI_SRELU:
      return srelu(x);
    case ACTI_SCRELU:
      return screlu(x);
  }
}

function activationDerivative(x) {
  switch (acti) {
    case ACTI_RELU:
      return drelu(x);
    case ACTI_CRELU:
      return dcrelu(x);
    case ACTI_SRELU:
      return dsrelu(x);
    case ACTI_SCRELU:
      return dscrelu(x);
  }
}

function activationName(x) {
  switch (acti) {
    case ACTI_RELU:
      return "relu";
    case ACTI_CRELU:
      return "crelu";
    case ACTI_SRELU:
      return "srelu";
    case ACTI_SCRELU:
      return "screlu";
  }
}

//}}}
//{{{  initializeParameters

function initializeParameters() {

    const scale = Math.sqrt(2 / inputSize);

    const params = {
      W1: new Float32Array(inputSize * hiddenSize).map(() => (Math.random() * 2 - 1) * scale),
      b1: new Float32Array(hiddenSize).fill(0),
      W2: new Float32Array(hiddenSize).map(() => (Math.random() * 2 - 1) * scale),
      b2: 0,
      vW1: new Float32Array(inputSize * hiddenSize).fill(0),
      vb1: new Float32Array(hiddenSize).fill(0),
      vW2: new Float32Array(hiddenSize).fill(0),
      vb2: 0,
      sW1: new Float32Array(inputSize * hiddenSize).fill(0),
      sb1: new Float32Array(hiddenSize).fill(0),
      sW2: new Float32Array(hiddenSize).fill(0),
      sb2: 0
    };

    return params;
}

//}}}
//{{{  saveModel

function saveModel(loss, params, epochs) {

  const actiName = activationName(acti);
  const opt      = optiName();

  var o = '//{{{  weights\r\n';

  o += 'const net_h1_size     = '  + hiddenSize             + ';\r\n';
  o += 'const net_lr          = '  + learningRate           + ';\r\n';
  o += 'const net_activation  = '  + actiName               + ';\r\n';
  o += 'const net_stretch     = '  + K                      + ';\r\n';
  o += 'const net_interp      = '  + interp                 + ';\r\n';
  o += 'const net_batch_size  = '  + batchSize              + ';\r\n';
  o += 'const net_num_batches = '  + numBatches             + ';\r\n';
  o += 'const net_positions   = '  + numBatches * batchSize + ';\r\n';
  o += 'const net_opt         = "' + opt                    + '";\r\n';
  o += 'const net_shuffle     = "' + shuffle                + '";\r\n';
  o += 'const net_l2_reg      = '  + useL2Reg               + ';\r\n';
  o += 'const net_epochs      = '  + epochs                 + ';\r\n';
  o += 'const net_loss        = '  + loss                   + ';\r\n';

  o += '//{{{  weights\r\n';

  //{{{  write h1 weights
  
  o += 'const net_h1_w = Array(768);\r\n';
  
  var a = params.W1;
  var a2 = [];
  
  for (var i=0; i < 768; i++) {
    a2 = [];
    const j = i * hiddenSize;
    for (var k=0; k < hiddenSize; k++) {
      a2.push(a[j+k]);
    }
    o += 'net_h1_w[' + i + ']  = new Float32Array([' + a2.toString() + ']);\r\n';
  }
  
  //}}}
  //{{{  write h1 biases
  
  var a = params.b1;
  
  o += 'const net_h1_b = new Float32Array([' + a.toString() + ']);\r\n';
  
  //}}}
  //{{{  write o weights
  
  var a = params.W2;
  
  o += 'const net_o_w = new Float32Array([' + a.toString() + ']);\r\n';
  
  //}}}
  //{{{  write o bias
  
  var a = params.b2;
  
  o += 'const net_o_b = ' + a.toString() + ';\r\n';
  
  //}}}

  o += '\r\n//}}}\r\n';
  o += '\r\n//}}}\r\n\r\n';

  const weightsFile= 'data/weights_' + id + '_' + epochs + '.js';

  fs.writeFileSync(weightsFile, o);
}

//}}}
//{{{  forwardPropagation

function forwardPropagation(activeIndices, params) {

  const Z1 = new Float32Array(activeIndices.length * hiddenSize);
  const A1 = new Float32Array(activeIndices.length * hiddenSize);
  const Z2 = new Float32Array(activeIndices.length);
  const A2 = new Float32Array(activeIndices.length);

  for (let i = 0; i < activeIndices.length; i++) {
    for (let j = 0; j < hiddenSize; j++) {
      Z1[i * hiddenSize + j] = params.b1[j];
      for (const idx of activeIndices[i]) {
        Z1[i * hiddenSize + j] += params.W1[idx * hiddenSize + j];
      }
      A1[i * hiddenSize + j] = activationFunction(Z1[i * hiddenSize + j]);
    }
    Z2[i] = params.b2;
    for (let j = 0; j < hiddenSize; j++) {
      Z2[i] += A1[i * hiddenSize + j] * params.W2[j];
    }
    A2[i] = sigmoid(Z2[i]);
  }

  return { Z1, A1, Z2, A2 };
}

//}}}
//{{{  backwardPropagation

function backwardPropagation(activeIndices, targets, params, forward) {

  const m = activeIndices.length;
  const dZ2 = new Float32Array(m);
  const dW2 = new Float32Array(hiddenSize);

  let db2 = 0;

  const dA1 = new Float32Array(m * hiddenSize);
  const dZ1 = new Float32Array(m * hiddenSize);
  const dW1 = new Float32Array(inputSize * hiddenSize);
  const db1 = new Float32Array(hiddenSize);

  for (let i = 0; i < m; i++) {
    dZ2[i] = forward.A2[i] - targets[i];
    db2 += dZ2[i];
    for (let j = 0; j < hiddenSize; j++) {
      dW2[j] += dZ2[i] * forward.A1[i * hiddenSize + j];
      dA1[i * hiddenSize + j] = dZ2[i] * params.W2[j];
      dZ1[i * hiddenSize + j] = dA1[i * hiddenSize + j] * activationDerivative(forward.Z1[i * hiddenSize + j]);
      db1[j] += dZ1[i * hiddenSize + j];
      for (const idx of activeIndices[i]) {
        dW1[idx * hiddenSize + j] += dZ1[i * hiddenSize + j];
      }
    }
  }

  for (let j = 0; j < hiddenSize; j++) {
    dW2[j] /= m;
    db1[j] /= m;
  }

  db2 /= m;

  for (let i = 0; i < inputSize * hiddenSize; i++) {
    dW1[i] /= m;
  }

  return { dW1, db1, dW2, db2 };
}

//}}}
//{{{  updateParameters

function updateParameters(params, grads, t) {

  const updateParam = (param, grad, v, s, i) => {
    v[i] = beta1 * v[i] + (1 - beta1) * grad[i];
    s[i] = beta2 * s[i] + (1 - beta2) * grad[i] * grad[i];
    const vCorrected = v[i] / (1 - Math.pow(beta1, t));
    const sCorrected = s[i] / (1 - Math.pow(beta2, t));
    let update = learningRate * vCorrected / (Math.sqrt(sCorrected) + epsilon);
    if (useL2Reg) {
      update -= l2RegFactor * param[i]; // Apply L2 regularization
    }
    if (useAdamW) {
      param[i] -= learningRate * weightDecay * param[i]; // ADAMW weight decay
    }
    return param[i] - update;
  };

  for (let i = 0; i < inputSize * hiddenSize; i++) {
    params.W1[i] = updateParam(params.W1, grads.dW1, params.vW1, params.sW1, i);
  }

  for (let i = 0; i < hiddenSize; i++) {
    params.b1[i] = updateParam(params.b1, grads.db1, params.vb1, params.sb1, i);
    params.W2[i] = updateParam(params.W2, grads.dW2, params.vW2, params.sW2, i);
  }

  params.b2 = updateParam([params.b2], [grads.db2], [params.vb2], [params.sb2], 0);

  return params;
}

//}}}
//{{{  decodeLine

//{{{  constants

const WHITE = 0;
const BLACK = 1;

const PAWN = 0;
const KNIGHT = 1;
const BISHOP = 2;
const ROOK = 3;
const QUEEN = 4;
const KING = 5;

const chPce = {
  'k': KING, 'q': QUEEN, 'r': ROOK, 'b': BISHOP, 'n': KNIGHT, 'p': PAWN,
  'K': KING, 'Q': QUEEN, 'R': ROOK, 'B': BISHOP, 'N': KNIGHT, 'P': PAWN
};

const chCol = {
  'k': BLACK, 'q': BLACK, 'r': BLACK, 'b': BLACK, 'n': BLACK, 'p': BLACK,
  'K': WHITE, 'Q': WHITE, 'R': WHITE, 'B': WHITE, 'N': WHITE, 'P': WHITE
};

const chNum = {'8': 8, '7': 7, '6': 6, '5': 5, '4': 4, '3': 3, '2': 2, '1': 1};

//}}}

function decodeLine(line) {

  const parts = line.split(' ');

  const board = parts[PART_BOARD].trim();
  const eval  = parseFloat(parts[PART_SCORE].trim());
  const wdl   = parseFloat(parts[PART_WDL].trim());

  var x = 0;
  var sq = 0;

  const activeIndices = [];

  let target = 0.0;

  if (!skipP(parts,eval,wdl)) {

    //{{{  decode board
    
    for (var j = 0; j < board.length; j++) {
      var ch = board.charAt(j);
      if (ch == '/')
        continue;
      var num = chNum[ch];
      if (typeof (num) == 'undefined') {
        if (chCol[ch] == WHITE)
          x = 0 + chPce[ch] * 64 + sq;
        else if (chCol[ch] == BLACK)
          x = 384 + chPce[ch] * 64 + sq;
        else {
          console.log(j,board.length,'colour',board,ch.charCodeAt(0),chCol[ch],'                        ');
          console.log(j,board.length,'colour',board,ch.charCodeAt(0),chCol[ch]);
          console.log(line);
          process.exit();
        }
        activeIndices.push(x);
        sq++;
      }
      else {
        sq += num;
      }
    }
    
    //}}}

    target = lerp(eval,wdl,interp);
  }

  return {activeIndices, target: [target]};
}

//}}}
//{{{  skipP

function skipP (parts,eval,wdl) {

  const noisy = parts[PART_NOISY].trim();
  if (noisy == 'n')
    return true;

  const inCh  = parts[PART_INCHECK].trim();
  if (inCh == 'c')
    return true;

  const gvCh  = parts[PART_GIVESCHECK].trim();
  if (gvCh == 'g')
    return true;

  if (parts[PART_MOVE].trim().length == 5)  // promotion
    return true;

  if (wdl == 0.5 && Math.abs(eval) > 300)
    return true;

  return false;
}

//}}}
//{{{  train

async function train(filenames) {

  //{{{  randomise
  
  let now = new Date();
  let midnight = new Date(now);
  midnight.setHours(0, 0, 0, 0);
  let n = Math.floor((now - midnight) / 1000);
  for (let i=0; i < n; i++)
    Math.random();
  
  //}}}

  let params = initializeParameters();
  let datasetLoss = 0;

  numBatches = await calculateNumBatches(filenames);
  saveModel(0, params, 0);

  console.log(id, 'hidden',hiddenSize,'acti',activationName(acti),'stretch',K,'shuffle',shuffle,'batchsize',batchSize,'lr',learningRate,'interp',interp,'num batches',numBatches,'filtered positions',numBatches*batchSize);

  let t = 0;

  for (let epoch = 0; epoch < epochs; epoch++) {
    //{{{  train epoch
    
    const lineStream = createLineStream(filenames);
    
    let batchActiveIndices = [];
    let batchTargets = [];
    let totalLoss = 0;
    let batchCount = 0;
    
    for await (const line of lineStream) {
      const {activeIndices, target} = decodeLine(line);
      if (activeIndices.length) {
        //{{{  use this position
        
        batchActiveIndices.push(activeIndices);
        batchTargets.push(target[0]);
        
        if (batchActiveIndices.length === batchSize) {
        
          t++;
        
          const forward = forwardPropagation(batchActiveIndices, params);
          const grads = backwardPropagation(batchActiveIndices, batchTargets, params, forward);
        
          params = updateParameters(params, grads, t);
        
          const batchLoss = forward.A2.reduce((sum, pred, i) =>
            sum + Math.pow(pred - batchTargets[i], 2), 0) / batchSize;
        
          totalLoss += batchLoss;
          batchCount++;
        
          batchActiveIndices = [];
          batchTargets = [];
        
          if (batchCount % reportRate === 0) {
            process.stdout.write(`${id} Epoch ${epoch + 1}, Batch ${batchCount}/${numBatches}, Mean Batch Loss: ${totalLoss / batchCount}\r`);
          }
        }
        
        //}}}
      }
    }
    
    console.log(`${id} Epoch ${epoch + 1} completed. Mean Batch Loss: ${totalLoss / batchCount}`);
    
    //{{{  calc dataset loss
    
    if ((epoch + 1) % lossRate === 0) {
    
      let marker = '';
      datasetLoss = await calculateDatasetLoss(filenames, params);
    
      if (datasetLoss < minLoss) {
        minLoss = datasetLoss;
        marker = '***';
      }
    
      console.log(`${id} Dataset Loss after ${epoch + 1} epochs: ${datasetLoss} ${marker}`);
    }
    
    else {
      datasetLoss = totalLoss / batchCount;
    }
    
    //}}}
    
    saveModel(datasetLoss, params, epoch + 1);
    
    if (shuffle)
      await shuffleAllFiles(dataFiles);
    
    //}}}
  }

  return params;
}

//}}}
//{{{  calculateNumBatches

async function calculateNumBatches(filenames) {

  const lineStream = createLineStream(filenames);

  let count = 0;

  for await (const line of lineStream) {

    const parts = line.split(' ');

    if (parts.length != 12) {
      console.log('line format', line, parts.length);
      process.exit();
    }

    const eval = parseFloat(parts[PART_SCORE].trim());
    const wdl  = parseFloat(parts[PART_WDL].trim());

    if (!skipP(parts,eval,wdl)) {

      count++;

      if ((count % 1000000) == 0)
        process.stdout.write(count + '\r');
    }
  }

  return count / batchSize | 0;
}

//}}}
//{{{  calculateDatasetLoss

async function calculateDatasetLoss(filenames, params) {

  const lineStream = createLineStream(filenames);

  let totalLoss = 0;
  let count = 0;

  for await (const line of lineStream) {
    const {activeIndices, target} = decodeLine(line);
    if (activeIndices.length) {
      //{{{  use this position
      
      const forward = forwardPropagation([activeIndices], params);
      
      const loss = Math.pow(forward.A2[0] - target[0], 2);
      
      totalLoss += loss;
      
      count++;
      
      if ((count % 100000) == 0)
        process.stdout.write(count + '\r');
      
      //}}}
    }
  }

  numBatches = count / batchSize | 0;

  return totalLoss / count;
}

//}}}

train(dataFiles).then(params => {
    console.log('Training completed.');
}).catch(error => {
    console.error('Error during training:', error);
});

