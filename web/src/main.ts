import './style.css';

type Square = {
  row: number;
  col: number;
  piece: string;
  owner: 'sente' | 'gote' | 'none';
};

type Move = {
  piece: string;
  side: 'sente' | 'gote';
  kind: 'normal' | 'promote' | 'drop';
  from: { row: number; col: number } | null;
  to: { row: number; col: number };
  display: string;
};

type BoardState = {
  squares: Square[];
  hands: Record<string, Record<string, number>>;
};

type BoardFrame = {
  board: BoardState;
  lastMove: Move;
};

type SolveJson = {
  ok: boolean;
  solver?: {
    status: string;
    maxPly: number;
    nodes: number;
    message: string;
    moves: Move[];
  };
  board?: BoardState;
  frames?: BoardFrame[];
  error?: {
    code: string;
    message: string;
  };
};

type ReplayJson = {
  ok: boolean;
  board?: BoardState;
  replay?: {
    moveCount: number;
    moves: Move[];
  };
  frames?: BoardFrame[];
  error?: {
    code: string;
    message: string;
  };
};

type PlaybackTrack = 'replay' | 'solution';

const boardOrigin = { x: 44, y: 44 };
const cellSize = 56;
const boardPixelSize = cellSize * 9;

const sampleInput = `作品：サンプル
後手の持駒：なし
  ９ ８ ７ ６ ５ ４ ３ ２ １
+---------------------------+
| ・ ・ ・ ・v玉 ・ ・ ・ ・|一
| ・ ・ ・ 飛 ・ ・ ・ ・ ・|二
| ・ ・ ・ ・ ・ ・ ・ ・ ・|三
| ・ ・ ・ ・ ・ ・ ・ ・ ・|四
| ・ ・ ・ ・ ・ ・ ・ ・ ・|五
| ・ ・ ・ ・ ・ ・ ・ ・ ・|六
| ・ ・ ・ ・ ・ ・ ・ ・ ・|七
| ・ ・ ・ ・ ・ ・ ・ ・ ・|八
| ・ ・ ・ ・ 玉 ・ ・ ・ ・|九
+---------------------------+
先手の持駒：金
`;

const pieceLabels: Record<string, string> = {
  FU: '歩',
  KYO: '香',
  KEI: '桂',
  GIN: '銀',
  KIN: '金',
  KAKU: '角',
  HISHA: '飛',
  GYOKU: '玉',
  OU: '王',
  TO: 'と',
  NARIKYO: '杏',
  NARIKEI: '圭',
  NARIGIN: '全',
  UMA: '馬',
  RYU: '龍',
};

const textarea = document.querySelector<HTMLTextAreaElement>('#board-input')!;
const maxPlyInput = document.querySelector<HTMLInputElement>('#max-ply')!;
const pasteButton = document.querySelector<HTMLButtonElement>('#paste-button')!;
const solveButton = document.querySelector<HTMLButtonElement>('#solve-button')!;
const replayTabButton = document.querySelector<HTMLButtonElement>('#replay-tab')!;
const solutionTabButton = document.querySelector<HTMLButtonElement>('#solution-tab')!;
const prevButton = document.querySelector<HTMLButtonElement>('#prev-button')!;
const nextButton = document.querySelector<HTMLButtonElement>('#next-button')!;
const statusOutput = document.querySelector<HTMLOutputElement>('#status')!;
const canvas = document.querySelector<HTMLCanvasElement>('#board-canvas')!;
const context = canvas.getContext('2d')!;

textarea.value = sampleInput;
textarea.placeholder = '貼り付けボタンで盤面テキストを貼り替えられます';

let activeTrack: PlaybackTrack = 'replay';
let loadedInputText = '';
let replayInitialBoard: BoardState | undefined;
let replayFrames: BoardFrame[] = [];
let replayFrameIndex = -1;
let solutionInitialBoard: BoardState | undefined;
let solutionFrames: BoardFrame[] = [];
let solutionFrameIndex = -1;
let solutionAnchorPly = 0;
let latestSolutionMessage = '';
let latestSolutionNodeCount = 0;

function boardPoint(row: number, col: number) {
  return {
    x: boardOrigin.x + col * cellSize + cellSize / 2,
    y: boardOrigin.y + row * cellSize + cellSize / 2,
  };
}

