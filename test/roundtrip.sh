#!/bin/sh
set -eu

ROOT=$(cd "$(dirname "$0")/.." && pwd)
BIN="$ROOT/huffman"
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT INT TERM

pass=0
fail() {
    echo "FAIL: $1" >&2
    exit 1
}
ok() {
    pass=$((pass + 1))
    echo "ok $pass - $1"
}

cd "$WORK"

mkdir -p src/sub/deep
printf 'hello huffman world\n' > src/a.txt
: > src/empty.bin
printf 'aaaaaaaaaa' > src/one.bin
printf 'deep\n' > src/sub/deep/d.txt
dd if=/dev/urandom of=src/sub/big.bin bs=1024 count=200 2>/dev/null
i=0
while [ "$i" -lt 256 ]; do
    printf "\\$(printf '%03o' "$i")"
    i=$((i + 1))
done > src/all.bin
dd if=/dev/zero bs=1000 count=1 2>/dev/null > src/run.bin
printf 'abcabcabc' >> src/run.bin
printf 'extra top level\n' > extra.txt
chmod 640 src/a.txt

"$BIN" c archive.huf src extra.txt 2>/dev/null || fail "compress"
mkdir out
"$BIN" d archive.huf out || fail "decompress"
diff -r src out/src || fail "directory content"
cmp extra.txt out/extra.txt || fail "extra.txt content"
[ "$(stat -c %a src/a.txt)" = "$(stat -c %a out/src/a.txt)" ] || fail "mode restore"
[ "$(stat -c %Y src/a.txt)" = "$(stat -c %Y out/src/a.txt)" ] || fail "mtime restore"
ok "round trip of text, binary, empty, single symbol and nested files"

"$BIN" c run.huf src/run.bin 2>/dev/null || fail "compress run heavy"
run_raw=$(stat -c %s src/run.bin)
run_arc=$(stat -c %s run.huf)
[ "$run_arc" -lt "$run_raw" ] || fail "run heavy file not shrunk"
ok "run length front end shrinks a run heavy file"

"$BIN" c rnd.huf src/sub/big.bin 2>/dev/null || fail "compress run free"
rnd_raw=$(stat -c %s src/sub/big.bin)
rnd_arc=$(stat -c %s rnd.huf)
[ "$rnd_arc" -lt $((rnd_raw + rnd_raw / 20 + 4096)) ] || fail "run free file expanded too much"
ok "run front end does not inflate run free data"

"$BIN" c only_empty.huf src/empty.bin 2>/dev/null || fail "compress empty only"
mkdir out_empty
"$BIN" d only_empty.huf out_empty || fail "decompress empty only"
[ -f out_empty/src/empty.bin ] || fail "empty file missing"
[ ! -s out_empty/src/empty.bin ] || fail "empty file not empty"
ok "archive with no symbols (empty payload)"

if "$BIN" c bad.huf /etc/hostname 2>/dev/null; then
    fail "absolute path accepted"
fi
ok "absolute input path rejected"

if "$BIN" c bad.huf does-not-exist 2>/dev/null; then
    fail "missing input accepted"
fi
ok "missing input rejected"

if "$BIN" c dup.huf src/a.txt src/a.txt 2>/dev/null; then
    fail "duplicate path accepted"
fi
ok "duplicate archive path rejected"

printf 'not an archive' > bad.huf
mkdir out_bad
if "$BIN" d bad.huf out_bad 2>/dev/null; then
    fail "bad magic accepted"
fi
ok "bad magic rejected"

if "$BIN" >/dev/null 2>&1; then
    fail "no arguments accepted"
fi
if "$BIN" x >/dev/null 2>&1; then
    fail "unknown command accepted"
fi
ok "usage errors rejected"

echo "PASS: $pass checks"
