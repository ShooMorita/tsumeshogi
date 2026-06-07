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
  from: { row: number; col: number } | null;
  to: { row: number; col: number };
  drop: boolean;
  promote: boolean;
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
const prevButton = document.querySelector<HTMLButtonElement>('#prev-button')!;
const nextButton = document.querySelector<HTMLButtonElement>('#next-button')!;
const statusOutput = document.querySelector<HTMLOutputElement>('#status')!;
const canvas = document.querySelector<HTMLCanvasElement>('#board-canvas')!;
const context = canvas.getContext('2d')!;

textarea.value = sampleInput;
textarea.placeholder = '貼り付けボタンで盤面テキストを貼り替えられます';

let initialBoard: BoardState | undefined;
let frameStack: BoardFrame[] = [];
let currentFrameIndex = -1;
let latestResultMessage = '';
let latestNodeCount = 0;

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

function updateStepButtons() {
  prevButton.disabled = currentFrameIndex < 0;
  nextButton.disabled = currentFrameIndex + 1 >= frameStack.length;
}

function resetResultState(message: string) {
  initialBoard = undefined;
  frameStack = [];
  currentFrameIndex = -1;
  latestResultMessage = '';
  latestNodeCount = 0;
  drawBoard();
  updateStepButtons();
  statusOutput.value = message;
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

function drawCurrentFrame() {
  if (currentFrameIndex < 0) {
    drawBoard(initialBoard);
    statusOutput.value = latestResultMessage
      ? `${latestResultMessage} (${latestNodeCount} nodes): 初期局面`
      : '初期局面';
  } else {
    const frame = frameStack[currentFrameIndex];
    drawBoard(frame.board, frame.lastMove);
    statusOutput.value = `${latestResultMessage} (${latestNodeCount} nodes): ${currentFrameIndex + 1}/${frameStack.length} ${frame.lastMove.display}`;
  }

  updateStepButtons();
}

function setFrameStack(result: SolveJson) {
  initialBoard = result.board;
  frameStack = result.frames ?? [];
  currentFrameIndex = -1;
  latestResultMessage = formatSolveResult(result);
  latestNodeCount = result.solver?.nodes ?? 0;
  drawCurrentFrame();
}

async function loadWasmModule(): Promise<any> {
  const module = await import('./wasm/tsume_shogi.js');
  return module.default();
}

function solveWithWasm(wasm: any, input: string, maxPly: number): string {
  const inputBytes = wasm.lengthBytesUTF8(input) + 1;
  const inputPointer = wasm._malloc(inputBytes);
  if (!inputPointer)
    throw new Error('Failed to allocate WASM input buffer.');

  let resultPointer = 0;
  try {
    wasm.stringToUTF8(input, inputPointer, inputBytes);
    resultPointer = wasm._tsume_solve_json(inputPointer, maxPly);
    if (!resultPointer)
      throw new Error('WASM solver returned an empty result pointer.');

    return wasm.UTF8ToString(resultPointer);
  } finally {
    if (resultPointer)
      wasm._tsume_free(resultPointer);
    wasm._free(inputPointer);
  }
}

async function solve() {
  if (textarea.value.trim().length === 0) {
    resetResultState('貼り付けボタンで盤面テキストを貼り付けてください');
    return;
  }

  solveButton.disabled = true;
  pasteButton.disabled = true;
  prevButton.disabled = true;
  nextButton.disabled = true;
  statusOutput.value = '探索中...';
  try {
    const wasm = await loadWasmModule();
    const jsonText = solveWithWasm(wasm, textarea.value, Number(maxPlyInput.value));
    const result = JSON.parse(jsonText) as SolveJson;
    if (result.solver || result.board) {
      setFrameStack(result);
    } else {
      initialBoard = result.board;
      frameStack = [];
      currentFrameIndex = -1;
      drawBoard(result.board);
      updateStepButtons();
      statusOutput.value = formatSolveResult(result);
    }
  } catch (error) {
    initialBoard = undefined;
    frameStack = [];
    currentFrameIndex = -1;
    updateStepButtons();
    statusOutput.value = error instanceof Error ? `エラー: ${error.message}` : `エラー: ${String(error)}`;
    drawBoard();
  } finally {
    pasteButton.disabled = false;
    solveButton.disabled = false;
  }
}

async function pasteBoardText() {
  pasteButton.disabled = true;
  try {
    textarea.value = await navigator.clipboard.readText();
    resetResultState('クリップボードから貼り付けました');
  } catch (error) {
    statusOutput.value = error instanceof Error
      ? `クリップボードを読めませんでした: ${error.message}`
      : `クリップボードを読めませんでした: ${String(error)}`;
  } finally {
    pasteButton.disabled = false;
  }
}

solveButton.addEventListener('click', () => {
  void solve();
});

pasteButton.addEventListener('click', () => {
  void pasteBoardText();
});

prevButton.addEventListener('click', () => {
  if (currentFrameIndex >= 0) {
    currentFrameIndex -= 1;
    drawCurrentFrame();
  }
});

nextButton.addEventListener('click', () => {
  if (currentFrameIndex + 1 < frameStack.length) {
    currentFrameIndex += 1;
    drawCurrentFrame();
  }
});

drawBoard();
updateStepButtons();
