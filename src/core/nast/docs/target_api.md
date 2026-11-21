# 各言語の生成物の API 像 (draft)

2026-11-18。nast のバックエンドが目指す「その言語で自然な API」を言語ごとに定める案。
**今の ebm2<lang> の出力を正解として写したものではない** — 基準は各言語の標準
ライブラリと定着した慣習で、ebm2<lang> の出力 (`rebrgen/save/api/` に udp / http2 を
全言語で生成して並べた) は「今どうなっているか」の比較材料として使う。

決めることは言語を問わず同じなので、先に軸を並べ、そのあと言語ごとに埋める。
決め切れていないところは末尾の「未決」に寄せた。

## 共通の約束 (全言語)

1. **外部の runtime に依存しない** (ADR 0022)。標準ライブラリだけ。生成物の中に
   小さな補助を抱えるのはよいが、次の 2 に反しない形で
2. **別々の .bgn から生成したものを同じ単位 (Go のパッケージ / Rust のモジュール /
   C の翻訳単位の集まり / Java のパッケージ …) に並べて置ける**。生成器が持ち込む
   補助の名前が衝突しないこと (ebm2go がその場展開にこだわる理由の 1 つ。memory
   `project_go_inline_same_package`)。言語ごとに「その場に展開する」「名前に .bgn
   由来の接頭辞を付ける」「別の 1 ファイルに出して共有する」のどれかを選ぶ
3. **union は有効な腕しか読めない形**を既定にする (直和型のある言語は直和型)。
   ヒープを避けたい用途向けに平らな形を選べる (ebm2go の `config.go.union = "noheap"`
   と同じ仕組みを全言語に: `config.<lang>.union`)
4. **エラーはどの field で起きたかを持つ** (`Format::field`、walker が決める)。
   言語の慣習の失敗の表し方 (error 値 / Result / 例外 / 戻り値のコード) に載せる
5. **バイト列は、言語が許すなら入力を借りる (zero-copy) 口と、所有する口の両方**。
   借りる口は「入力のバッファを書き換えない」前提を API の型か文書で示す
6. **入力はバッファからとストリームからの両方、出力はバッファへ足すとストリームへ
   流すの両方**。ストリームへ流す口は全体を一度組み立てない (今の nast2go の
   `Write` は Append してから 1 回で書くので、この約束をまだ満たしていない)
7. 名前は言語の慣習に合わせる (型は各言語の型の綴り、field は Go なら CamelCase、
   Rust / Python なら snake_case …)。生成器の採番した名前 (`tmp688` / `Variant90`) を
   公開しない (ADR 0044)

## 軸

| 軸 | 決めること |
| --- | --- |
| 値 | struct / class / record のどれか、既定値、比較できるか |
| union | 直和型か、平らか、有効な腕の読み方 |
| バイト列 | 借りるか所有するか、固定長は配列か |
| decode | バッファから (読んだ長さか残りを返す) / ストリームから / 1 つ丸ごと (余りはエラー) |
| encode | 新しいバッファ / 既存のバッファへ足す / ストリームへ / 必要な長さを先に知る |
| エラー | 型と、field の場所の持たせ方 |
| 並べて置く | 生成物どうしの衝突の避け方 |

## 言語ごと

### Go

- 値: `type X struct`、公開 field は CamelCase。比較は `==` が使える形 (スライスを含む
  ものは除く)
- union: 既定は interface (腕ごとの型) + どの腕かを返す accessor。`noheap` で平ら
- バイト列: `[]byte` (decode のスライスの口は入力を借りる)。固定長は `[N]byte`
- decode: `DecodeSlice(buf []byte, off *int) error` / `Read(r io.Reader) error` /
  `UnmarshalBinary(data []byte) error` (標準の `encoding.BinaryUnmarshaler`、余りは
  エラー)
- encode: `AppendBinary(b []byte) ([]byte, error)` (Go 1.24 の `encoding.BinaryAppender`) /
  `MarshalBinary() ([]byte, error)` / `WriteTo(w io.Writer) (int64, error)`
  (`io.WriterTo`。field ごとに流す)
- エラー: `error`。field の場所を持つ型 (`*DecodeError{Field string; Err error}`) を
  返し、`errors.Is(err, io.ErrUnexpectedEOF)` で元を辿れる
- 並べて置く: パッケージ直下に補助を出さない (その場に展開)
- 今の ebm2go / nast2go との差: 標準の interface (`BinaryUnmarshaler` / `BinaryAppender` /
  `WriterTo`) の名前に合わせていない。ebm2go の `Decode([]byte) ([]byte, error)` と
  unictest のハーネスが使う名前は残す

### Rust

- 値: `pub struct X`、`#[derive(Debug, Clone, PartialEq, Eq, Default)]`
- union: `pub enum` (腕ごとの variant)。判別に使う field との整合は encode で検査
- バイト列: 借りる口は `X<'a>` に `&'a [u8]`、所有する口は `Vec<u8>`。同じ型で
  両方は持てないので、借りる型を基本にし `to_owned` 相当を足す案と、所有する型だけに
  する案がある (未決)
