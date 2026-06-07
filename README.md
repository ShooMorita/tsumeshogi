# tsume_shogi

詰将棋を解くための実験的な Web アプリです。

盤面テキストのパースと詰将棋探索は C で実装し、WASM としてブラウザに読み込みます。JavaScript / TypeScript 側は入力 UI、WASM 呼び出し、Canvas 上の盤面表示とアニメーションだけを担当します。

## 必要なもの

- Docker
- Docker Compose

ホスト側に clang、Emscripten、Node.js を直接入れる前提にはしていません。開発用コマンドは Docker コンテナ経由で実行します。

## 開発サーバー

すべてをビルドしてから Web アプリを起動します。

```sh
scripts/dev.sh
```

このコマンドは次の順で実行します。

1. ネイティブテストを clang コンテナで実行
2. ネイティブ unity build を clang コンテナで実行
3. WASM を Emscripten コンテナでビルド
4. Web コンテナで `npm ci`、`npm run build:local`、`vite` 開発サーバーを起動

起動後は次の URL を開きます。

```text
http://localhost:5173/
```

## 個別コマンド

ネイティブ unity build:

```sh
scripts/build_native.sh
```

ネイティブテスト:

```sh
scripts/test_native.sh
```

WASM build:

```sh
scripts/build_wasm.sh
```

Web production build:

```sh
scripts/build_web.sh
```

Web preview:

```sh
scripts/preview_web.sh
```

## GitHub Pages

`main` branch に push すると GitHub Actions が WASM と Web アプリをビルドし、`web/dist` を GitHub Pages にデプロイします。

初回だけ GitHub のリポジトリ設定で Pages の source を `GitHub Actions` にしてください。

手動でデプロイしたい場合は、GitHub Actions の `Deploy GitHub Pages` workflow を `workflow_dispatch` で実行します。

## 構成

```text
native/
  include/tsume_shogi.h    C の公開 API
  src/                     パーサー、盤面操作、合法手生成、DFPN ソルバー、WASM API
  tests/                   ネイティブテスト
  unity.c                  unity build 用の単一 translation unit

web/
  src/main.ts              UI、WASM 呼び出し、Canvas 描画
  src/wasm/                Emscripten の出力先

docker/
  native.Dockerfile        clang 用
  wasm.Dockerfile          Emscripten 用
  web.Dockerfile           Node / Vite 用

scripts/
  *.sh                     Docker Compose 経由の開発コマンド
```

## ネイティブ実装

ネイティブ側は C11 です。ビルドは `native/unity.c` を使う unity build です。

主な責務:

- 盤面テキストのパース
- 盤面と持駒の表現
- 合法手生成
- 王手判定
- DFPN ベースの詰将棋探索
- WASM 向け JSON API

V1 では攻方を先手固定として扱います。

## コーディングスタイル

- C は C11、インデントは 4 spaces です。
- 公開 API には `tsume_` prefix を付けます。
- 共有する型と関数宣言は `native/include/tsume_shogi.h` に置きます。
- 実装ファイルは責務ごとに `native/src/` に分け、ビルド時は `native/unity.c` から include します。
- JavaScript / TypeScript 側には将棋ルールを重複実装せず、WASM の結果を表示・アニメーションする処理だけを書きます。
- shell script は POSIX `sh` と `set -eu` を基本にします。

## WASM API

ブラウザから呼ぶ主な関数は次の 2 つです。

```c
char* tsume_solve_json(const char* input, int maxPly);
void tsume_free(char* ptr);
```

`tsume_solve_json` は、パース結果、盤面、探索結果、アニメーション用の指し手列を JSON 文字列として返します。呼び出し側は文字列を読み取ったあと `tsume_free` で解放します。

## テスト

```sh
scripts/test_native.sh
```

現在のテスト対象:

- パーサー
- 合法手生成
- DFPN ソルバーの基本ケース

## 注意

- `web/src/wasm/tsume_shogi.js` と `web/src/wasm/tsume_shogi.wasm` は Emscripten の生成物なので Git 管理しません。
- WASM 関連ファイルは `scripts/build_wasm.sh` または `scripts/dev.sh` で生成します。
- `build/`、`web/dist/`、`web/node_modules/` などの生成物は Git 管理しません。
