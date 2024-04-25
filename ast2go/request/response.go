package request

import "io"

type RequestStream struct {
	r io.Reader
	w io.Writer
}

func NewRequestStream(r io.Reader, w io.Writer) (*RequestStream, error) {
	h := &RequestStream{r: r, w: w}
	if err := h.handshake(); err != nil {
		return nil, err
	}
	return h, nil
}

func (rs *RequestStream) handshake() error {
	resp := &GeneratorResponseHeader{Version: 1}
	enc := resp.MustEncode()
	if _, err := rs.w.Write(enc); err != nil {
		return err
	}
	hdr := &GeneratorRequestHeader{}
	return hdr.Read(rs.r)
}

func (rs *RequestStream) Receive() (*GenerateSource, error) {
	src := &GenerateSource{}
	err := src.Read(rs.r)
	if err != nil {
		return nil, err
	}
	return src, nil
}

func (rs *RequestStream) respond(id uint64, status ResponseStatus, name string, code []byte, warn string) error {
	src := &SourceCode{
		ID:     id,
		Status: status,
	}
	src.SetName([]byte(name))
	src.SetCode(code)
	src.SetErrorMessage([]byte(warn))
	resp := &Response{
		Type: ResponseType_Code,
	}
	resp.SetSource(*src)
	enc := resp.MustEncode()
	_, err := rs.w.Write(enc)
	return err
}

func (rs *RequestStream) RespondSource(id uint64, name string, code []byte, warn string) error {
	return rs.respond(id, ResponseStatus_Ok, name, code, warn)
}

func (rs *RequestStream) RespondError(id uint64, err string) error {
	return rs.respond(id, ResponseStatus_Error, "", nil, err)
}

func (rs *RequestStream) RespondEnd(id uint64) error {
	resp := &Response{
		Type: ResponseType_EndOfCode,
	}
	resp.SetEnd(EndOfCode{ID: id})
	enc := resp.MustEncode()
	_, err := rs.w.Write(enc)
	return err
}
