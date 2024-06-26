// Code generated by json2go. DO NOT EDIT.
package request

import (
	"bufio"
	"bytes"
	"encoding/binary"
	"fmt"
	"io"
)

type ResponseType uint8

const (
	ResponseType_Code      ResponseType = 0
	ResponseType_EndOfCode ResponseType = 1
)

func (t ResponseType) String() string {
	switch t {
	case ResponseType_Code:
		return "Code"
	case ResponseType_EndOfCode:
		return "EndOfCode"
	}
	return fmt.Sprintf("ResponseType(%d)", t)
}

type ResponseStatus uint8

const (
	ResponseStatus_Ok    ResponseStatus = 0
	ResponseStatus_Error ResponseStatus = 1
)

func (t ResponseStatus) String() string {
	switch t {
	case ResponseStatus_Ok:
		return "Ok"
	case ResponseStatus_Error:
		return "Error"
	}
	return fmt.Sprintf("ResponseStatus(%d)", t)
}

type GenerateSource struct {
	ID       uint64
	NameLen  uint64
	Name     []uint8
	Len      uint64
	JsonText []uint8
}
type VisitorTEDIH interface {
	Visit(v VisitorTEDIH, name string, field any)
}
type VisitorTEDIHFunc func(v VisitorTEDIH, name string, field any)

func (f VisitorTEDIHFunc) Visit(v VisitorTEDIH, name string, field any) {
	f(v, name, field)
}

type GeneratorRequestHeader struct {
	Version uint32
}
type EndOfCode struct {
	ID uint64
}
type SourceCode struct {
	ID           uint64
	Status       ResponseStatus
	NameLen      uint64
	Name         []uint8
	ErrorLen     uint64
	ErrorMessage []uint8
	Len          uint64
	Code         []uint8
}
type GeneratorResponseHeader struct {
	Version uint32
}
type GeneratorRequest struct {
	Header GeneratorRequestHeader
	Source []GenerateSource
}
type Response struct {
	Type      ResponseType
	union_13_ struct {
		Source SourceCode
	}
	union_14_ struct {
		End EndOfCode
	}
}
type GeneratorResponse struct {
	Header GeneratorResponseHeader
	Source []Response
}

