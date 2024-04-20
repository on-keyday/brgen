package request

import (
	"fmt"
	"io"
	"sync"
)

type SourceClient interface {
	SendRequest(name string, sourceJSON []byte) error
	ReceiveResponse() (*SourceCode, error)
}

type SourceManager interface {
	CreateStream() SourceClient
}

var _ SourceManager = &binarySourceClients{}

type binarySourceClient struct {
	ID   uint64
	src  chan *SourceCode
	base *binarySourceClients
}

type binarySourceClients struct {
	streams     map[uint64]*binarySourceClient
	nextID      uint64
	w           io.WriteCloser
	r           io.Reader
	closeError  chan struct{}
	closeWriter chan struct{}
	send        chan []byte
	// only set by read
	// read by senders
	err     error
	once    sync.Once
	onError func(error)
}

func NewRequestManager(r io.Reader, w io.WriteCloser, onError func(error)) SourceManager {
	s := &binarySourceClients{
		streams:     make(map[uint64]*binarySourceClient),
		nextID:      0,
		w:           w,
		r:           r,
		send:        make(chan []byte),
		closeError:  make(chan struct{}),
		closeWriter: make(chan struct{}),
	}
	go s.sender()
	go s.reader()
	return s
}

func (ss *binarySourceClients) CreateStream() SourceClient {
	id := ss.nextID
	ss.nextID++
	s := &binarySourceClient{
		ID:   id,
		base: ss,
	}
	ss.streams[id] = s
	return s
}

// may returns nil
func (ss *binarySourceClients) GetStream(id uint64) *binarySourceClient {
	return ss.streams[id]
}

func (s *binarySourceClient) SendRequest(name string, sourceJSON []byte) error {
	src := &GenerateSource{}
	src.ID = s.ID
	src.SetName([]byte(name))
	src.SetJsonText(sourceJSON)
	enc := src.MustEncode()
	select {
	case s.base.send <- enc:
		return nil
	case <-s.base.closeWriter:
		return fmt.Errorf("stream %d closed", s.ID)
	case <-s.base.closeError:
		return s.base.err
	}
}

func (s *binarySourceClient) ReceiveResponse() (*SourceCode, error) {
	select {
	case <-s.base.closeError:
		return nil, s.base.err
	case code := <-s.src:
		return code, nil
	}
}

func (s *binarySourceClients) closeOnce(err error) {
	s.once.Do(func() {
		s.err = err
		close(s.closeError)
		s.onError(err)
	})
}

func (s *binarySourceClient) SendComplete() {
	close(s.base.closeWriter)
}

func (s *binarySourceClients) sender() {
	for {
		select {
		case <-s.closeError:
			return
		case <-s.closeWriter:
			return
		case enc := <-s.send:
			if _, err := s.w.Write(enc); err != nil {
				s.closeOnce(err)
				return
			}
		}
	}
}

func (s *binarySourceClients) reader() {
	for {
		code := &SourceCode{}
		if err := code.Read(s.r); err != nil {
			s.closeOnce(err)
			return
		}
		got := s.GetStream(code.ID)
		if got == nil {
			s.closeOnce(fmt.Errorf("stream %d not found", code.ID))
			return
		}
	}
}