function drawHands(board?: BoardState) {
  if (!board)
    return;

  const handText = (side: 'sente' | 'gote') => {
    const entries = Object.entries(board.hands[side])
      .filter(([, count]) => count > 0)
      .map(([piece, count]) => `${pieceLabels[piece] ?? piece}${count}`);
    return entries.length > 0 ? entries.join(' ') : 'なし';
  };

  context.fillStyle = '#111111';
  context.font = '18px system-ui, sans-serif';
  context.textAlign = 'left';
  context.textBaseline = 'top';
  context.fillText(`後手: ${handText('gote')}`, boardOrigin.x + boardPixelSize + 28, boardOrigin.y);
  context.fillText(`先手: ${handText('sente')}`, boardOrigin.x + boardPixelSize + 28, boardOrigin.y + boardPixelSize - 28);
}

function drawBoard(board?: BoardState, lastMove?: Move) {
  context.clearRect(0, 0, canvas.width, canvas.height);
  context.fillStyle = '#f2f2f2';
  context.fillRect(0, 0, canvas.width, canvas.height);

  context.fillStyle = '#ffffff';
  context.fillRect(boardOrigin.x, boardOrigin.y, boardPixelSize, boardPixelSize);

  if (lastMove) {
    context.fillStyle = '#d8d8d8';
    context.fillRect(
      boardOrigin.x + lastMove.to.col * cellSize,
      boardOrigin.y + lastMove.to.row * cellSize,
      cellSize,
      cellSize,
    );
  }

  context.strokeStyle = '#111111';
  context.lineWidth = 1;

  for (let i = 0; i <= 9; i += 1) {
    context.beginPath();
    context.moveTo(boardOrigin.x + i * cellSize, boardOrigin.y);
    context.lineTo(boardOrigin.x + i * cellSize, boardOrigin.y + boardPixelSize);
    context.stroke();
    context.beginPath();
    context.moveTo(boardOrigin.x, boardOrigin.y + i * cellSize);
    context.lineTo(boardOrigin.x + boardPixelSize, boardOrigin.y + i * cellSize);
    context.stroke();
  }

  context.fillStyle = '#111111';
  context.font = '24px "Hiragino Mincho ProN", "Yu Mincho", serif';
  context.textAlign = 'center';
  context.textBaseline = 'middle';

  for (const square of board?.squares ?? []) {
    if (square.piece === 'NO_KOMA')
      continue;
    const point = boardPoint(square.row, square.col);
    const label = pieceLabels[square.piece] ?? square.piece;
    context.save();
    context.translate(point.x, point.y);
    if (square.owner === 'gote')
      context.rotate(Math.PI);
    context.fillText(label, 0, 0);
    context.restore();
  }

  drawHands(board);
}

function formatSolveResult(result: SolveJson): string {
  const status = result.solver?.status;
  const maxPly = result.solver?.maxPly ?? Number(maxPlyInput.value);
  const mateLength = result.frames?.length ?? 0;

  if (status === 'ok')
    return `詰みあり: ${mateLength}手詰め`;
  if (status === 'no_mate')
    return `詰み無し: ${maxPly}手以内`;
  if (status === 'invalid_depth')
    return '探索手数が不正です';
  if (status === 'timeout')
    return '探索が時間切れです';
  if (result.error)
    return `エラー: ${result.error.message}`;

  return '結果を取得しました';
}

async function loadWasmModule(): Promise<any> {
  const module = await import('./wasm/tsume_shogi.js');
  return module.default();
}

function callWasmWithInput(wasm: any, input: string, call: (inputPointer: number) => number): string {
  const inputBytes = wasm.lengthBytesUTF8(input) + 1;
  const inputPointer = wasm._malloc(inputBytes);
  if (!inputPointer)
    throw new Error('Failed to allocate WASM input buffer.');

  let resultPointer = 0;
  try {
    wasm.stringToUTF8(input, inputPointer, inputBytes);
    resultPointer = call(inputPointer);
    if (!resultPointer)
      throw new Error('WASM returned an empty result pointer.');

    return wasm.UTF8ToString(resultPointer);
  } finally {
    if (resultPointer)
      wasm._tsume_free(resultPointer);
    wasm._free(inputPointer);
  }
}

