// This file was generated from JSON Schema using quicktype, do not modify it directly.
// To parse and unparse this JSON data, add this code to your project and do:
//
//    coordinate, err := UnmarshalCoordinate(bytes)
//    bytes, err = coordinate.Marshal()

package main

import (
	"bytes"
	"encoding/json"
	"errors"
)

func UnmarshalCoordinate(data []byte) (Type, error) {
	var r Type
	err := json.Unmarshal(data, &r)
	return r, err
}

func (r *Type) Marshal() ([]byte, error) {
	return json.Marshal(r)
}

type EnumSpec map[string]string

// the schema for ksy files
type Type struct {
	Doc    *string `json:"doc,omitempty"`
	DocRef *DocRef `json:"doc-ref,omitempty"`
	// Purpose: description of data that lies outside of normal sequential parsing flow (for
	// example, that requires seeking somewhere in the file) or just needs to be loaded only by
	// special request
	//
	// Influences: would be translated into distinct methods (that read desired data on demand)
	// in current class
	Instances *InstancesSpec `json:"instances,omitempty"`
	Meta      *Meta          `json:"meta,omitempty"`
	Params    []ParamSpec    `json:"params,omitempty"`
	// identifier for a primary structure described in top-level map
	Seq []*Attribute `json:"seq,omitempty"`
	// maps of strings to user-defined types
	//
	// declares types for substructures that can be referenced in the attributes of seq or
	// instances element
	//
	// would be directly translated into classes
	Types map[string]*Type `json:"types,omitempty"`
	// allows for the setup of named enums, mappings of integer constants to symbolic names. Can
	// be used with integer attributes using the enum key.
	//
	// would be represented as enum-like construct (or closest equivalent, if target language
	// doesn't support enums), nested or namespaced in current type/class
	Enums map[string]EnumSpec `json:"enums,omitempty"`
}

// allows for the setup of named enums, mappings of integer constants to symbolic names. Can
// be used with integer attributes using the enum key.
//
// would be represented as enum-like construct (or closest equivalent, if target language
// doesn't support enums), nested or namespaced in current type/class
type EnumsSpec struct {
}

// Purpose: description of data that lies outside of normal sequential parsing flow (for
// example, that requires seeking somewhere in the file) or just needs to be loaded only by
// special request
//
// Influences: would be translated into distinct methods (that read desired data on demand)
// in current class
type InstancesSpec struct {
}

type TypeSpec struct{}

type Meta struct {
	ID            *Identifier  `json:"id"`
	Application   *DocRef      `json:"application,omitempty"`
	Encoding      *string      `json:"encoding,omitempty"`
	Endian        *EndianUnion `json:"endian,omitempty"`
	FileExtension *DocRef      `json:"file-extension,omitempty"`
	// list of relative or absolute paths to another `.ksy` files to import (**without** the
	// `.ksy` extension)
	//
	// the top-level type of the imported file will be accessible in the current spec under the
	// name specified in the top-level `/meta/id` of the imported file
	Imports []string `json:"imports,omitempty"`
	// advise the Kaitai Struct Compiler (KSC) to use debug mode
	KsDebug *bool `json:"ks-debug,omitempty"`
	// advise the Kaitai Struct Compiler (KSC) to ignore missing types in the .ksy file, and
	// assume that these types are already provided externally by the environment the classes
	// are generated for
	KsOpaqueTypes *bool      `json:"ks-opaque-types,omitempty"`
	KsVersion     *KsVersion `json:"ks-version,omitempty"`
	License       *string    `json:"license,omitempty"`
	Title         *string    `json:"title,omitempty"`
	Xref          *Xref      `json:"xref,omitempty"`
}

type EndianClass struct {
	Cases    EndianCases `json:"cases"`
	SwitchOn *AnyScalar  `json:"switch-on"`
}

type EndianCases struct {
}