func (t *GenerateSource) SetName(v []uint8) bool {
	if uint64(len(v)) > uint64(^uint64(0)) {
		return false
	}
	t.NameLen = uint64(uint64(len(v)))
	t.Name = v
	return true
}
func (t *GenerateSource) SetJsonText(v []uint8) bool {
	if uint64(len(v)) > uint64(^uint64(0)) {
		return false
	}
	t.Len = uint64(uint64(len(v)))
	t.JsonText = v
	return true
}
func (t *GenerateSource) Visit(v VisitorTEDIH) {
	v.Visit(v, "ID", t.ID)
	v.Visit(v, "NameLen", t.NameLen)
	v.Visit(v, "Name", t.Name)
	v.Visit(v, "Len", t.Len)
	v.Visit(v, "JsonText", t.JsonText)
}
func (t *GenerateSource) Write(w io.Writer) (err error) {
	tmp1 := [8]byte{}
	binary.BigEndian.PutUint64(tmp1[:], uint64(t.ID))
	if n, err := w.Write(tmp1[:]); err != nil || n != 8 {
		return fmt.Errorf("encode t.ID: %w", err)
	}
	tmp2 := [8]byte{}
	binary.BigEndian.PutUint64(tmp2[:], uint64(t.NameLen))
	if n, err := w.Write(tmp2[:]); err != nil || n != 8 {
		return fmt.Errorf("encode t.NameLen: %w", err)
	}
	len_Name := int(t.NameLen)
	if len(t.Name) != len_Name {
		return fmt.Errorf("encode Name: expect %d bytes but got %d bytes", len_Name, len(t.Name))
	}
	if n, err := w.Write(t.Name); err != nil || n != len(t.Name) {
		return fmt.Errorf("encode Name: %w", err)
	}
	tmp3 := [8]byte{}
	binary.BigEndian.PutUint64(tmp3[:], uint64(t.Len))
	if n, err := w.Write(tmp3[:]); err != nil || n != 8 {
		return fmt.Errorf("encode t.Len: %w", err)
	}
	len_JsonText := int(t.Len)
	if len(t.JsonText) != len_JsonText {
		return fmt.Errorf("encode JsonText: expect %d bytes but got %d bytes", len_JsonText, len(t.JsonText))
	}
	if n, err := w.Write(t.JsonText); err != nil || n != len(t.JsonText) {
		return fmt.Errorf("encode JsonText: %w", err)
	}
	return nil
}
func (t *GenerateSource) Encode() ([]byte, error) {
	w := bytes.NewBuffer(make([]byte, 0, 16))
	if err := t.Write(w); err != nil {
		return nil, err
	}
	return w.Bytes(), nil
}
func (t *GenerateSource) MustEncode() []byte {
	buf, err := t.Encode()
	if err != nil {
		panic(err)
	}
	return buf
}
func (t *GenerateSource) Read(r io.Reader) (err error) {
	tmpID := [8]byte{}
	n_ID, err := io.ReadFull(r, tmpID[:])
	if err != nil {
		if err == io.ErrUnexpectedEOF || n_ID != 8 /*stdlib bug?*/ {
			return fmt.Errorf("read ID: %w: expect 8 bytes but read %d bytes", io.ErrUnexpectedEOF, n_ID)
		}
		return err
	}
	t.ID = uint64(binary.BigEndian.Uint64(tmpID[:]))
	tmpNameLen := [8]byte{}
	n_NameLen, err := io.ReadFull(r, tmpNameLen[:])
	if err != nil {
		if err == io.ErrUnexpectedEOF || n_NameLen != 8 /*stdlib bug?*/ {
			return fmt.Errorf("read NameLen: %w: expect 8 bytes but read %d bytes", io.ErrUnexpectedEOF, n_NameLen)
		}
		return err
	}
	t.NameLen = uint64(binary.BigEndian.Uint64(tmpNameLen[:]))
	len_Name := int(t.NameLen)
	if len_Name != 0 {
		tmpName := make([]byte, len_Name)
		n_Name, err := io.ReadFull(r, tmpName[:])
		if err != nil {
			if err == io.ErrUnexpectedEOF || n_Name != len_Name /*stdlib bug?*/ {
				return fmt.Errorf("read Name: %w: expect %d bytes but read %d bytes", io.ErrUnexpectedEOF, len_Name, n_Name)
			}
			return err
		}
		t.Name = tmpName[:]
	} else {
		t.Name = nil
	}
	tmpLen := [8]byte{}
	n_Len, err := io.ReadFull(r, tmpLen[:])
	if err != nil {
		if err == io.ErrUnexpectedEOF || n_Len != 8 /*stdlib bug?*/ {
			return fmt.Errorf("read Len: %w: expect 8 bytes but read %d bytes", io.ErrUnexpectedEOF, n_Len)
		}
		return err
	}
	t.Len = uint64(binary.BigEndian.Uint64(tmpLen[:]))
	len_JsonText := int(t.Len)
	if len_JsonText != 0 {
		tmpJsonText := make([]byte, len_JsonText)
		n_JsonText, err := io.ReadFull(r, tmpJsonText[:])
		if err != nil {
			if err == io.ErrUnexpectedEOF || n_JsonText != len_JsonText /*stdlib bug?*/ {
				return fmt.Errorf("read JsonText: %w: expect %d bytes but read %d bytes", io.ErrUnexpectedEOF, len_JsonText, n_JsonText)
			}
			return err
		}
		t.JsonText = tmpJsonText[:]
	} else {
		t.JsonText = nil
	}
	return nil
}