function replayWithWasm(wasm: any, input: string): string {
  return callWasmWithInput(wasm, input, (inputPointer) => wasm._tsume_replay_json(inputPointer));
}

function solveAtWithWasm(wasm: any, input: string, maxPly: number, replayPly: number): string {
  return callWasmWithInput(wasm, input, (inputPointer) => wasm._tsume_solve_at_json(inputPointer, maxPly, replayPly));
}

function activeFrames() {
  return activeTrack === 'solution' ? solutionFrames : replayFrames;
}

function activeFrameIndex() {
  return activeTrack === 'solution' ? solutionFrameIndex : replayFrameIndex;
}

function currentReplayPly() {
  return replayFrameIndex + 1;
}

function setBusy(isBusy: boolean) {
  pasteButton.disabled = isBusy;
  solveButton.disabled = isBusy;
  replayTabButton.disabled = isBusy;
  solutionTabButton.disabled = isBusy || !solutionInitialBoard;
  prevButton.disabled = isBusy;
  nextButton.disabled = isBusy;
}

function updateSegmentButtons() {
  replayTabButton.classList.toggle('is-active', activeTrack === 'replay');
  solutionTabButton.classList.toggle('is-active', activeTrack === 'solution');
  replayTabButton.setAttribute('aria-pressed', activeTrack === 'replay' ? 'true' : 'false');
  solutionTabButton.setAttribute('aria-pressed', activeTrack === 'solution' ? 'true' : 'false');
  solutionTabButton.disabled = !solutionInitialBoard;
}

function updateStepButtons() {
  const frames = activeFrames();
  const index = activeFrameIndex();
  prevButton.disabled = index < 0;
  nextButton.disabled = index + 1 >= frames.length;
}

function clearSolution() {
  solutionInitialBoard = undefined;
  solutionFrames = [];
  solutionFrameIndex = -1;
  solutionAnchorPly = 0;
  latestSolutionMessage = '';
  latestSolutionNodeCount = 0;
}

function resetPlaybackState(message: string) {
  loadedInputText = '';
  replayInitialBoard = undefined;
  replayFrames = [];
  replayFrameIndex = -1;
  clearSolution();
  activeTrack = 'replay';
  drawBoard();
  updateSegmentButtons();
  updateStepButtons();
  statusOutput.value = message;
}

function drawReplayFrame() {
  if (!replayInitialBoard) {
    drawBoard();
    statusOutput.value = '貼り付けボタンで盤面テキストを貼り付けてください';
    return;
  }

  if (replayFrameIndex < 0) {
    drawBoard(replayInitialBoard);
    statusOutput.value = '棋譜: 初期局面';
    return;
  }

  const frame = replayFrames[replayFrameIndex];
  drawBoard(frame.board, frame.lastMove);
  statusOutput.value = `棋譜: ${replayFrameIndex + 1}/${replayFrames.length} ${frame.lastMove.display}`;
}

function drawSolutionFrame() {
  if (!solutionInitialBoard) {
    activeTrack = 'replay';
    drawReplayFrame();
    return;
  }

  const anchorText = `${solutionAnchorPly}手目`;
  if (solutionFrameIndex < 0) {
    drawBoard(solutionInitialBoard);
    statusOutput.value = `${latestSolutionMessage} (${latestSolutionNodeCount} nodes): ${anchorText}`;
    return;
  }

  const frame = solutionFrames[solutionFrameIndex];
  drawBoard(frame.board, frame.lastMove);
  statusOutput.value = `${latestSolutionMessage} (${latestSolutionNodeCount} nodes): ${solutionFrameIndex + 1}/${solutionFrames.length} ${frame.lastMove.display}`;
}

function drawActiveFrame() {
  if (activeTrack === 'solution')
    drawSolutionFrame();
  else
    drawReplayFrame();

  updateSegmentButtons();
  updateStepButtons();
}