type Xref struct {
	// article name at [Forensics Wiki](https://forensics.wiki/), which is a CC-BY-SA-licensed
	// wiki with information on digital forensics, file formats and tools
	//
	// full link name could be generated as `https://forensics.wiki/` + this value + `/`
	Forensicswiki *Forensicswiki `json:"forensicswiki"`
	// ISO/IEC standard number, reference to a standard accepted and published by
	// [ISO](https://www.iso.org/) (International Organization for Standardization).
	//
	// ISO standards typically have clear designations like "ISO/IEC 15948:2004", so value
	// should be citing everything except for "ISO/IEC", i.e. `15948:2004`
	ISO *ISO `json:"iso"`
	// article name at ["Just Solve the File Format Problem"
	// wiki](http://fileformats.archiveteam.org/wiki/Main_Page), a wiki that collects
	// information on many file formats
	//
	// full link name could be generated as `http://fileformats.archiveteam.org/wiki/` + this
	// value
	Justsolve *Forensicswiki `json:"justsolve"`
	// identifier in [Digital
	// Formats](https://www.loc.gov/preservation/digital/formats/fdd/browse_list.shtml) database
	// of [US Library of Congress](https://www.loc.gov/)
	//
	// value typically looks like `fddXXXXXX`, where `XXXXXX` is a 6-digit identifier
	LOC *LOC `json:"loc"`
	// MIME type (IANA media type), a string typically used in various Internet protocols to
	// specify format of binary payload
	//
	// there is a [central registry of media
	// types](https://www.iana.org/assignments/media-types/media-types.xhtml) managed by IANA
	//
	// value must specify full MIME type (both parts), e.g. `image/png`
	MIME *MIME `json:"mime"`
	// format identifier in [PRONOM Technical
	// Registry](https://www.nationalarchives.gov.uk/PRONOM/BasicSearch/proBasicSearch.aspx?status=new)
	// of [UK National Archives](https://www.nationalarchives.gov.uk/), which is a massive file
	// formats database that catalogues many file formats for digital preservation purposes
	Pronom *Pronom `json:"pronom"`
	// reference to [RFC](https://en.wikipedia.org/wiki/Request_for_Comments), "Request for
	// Comments" documents maintained by ISOC (Internet Society)
	//
	// RFCs are typically treated as global, Internet-wide standards, and, for example, many
	// networking / interoperability protocols are specified in RFCs
	//
	// value should be just raw RFC number, without any prefixes, e.g. `1234`
	RFC *RFCUnion `json:"rfc"`
	// item identifier at [Wikidata](https://www.wikidata.org/wiki/Wikidata:Main_Page), a global
	// knowledge base
	//
	// value typically follows `Qxxx` pattern, where `xxx` is a number generated by
	// [Wikidata](https://www.wikidata.org/wiki/Wikidata:Main_Page), e.g. `Q535473`
	Wikidata *Wikidata `json:"wikidata"`
}

type ParamSpec struct {
	Doc    *string `json:"doc,omitempty"`
	DocRef *DocRef `json:"doc-ref"`
	// path to an enum type (defined in the `enums` map), which will become the type of the
	// parameter
	//
	// only integer-based enums are supported, so `type` must be an integer type (`type: uX`,
	// `type: sX` or `type: bX`) for this property to work
	//
	// you can use `enum` with `type: b1` as well: `b1` means a 1-bit **integer** (0 or 1) when
	// used with `enum` (**not** a boolean)
	//
	// one can reference an enum type of a subtype by specifying a relative path to it from the
	// current type, with a double colon as a path delimiter (e.g. `foo::bar::my_enum`)
	Enum *string `json:"enum,omitempty"`
	ID   string  `json:"id"`
	// specifies "pure" type of the parameter, without any serialization details (like
	// endianness, sizes, encodings)
	//
	// | Value                  | Description
	// |-
	// | `u1`, `u2`, `u4`, `u8` | unsigned integer
	// | `s1`, `s2`, `s4`, `s8` | signed integer
	// | `bX`                   | bit-sized integer (if `X` != 1)
	// | `f4`, `f8`             | floating point number
	// | `type` key missing<br>or `bytes` | byte array
	// | `str`                  | string
	// | `bool` (or `b1`)       | boolean
	// | `struct`               | arbitrary KaitaiStruct-compatible user type
	// | `io`                   | KaitaiStream-compatible IO stream
	// | `any`                  | allow any type (if target language supports that)
	// | other identifier       | user-defined type, without parameters<br>a nested type can be
	// referenced with double colon (e.g. `type: 'foo::bar'`)
	//
	// one can specify arrays by appending `[]` after the type identifier (e.g. `type: u2[]`,
	// `type: 'foo::bar[]'`, `type: struct[]` etc.)
	Type *string `json:"type,omitempty"`
}