func (t *GenerateSource) Decode(d []byte) (int, error) {
	r := bytes.NewReader(d)
	err := t.Read(r)
	return int(int(r.Size()) - r.Len()), err
}
func (t *GeneratorRequestHeader) Visit(v VisitorTEDIH) {
	v.Visit(v, "Version", t.Version)
}
func (t *GeneratorRequestHeader) Write(w io.Writer) (err error) {
	tmp4 := [4]byte{}
	binary.BigEndian.PutUint32(tmp4[:], uint32(t.Version))
	if n, err := w.Write(tmp4[:]); err != nil || n != 4 {
		return fmt.Errorf("encode t.Version: %w", err)
	}
	return nil
}
func (t *GeneratorRequestHeader) Encode() ([]byte, error) {
	w := bytes.NewBuffer(make([]byte, 0, 4))
	if err := t.Write(w); err != nil {
		return nil, err
	}
	return w.Bytes(), nil
}
func (t *GeneratorRequestHeader) MustEncode() []byte {
	buf, err := t.Encode()
	if err != nil {
		panic(err)
	}
	return buf
}
func (t *GeneratorRequestHeader) Read(r io.Reader) (err error) {
	tmpVersion := [4]byte{}
	n_Version, err := io.ReadFull(r, tmpVersion[:])
	if err != nil {
		if err == io.ErrUnexpectedEOF || n_Version != 4 /*stdlib bug?*/ {
			return fmt.Errorf("read Version: %w: expect 4 bytes but read %d bytes", io.ErrUnexpectedEOF, n_Version)
		}
		return err
	}
	t.Version = uint32(binary.BigEndian.Uint32(tmpVersion[:]))
	return nil
}

func (t *GeneratorRequestHeader) Decode(d []byte) (int, error) {
	r := bytes.NewReader(d)
	err := t.Read(r)
	return int(int(r.Size()) - r.Len()), err
}
func (t *EndOfCode) Visit(v VisitorTEDIH) {
	v.Visit(v, "ID", t.ID)
}
func (t *EndOfCode) Write(w io.Writer) (err error) {
	tmp5 := [8]byte{}
	binary.BigEndian.PutUint64(tmp5[:], uint64(t.ID))
	if n, err := w.Write(tmp5[:]); err != nil || n != 8 {
		return fmt.Errorf("encode t.ID: %w", err)
	}
	return nil
}
func (t *EndOfCode) Encode() ([]byte, error) {
	w := bytes.NewBuffer(make([]byte, 0, 8))
	if err := t.Write(w); err != nil {
		return nil, err
	}
	return w.Bytes(), nil
}
func (t *EndOfCode) MustEncode() []byte {
	buf, err := t.Encode()
	if err != nil {
		panic(err)
	}
	return buf
}
func (t *EndOfCode) Read(r io.Reader) (err error) {
	tmpID := [8]byte{}
	n_ID, err := io.ReadFull(r, tmpID[:])
	if err != nil {
		if err == io.ErrUnexpectedEOF || n_ID != 8 /*stdlib bug?*/ {
			return fmt.Errorf("read ID: %w: expect 8 bytes but read %d bytes", io.ErrUnexpectedEOF, n_ID)
		}
		return err
	}
	t.ID = uint64(binary.BigEndian.Uint64(tmpID[:]))
	return nil
}

