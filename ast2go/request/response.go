package request

import "io"

type RequestStream struct {
	r io.Reader
	w io.Writer
}

func NewRequestStream(r io.Reader, w io.Writer) *RequestStream {
	return &RequestStream{r: r, w: w}
}

func (rs *RequestStream) Receive() (*GenerateSource, error) {
	src := &GenerateSource{}
	err := src.Read(rs.r)
	if err != nil {
		return nil, err
	}
	return src, nil
}

func (rs *RequestStream) Respond(src *SourceCode) error {
	enc, err := src.Encode()
	if err != nil {
		return err
	}
	_, err = rs.w.Write(enc)
	return err
}