// identifier for a primary structure described in top-level map
type Attribute struct {
	// specify if terminator byte should be "consumed" when reading
	//
	// if true: the stream pointer will point to the byte after the terminator byte
	//
	// if false: the stream pointer will point to the terminator byte itself
	//
	// default is true
	Consume *bool `json:"consume,omitempty"`
	// specify fixed contents that the parser should encounter at this point. If the content of
	// the stream doesn't match the given bytes, an error is thrown and it's meaningless to
	// continue parsing
	Contents *Contents `json:"contents,omitempty"`
	Doc      *string   `json:"doc,omitempty"`
	DocRef   *DocRef   `json:"doc-ref,omitempty"`
	Encoding *string   `json:"encoding,omitempty"`
	// name of existing enum field data type becomes given enum
	Enum *string `json:"enum,omitempty"`
	// allows the compiler to ignore the lack of a terminator if eos-error is disabled, string
	// reading will stop at either:
	//
	// 1. terminator being encountered
	//
	// 2. end of stream is reached
	//
	// default is `true`
	EOSError *bool `json:"eos-error,omitempty"`
	// contains a string used to identify one attribute among others
	ID *string `json:"id,omitempty"`
	// marks the attribute as optional (attribute is parsed only if the condition specified
	// evaluates to `true`)
	If *If `json:"if,omitempty"`
	// specifies if terminator byte should be considered part of the string read and thus be
	// appended to it
	//
	// default is false
	Include *bool `json:"include,omitempty"`
	// specifies an IO stream from which a value should be parsed
	Io *string `json:"io,omitempty"`
	// specify a byte which is the string or byte array padded with after the end up to the
	// total size
	//
	// can be used only with `size` or `size-eos: true` (when the size is fixed)
	//
	// when `terminator`:
	// - isn't specified, then the `pad-right` controls where the string ends (basically acts
	// like a terminator)
	// - is specified, padding comes after the terminator, not before. The value is terminated
	// immediately after the terminator occurs, so the `pad-right` has no effect on parsing and
	// is only relevant for serialization
	PadRight *int64 `json:"pad-right,omitempty"`
	// specifies position at which the value should be parsed
	Pos *StringOrInteger `json:"pos,omitempty"`
	// specifies an algorithm to be applied to the underlying byte buffer of the attribute
	// before parsing
	//
	// can be used only if the size is known (either `size`, `size-eos: true` or `terminator`
	// are specified), see [Applying `process` without a
	// size](https://doc.kaitai.io/user_guide.html#_applying_process_without_a_size) in the User
	// Guide
	//
	// | Value | Description
	// |-
	// | `xor(key)` | <p>apply a bitwise XOR (written as `^` in most C-like languages) to every
	// byte of the buffer using the provided `key`</p><p>**`key`** is required, and can be
	// either <ul><li>a single byte value — will be XORed with every byte of the input stream
	// <ul><li>make sure that the **`key`** is in range **0-255**, otherwise you may get
	// unexpected results</li></ul></li><li>a byte array — first byte of the input will be XORed
	// with the first byte of the key, second byte of the input with the second byte of the key,
	// etc. <ul><li>when the end of the key is reached, it starts again from the first
	// byte</li></ul></li></ul></p><p>_the output length remains the same as the input
	// length_</p>
	// | `rol(n)`, `ror(n)` | <p>apply a [bitwise
	// rotation](https://en.wikipedia.org/wiki/Bitwise_operation#bit_rotation) (also known as a
	// [circular shift](https://en.wikipedia.org/wiki/Circular_shift)) by **`n`** bits to every
	// byte of the buffer</p><p>`rol` = left circular shift, `ror` = right circular
	// shift</p><p>**`n`** is required, and should be in range **0-7** for consistent results
	// (to be safe, use `shift_amount % 8` as the **`n`** parameter, if the value of
	// `shift_amount` itself may not fall into that range)</p>
	// | `zlib` | <p>apply a _zlib_ decompression to the input buffer, expecting it to be a
	// full-fledged _zlib_ stream, i.e. having a regular 2-byte _zlib_ header.</p><p>typical
	// _zlib_ header values: <ul><li>`78 01` — low compression</li><li>`78 9C` — default
	// compression</li><li>`78 DA` — best compression</li></ul></p>
	// | <p>`{my_custom_processor}(a, b, ...)`</p><p><i>(`{my_custom_processor}` is an arbitrary
	// name matching `[a-z][a-z0-9_.]*`)</i></p> | <p>use a custom processing routine, which you
	// implement in imperative code in the target language</p><p>the generated code will use the
	// class name `{my_custom_processor}` using the naming convention of the target language (in
	// most languages `MyCustomProcessor`, but e.g. `my_custom_processor_t` in C++: check the
	// generated code)</p><p>the processing class **must** define the method `public byte[]
	// decode(byte[] src)` and should implement the interface `CustomDecoder` (available in
	// [C++](https://github.com/kaitai-io/kaitai_struct_cpp_stl_runtime/blob/d020781b96ea1e2fe7e0ecf47ff3ae3c829ccd31/kaitai/custom_decoder.h#L8-L12),
	// [C#](https://github.com/kaitai-io/kaitai_struct_csharp_runtime/blob/1b66a9a85f39c52728893400a23504844cc78e34/KaitaiStruct.cs#L24-L37)
	// and
	// [Java](https://github.com/kaitai-io/kaitai_struct_java_runtime/blob/c342c6e836ddba03c9ec33a59034e209bb04a976/src/main/java/io/kaitai/struct/CustomDecoder.java#L26-L39))</p><p>you
	// can pass any parameters `(a, b, ...)` to your `{my_custom_processor}` class constructor
	// (**omit** the `()` brackets for parameter-less invocation)</p><p></p><p>one can reference
	// a class in a different namespace/package like `com.example.my_rle(5, 3)`</p><p><i>see
	// [Custom processing routines](https://doc.kaitai.io/user_guide.html#custom-process) in the
	// User Guide for more info</i></p>
	Process *string `json:"process,omitempty"`
	// designates repeated attribute in a structure
	//
	// | Value     | Description
	// |-
	// | `eos`     | repeat until the end of the current stream
	// | `expr`    | repeat as many times as specified in `repeat-expr`
	// | `until`   | repeat until the expression in `repeat-until` becomes **`true`**
	//
	// attribute read as array/list/sequence
	Repeat *Repeat `json:"repeat,omitempty"`
	// specify number of repetitions for repeated attribute
	RepeatExpr *StringOrInteger `json:"repeat-expr,omitempty"`
	// specifies a condition to be checked **after** each parsed item, repeating while the
	// expression is `false`
	//
	// one can use `_` in the expression, which is a special **local** variable that references
	// the last read element
	RepeatUntil *If `json:"repeat-until,omitempty"`
	// the number of bytes to read if `type` isn't defined.
	//
	// can also be an expression
	Size *StringOrInteger `json:"size,omitempty"`
	// if `true`, reads all the bytes till the end of the stream
	//
	// default is `false`
	SizeEOS *bool `json:"size-eos,omitempty"`
	// string or byte array reading will stop when it encounters this byte
	//
	// cannot be used with `type: strz` (which already implies `terminator: 0` - null-terminated
	// string)
	Terminator *int64 `json:"terminator,omitempty"`
	// defines data type for an attribute
	//
	// the type can also be user-defined in the `types` key
	//
	// one can reference a nested user-defined type by specifying a relative path to it from the
	// current type, with a double colon as a path delimiter (e.g. `foo::bar::my_type`)
	Type *TypeRef `json:"type"`
	// overrides any reading & parsing. Instead, just calculates function specified in value and
	// returns the result as this instance. Has many purposes
	Value interface{} `json:"value,omitempty"`
}