func (t *EndOfCode) Decode(d []byte) (int, error) {
	r := bytes.NewReader(d)
	err := t.Read(r)
	return int(int(r.Size()) - r.Len()), err
}
func (t *SourceCode) SetName(v []uint8) bool {
	if uint64(len(v)) > uint64(^uint64(0)) {
		return false
	}
	t.NameLen = uint64(uint64(len(v)))
	t.Name = v
	return true
}
func (t *SourceCode) SetErrorMessage(v []uint8) bool {
	if uint64(len(v)) > uint64(^uint64(0)) {
		return false
	}
	t.ErrorLen = uint64(uint64(len(v)))
	t.ErrorMessage = v
	return true
}
func (t *SourceCode) SetCode(v []uint8) bool {
	if uint64(len(v)) > uint64(^uint64(0)) {
		return false
	}
	t.Len = uint64(uint64(len(v)))
	t.Code = v
	return true
}
func (t *SourceCode) Visit(v VisitorTEDIH) {
	v.Visit(v, "ID", t.ID)
	v.Visit(v, "Status", t.Status)
	v.Visit(v, "NameLen", t.NameLen)
	v.Visit(v, "Name", t.Name)
	v.Visit(v, "ErrorLen", t.ErrorLen)
	v.Visit(v, "ErrorMessage", t.ErrorMessage)
	v.Visit(v, "Len", t.Len)
	v.Visit(v, "Code", t.Code)
}
func (t *SourceCode) Write(w io.Writer) (err error) {
	tmp6 := [8]byte{}
	binary.BigEndian.PutUint64(tmp6[:], uint64(t.ID))
	if n, err := w.Write(tmp6[:]); err != nil || n != 8 {
		return fmt.Errorf("encode t.ID: %w", err)
	}
	if n, err := w.Write([]byte{byte(t.Status)}); err != nil || n != 1 {
		return fmt.Errorf("encode t.Status: %w", err)
	}
	tmp7 := [8]byte{}
	binary.BigEndian.PutUint64(tmp7[:], uint64(t.NameLen))
	if n, err := w.Write(tmp7[:]); err != nil || n != 8 {
		return fmt.Errorf("encode t.NameLen: %w", err)
	}
	len_Name := int(t.NameLen)
	if len(t.Name) != len_Name {
		return fmt.Errorf("encode Name: expect %d bytes but got %d bytes", len_Name, len(t.Name))
	}
	if n, err := w.Write(t.Name); err != nil || n != len(t.Name) {
		return fmt.Errorf("encode Name: %w", err)
	}
	tmp8 := [8]byte{}
	binary.BigEndian.PutUint64(tmp8[:], uint64(t.ErrorLen))
	if n, err := w.Write(tmp8[:]); err != nil || n != 8 {
		return fmt.Errorf("encode t.ErrorLen: %w", err)
	}
	len_ErrorMessage := int(t.ErrorLen)
	if len(t.ErrorMessage) != len_ErrorMessage {
		return fmt.Errorf("encode ErrorMessage: expect %d bytes but got %d bytes", len_ErrorMessage, len(t.ErrorMessage))
	}
	if n, err := w.Write(t.ErrorMessage); err != nil || n != len(t.ErrorMessage) {
		return fmt.Errorf("encode ErrorMessage: %w", err)
	}
	tmp9 := [8]byte{}
	binary.BigEndian.PutUint64(tmp9[:], uint64(t.Len))
	if n, err := w.Write(tmp9[:]); err != nil || n != 8 {
		return fmt.Errorf("encode t.Len: %w", err)
	}
	len_Code := int(t.Len)
	if len(t.Code) != len_Code {
		return fmt.Errorf("encode Code: expect %d bytes but got %d bytes", len_Code, len(t.Code))
	}
	if n, err := w.Write(t.Code); err != nil || n != len(t.Code) {
		return fmt.Errorf("encode Code: %w", err)
	}
	return nil
}
func (t *SourceCode) Encode() ([]byte, error) {
	w := bytes.NewBuffer(make([]byte, 0, 17))
	if err := t.Write(w); err != nil {
		return nil, err
	}
	return w.Bytes(), nil
}
func (t *SourceCode) MustEncode() []byte {
	buf, err := t.Encode()
	if err != nil {
		panic(err)
	}
	return buf
}
func (t *SourceCode) Read(r io.Reader) (err error) {
	tmpID := [8]byte{}
	n_ID, err := io.ReadFull(r, tmpID[:])
	if err != nil {
		if err == io.ErrUnexpectedEOF || n_ID != 8 /*stdlib bug?*/ {
			return fmt.Errorf("read ID: %w: expect 8 bytes but read %d bytes", io.ErrUnexpectedEOF, n_ID)
		}
		return err
	}
	t.ID = uint64(binary.BigEndian.Uint64(tmpID[:]))
	tmpStatus := [1]byte{}
	n_Status, err := io.ReadFull(r, tmpStatus[:])
	if err != nil {
		if err == io.ErrUnexpectedEOF || n_Status != 1 /*stdlib bug?*/ {
			return fmt.Errorf("read Status: %w: expect 1 byte but read %d bytes", io.ErrUnexpectedEOF, n_Status)
		}
		return err
	}
	t.Status = ResponseStatus(tmpStatus[0])
	tmpNameLen := [8]byte{}
	n_NameLen, err := io.ReadFull(r, tmpNameLen[:])
	if err != nil {
		if err == io.ErrUnexpectedEOF || n_NameLen != 8 /*stdlib bug?*/ {
			return fmt.Errorf("read NameLen: %w: expect 8 bytes but read %d bytes", io.ErrUnexpectedEOF, n_NameLen)
		}
		return err
	}
	t.NameLen = uint64(binary.BigEndian.Uint64(tmpNameLen[:]))
	len_Name := int(t.NameLen)
	if len_Name != 0 {
		tmpName := make([]byte, len_Name)
		n_Name, err := io.ReadFull(r, tmpName[:])
		if err != nil {
			if err == io.ErrUnexpectedEOF || n_Name != len_Name /*stdlib bug?*/ {
				return fmt.Errorf("read Name: %w: expect %d bytes but read %d bytes", io.ErrUnexpectedEOF, len_Name, n_Name)
			}
			return err
		}
		t.Name = tmpName[:]
	} else {
		t.Name = nil
	}
	tmpErrorLen := [8]byte{}
	n_ErrorLen, err := io.ReadFull(r, tmpErrorLen[:])
	if err != nil {
		if err == io.ErrUnexpectedEOF || n_ErrorLen != 8 /*stdlib bug?*/ {
			return fmt.Errorf("read ErrorLen: %w: expect 8 bytes but read %d bytes", io.ErrUnexpectedEOF, n_ErrorLen)
		}
		return err
	}
	t.ErrorLen = uint64(binary.BigEndian.Uint64(tmpErrorLen[:]))
	len_ErrorMessage := int(t.ErrorLen)
	if len_ErrorMessage != 0 {
		tmpErrorMessage := make([]byte, len_ErrorMessage)
		n_ErrorMessage, err := io.ReadFull(r, tmpErrorMessage[:])
		if err != nil {
			if err == io.ErrUnexpectedEOF || n_ErrorMessage != len_ErrorMessage /*stdlib bug?*/ {
				return fmt.Errorf("read ErrorMessage: %w: expect %d bytes but read %d bytes", io.ErrUnexpectedEOF, len_ErrorMessage, n_ErrorMessage)
			}
			return err
		}
		t.ErrorMessage = tmpErrorMessage[:]
	} else {
		t.ErrorMessage = nil
	}
	tmpLen := [8]byte{}
	n_Len, err := io.ReadFull(r, tmpLen[:])
	if err != nil {
		if err == io.ErrUnexpectedEOF || n_Len != 8 /*stdlib bug?*/ {
			return fmt.Errorf("read Len: %w: expect 8 bytes but read %d bytes", io.ErrUnexpectedEOF, n_Len)
		}
		return err
	}
	t.Len = uint64(binary.BigEndian.Uint64(tmpLen[:]))
	len_Code := int(t.Len)
	if len_Code != 0 {
		tmpCode := make([]byte, len_Code)
		n_Code, err := io.ReadFull(r, tmpCode[:])
		if err != nil {
			if err == io.ErrUnexpectedEOF || n_Code != len_Code /*stdlib bug?*/ {
				return fmt.Errorf("read Code: %w: expect %d bytes but read %d bytes", io.ErrUnexpectedEOF, len_Code, n_Code)
			}
			return err
		}
		t.Code = tmpCode[:]
	} else {
		t.Code = nil
	}
	return nil
}

