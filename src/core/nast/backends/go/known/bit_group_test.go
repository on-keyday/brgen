package bit_group

import (
	"bytes"
	"testing"
)

// 既知の値で切り方を見る。往復だけでは「同じ間違いを両方向でする」ものを
// 見逃すので、ここは手で計算した値と比べる。

func roundtrip(t *testing.T, in []byte, v interface {
	Decode([]byte) (int, error)
	Encode() ([]byte, error)
}) {
	t.Helper()
	n, err := v.Decode(in)
	if err != nil || n != len(in) {
		t.Fatalf("decode %x: n=%d err=%v", in, n, err)
	}
	out, err := v.Encode()
	if err != nil || !bytes.Equal(out, in) {
		t.Fatalf("encode: %x err=%v want %x", out, err, in)
	}
}

func TestOneByte(t *testing.T) {
	var v OneByte
	roundtrip(t, []byte{0x21}, &v)
	if v.Kind != NibY || v.Reserved != 0 || v.Flag != 1 {
		t.Fatalf("%+v", v)
	}
	if _, err := v.Decode([]byte{0x23}); err == nil {
		t.Fatal("reserved != 0 must fail")
	}
}

func TestStraddle(t *testing.T) {
	var v Straddle
	roundtrip(t, []byte{0x12, 0x34}, &v)
	if v.Hi != 1 || v.Mid != 0x23 || v.Lo != 4 {
		t.Fatalf("%+v", v)
	}
}

func TestLittle(t *testing.T) {
	// LE で 0x654321、lsb から a b c d。
	var v Little
	roundtrip(t, []byte{0x21, 0x43, 0x65}, &v)
	if v.A != 1 || v.B != 2 || v.C != 3 || v.D != 0x654 {
		t.Fatalf("%+v", v)
	}
}

func TestVarIntLike(t *testing.T) {
	// RFC 9000 A.1 の例。
	cases := []struct {
		in   []byte
		want uint64
	}{
		{[]byte{0x25}, 37},
		{[]byte{0x7b, 0xbd}, 15293},
		{[]byte{0x9d, 0x7f, 0x3e, 0x7d}, 494878333},
		{[]byte{0xc2, 0x19, 0x7c, 0x5e, 0xff, 0x14, 0xe8, 0x8c}, 151288809941952652},
	}
	for _, c := range cases {
		var v VarIntLike
		roundtrip(t, c.in, &v)
		if v.V != c.want {
			t.Fatalf("%x: got %d want %d", c.in, v.V, c.want)
		}
	}
}

func TestRoutedLittle(t *testing.T) {
	var v RoutedLittle
	roundtrip(t, []byte{0x50}, &v)
	if v.Sel != 0 || v.A != 5 {
		t.Fatalf("%+v", v)
	}
	var w RoutedLittle
	roundtrip(t, []byte{0x21, 0x43}, &w)
	if w.Sel != 1 || w.B != 0x432 {
		t.Fatalf("%+v", w)
	}
}

func TestArmThenTail(t *testing.T) {
	var v ArmThenTail
	roundtrip(t, []byte{0xd6}, &v)
	if v.Flag != 1 || v.X != 5 || v.Tail != 6 {
		t.Fatalf("%+v", v)
	}
	var w ArmThenTail
	roundtrip(t, []byte{0x56}, &w)
	if w.Flag != 0 || w.Y != 5 || w.Tail != 6 {
		t.Fatalf("%+v", w)
	}
}