type TypeSwitch struct {
	Cases    map[string]*TypeRef `json:"cases"`
	SwitchOn *AnyScalar          `json:"switch-on"`
}

// maps of strings to user-defined types
//
// declares types for substructures that can be referenced in the attributes of seq or
// instances element
//
// would be directly translated into classes
type TypesSpec struct {
}

type EndianEnum string

const (
	Be EndianEnum = "be"
	LE EndianEnum = "le"
)

// designates repeated attribute in a structure
//
// | Value     | Description
// |-
// | `eos`     | repeat until the end of the current stream
// | `expr`    | repeat as many times as specified in `repeat-expr`
// | `until`   | repeat until the expression in `repeat-until` becomes **`true`**
//
// attribute read as array/list/sequence
type Repeat string

const (
	EOS   Repeat = "eos"
	Expr  Repeat = "expr"
	Until Repeat = "until"
)

// used to provide reference to original documentation (if the ksy file is actually an
// implementation of some documented format).
//
// Contains:
// 1. URL as text,
// 2. arbitrary string, or
// 3. URL as text + space + arbitrary string
type DocRef struct {
	String      *string
	StringArray []string
}

func (x *DocRef) UnmarshalJSON(data []byte) error {
	x.StringArray = nil
	object, err := unmarshalUnion(data, nil, nil, nil, &x.String, true, &x.StringArray, false, nil, false, nil, false, nil, false)
	if err != nil {
		return err
	}
	if object {
	}
	return nil
}