func (t *SourceCode) Decode(d []byte) (int, error) {
	r := bytes.NewReader(d)
	err := t.Read(r)
	return int(int(r.Size()) - r.Len()), err
}
func (t *GeneratorResponseHeader) Visit(v VisitorTEDIH) {
	v.Visit(v, "Version", t.Version)
}
func (t *GeneratorResponseHeader) Write(w io.Writer) (err error) {
	tmp10 := [4]byte{}
	binary.BigEndian.PutUint32(tmp10[:], uint32(t.Version))
	if n, err := w.Write(tmp10[:]); err != nil || n != 4 {
		return fmt.Errorf("encode t.Version: %w", err)
	}
	return nil
}
func (t *GeneratorResponseHeader) Encode() ([]byte, error) {
	w := bytes.NewBuffer(make([]byte, 0, 4))
	if err := t.Write(w); err != nil {
		return nil, err
	}
	return w.Bytes(), nil
}
func (t *GeneratorResponseHeader) MustEncode() []byte {
	buf, err := t.Encode()
	if err != nil {
		panic(err)
	}
	return buf
}
func (t *GeneratorResponseHeader) Read(r io.Reader) (err error) {
	tmpVersion := [4]byte{}
	n_Version, err := io.ReadFull(r, tmpVersion[:])
	if err != nil {
		if err == io.ErrUnexpectedEOF || n_Version != 4 /*stdlib bug?*/ {
			return fmt.Errorf("read Version: %w: expect 4 bytes but read %d bytes", io.ErrUnexpectedEOF, n_Version)
		}
		return err
	}
	t.Version = uint32(binary.BigEndian.Uint32(tmpVersion[:]))
	return nil
}

