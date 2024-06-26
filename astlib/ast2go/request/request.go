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
	CloseWithError(error)
}

var _ SourceManager = &binarySourceClients{}

type binarySourceClient struct {
	ID   uint64
	src  chan *SourceCode
	base *binarySourceClients
}

type binarySourceClients struct {
	mapLock     sync.RWMutex
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
		nextID:      1,
		w:           w,
		r:           r,
		send:        make(chan []byte),
		closeError:  make(chan struct{}),
		closeWriter: make(chan struct{}),
		onError:     onError,
	}
	go s.sender()
	go s.reader()
	return s
}

func (ss *binarySourceClients) CreateStream() SourceClient {
	ss.mapLock.Lock()
	defer ss.mapLock.Unlock()
	id := ss.nextID
	ss.nextID++
	s := &binarySourceClient{
		ID:   id,
		src:  make(chan *SourceCode),
		base: ss,
	}
	ss.streams[id] = s
	return s
}

// may returns nil
func (ss *binarySourceClients) getStream(id uint64) *binarySourceClient {
	ss.mapLock.RLock()
	defer ss.mapLock.RUnlock()
	return ss.streams[id]
}

func (ss *binarySourceClients) deleteStream(id uint64) {
	ss.mapLock.Lock()
	defer ss.mapLock.Unlock()
	delete(ss.streams, id)
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
		if code == nil {
			return nil, io.EOF
		}
		return code, nil
	}
}

func (s *binarySourceClients) closeOnce(err error) {
	s.once.Do(func() {
		s.err = err
		close(s.closeError)
		if s.onError != nil {
			s.onError(err)
		}
	})
}

func (s *binarySourceClients) CloseWithError(err error) {
	if err == nil {
		err = io.EOF
	}
	s.closeOnce(err)
}

func (s *binarySourceClient) SendComplete() {
	close(s.base.closeWriter)
}

func (s *binarySourceClients) sender() {
	hdr := &GeneratorRequestHeader{Version: 1}
	if _, err := s.w.Write(hdr.MustEncode()); err != nil {
		s.closeOnce(err)
		return
	}
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
	hdr := &GeneratorResponseHeader{}
	if err := hdr.Read(s.r); err != nil {
		s.closeOnce(err)
		return
	}
	for {
		code := &Response{}
		if err := code.Read(s.r); err != nil {
			s.closeOnce(err)
			return
		}
		if code.Type == ResponseType_EndOfCode {
			got := s.getStream(code.End().ID)
			if got == nil {
				s.closeOnce(fmt.Errorf("stream %d not found", code.End().ID))
				return
			}
			close(got.src)
			s.deleteStream(code.End().ID)
			continue
		}
		src := code.Source()
		if src == nil {
			s.closeOnce(fmt.Errorf("invalid response type %d", code.Type))
			return
		}
		id := src.ID
		got := s.getStream(id)
		if got == nil {
			if id == 0 && code.Source().Status == ResponseStatus_Error {
				s.closeOnce(fmt.Errorf("client response decoder error: " + string(code.Source().ErrorMessage)))
				return
			}
			s.closeOnce(fmt.Errorf("stream %d not found", id))
			return
		}
		got.src <- code.Source()
	}
}