func (x *DocRef) MarshalJSON() ([]byte, error) {
	return marshalUnion(nil, nil, nil, x.String, x.StringArray != nil, x.StringArray, false, nil, false, nil, false, nil, false)
}

type EndianUnion struct {
	EndianClass *EndianClass
	Enum        *EndianEnum
}

func (x *EndianUnion) UnmarshalJSON(data []byte) error {
	x.EndianClass = nil
	x.Enum = nil
	var c EndianClass
	object, err := unmarshalUnion(data, nil, nil, nil, nil, false, nil, true, &c, false, nil, true, &x.Enum, false)
	if err != nil {
		return err
	}
	if object {
		x.EndianClass = &c
	}
	return nil
}

func (x *EndianUnion) MarshalJSON() ([]byte, error) {
	return marshalUnion(nil, nil, nil, nil, false, nil, x.EndianClass != nil, x.EndianClass, false, nil, x.Enum != nil, x.Enum, false)
}

type AnyScalar struct {
	Bool    *bool
	Double  *float64
	Integer *int64
	String  *string
}

func (x *AnyScalar) UnmarshalYAML(data []byte) error {
	object, err := unmarshalUnion(data, &x.Integer, &x.Double, &x.Bool, &x.String, false, nil, false, nil, false, nil, false, nil, true)
	if err != nil {
		return err
	}
	if object {
	}
	return nil
}

func (x *AnyScalar) MarshalJSON() ([]byte, error) {
	return marshalUnion(x.Integer, x.Double, x.Bool, x.String, false, nil, false, nil, false, nil, false, nil, true)
}

type Identifier struct {
	Bool   *bool
	String *string
}

func (x *Identifier) UnmarshalJSON(data []byte) error {
	object, err := unmarshalUnion(data, nil, nil, &x.Bool, &x.String, false, nil, false, nil, false, nil, false, nil, false)
	if err != nil {
		return err
	}
	if object {
	}
	return nil
}

func (x *Identifier) MarshalJSON() ([]byte, error) {
	return marshalUnion(nil, nil, x.Bool, x.String, false, nil, false, nil, false, nil, false, nil, false)
}

type KsVersion struct {
	Double *float64
	String *string
}

func (x *KsVersion) UnmarshalJSON(data []byte) error {
	object, err := unmarshalUnion(data, nil, &x.Double, nil, &x.String, false, nil, false, nil, false, nil, false, nil, false)
	if err != nil {
		return err
	}
	if object {
	}
	return nil
}

func (x *KsVersion) MarshalJSON() ([]byte, error) {
	return marshalUnion(nil, x.Double, nil, x.String, false, nil, false, nil, false, nil, false, nil, false)
}

type Forensicswiki struct {
	String      *string
	StringArray []string
}