- decode: `fn decode(buf: &[u8]) -> Result<(Self, &[u8]), Error>` (残りを返す) /
  `fn read_from<R: std::io::Read>(r: &mut R) -> Result<Self, Error>` /
  `impl TryFrom<&[u8]> for X` (余りはエラー)
- encode: `fn encode_to(&self, out: &mut Vec<u8>) -> Result<(), Error>` /
  `fn write_to<W: std::io::Write>(&self, w: &mut W) -> Result<(), Error>` /
  `fn encoded_len(&self) -> usize`
- エラー: `pub enum Error`、`impl std::error::Error + Display`。field の場所と
  `std::io::Error` を持つ
- 並べて置く: 生成物 1 つが 1 モジュール (`mod x;`)。補助はそのモジュールに閉じる
- 今の ebm2rust との差: union が `Variant90` を持つ struct + accessor (直和型を
  使っていない)。`decode(&mut self, ..)` で既存の値を書き換える形

### C

- 値: `typedef struct x_t { ... } x_t;`、名前は `.bgn` 由来の接頭辞つき snake_case
- union: tagged union (`enum` の tag + `union`)。有効な腕は tag で判別
- バイト列: 借りる口は `const uint8_t* ptr; size_t len;` (入力の中を指す)。所有させない
  (C では誰が free するかが API の一部になるので、生成物は確保しない)
- decode: `int x_decode(x_t* out, const uint8_t* buf, size_t len, size_t* consumed);`
  入力を借りるので確保は不要。可変長の配列は入力を指す
- encode: `int x_encoded_size(const x_t* in, size_t* size);` と
  `int x_encode(const x_t* in, uint8_t* buf, size_t cap, size_t* written);`
  (呼ぶ側がバッファを用意する。足りなければ必要な長さを返す)
- ストリーム: `int x_read(x_t* out, x_reader_t* r);` (`r` は読み出しの関数ポインタと
  文脈)。要素ごとに確保が要る形 (長さの無い配列の要素など) は呼ぶ側の allocator を
  渡す
- エラー: 戻り値の `int` (0 が成功) と、失敗した field を指す
  `const char* x_error_field(int code)` 相当、または `x_error_t` を出力引数で
- 並べて置く: 関数は接頭辞つき。ヘッダだけで使う形なら `static inline`、.c / .h に
  分けるなら補助を 1 つの共有ヘッダ (`#ifndef` で 1 度だけ) に
- 今の ebm2c との差: `DecoderInput` が関数ポインタ (append / reserve / can_read) を
  持ち、確保を生成物の中から呼ぶ (`*_free` がある)。呼ぶ側の用意したバッファに
  閉じる形を基本にしたい

### C++

- 値: `struct X` (集成体)、`namespace` に入れる
- union: `std::variant`
- バイト列: `std::span<const std::byte>` (借りる) / `std::vector<std::byte>` (所有)
- decode: `static std::expected<X, Error> decode(std::span<const std::byte>& in);`
  (C++23)。C++20 なら `Error decode(X&, span&)`
- encode: `Error encode(std::vector<std::byte>& out) const;` /
  `Error encode(std::span<std::byte>& out) const;` / `std::size_t encoded_size() const;`
- 並べて置く: namespace と inline 関数
- 今の ebm2cpp との差: futils (`binary/flags.h` など) に依存している — ADR 0022 の
  「標準ライブラリだけ」から外れる

### TypeScript

- 値: `interface X` (データ) + 関数。class にしない (JSON と行き来しやすい)
- union: 判別子つきの union (`{ kind: "data"; data: DataFrame } | ...`)
- バイト列: `Uint8Array` (decode は `subarray` で借りる)
- decode: `decodeX(buf: Uint8Array, offset?: number): { value: X; offset: number }` /
  `decodeXExact(buf: Uint8Array): X`
- encode: `encodeX(v: X): Uint8Array` / `encodeXInto(v: X, buf: Uint8Array, offset: number): number`
- エラー: `class DecodeError extends Error { field: string }` を throw
- 並べて置く: ES module (ファイルが名前空間)
- 今の ebm2ts との差: `{view, offset, resizeLimit}` の入れ物を渡す形、union の field が
  `field688`

### Python

- 値: `@dataclasses.dataclass class X`、field は snake_case、整数は `int`
- union: 腕ごとの dataclass の `Union[...]` (3.10 以降は `A | B`)、`match` で分けられる
- バイト列: `bytes` (所有)。借りる口は `memoryview`
- decode: `@classmethod def decode(cls, data: bytes) -> tuple["X", int]` /
  `@classmethod def read(cls, f: typing.BinaryIO) -> "X"` / `from_bytes` (余りはエラー)
- encode: `def to_bytes(self) -> bytes` / `def write(self, f: typing.BinaryIO) -> None`
- エラー: `class DecodeError(ValueError)` (field を属性に持つ)
- 今の ebm2python との差: `decode(self, f)` で既存の値を書き換える形、`__init__` を
  手で書いていて dataclass の既定値を使っていない

### Java

- 値: `public final class X` (可変) か `record` (Java 16+、不変)。生成物は可変 class
  が扱いやすい (decode で埋める)