function setReplayStack(result: ReplayJson, preferFinalPosition: boolean) {
  if (result.error)
    throw new Error(result.error.message);

  replayInitialBoard = result.board;
  replayFrames = result.frames ?? [];
  replayFrameIndex = preferFinalPosition && replayFrames.length > 0 ? replayFrames.length - 1 : -1;
  loadedInputText = textarea.value;
  clearSolution();
  activeTrack = 'replay';
  drawActiveFrame();
}

function setSolutionStack(result: SolveJson) {
  if (result.error)
    throw new Error(result.error.message);

  solutionInitialBoard = result.board;
  solutionFrames = result.frames ?? [];
  solutionFrameIndex = -1;
  solutionAnchorPly = currentReplayPly();
  latestSolutionMessage = formatSolveResult(result);
  latestSolutionNodeCount = result.solver?.nodes ?? 0;
  activeTrack = 'solution';
  drawActiveFrame();
}

async function loadReplay(preferFinalPosition: boolean) {
  if (textarea.value.trim().length === 0) {
    resetPlaybackState('貼り付けボタンで盤面テキストを貼り付けてください');
    return false;
  }

  setBusy(true);
  statusOutput.value = '読み込み中...';
  try {
    const wasm = await loadWasmModule();
    const jsonText = replayWithWasm(wasm, textarea.value);
    setReplayStack(JSON.parse(jsonText) as ReplayJson, preferFinalPosition);
    return true;
  } catch (error) {
    resetPlaybackState(error instanceof Error ? `エラー: ${error.message}` : `エラー: ${String(error)}`);
    return false;
  } finally {
    setBusy(false);
    updateSegmentButtons();
    updateStepButtons();
  }
}

async function ensureReplayLoaded() {
  if (loadedInputText === textarea.value && replayInitialBoard)
    return true;

  return loadReplay(true);
}

async function solve() {
  if (textarea.value.trim().length === 0) {
    resetPlaybackState('貼り付けボタンで盤面テキストを貼り付けてください');
    return;
  }

  if (!await ensureReplayLoaded())
    return;

  setBusy(true);
  statusOutput.value = '探索中...';
  try {
    const wasm = await loadWasmModule();
    const jsonText = solveAtWithWasm(wasm, textarea.value, Number(maxPlyInput.value), currentReplayPly());
    setSolutionStack(JSON.parse(jsonText) as SolveJson);
  } catch (error) {
    statusOutput.value = error instanceof Error ? `エラー: ${error.message}` : `エラー: ${String(error)}`;
  } finally {
    setBusy(false);
    updateSegmentButtons();
    updateStepButtons();
  }
}

async function pasteBoardText() {
  setBusy(true);
  try {
    textarea.value = await navigator.clipboard.readText();
    await loadReplay(true);
  } catch (error) {
    statusOutput.value = error instanceof Error
      ? `クリップボードを読めませんでした: ${error.message}`
      : `クリップボードを読めませんでした: ${String(error)}`;
  } finally {
    setBusy(false);
    updateSegmentButtons();
    updateStepButtons();
  }
}

solveButton.addEventListener('click', () => {
  void solve();
});

pasteButton.addEventListener('click', () => {
  void pasteBoardText();
});

replayTabButton.addEventListener('click', () => {
  activeTrack = 'replay';
  drawActiveFrame();
});

solutionTabButton.addEventListener('click', () => {
  if (solutionInitialBoard) {
    activeTrack = 'solution';
    drawActiveFrame();
  }
});

prevButton.addEventListener('click', () => {
  if (activeTrack === 'solution') {
    if (solutionFrameIndex >= 0) {
      solutionFrameIndex -= 1;
      drawActiveFrame();
    }
  } else if (replayFrameIndex >= 0) {
    replayFrameIndex -= 1;
    drawActiveFrame();
  }
});

nextButton.addEventListener('click', () => {
  if (activeTrack === 'solution') {
    if (solutionFrameIndex + 1 < solutionFrames.length) {
      solutionFrameIndex += 1;
      drawActiveFrame();
    }
  } else if (replayFrameIndex + 1 < replayFrames.length) {
    replayFrameIndex += 1;
    drawActiveFrame();
  }
});

textarea.addEventListener('input', () => {
  if (loadedInputText !== textarea.value)
    resetPlaybackState('入力が変更されました');
});

drawBoard();
updateSegmentButtons();
updateStepButtons();