func (x *Forensicswiki) UnmarshalJSON(data []byte) error {
	x.StringArray = nil
	object, err := unmarshalUnion(data, nil, nil, nil, &x.String, true, &x.StringArray, false, nil, false, nil, false, nil, false)
	if err != nil {
		return err
	}
	if object {
	}
	return nil
}

func (x *Forensicswiki) MarshalJSON() ([]byte, error) {
	return marshalUnion(nil, nil, nil, x.String, x.StringArray != nil, x.StringArray, false, nil, false, nil, false, nil, false)
}

// ISO/IEC standard number, reference to a standard accepted and published by
// [ISO](https://www.iso.org/) (International Organization for Standardization).
//
// ISO standards typically have clear designations like "ISO/IEC 15948:2004", so value
// should be citing everything except for "ISO/IEC", i.e. `15948:2004`
type ISO struct {
	String      *string
	StringArray []string
}

func (x *ISO) UnmarshalJSON(data []byte) error {
	x.StringArray = nil
	object, err := unmarshalUnion(data, nil, nil, nil, &x.String, true, &x.StringArray, false, nil, false, nil, false, nil, false)
	if err != nil {
		return err
	}
	if object {
	}
	return nil
}

func (x *ISO) MarshalJSON() ([]byte, error) {
	return marshalUnion(nil, nil, nil, x.String, x.StringArray != nil, x.StringArray, false, nil, false, nil, false, nil, false)
}

// identifier in [Digital
// Formats](https://www.loc.gov/preservation/digital/formats/fdd/browse_list.shtml) database
// of [US Library of Congress](https://www.loc.gov/)
//
// value typically looks like `fddXXXXXX`, where `XXXXXX` is a 6-digit identifier
type LOC struct {
	String      *string
	StringArray []string
}

func (x *LOC) UnmarshalJSON(data []byte) error {
	x.StringArray = nil
	object, err := unmarshalUnion(data, nil, nil, nil, &x.String, true, &x.StringArray, false, nil, false, nil, false, nil, false)
	if err != nil {
		return err
	}
	if object {
	}
	return nil
}

func (x *LOC) MarshalJSON() ([]byte, error) {
	return marshalUnion(nil, nil, nil, x.String, x.StringArray != nil, x.StringArray, false, nil, false, nil, false, nil, false)
}

// MIME type (IANA media type), a string typically used in various Internet protocols to
// specify format of binary payload
//
// there is a [central registry of media
// types](https://www.iana.org/assignments/media-types/media-types.xhtml) managed by IANA
//
// value must specify full MIME type (both parts), e.g. `image/png`
type MIME struct {
	String      *string
	StringArray []string
}

func (x *MIME) UnmarshalJSON(data []byte) error {
	x.StringArray = nil
	object, err := unmarshalUnion(data, nil, nil, nil, &x.String, true, &x.StringArray, false, nil, false, nil, false, nil, false)
	if err != nil {
		return err
	}
	if object {
	}
	return nil
}

func (x *MIME) MarshalJSON() ([]byte, error) {
	return marshalUnion(nil, nil, nil, x.String, x.StringArray != nil, x.StringArray, false, nil, false, nil, false, nil, false)
}

// format identifier in [PRONOM Technical
// Registry](https://www.nationalarchives.gov.uk/PRONOM/BasicSearch/proBasicSearch.aspx?status=new)
// of [UK National Archives](https://www.nationalarchives.gov.uk/), which is a massive file
// formats database that catalogues many file formats for digital preservation purposes
type Pronom struct {
	String      *string
	StringArray []string
}

func (x *Pronom) UnmarshalJSON(data []byte) error {
	x.StringArray = nil
	object, err := unmarshalUnion(data, nil, nil, nil, &x.String, true, &x.StringArray, false, nil, false, nil, false, nil, false)
	if err != nil {
		return err
	}
	if object {
	}
	return nil
}

func (x *Pronom) MarshalJSON() ([]byte, error) {
	return marshalUnion(nil, nil, nil, x.String, x.StringArray != nil, x.StringArray, false, nil, false, nil, false, nil, false)
}