- union: `sealed interface` + 腕ごとの `record` (Java 17+)
- バイト列: `byte[]` (所有)。借りる口は `ByteBuffer` の slice
- decode: `public static X decode(ByteBuffer in)` (読んだ分 position が進む) /
  `public static X read(InputStream in) throws IOException`
- encode: `public void encode(ByteBuffer out)` / `public byte[] toByteArray()` /
  `public void write(OutputStream out) throws IOException` / `public int encodedSize()`
- エラー: `class DecodeException extends RuntimeException` (field を持つ)。ストリームは
  `IOException`
- 並べて置く: 生成物ごとに 1 つの package か、1 つの外側 class
- 今の ebm2java との差: `BgnInput` / `BgnOutput` という独自の入れ物、全部が
  `BgnGenerated` の内側 class

### C#

- 値: `public sealed class X` (可変)、プロパティは PascalCase
- union: 抽象 record と派生 record (C# に直和型は無い)
- バイト列: `byte[]` / 借りる口は `ReadOnlyMemory<byte>`
- decode: `public static X Decode(ReadOnlySpan<byte> src, out int consumed)` /
  `public static bool TryDecode(ReadOnlySpan<byte> src, out X value, out int consumed)`
  (`Try` の慣習) / `public static X Read(Stream s)`
- encode: `public int Encode(Span<byte> dst)` / `public byte[] ToArray()` /
  `public void Write(Stream s)` / `public int EncodedSize`
- 整数は `System.Buffers.Binary.BinaryPrimitives`
- エラー: `class DecodeException : Exception` (field を持つ)
- 今の ebm2csharp との差: `BgnInput` / `BgnOutput`、`BgnGenerated` の内側 class

### Scala

- 値: `final case class X(...)` (不変、`copy` で変える)
- union: `sealed trait` + 腕ごとの `case class` (Scala 3 なら `enum`)
- decode: `def decode(in: java.nio.ByteBuffer): Either[DecodeError, X]`
- encode: `def encode(out: ByteBuffer): Unit` / `def toBytes: Array[Byte]`
- 今の ebm2scala との差: 命令型の Java の写し (memory の「段階 1」)。関数型化 (不変な
  case class、Either) は段階 2 として未着手だったもの

### Ruby

- 値: `class X` に `attr_accessor`、または `Struct.new(..., keyword_init: true)`
- union: 腕ごとの class (duck typing)
- decode: `X.read(io)` / `X.decode(string)` (BinData gem の `read` と同じ名前の慣習)
- encode: `x.to_binary_s` / `x.write(io)`、整数は `pack` / `unpack1`
- エラー: `class DecodeError < StandardError`

### Zig

- 値: `pub const X = struct { ... }`、既定値つき
- union: `union(enum)` (tagged union)
- バイト列: 借りる口は `[]const u8`、所有する口は `allocator` で確保した `[]u8`
- decode: `pub fn decode(reader: anytype, allocator: std.mem.Allocator) !X` /
  `pub fn decodeSlice(buf: []const u8) !struct { value: X, rest: []const u8 }`
- encode: `pub fn encode(self: X, writer: anytype) !void`
- エラー: error union (`error{ShortInput, ...}`)。field の場所はエラー値に載らないので
  診断用の出力引数で
- 今の ebm2zig との差: union が `Variant90` (tagged union を使っていない)

### Hike

ADR 0049 のとおり、構文は Go、IO とエラーは C の形 (呼ぶ側の `*Decoder` と `int` の
エラーコード、import なし)。上の C の API 像を Go の構文で綴る。

### 別の性質のもの (汎用の codec API ではない)

- **LLVM**: 出力は IR。外から見える API は C の ABI なので、上の C の像に揃える
- **Wuffs**: Wuffs の慣習 (decoder の struct、`io_reader`、中断できる `?` 関数、
  status) がそのまま API。Wuffs の流儀に従い、ここでは決めない
- **P4**: parser の状態機械と deparser。decode / encode の API ではなく、header の型と
  状態遷移が出力
- **Z3**: 正しいメッセージの制約。実行時の API は無い

## 未決 (決めてもらうこと)

1. **借りるか所有するか** (Rust / C++ / C#)。借りる型だけにすると zero-copy だが
   値を入力より長く持てない。両方出すと型が倍になる
2. **Go で標準の interface 名に寄せるか**。`UnmarshalBinary` / `AppendBinary` / `WriteTo`
   に寄せると標準ライブラリから使えるが、ebm2go の名前 (`Decode` / `Append` /
   `Write`) と unictest のハーネスの両方を残すか、ハーネスを直すか
3. **値を書き換えるか新しく返すか** (Python / Rust / Java)。`X.decode(data) -> X` は
   自然だが、state の受け渡しや大きな値の再利用では「既存の値に読む」形が要る
4. **C の確保**。入力を借りる形に閉じると、長さの無い配列の要素 (format の配列) の
   置き場所が無い。呼ぶ側の allocator を渡すか、要素数を先に数える 2 段にするか
5. **不変 (Scala / Java record)** を既定にするか。encode 前に値を組み立てる側には
   可変のほうが書きやすい