func (t *GeneratorResponseHeader) Decode(d []byte) (int, error) {
	r := bytes.NewReader(d)
	err := t.Read(r)
	return int(int(r.Size()) - r.Len()), err
}
func (t *GeneratorRequest) Visit(v VisitorTEDIH) {
	v.Visit(v, "Header", t.Header)
	v.Visit(v, "Source", t.Source)
}
func (t *GeneratorRequest) Write(w io.Writer) (err error) {
	if err := t.Header.Write(w); err != nil {
		return fmt.Errorf("encode Header: %w", err)
	}
	for _, v := range t.Source {
		if err := v.Write(w); err != nil {
			return fmt.Errorf("encode Source: %w", err)
		}
	}
	return nil
}
func (t *GeneratorRequest) Encode() ([]byte, error) {
	w := bytes.NewBuffer(make([]byte, 0, 4))
	if err := t.Write(w); err != nil {
		return nil, err
	}
	return w.Bytes(), nil
}
func (t *GeneratorRequest) MustEncode() []byte {
	buf, err := t.Encode()
	if err != nil {
		panic(err)
	}
	return buf
}
func (t *GeneratorRequest) Read(r io.Reader) (err error) {
	if err := t.Header.Read(r); err != nil {
		return fmt.Errorf("read Header: %w", err)
	}
	tmp_byte_scanner11_ := bufio.NewReader(r)
	old_r_Source := r
	r = tmp_byte_scanner11_
	for {
		_, err := tmp_byte_scanner11_.ReadByte()
		if err != nil {
			if err != io.EOF {
				return fmt.Errorf("read Source: %w", err)
			}
			break
		}
		if err := tmp_byte_scanner11_.UnreadByte(); err != nil {
			return fmt.Errorf("read Source: unexpected unread error: %w", err)
		}
		var tmp12_ GenerateSource
		if err := tmp12_.Read(r); err != nil {
			return fmt.Errorf("read Source: %w", err)
		}
		t.Source = append(t.Source, tmp12_)
	}
	r = old_r_Source
	return nil
}