// reference to [RFC](https://en.wikipedia.org/wiki/Request_for_Comments), "Request for
// Comments" documents maintained by ISOC (Internet Society)
//
// RFCs are typically treated as global, Internet-wide standards, and, for example, many
// networking / interoperability protocols are specified in RFCs
//
// value should be just raw RFC number, without any prefixes, e.g. `1234`
type RFCUnion struct {
	Integer    *int64
	String     *string
	UnionArray []RFCIdentifier
}

func (x *RFCUnion) UnmarshalJSON(data []byte) error {
	x.UnionArray = nil
	object, err := unmarshalUnion(data, &x.Integer, nil, nil, &x.String, true, &x.UnionArray, false, nil, false, nil, false, nil, false)
	if err != nil {
		return err
	}
	if object {
	}
	return nil
}

func (x *RFCUnion) MarshalJSON() ([]byte, error) {
	return marshalUnion(x.Integer, nil, nil, x.String, x.UnionArray != nil, x.UnionArray, false, nil, false, nil, false, nil, false)
}

type RFCIdentifier struct {
	Integer *int64
	String  *string
}

func (x *RFCIdentifier) UnmarshalJSON(data []byte) error {
	object, err := unmarshalUnion(data, &x.Integer, nil, nil, &x.String, false, nil, false, nil, false, nil, false, nil, false)
	if err != nil {
		return err
	}
	if object {
	}
	return nil
}

func (x *RFCIdentifier) MarshalJSON() ([]byte, error) {
	return marshalUnion(x.Integer, nil, nil, x.String, false, nil, false, nil, false, nil, false, nil, false)
}

// item identifier at [Wikidata](https://www.wikidata.org/wiki/Wikidata:Main_Page), a global
// knowledge base
//
// value typically follows `Qxxx` pattern, where `xxx` is a number generated by
// [Wikidata](https://www.wikidata.org/wiki/Wikidata:Main_Page), e.g. `Q535473`
type Wikidata struct {
	String      *string
	StringArray []string
}

func (x *Wikidata) UnmarshalJSON(data []byte) error {
	x.StringArray = nil
	object, err := unmarshalUnion(data, nil, nil, nil, &x.String, true, &x.StringArray, false, nil, false, nil, false, nil, false)
	if err != nil {
		return err
	}
	if object {
	}
	return nil
}

func (x *Wikidata) MarshalJSON() ([]byte, error) {
	return marshalUnion(nil, nil, nil, x.String, x.StringArray != nil, x.StringArray, false, nil, false, nil, false, nil, false)
}

// specify fixed contents that the parser should encounter at this point. If the content of
// the stream doesn't match the given bytes, an error is thrown and it's meaningless to
// continue parsing
type Contents struct {
	String     *string
	UnionArray []StringOrInteger
}

func (x *Contents) UnmarshalJSON(data []byte) error {
	x.UnionArray = nil
	object, err := unmarshalUnion(data, nil, nil, nil, &x.String, true, &x.UnionArray, false, nil, false, nil, false, nil, false)
	if err != nil {
		return err
	}
	if object {
	}
	return nil
}

func (x *Contents) MarshalJSON() ([]byte, error) {
	return marshalUnion(nil, nil, nil, x.String, x.UnionArray != nil, x.UnionArray, false, nil, false, nil, false, nil, false)
}

// specifies position at which the value should be parsed
//
// specify number of repetitions for repeated attribute
//
// the number of bytes to read if `type` isn't defined.
//
// can also be an expression
type StringOrInteger struct {
	Integer *int64
	String  *string
}

func (x *StringOrInteger) UnmarshalJSON(data []byte) error {
	object, err := unmarshalUnion(data, &x.Integer, nil, nil, &x.String, false, nil, false, nil, false, nil, false, nil, false)
	if err != nil {
		return err
	}
	if object {
	}
	return nil
}

func (x *StringOrInteger) MarshalJSON() ([]byte, error) {
	return marshalUnion(x.Integer, nil, nil, x.String, false, nil, false, nil, false, nil, false, nil, false)
}

type If struct {
	Bool   *bool
	String *string
}

func (x *If) UnmarshalJSON(data []byte) error {
	object, err := unmarshalUnion(data, nil, nil, &x.Bool, &x.String, false, nil, false, nil, false, nil, false, nil, false)
	if err != nil {
		return err
	}
	if object {
	}
	return nil
}