func (t *GeneratorRequest) Decode(d []byte) (int, error) {
	r := bytes.NewReader(d)
	err := t.Read(r)
	return int(int(r.Size()) - r.Len()), err
}
func (t *Response) End() *EndOfCode {
	if t.Type == ResponseType_Code {
		return nil
	} else if t.Type == ResponseType_EndOfCode {
		tmp := EndOfCode(t.union_14_.End)
		return &tmp
	}
	return nil
}
func (t *Response) SetEnd(v EndOfCode) bool {
	if t.Type == ResponseType_Code {
		return false
	} else if t.Type == ResponseType_EndOfCode {
		t.union_14_.End = EndOfCode(v)
		return true
	}
	return false
}
func (t *Response) Source() *SourceCode {
	if t.Type == ResponseType_Code {
		tmp := SourceCode(t.union_13_.Source)
		return &tmp
	}
	return nil
}
func (t *Response) SetSource(v SourceCode) bool {
	if t.Type == ResponseType_Code {
		t.union_13_.Source = SourceCode(v)
		return true
	}
	return false
}
func (t *Response) Visit(v VisitorTEDIH) {
	v.Visit(v, "Type", t.Type)
	v.Visit(v, "End", (t.End()))
	v.Visit(v, "Source", (t.Source()))
}
func (t *Response) Write(w io.Writer) (err error) {
	if n, err := w.Write([]byte{byte(t.Type)}); err != nil || n != 1 {
		return fmt.Errorf("encode t.Type: %w", err)
	}
	switch t.Type {
	case ResponseType_Code:
		if err := t.union_13_.Source.Write(w); err != nil {
			return fmt.Errorf("encode Source: %w", err)
		}
	case ResponseType_EndOfCode:
		if err := t.union_14_.End.Write(w); err != nil {
			return fmt.Errorf("encode End: %w", err)
		}
	}
	return nil
}
func (t *Response) Encode() ([]byte, error) {
	w := bytes.NewBuffer(make([]byte, 0, 1))
	if err := t.Write(w); err != nil {
		return nil, err
	}
	return w.Bytes(), nil
}
func (t *Response) MustEncode() []byte {
	buf, err := t.Encode()
	if err != nil {
		panic(err)
	}
	return buf
}
func (t *Response) Read(r io.Reader) (err error) {
	tmpType := [1]byte{}
	n_Type, err := io.ReadFull(r, tmpType[:])
	if err != nil {
		if err == io.ErrUnexpectedEOF || n_Type != 1 /*stdlib bug?*/ {
			return fmt.Errorf("read Type: %w: expect 1 byte but read %d bytes", io.ErrUnexpectedEOF, n_Type)
		}
		return err
	}
	t.Type = ResponseType(tmpType[0])
	switch t.Type {
	case ResponseType_Code:
		if err := t.union_13_.Source.Read(r); err != nil {
			return fmt.Errorf("read Source: %w", err)
		}
	case ResponseType_EndOfCode:
		if err := t.union_14_.End.Read(r); err != nil {
			return fmt.Errorf("read End: %w", err)
		}
	}
	return nil
}

func (t *Response) Decode(d []byte) (int, error) {
	r := bytes.NewReader(d)
	err := t.Read(r)
	return int(int(r.Size()) - r.Len()), err
}
func (t *GeneratorResponse) Visit(v VisitorTEDIH) {
	v.Visit(v, "Header", t.Header)
	v.Visit(v, "Source", t.Source)
}
func (t *GeneratorResponse) Write(w io.Writer) (err error) {
	if err := t.Header.Write(w); err != nil {
		return fmt.Errorf("encode Header: %w", err)
	}
	for _, v := range t.Source {
		if err := v.Write(w); err != nil {
			return fmt.Errorf("encode Source: %w", err)
		}
	}
	return nil
}
func (t *GeneratorResponse) Encode() ([]byte, error) {
	w := bytes.NewBuffer(make([]byte, 0, 4))
	if err := t.Write(w); err != nil {
		return nil, err
	}
	return w.Bytes(), nil
}
func (t *GeneratorResponse) MustEncode() []byte {
	buf, err := t.Encode()
	if err != nil {
		panic(err)
	}
	return buf
}
func (t *GeneratorResponse) Read(r io.Reader) (err error) {
	if err := t.Header.Read(r); err != nil {
		return fmt.Errorf("read Header: %w", err)
	}
	tmp_byte_scanner15_ := bufio.NewReader(r)
	old_r_Source := r
	r = tmp_byte_scanner15_
	for {
		_, err := tmp_byte_scanner15_.ReadByte()
		if err != nil {
			if err != io.EOF {
				return fmt.Errorf("read Source: %w", err)
			}
			break
		}
		if err := tmp_byte_scanner15_.UnreadByte(); err != nil {
			return fmt.Errorf("read Source: unexpected unread error: %w", err)
		}
		var tmp16_ Response
		if err := tmp16_.Read(r); err != nil {
			return fmt.Errorf("read Source: %w", err)
		}
		t.Source = append(t.Source, tmp16_)
	}
	r = old_r_Source
	return nil
}

func (t *GeneratorResponse) Decode(d []byte) (int, error) {
	r := bytes.NewReader(d)
	err := t.Read(r)
	return int(int(r.Size()) - r.Len()), err
}