func (x *If) MarshalJSON() ([]byte, error) {
	return marshalUnion(nil, nil, x.Bool, x.String, false, nil, false, nil, false, nil, false, nil, false)
}

// defines data type for an attribute
//
// the type can also be user-defined in the `types` key
//
// one can reference a nested user-defined type by specifying a relative path to it from the
// current type, with a double colon as a path delimiter (e.g. `foo::bar::my_type`)
type TypeRef struct {
	Bool   *bool
	String *string
	Switch *TypeSwitch
}

func (x *TypeRef) UnmarshalJSON(data []byte) error {
	x.Switch = nil
	var c TypeSwitch
	object, err := unmarshalUnion(data, nil, nil, &x.Bool, &x.String, false, nil, true, &c, false, nil, false, nil, true)
	if err != nil {
		return err
	}
	if object {
		x.Switch = &c
	}
	return nil
}

func (x *TypeRef) MarshalJSON() ([]byte, error) {
	return marshalUnion(nil, nil, x.Bool, x.String, false, nil, x.Switch != nil, x.Switch, false, nil, false, nil, true)
}

func unmarshalUnion(data []byte, pi **int64, pf **float64, pb **bool, ps **string, haveArray bool, pa interface{}, haveObject bool, pc interface{}, haveMap bool, pm interface{}, haveEnum bool, pe interface{}, nullable bool) (bool, error) {
	if pi != nil {
		*pi = nil
	}
	if pf != nil {
		*pf = nil
	}
	if pb != nil {
		*pb = nil
	}
	if ps != nil {
		*ps = nil
	}

	dec := json.NewDecoder(bytes.NewReader(data))
	dec.UseNumber()
	tok, err := dec.Token()
	if err != nil {
		return false, err
	}

	switch v := tok.(type) {
	case json.Number:
		if pi != nil {
			i, err := v.Int64()
			if err == nil {
				*pi = &i
				return false, nil
			}
		}
		if pf != nil {
			f, err := v.Float64()
			if err == nil {
				*pf = &f
				return false, nil
			}
			return false, errors.New("Unparsable number")
		}
		return false, errors.New("Union does not contain number")
	case float64:
		return false, errors.New("Decoder should not return float64")
	case bool:
		if pb != nil {
			*pb = &v
			return false, nil
		}
		return false, errors.New("Union does not contain bool")
	case string:
		if haveEnum {
			return false, json.Unmarshal(data, pe)
		}
		if ps != nil {
			*ps = &v
			return false, nil
		}
		return false, errors.New("Union does not contain string")
	case nil:
		if nullable {
			return false, nil
		}
		return false, errors.New("Union does not contain null")
	case json.Delim:
		if v == '{' {
			if haveObject {
				return true, json.Unmarshal(data, pc)
			}
			if haveMap {
				return false, json.Unmarshal(data, pm)
			}
			return false, errors.New("Union does not contain object")
		}
		if v == '[' {
			if haveArray {
				return false, json.Unmarshal(data, pa)
			}
			return false, errors.New("Union does not contain array")
		}
		return false, errors.New("Cannot handle delimiter")
	}
	return false, errors.New("Cannot unmarshal union")

}

func marshalUnion(pi *int64, pf *float64, pb *bool, ps *string, haveArray bool, pa interface{}, haveObject bool, pc interface{}, haveMap bool, pm interface{}, haveEnum bool, pe interface{}, nullable bool) ([]byte, error) {
	if pi != nil {
		return json.Marshal(*pi)
	}
	if pf != nil {
		return json.Marshal(*pf)
	}
	if pb != nil {
		return json.Marshal(*pb)
	}
	if ps != nil {
		return json.Marshal(*ps)
	}
	if haveArray {
		return json.Marshal(pa)
	}
	if haveObject {
		return json.Marshal(pc)
	}
	if haveMap {
		return json.Marshal(pm)
	}
	if haveEnum {
		return json.Marshal(pe)
	}
	if nullable {
		return json.Marshal(nil)
	}
	return nil, errors.New("Union must not be null")
}
